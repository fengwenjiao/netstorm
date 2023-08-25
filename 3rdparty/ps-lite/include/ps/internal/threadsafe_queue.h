/**
 *  Copyright (c) 2015 by Contributors
 */
#ifndef PS_INTERNAL_THREADSAFE_QUEUE_H_
#define PS_INTERNAL_THREADSAFE_QUEUE_H_
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <iostream>
#include "ps/base.h"
#include "ps/internal/message.h"
namespace ps {

/**
 * \brief thread-safe queue allowing push and waited pop
 */
class ThreadsafeQueue {
 public:
    ThreadsafeQueue() { }
    ~ThreadsafeQueue() { }

    /**
        * \brief push an value into the end. threadsafe.
        * \param new_value the value
        */
    void Push(Message new_value) {
        mu_.lock();
        queue_.push(std::move(new_value));
        mu_.unlock();
        cond_.notify_all();
    }

    /**
    * \brief wait until pop an element from the beginning, threadsafe
    * \param value the poped value
    */
    void WaitAndPop(Message* value) {
        std::unique_lock<std::mutex> lk(mu_);
        while(queue_.empty()){
            cond_.wait(lk);
        }
        *value = std::move(queue_.top());
        queue_.pop();
    }

    bool empty(){
        return queue_.empty();
    }

    int size(){
        return queue_.size();
    }

    bool queue_check(){
        if(!queue_.empty() && queue_.top().meta.control.cmd==Control::REPLY1
        && !(queue_.top().meta.recver%2)){
            return true;
        }else{
            return false;
        }
    }

    void QueuePush(Message new_value) {
        mu_.lock();
        queue_.push(std::move(new_value));
        mu_.unlock();
    }

    void WaitAndPop2(Message* value) {
        mu_.lock();
        *value = std::move(queue_.top());
        queue_.pop();
        mu_.unlock();
    }

    Message top(){
        return queue_.top();
    }

 private:
    class Compare {
     public:
        bool operator()(Message &l, Message &r) {
            return l.meta.priority <= r.meta.priority;
        }
    }; 

    mutable std::mutex mu_;
    std::priority_queue<Message, std::vector<Message>, Compare> queue_;
    std::condition_variable cond_;
};

}  // namespace ps

// bool TryPop(T& value) {
//   std::lock_guard<std::mutex> lk(mut);
//   if(data_queue.empty())
//     return false;
//   value=std::move(data_queue.front());
//   data_queue.pop();
//   return true;
// }
#endif  // PS_INTERNAL_THREADSAFE_QUEUE_H_
