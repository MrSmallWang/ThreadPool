/*
    Implementation of aligned High-Performance Array
*/

#pragma once

#include <cmath>
#include <vector>
#include <functional>
#include <cassert>
#include <string>
#include <memory>
#include <cstring>

namespace alignedArr {

// ODR 导致这里必须写inline
inline void* alignSpace(size_t __align, size_t __size, void*& __ptr, size_t& __space) noexcept
{
    // 1. 转为整型
    const auto __intptr = reinterpret_cast<uintptr_t>(__ptr);
    // 2. calc aligned size
    const auto __alignedSize = (__intptr - 1u + __align) & (-__align);
    // 3. calc diff
    const auto diff_ = __alignedSize - __intptr;
    if (diff_ + __size > __space)
    {
        return nullptr;
    }
    else
    {
        __space -= diff_;
        __ptr = reinterpret_cast<void*>(__alignedSize);
        return __ptr;
    }
}

template<typename T, size_t N>
class AlignedVector
{
public:
    explicit AlignedVector() noexcept
        : data_(nullptr)
        , rawData_(nullptr)
        , capacity_(0)
        , size_(0)
    {}
    explicit AlignedVector(size_t n) noexcept
        : data_(nullptr)
        , rawData_(nullptr)
        , capacity_(0)
        , size_(0)
    {
        this->resize(n);
    }

    explicit AlignedVector(size_t n, const T& val) noexcept
        : data_(nullptr)
        , rawData_(nullptr)
        , size_(0)
        , capacity_(0)
    {
        this->resize(n);
        for (size_t i = 0; i < n; i++)
        {
            this->push_back(val);
        }
    }

    // copy constructor explicit is not necessary
    AlignedVector(const AlignedVector& vec) noexcept
    {
        this->copyFrom(vec);
    }

    // moving constructor
    AlignedVector(AlignedVector&& vec)
        : data_(vec.data_)
        , rawData_(vec.rawData_)
        , size_(vec.size_)
        , capacity_(vec.capacity_)
    {
        vec.data_ = nullptr;
        vec.rawData_ = nullptr;
        vec.size_ = 0;
        vec.capacity_ = 0;
        // never return *this; here!!!
    }

    // moving copy
    AlignedVector& operator = (AlignedVector&& vec)
    {
        if (this != &vec)
        {
            this->freeData();
            this->data_ = vec.data_;
            this->rawData_ = vec.rawData_;
            this->size_ = vec.size_;
            this->capacity_ = vec.capacity_;
            vec.zise_ = 0;
            vec.capacity_ = 0;
            vec.rawData_ = nullptr;
            vec.data_ = nullptr;
        }
        return *this; // return here is because the calc comma = here!!!
    }

    ~AlignedVector() noexcept
    {
        this->freeData();
    }

    // ops overloading
    AlignedVector& operator = (const AlignedVector& vec)
    {
        this->copyFrom(vec);
        return *this;
    }

    // 这里的size_t为什么不能是const?
    T& operator [] (size_t pos)
    {
        assert(pos >= 0 && pos <size_);
        return data_[pos];
    }
    const T& operator [] (size_t pos) const
    {
        assert(pos >= 0 && pos < size_);
        return data_[pos];
    }
    T& at(size_t pos) const
    {
        assert(pos >= 0 && pos < size_);
        return data_[pos];
    }

    void clear()
    {
        this->freeData();
    }

    void resize(size_t n)
    {
        // 
        this->freeData();
        this->reserve(n);
    }
    void reserve(size_t capacity)
    {
        // register buffer core logic
        T* newRawData = nullptr;
        T* newData = this->makeAlign_(capacity, N, &newRawData);
        assert(newRawData);
        assert(newData);

        size_t minSize = (size_ < capacity) ? size_ : capacity;
        std::memmove(newData, data_, minSize * sizeof(T));

        if (rawData_)
        {
            delete [] rawData_;
        }
        data_ = newData;
        rawData_ = newRawData;
        size_ = minSize;
        capacity_ = capacity;
    }

    void push_back(const T& val)
    {
        if (size_ == capacity_)
        {
            this->reserve((capacity_ == 0) ? (1) : (capacity_ * 2));
        }
        data_[size_] = val;
        ++size_;
    }
    void push_back(T& val)
    {
        if (size_ == capacity_)
        {
            this->reserve((capacity_ == 0) ? (1) : (capacity_ * 2));
        }
        data_[size_] = val;
        ++size_;
    }

    T& data() const { return data_;}
    bool empty() { return (size_ == 0); }
    size_t size()
    {
        return this->size_;
    }

    size_t capacity()
    {
        return this->capacity_;
    }

private:
    T* data_;
    T* rawData_;
    size_t size_;
    size_t capacity_;
    
    void copyFrom(const AlignedVector& vec)
    {
        // copy data only and reinit the rawData?
        if (this == &vec)
        {
            return;
        }
        this->freeData();
        this->reserve(vec.capacity_);
        this->size_ = vec.size_;
        std::memmove(data_, vec.data(), sizeof(T) * vec.size());
    }
    
    void freeData()
    {
        if (rawData_)
        {
            delete [] rawData_;
        }
        rawData_ = nullptr;
        data_ = nullptr;
        size_ = 0;
        capacity_ = 0;
    }

    T* makeAlign_(size_t size, size_t alignmentSize, T** rawPtr)
    {
        // make align space via input cap and the align requirement.
        // size is the original byte size of the rawptr. alignmentSize is the alignment demand
        *rawPtr = new T[size + alignmentSize - 1];
        void* ptr = (void*) (*rawPtr);
        size_t len = (size + alignmentSize - 1) * sizeof(T);
        return  static_cast<T*>(alignSpace(alignmentSize, sizeof(T) * size, ptr, len));
    }
};




} // alignedArr