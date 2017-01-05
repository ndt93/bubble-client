#ifndef __BLOCKING_QUEUE_H__
#define __BLOCKING_QUEUE_H__

#include <queue>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

template<typename Data>
class ConcurrentQueue
{
private:
    std::queue<Data> mQueue;
    size_t mSize;
    mutable boost::mutex mMutex;
    boost::condition_variable mConsumerNotifier;
    boost::condition_variable mProducerNotifier;
public:
    ConcurrentQueue(size_t size=0)
    {
        mSize = size;
    }

    bool tryPush(Data const& data)
    {
        boost::mutex::scoped_lock lock(mMutex);
        if (mSize == 0 || mQueue.size() < mSize)
        {
            mQueue.push(data);
            lock.unlock();
            mConsumerNotifier.notify_one();
            return true;
        }
        lock.unlock();
        return false;
    }

    void waitAndPush(Data const& data)
    {
        boost::mutex::scoped_lock lock(mMutex);
        while (mSize > 0 && mQueue.size() >= mSize)
        {
            mProducerNotifier.wait(lock);
        }
        mQueue.push(data);
        lock.unlock();
        mConsumerNotifier.notify_one();
    }

    bool empty() const
    {
        boost::mutex::scoped_lock lock(mMutex);
        return mQueue.empty();
    }

    bool tryPop(Data& popped_value)
    {
        boost::mutex::scoped_lock lock(mMutex);
        if (mQueue.empty())
        {
            return false;
        }

        popped_value=mQueue.front();
        mQueue.pop();
        lock.unlock();
        mProducerNotifier.notify_one();
        return true;
    }

    void waitAndPop(Data& popped_value)
    {
        boost::mutex::scoped_lock lock(mMutex);
        while(mQueue.empty())
        {
            mConsumerNotifier.wait(lock);
        }

        popped_value=mQueue.front();
        lock.unlock();
        mProducerNotifier.notify_one();
        mQueue.pop();
    }
};

#endif
