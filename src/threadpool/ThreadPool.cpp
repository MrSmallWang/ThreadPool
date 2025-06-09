
#include <chrono>
#include <functional>
#include <mutex>
#include "threadpool/ThreadPool.hpp"

namespace ThreadPoolManual {

constexpr int TASK_QUEUE_MAX_SIZE = 1024;

ThreadPool::ThreadPool()
    : initThreadSize_(0)
    , curThreadSize_(0)
    , idleThreadSize_(0)
    , taskQueMaxThreshold_(TASK_QUEUE_MAX_SIZE)
    , isPoolRunning_(false)
    , poolMode_(PoolMode::MODE_FIXED)
    , taskQueSize_(0)
    // , threads_(std::vector)
{}

ThreadPool::~ThreadPool()
{}

void ThreadPool::setMode(PoolMode mode)
{
    this->poolMode_ = mode;
}

void ThreadPool::setMaxThreadNum(int number)
{
    this->initThreadSize_ = number;
}

Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
    std::unique_lock<std::mutex> lock(taskQueueMtx_);
    if (!notFull_.wait_for(lock, std::chrono::seconds(1),
            [&]()->bool { return taskQueue_.size() < taskQueMaxThreshold_; }))
    {
        std::cerr << "task queue is full, fail submission." << std::endl;
        return Result(sp, false);
    }
    if (poolMode_ == PoolMode::MODE_CACHED)
    {
        
    }

    taskQueue_.emplace(sp);
    taskQueSize_++;
    notEmpty_.notify_all();

    return Result(sp, true);
}

void ThreadPool::start(int threadSize)
{
    initThreadSize_ = threadSize;
    for (int i = 0; i < initThreadSize_; i++)
    {
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
        threads_.emplace_back(std::move(ptr));
    }
    for (int i = 0; i < initThreadSize_; i++)
    {
        threads_[i]->start();
        ++idleThreadSize_;
        ++curThreadSize_;
    }


}

void ThreadPool::threadFunc()
{
    // 启动线程执行函数
    for (;;)
    {
        std::shared_ptr<Task> task;
        {
            if (poolMode_ == PoolMode::MODE_CACHED)
            {

            }
            else
            {
                // fixed 模式下 等待任务队列有任务的情况下执行线程函数
                std::unique_lock<std::mutex> lock(taskQueueMtx_);
                notEmpty_.wait(lock, [&]()-> { return taskQueSize_ > 0; });
                --idleThreadSize_;
                task = taskQueue_.front();
                taskQueue_.pop();
                taskQueSize_--;

                // notification
                if (taskQueSize_ > 0)
                {
                    notEmpty_.notify_all();
                }
                notFull_.notify_all();
            }
        }

        // execution
        if (!task)
        {
            task->exec();
            ++idleThreadSize_;
        }
    }
}

bool ThreadPool::checkRunningState()
{
    return this->isPoolRunning_;
}

} // namespace ThreadPoolManual