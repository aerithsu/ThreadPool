//
// Created by 27262 on 2024/5/25.
//

#ifndef THREADPOOL_MYTHREADPOOL_H
#define THREADPOOL_MYTHREADPOOL_H

#pragma once

#include<atomic>
#include<utility>
#include<vector>
#include<thread>
#include<functional>
#include<mutex>
#include<condition_variable>
#include<queue>
#include<iostream>
#include<chrono>
#include<memory>
#include<unordered_map>

class Result;

using std::make_unique;

//Any类型:可以接受任意数据的类型
class Any {
public:
    //这个构造函数能让Any类型接受任意的类型的数据
    template<class T>
    Any(T data) :base_(make_unique<Derive < T>>

    (data)) {}

    Any(Any &&) = default;

    Any &operator=(Any &&) = default;

    template<class T>
    T cast() {
        //从Base里面找到其所指向的派生类对象,从它里面取出data成员变量
        //基类指针转向派生类指针
        auto *pd = dynamic_cast<Derive <T> *>(base_.get());
        if (pd == nullptr) {
            throw std::runtime_error("type is unmatched");
        }
        return pd->data_;
    }

    Any() = default;

    ~Any() = default;

private:
    //基类类型
    class Base {
    public:
        virtual ~Base() = default;
    };

    //派生类类型
    template<class T>
    class Derive : public Base {
    public:
        Derive(T data) : data_(data) {}

        T data_;
    };

private:
    //不管是什么类型的unique_ptr<Derived<T>>都能被接收
    std::unique_ptr<Base> base_;
};

class Semaphore {
public:
    Semaphore(int limit = 0) : resLimit_{limit} {}

    ~Semaphore() = default;

    //p操作,获取一个信号量资源
    void wait() {
        std::unique_lock<std::mutex> lock{mtx_};
        //等待信号量有资源,没有资源的话阻塞此线程
        cond_.wait(lock, [this]() { return resLimit_ > 0; });
        --resLimit_;
    }

    //v操作,增加一个信号量资源
    void post() {
        std::unique_lock<std::mutex> lock{mtx_};
        resLimit_++;
        cond_.notify_all();
    }

private:
    int resLimit_{};
    std::mutex mtx_{};
    std::condition_variable cond_{};
};

//用户可以自定义任意任务类型,从Task继承而来,从写run方法,实现自定义任务处理
class Task {
public:
    Task() = default;

    //执行的都是基类的exec方法
    void exec();

    void setResult(Result *res);

    virtual Any run() = 0;

    virtual ~Task() = default;

private:
    //不用智能指针是因为防止交叉引用
    Result *result_{}; //result的生命周期长于这个task
};

class Result {
public:
    Result(std::shared_ptr<Task> task, bool is_valid = true) : task_{std::move(task)}, isValid{is_valid} {
        task_->setResult(this);
    }

    Result(const Result &) = delete;

    Result &operator=(const Result &) = delete;

    //获取任务执行完的返回值,谁来调用呢?
    void setVal(Any any) {
        // 存储task的返回值
        any_ = std::move(any);
        sem_.post(); // 已经获取的任务的返回值,增加信号量资源
    }

    //用户来调用
    Any get() {
        if (!isValid)
            return "";
        sem_.wait(); //task任务如果没有执行完,这里会阻塞用户的线程
        return std::move(any_);
    }
    //用户调用这个方法获取task的返回值

    ~Result() = default;

private:
    Any any_;
    Semaphore sem_;
    std::shared_ptr<Task> task_;//指向对应获取返回值的任务对象,保证Result得到任务结果之前,task_不会析构
    std::atomic<bool> isValid{};//返回值是否有效
};

//线程池支持的模式
enum class PoolMode {
    MODE_FIXED,//固定数量的线程
    MODE_CACHED,//线程数量可动态增长
};

class Thread {
public:
    using ThreadFunc = std::function<void(int)>;

    Thread(ThreadFunc func) : func_(std::move(func)), threadID_{generateID++} {

    }

    //启动线程
    void start() {
        //创建一个线程来执行一个线程函数
        std::thread t{func_, threadID_};//线程对象t和线程函数func_
        t.detach();//设置分离线程
    }

    int getID() const {
        return threadID_;
    }

    ~Thread() = default;

private:
    static inline int generateID = 0;
    ThreadFunc func_;
    int threadID_; //保存线程ID
};

class ThreadPool {
public:
    ThreadPool();

    ~ThreadPool();

    void start(int size = std::thread::hardware_concurrency());

    //设置线程池工作模式
    void setMode(PoolMode mode);

    //设置task任务队列数量上限阈值
    void setTaskQueThreshold(int threshold);

    //设置线程池cached模式下线程阈值
    void setThreadSizeThreshold(int size);

    Result submitTask(std::shared_ptr<Task> task);

    //void setInitThreadSize(int size);
    ThreadPool(const ThreadPool &p) = delete;

    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    //std::vector<std::unique_ptr<Thread>> threads_{};//所有线程的集合
    std::unordered_map<int, std::unique_ptr<Thread>> threads_{};
    int initThreadSize_{};//初始的线程数量
    int threadSizeThreshold_{};//线程数量的上限
    std::atomic<int> curThreadSize{};//记录当前线程池里面线程的总数量,不使用vector的size的原因是vector不是线程安全的
    std::atomic<size_t> idleThreadSize{};//空闲线程的数量

    //使用智能指针的原因,1.防止用户传入一个生命周期短的对象,需要使用智能指针延长生命周期2.自动释放资源
    std::queue<std::shared_ptr<Task>> taskQue_{};
    int taskQueThreshold_{};//任务数量上限的阈值
    std::atomic<std::size_t> taskSize_{};//当前任务的数量

    std::mutex taskQueMtx;//保证任务队列的线程安全
    std::condition_variable notFull_;//表示任务不满
    std::condition_variable notEmpty_;//表示任务队列不空
    std::condition_variable exitCond_;//等待线程资源全部回收

    PoolMode poolMode_;
    //表示当前线程池的启动状态
    std::atomic<bool> isPoolRunning_{};


private:
    void threadFunc(int thread_id);

    [[nodiscard]] bool checkRunningState() const {
        return isPoolRunning_;
    }
};


#endif //THREADPOOL_MYTHREADPOOL_H
