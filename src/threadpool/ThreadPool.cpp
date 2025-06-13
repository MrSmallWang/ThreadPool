#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include "../../include/threadpool/ThreadPool.hpp"

namespace ThreadPoolManual {

constexpr int TASK_QUEUE_MAX_SIZE = INT32_MAX;
constexpr int THREAD_MAX_IDLE_TIME = 60;

Semaphore::Semaphore(int semnum = 0)
    : semaNum_(semnum)
{}

Semaphore::~Semaphore() = default;

void Semaphore::post()
{
    std::unique_lock<std::mutex> lock(semaphoreMtx_);
    semaNum_++;
    isZero_.notify_all();
}

void Semaphore::wait()
{
    std::unique_lock<std::mutex> lock(semaphoreMtx_);
    isZero_.wait(lock, [&]()->bool { return semaNum_ > 0; });
}

ResultImpl::ResultImpl(std::shared_ptr<Task> task, bool isvalid = false)
    : task_(task)
    , isValid_(isvalid)
{}

Result::Result(std::shared_ptr<ResultImpl> impl)
    : impl_(impl)
{
    // 由于这里的信号量在转移的时候会出现悬空指针的情况 因此这里的逻辑需要进行巧妙地调整。
    impl_->task_->setResult(impl_);
    // task_->setResult(this);
    // Semaphore sem_;
}

Result::~Result()
{}

// void Result::setValue(Any anyval)
// {
//     impl_->anyValue_
//     anyValue_ = std::move(anyval);
//     sem_.post();
// }

Any Result::get()
{
    if (!this->impl_->isValid_)
    {
        std::cerr << "task submission error occurred..." << std::endl;
        throw "error getting result!";
    }
    this->impl_->sem_.wait();
    return std::move(this->impl_->anyValue_);
}

Task::Task()
    :impl_(nullptr)
{}

Task::~Task()
{}

void Task::exec()
{
    if (impl_)
    {
        Any anyval = this->run();
        this->impl_->anyValue_ = std::move(anyval);
        this->impl_->sem_.post();
    }
    // if (result_ != nullptr)
    // {
    //     this->result_->setValue(this->run());
    // }
}

void Task::setResult(std::shared_ptr<ResultImpl> impl)
{
    this->impl_ = impl;
}

Thread::Thread(std::function<void(int)> func)
    : func_(func)
    , threadId_(generateId_++)
{}

Thread::~Thread()
{}

int Thread::generateId_ = 0;

int Thread::getThreadId() const
{
    return this->threadId_;
}

void Thread::start()
{
    std::thread t(func_, threadId_);
    t.detach();
}


ThreadPool::ThreadPool()
    : initThreadSize_(0)
    , curThreadSize_(0)
    , idleThreadSize_(0)
    , taskQueMaxThreshold_(TASK_QUEUE_MAX_SIZE)
    , isPoolRunning_(false)
    , poolMode_(PoolMode::MODE_FIXED)
    , taskQueSize_(0)
{}

ThreadPool::~ThreadPool()
{
    isPoolRunning_ = false;
    std::unique_lock<std::mutex> lock(taskQueueMtx_);
    notEmpty_.notify_all();
    exitCondition_.wait(lock, [&]()->bool { return threads_.size() == 0; });
}

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
            [&]()->bool { return taskQueue_.size() < (size_t)taskQueMaxThreshold_; }))
    {
        std::cerr << "task queue is full, fail submission." << std::endl;
        return Result(std::make_shared<ResultImpl>(sp, false));
    }
    taskQueue_.emplace(sp);
    taskQueSize_++;
    notEmpty_.notify_all();
    if (poolMode_ == PoolMode::MODE_CACHED
        && curThreadSize_ < threadSizeMaxThreshold_
        && taskQueSize_ > idleThreadSize_)
    {
        // create thread
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int threadId = ptr->getThreadId();
        threads_.emplace(threadId, std::move(ptr));
        // start thread
        threads_[threadId]->start();
        curThreadSize_++;
        idleThreadSize_++;
    }
    return Result(std::make_shared<ResultImpl>(sp, true));
}

void ThreadPool::start()
{
    isPoolRunning_ = true;
    for (int i = 0; i < initThreadSize_; i++)
    {
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        threads_.emplace(ptr->getThreadId(), std::move(ptr));
    }
    for (int i = 0; i < initThreadSize_; i++)
    {
        threads_[i]->start();
        ++idleThreadSize_;
        ++curThreadSize_;
    }
}

void ThreadPool::threadFunc(int threadid)
{
    auto lastTime = std::chrono::high_resolution_clock().now();
    for (;;)
    {
        std::shared_ptr<Task> task;
        {
            std::unique_lock<std::mutex> lock(taskQueueMtx_);
            if (poolMode_ == PoolMode::MODE_CACHED)
            {
                if (!notEmpty_.wait_for(lock, std::chrono::seconds(1),
                                        [&]()->bool { return taskQueSize_ > 0; }))
                {
                    auto timeNow = std::chrono::high_resolution_clock().now();
                    // int timeDiffer = (int)(timeNow - lastTime); // 不能这么写！
                    auto dur = std::chrono::duration_cast<std::chrono::seconds>(timeNow - lastTime).count();
                    if (!isPoolRunning_ || (dur > THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_))
                    {
                        // 线程的unique_ptr生命周期结束则自动析构
                        threads_.erase(threadid);
                        curThreadSize_--;
                        idleThreadSize_--;
                        std::cout << "threadid: " << std::this_thread::get_id() << "exit!"
                         << std::endl;
                        if(!isPoolRunning_)
                        {
                            exitCondition_.notify_all();
                        }
                        return;
                    }
                    continue;
                }
            }
            else
            {
                // fixed
                notEmpty_.wait(lock, [&]()->bool { return (taskQueSize_ > 0 || !isPoolRunning_); });
                if (!isPoolRunning_)
                {
                    threads_.erase(threadid);
                    exitCondition_.notify_all();
                    return;
                }
            }
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

        // execution
        if (task)
        {
            task->exec();
            ++idleThreadSize_;
            lastTime = std::chrono::high_resolution_clock().now();
        }
        if (!isPoolRunning_)
        {
            threads_.erase(threadid);
            exitCondition_.notify_all();
            return;
        }
    }
}

bool ThreadPool::checkRunningState()
{
    return this->isPoolRunning_;
}

} // namespace ThreadPoolManual