#include <iostream>
#include "myThreadPool.h"

using ull = unsigned long long;

class MyTask : public Task {
public:
    MyTask(int begin, int end) : begin_{begin}, end_{end} {}

    virtual Any run() override {
        std::cout << std::this_thread::get_id() << "start" << std::endl;
        ull ans{};
        for (int i = begin_; i <= end_; ++i)
            ans += i;
        return ans;
    }

    int begin_, end_;
};


int main() {
    {
        try {
            ThreadPool pool;
            pool.start();
            Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 1e8));
            Result res2 = pool.submitTask(std::make_shared<MyTask>(1, 1e8));
            Result res3 = pool.submitTask(std::make_shared<MyTask>(1, 1e8));
            Result res4 = pool.submitTask(std::make_shared<MyTask>(1, 1e8));
            Result res5 = pool.submitTask(std::make_shared<MyTask>(1, 1e8));
            Result res6 = pool.submitTask(std::make_shared<MyTask>(1, 1e8));
            Result res7 = pool.submitTask(std::make_shared<MyTask>(1, 1e8));

            ull sum1 = res1.get().cast<ull>();
            ull sum2 = res2.get().cast<ull>();
            ull sum3 = res3.get().cast<ull>();
            ull sum4 = res4.get().cast<ull>();
            ull sum5 = res5.get().cast<ull>();
            ull sum6 = res6.get().cast<ull>();
            ull sum7 = res7.get().cast<ull>();
            std::cout << sum1 << std::endl;
            std::cout << sum2 << std::endl;
            std::cout << sum3 << std::endl;
            std::cout << sum4 << std::endl;
            std::cout << sum5 << std::endl;
            std::cout << sum6 << std::endl;
            std::cout << sum7 << std::endl;
        } catch (std::exception &e) {
            std::cout << e.what() << std::endl;
        }
    }
    return 0;
}
