#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

#include "../include/threadpool/ThreadPool.hpp"
#include "../src/threadpool/ThreadPool.cpp"
#include "../include/arrays/aligned_arr.hpp"

using namespace ThreadPoolManual;

class MyTask : public Task
{
    /*
    sum from lower_ to upper_
    */
public:
    MyTask(int a = 0, int b = 10000)
        : lower_(a)
        , upper_(b)
    {}
    MyTask() = default;
    ~MyTask() = default;
    Any run() override
    {
        int sum = 0;
        for (int i = lower_; i < upper_; ++i)
        {
            sum += i;
        }
        // how to return ->> call template copy construct func of class Any
        return Any(sum);
    }
    int setDiff()
    {
        return upper_ - lower_;
    }
    int getLower()
    {
        return lower_;
    }
    int getUpper()
    {
        return upper_;
    }

private:
    int lower_;
    int upper_;
};

class TaskWrapper
{
public:
    // seperate Task obj into blocks for multi-thread call
    TaskWrapper()
        : divNum_(CPU_KERNEL_NUM)
    {}
    ~TaskWrapper() = default;

    void createTasks(std::shared_ptr<MyTask> mytask)
    {
        int diff_ = mytask->setDiff() / divNum_;
        for (int i = mytask->getLower(); i < mytask->getUpper(); i += diff_)
        {
            taskS_.emplace_back(std::make_shared<MyTask>(i, i + diff_));
        }
    }

    void setTask(std::vector<std::shared_ptr<MyTask>> tasks)
    {
        this->taskS_ = tasks;
    }
    std::vector<std::shared_ptr<MyTask>> getTask()
    {
        return this->taskS_;
    }

private:
    int divNum_;
    std::vector<std::shared_ptr<MyTask>> taskS_;
};

int main()
{
    {
        ThreadPool pool;
        pool.setMode(PoolMode::MODE_CACHED);
        pool.start();

        TaskWrapper task1st_;
        task1st_.createTasks(std::make_shared<MyTask>(0, 100000));
        std::vector<std::shared_ptr<MyTask>> tasks = task1st_.getTask();

        std::vector<Result> results{};
        for (std::shared_ptr<MyTask> task : tasks)
        {
            Result res = pool.submitTask(task);
            results.emplace_back(res);
        }
        int sum = 0;
        for (Result res : results)
        {
            int sum1 = res.get().cast_<int>();
            sum += sum1;
        }
        std::cout << "sum is " << sum << std::endl;
    }

    return 0;
}