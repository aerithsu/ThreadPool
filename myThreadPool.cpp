#include"myThreadPool.h"

constexpr int TASK_MAX_THRESHOLD = 1024;
constexpr int THREAD_MAX_THRESHOLD = 16;
constexpr int THREAD_MAX_IDLE_TIME = 60;

ThreadPool::ThreadPool()
        : initThreadSize_{},
          taskSize_{0},
          taskQueThreshold_{TASK_MAX_THRESHOLD},
          threadSizeThreshold_{THREAD_MAX_THRESHOLD},
          poolMode_{PoolMode::MODE_FIXED} {}

ThreadPool::~ThreadPool() {
    isPoolRunning_ = false;
    //等待线程池里面所有的线程返回,有两种状态:1.阻塞的 2.正在执行任务中
    //notEmpty_.notify_all();
    std::unique_lock lock{taskQueMtx};
    notEmpty_.notify_all();
    exitCond_.wait(lock, [&]() { return threads_.empty(); });
    std::cout << "Exit ThreadPool" << std::endl;
}

void ThreadPool::setMode(PoolMode mode) {
    //线程池已经在运行状态,不能设置运行模式了,必须先调用这个方法再start线程池
    if (checkRunningState())
        return;
    poolMode_ = mode;
}

//void ThreadPool::setInitThreadSize(int sz) {
//	initThreadSize_ = sz;
//}
void ThreadPool::setTaskQueThreshold(int threshold) {
    taskQueThreshold_ = threshold;
}

Result ThreadPool::submitTask(std::shared_ptr<Task> task) {
    //获取锁
    std::unique_lock<std::mutex> lock{taskQueMtx};

    //线程的通信,等待任务队列有空余,等待任务队列有空余
    //wait做了什么1.释放锁2.线程进入wait状态,直到等待的条件为true
    if (!notFull_.wait_for(lock, std::chrono::seconds{1}, [&]() { return taskQue_.size() < taskQueThreshold_; })) {
        //wait_for返回false代表等待了指定时间后条件仍未满足
        std::cerr << "task queue is full, submit task failed!" << std::endl;
        //return task->getResult(); 为什么不能这样返回呢:线程执行完task,task对象就被析构掉了,依赖于task对象的Result对象已经没了
        //所有不能让Result依赖于task
        return {task, false}; //使用了RVO,虽然没有拷贝构造也没有移动构造,但是可以直接构造
    }

    //如果为空余,把任务放入任务队列里
    taskQue_.emplace(task);
    ++taskSize_;
    //因为放了任务,任务队列肯定不为空,在notEmpty上进行通知
    notEmpty_.notify_all();
    //cached模式,任务处理比较紧急,可以快速处理完从而不会让线程数目太多,适合小而快的任务
    //需要根据任务数量和空闲线程的数量,判断是否需要增加线程数目
    if (poolMode_ == PoolMode::MODE_CACHED && taskSize_ > idleThreadSize && curThreadSize < threadSizeThreshold_) {
        //创建新线程
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int id = ptr->getID();
        threads_.emplace(id, std::move(ptr));
        curThreadSize++;
        idleThreadSize++;
        threads_[id]->start(); //使用了已经移动的unique_ptr
    }
    return {task};
}

//使用了已经移动的unique_ptr
void ThreadPool::start(int size) {
    isPoolRunning_ = true;
    initThreadSize_ = size;
    //创建线程对象
    for (int i = 0; i < initThreadSize_; ++i) {
        //创建thread线程对象的时候,把线程函数给到thread线程对象
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int id = ptr->getID();
        threads_.emplace(id, std::move(ptr));
    }
    curThreadSize = initThreadSize_;
    //启动所有线程
    for (int i = 0; i < initThreadSize_; ++i) {
        threads_[i]->start();
        ++idleThreadSize;//记录初始空闲线程的数量
    }
}

//消费Task队列里面的任务
void ThreadPool::threadFunc(int thread_id) {
    //随着线程池的结束,线程也要结束
    //线程不断地循环从线程池里取出任务
    auto lastTime = std::chrono::high_resolution_clock::now();
    while (isPoolRunning_) {
        //先获取锁
        std::unique_lock<std::mutex> lock{taskQueMtx};
        //cached模式下,有可能已经创建了很多线程,但空闲时间超过60s,应该把多余的线程结束回收掉(超过initThreadSize的线程进行回收)
        //若当前时间 - 上一次线程执行的时间 > 60s
        //锁 + 双重判断
        while (isPoolRunning_ && taskQue_.empty()) {
            if (poolMode_ == PoolMode::MODE_CACHED) {
                //每一秒钟返回一次,区分超时返回还是有任务待执行呢
                if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds{1})) {
                    auto now = std::chrono::high_resolution_clock::now();
                    auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime).count();
                    if (dur >= THREAD_MAX_IDLE_TIME && curThreadSize > initThreadSize_) {
                        //回收当前线程
                        //记录线程数量的变量修改
                        --idleThreadSize;
                        --curThreadSize;
                        //从容器里移除线程对象
                        threads_.erase(thread_id);
                        return;//结束该线程
                    }
                }
            } else {
                //等待notEmpty条件,这里不用等待一段时间就返回,可以一直等待
                notEmpty_.wait(lock);
            }
            //if (!isPoolRunning_) {
            //    threads_.erase(thread_id);
            //    std::cout << "thread " << thread_id << " exit!" << threads_.size() << "thread(s) left" << std::endl;
            //    if (threads_.empty())
            //        exitCond_.notify_all();
            //    return;
            //}
        }
        //线程池结束,回收线程资源,这里让正在阻塞的资源返回
        if (!isPoolRunning_) {
            break;
        }
        --idleThreadSize;//分配一个任务的时候减少空闲线程数
        //从任务队列取出一个任务
        auto task = taskQue_.front();
        taskQue_.pop();
        --taskSize_;
        //取完一个Task后就能释放锁了

        //如果task队列里面还要任务,继续通知其他线程取任务
        if (!taskQue_.empty())
            notEmpty_.notify_all();
        //取完一个任务就可以通知生产者线程继续工作了
        notFull_.notify_all();
        lock.unlock();
        //当前线程执行这个任务
        task->exec();
        //执行完任务后继续重复做取任务的工作
        idleThreadSize++;//处理完任务空闲线程数+1
        //更新线程执行完任务的时间
        lastTime = std::chrono::high_resolution_clock::now();
    }
    std::unique_lock lock{taskQueMtx};
    //正在执行任务的线程执行完这个任务看到isPoolRunning为false的时候退出while循环
    threads_.erase(thread_id);
    std::cout << "thread " << thread_id << " exit!" << std::endl;
    //notify和wait要成对出现
    if (threads_.empty())
        exitCond_.notify_all();
}

void ThreadPool::setThreadSizeThreshold(int size) {
    //只能在start线程池之前调用这些设置线程池参数的方法
    if (checkRunningState())
        return;
    if (poolMode_ == PoolMode::MODE_CACHED) {
        threadSizeThreshold_ = size;
    }
}

void Task::exec() {
    //加上判断可以让代码更健壮
    if (result_)
        result_->setVal(run()); //这里发生了多态调用
}

void Task::setResult(Result *res) {
    result_ = res;
}