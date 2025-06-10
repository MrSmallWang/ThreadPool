#pragma once

#include <iostream>
#include <atomic>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <type_traits>
#include <vector>
#include <unordered_map>

namespace ThreadPoolManual {

enum class PoolMode
{
    MODE_CACHED,
    MODE_FIXED
};

class Semaphore
{
public:
    Semaphore(int semanum);
    ~Semaphore();
    void wait();
    void post();

private:
    int semaNum_;
    std::mutex semaphoreMtx_;
    std::condition_variable isZero_;
};

class Any
{
public:
    Any() = default;
    ~Any() = default;
    Any(const Any&) = delete;
    Any& operator = (const Any&) = delete;
    Any(Any&&) = default;
    Any& operator = (Any&&) = default;

    template<typename T>
    T cast_()
    {
        Derive<T>* p = dynamic_cast<Derive<T>*>(base_.get());
        if (!p)
        {
            std::cerr << "error type casting. " << std::endl;
            throw "error";
        }
        return p->data_;
    }
private:
    class Base
    {
        public:
            virtual ~Base() = default;
    };
    // 模板派生类
    template<typename T>
    class Derive : public Base
    {
        public:
            Derive(T data)
                : data_(data)
            {}
            ~Derive() = default;
        private:
            T data_;
    };
    // 指向派生类的基类指针
    std::unique_ptr<Base> base_;
};

class Task;

class Result
{
public:
    Result(std::shared_ptr<Task> sp, bool isvalid);
    ~Result();
    void setValue(Any val);
    Any get();

private:
    Any anyValue_;
    Semaphore sem_;
    std::shared_ptr<Task> task_;
    std::atomic_bool isValid_;
};

class Task
{
public:
    Task();
    ~Task();
    virtual Any run();
    void exec();
    void setResult(Result* res);

private:
    Result* result_;

};

class Thread
{
public:
    // 注意这里加速int的细节！
    Thread(std::function<void(int)> func);
    ~Thread();

    void start();
    int getThreadId() const;
private:
    std::function<void(int)> func_;
    static int generateId_;
    int threadId_;
};

//线程池类->应该不是模板类吧
class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    void setMode(PoolMode mode);
    void setMaxThreadNum(int number);
    Result submitTask(std::shared_ptr<Task> task);
    void start(int initThreadSize = 4);

    // 禁用拷贝构造函数 禁用拷贝赋值
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator = (const ThreadPool&) = delete;

private:
    void threadFunc(int threadid);
    bool checkRunningState();

private:
    // std::vector<std::unique_ptr<Thread>> threads_;
    std::unordered_map<int, std::unique_ptr<Thread>> threads_;
    int initThreadSize_;
    std::atomic_int curThreadSize_;
    std::atomic_int idleThreadSize_;
    int threadSizeMaxThreshold_;

    std::queue<std::shared_ptr<Task>> taskQueue_;
    std::atomic_int taskQueSize_;
    int taskQueMaxThreshold_;
    
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    std::condition_variable exitCondition_;
    std::mutex taskQueueMtx_;
    // TODO
    std::atomic_bool isPoolRunning_;
    PoolMode poolMode_;
};

}