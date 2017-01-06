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
    size_t mMaxSize;
    bool mIsReleased;
    mutable boost::mutex mMutex;
    boost::condition_variable mConsumerNotifier;
    boost::condition_variable mProducerNotifier;
public:
    /* Sets max_size = 0 for unbounded queue */
    ConcurrentQueue(size_t max_size=100)
    {
        mMaxSize = max_size;
        mIsReleased = false;
    }

    /* Release all threads waiting without any data guarantee.
     * Should only be used when the queue is no longer used */
    void release()
    {
        boost::mutex::scoped_lock lock(mMutex);
        mIsReleased = true;
        lock.unlock();
        mConsumerNotifier.notify_all();
        mProducerNotifier.notify_all();
    }

    bool tryPush(Data const& data)
    {
        boost::mutex::scoped_lock lock(mMutex);
        if (mMaxSize == 0 || mQueue.size() < mMaxSize)
        {
            mQueue.push(data);
            lock.unlock();
            mConsumerNotifier.notify_one();
            return true;
        }
        lock.unlock();
        return false;
    }

    int waitAndPush(Data const& data)
    {
        boost::mutex::scoped_lock lock(mMutex);
        while (!mIsReleased && mMaxSize > 0 && mQueue.size() >= mMaxSize)
        {
            mProducerNotifier.wait(lock);
        }

        if (mIsReleased)
        {
            return -1;
        }
        mQueue.push(data);
        lock.unlock();
        mConsumerNotifier.notify_one();
        return 0;
    }

    bool empty() const
    {
        boost::mutex::scoped_lock lock(mMutex);
        return mQueue.empty();
    }

    size_t size() const
    {
        boost::mutex::scoped_lock lock(mMutex);
        return mQueue.size();
    }

    bool tryPop(Data& popped_value)
    {
        boost::mutex::scoped_lock lock(mMutex);
        if (mQueue.empty())
        {
            return false;
        }

        popped_value = mQueue.front();
        mQueue.pop();
        lock.unlock();
        mProducerNotifier.notify_one();
        return true;
    }

    int waitAndPop(Data& popped_value)
    {
        boost::mutex::scoped_lock lock(mMutex);
        while(!mIsReleased && mQueue.empty())
        {
            mConsumerNotifier.wait(lock);
        }

        if (mIsReleased)
        {
            return -1;
        }
        popped_value = mQueue.front();
        lock.unlock();
        mProducerNotifier.notify_one();
        mQueue.pop();
        return 0;
    }
};

#endif
