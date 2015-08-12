//
//  concurrent_queue.hpp
//  Boost.GreenThread
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// Copyright (c) 2015 Chen Xu
//

#ifndef BOOST_GREEN_THREAD_CONCURRENT_QUEUE_HPP
#define BOOST_GREEN_THREAD_CONCURRENT_QUEUE_HPP

#include <deque>
#include <queue>
#include <iterator>
#include <boost/green_thread/mutex.hpp>
#include <boost/green_thread/condition_variable.hpp>

namespace boost { namespace green_thread {
    template<typename T, typename LockType, typename CVType, typename Container = std::deque<T>>
    struct basic_concurrent_queue {
        typedef basic_concurrent_queue<T, LockType, CVType, Container> this_type;
        typedef std::queue<T, Container> queue_type;
        typedef typename LockType::mutex_type mutex_type;
        
        typedef typename queue_type::container_type container_type;
        typedef typename queue_type::value_type value_type;
        typedef typename queue_type::size_type size_type;
        typedef typename queue_type::reference reference;
        typedef typename queue_type::const_reference const_reference;
        
        inline explicit basic_concurrent_queue(size_type capacity=size_type(-1), bool auto_open=true)
        : opened_(auto_open)
        , capacity_(capacity)
        {}
        
        inline bool open() {
            LockType lock(the_mutex_);
            opened_=true;
            return opened_;
        }
        
        inline void close() {
            LockType lock(the_mutex_);
            opened_=false;
            full_cv_.notify_all();
            empty_cv_.notify_all();
        }
        
        inline bool is_open() const {
            LockType lock(the_mutex_);
            return opened_;
        }
        
        inline bool push(const T &data) {
            LockType lock(the_mutex_);
            if (!opened_) {
                // Cannot push into a closed queue
                return false;
            }
            // Wait until queue is closed or not full
            while((the_queue_.size()>=capacity_) && opened_)
            {
                full_cv_.wait(lock);
            }
            if (!opened_) {
                // Queue closed
                return false;
            }
            the_queue_.push(data);
            empty_cv_.notify_one();
            return true;
        }
        
        inline bool push(T &&data) {
            LockType lock(the_mutex_);
            if (!opened_) {
                // Cannot push into a closed queue
                return false;
            }
            // Wait until queue is closed or not full
            while((the_queue_.size()>=capacity_) && opened_)
            {
                full_cv_.wait(lock);
            }
            if (!opened_) {
                // Queue closed
                return false;
            }
            the_queue_.push(std::move(data));
            empty_cv_.notify_one();
            return true;
        }
        
        // std::back_inserter support
        inline void push_back(const T &data)
        { push(data); }
        
        inline void push_back(T &&data)
        { push(std::move(data)); }
        
        // Push items without blocking
        template<typename InIterator>
        inline InIterator push_some(InIterator first, InIterator last) {
            LockType lock(the_mutex_);
            if (!opened_) {
                // Cannot push into a closed queue
                return first;
            }
            for( ; first!=last && the_queue_.size()<capacity_; ++first) {
                the_queue_.push(*first);
                empty_cv_.notify_one();
            }
            return first;
        }
        
        template<typename InIterator>
        inline bool push_all(InIterator first, InIterator last) {
            LockType lock(the_mutex_);
            if (!opened_) {
                // Cannot push into a closed queue
                return false;
            }
            while(first!=last) {
                the_queue_.push(*first);
                empty_cv_.notify_one();
                ++first;
            }
            return true;
        }
        
        inline bool try_push(const T &data) {
            LockType lock(the_mutex_);
            if (!opened_) {
                // Cannot push into a closed queue
                return false;
            }
            // Check if the queue is full
            if(the_queue_.size()>=capacity_)
            {
                return false;
            }
            the_queue_.push(std::move(data));
            empty_cv_.notify_one();
            return true;
        }
        
        inline bool try_push(T &&data) {
            LockType lock(the_mutex_);
            if (!opened_) {
                // Cannot push into a closed queue
                return false;
            }
            // Check if the queue is full
            if(the_queue_.size()>=capacity_)
            {
                return false;
            }
            the_queue_.push(std::move(data));
            empty_cv_.notify_one();
            return true;
        }

        template<class Rep, class Period>
        inline bool try_push_for(const T &data, const boost::chrono::duration<Rep,Period>& timeout_duration) {
            LockType lock(the_mutex_);
            if (!opened_) {
                // Cannot push into a closed queue
                return false;
            }
            // Wait until queue is closed or not full
            boost::cv_status ret=cv_status::no_timeout;
            while((the_queue_.size()>=capacity_) && opened_)
            {
                ret=full_cv_.wait_for(lock, timeout_duration);
            }
            if (!opened_) {
                // Queue closed
                return false;
            }
            if (ret==cv_status::no_timeout) {
                the_queue_.push(std::move(data));
                empty_cv_.notify_one();
                return true;
            }
            return false;
        }

        template<class Rep, class Period>
        inline bool try_push_for(T &&data, const boost::chrono::duration<Rep,Period>& timeout_duration) {
            LockType lock(the_mutex_);
            if (!opened_) {
                // Cannot push into a closed queue
                return false;
            }
            // Wait until queue is closed or not full
            boost::cv_status ret=cv_status::no_timeout;
            while((the_queue_.size()>=capacity_) && opened_)
            {
                ret=full_cv_.wait_for(lock, timeout_duration);
            }
            if (!opened_) {
                // Queue closed
                return false;
            }
            if (ret==cv_status::no_timeout) {
                the_queue_.push(std::move(data));
                empty_cv_.notify_one();
                return true;
            }
            return false;
        }
        
        template< class Clock, class Duration >
        bool try_push_until(const T &data, const boost::chrono::time_point<Clock,Duration>& timeout_time ) {
            LockType lock(the_mutex_);
            if (!opened_) {
                // Cannot push into a closed queue
                return false;
            }
            // Wait until queue is closed or not full
            boost::cv_status ret=cv_status::no_timeout;
            while((the_queue_.size()>=capacity_) && opened_)
            {
                ret=full_cv_.wait_until(lock, timeout_time);
            }
            if (!opened_) {
                // Queue closed
                return false;
            }
            if (ret==cv_status::no_timeout) {
                the_queue_.push(std::move(data));
                empty_cv_.notify_one();
                return true;
            }
            return false;
        }
        
        template< class Clock, class Duration >
        bool try_push_until(T &&data, const boost::chrono::time_point<Clock,Duration>& timeout_time ) {
            LockType lock(the_mutex_);
            if (!opened_) {
                // Cannot push into a closed queue
                return false;
            }
            // Wait until queue is closed or not full
            boost::cv_status ret=cv_status::no_timeout;
            while((the_queue_.size()>=capacity_) && opened_)
            {
                ret=full_cv_.wait_for(lock, timeout_time);
            }
            if (!opened_) {
                // Queue closed
                return false;
            }
            if (ret==cv_status::no_timeout) {
                the_queue_.push(std::move(data));
                empty_cv_.notify_one();
                return true;
            }
            return false;
        }
        
        inline bool pop(T &popped_value) {
            LockType lock(the_mutex_);
            // Wait only if the queue is open and empty
            while(the_queue_.empty() && opened_)
            {
                empty_cv_.wait(lock);
            }
            if (the_queue_.empty()) {
                // Last loop ensure queue will not empty only if queue is closed
                // So here we have an empty and closed queue
                return false;
            }
            std::swap(popped_value, the_queue_.front());
            the_queue_.pop();
            full_cv_.notify_one();
            return true;
        }
        
        inline bool try_pop(T& popped_value)
        {
            LockType lock(the_mutex_);
            if(the_queue_.empty())
            {
                return false;
            }
            std::swap(popped_value, the_queue_.front());
            the_queue_.pop();
            full_cv_.notify_one();
            return true;
        }
        
        template<class Rep, class Period>
        inline bool try_pop_for(T &popped_value, const boost::chrono::duration<Rep,Period>& timeout_duration) {
            LockType lock(the_mutex_);
            // Wait only if the queue is open and empty
            boost::cv_status ret=cv_status::no_timeout;
            while(the_queue_.empty() && opened_)
            {
                ret=empty_cv_.wait_for(lock, timeout_duration);
            }
            if (the_queue_.empty()) {
                // Last loop ensure queue will not empty only if queue is closed
                // So here we have an empty and closed queue
                return false;
            }
            if (ret==cv_status::no_timeout) {
                std::swap(popped_value, the_queue_.front());
                the_queue_.pop();
                full_cv_.notify_one();
                return true;
            }
            return false;
        }
        
        template< class Clock, class Duration >
        bool try_pop_until(T &popped_value, const boost::chrono::time_point<Clock,Duration>& timeout_time ) {
            LockType lock(the_mutex_);
            // Wait only if the queue is open and empty
            boost::cv_status ret=cv_status::no_timeout;
            while(the_queue_.empty() && opened_)
            {
                ret=empty_cv_.wait_until(lock, timeout_time);
            }
            if (the_queue_.empty()) {
                // Last loop ensure queue will not empty only if queue is closed
                // So here we have an empty and closed queue
                return false;
            }
            if (ret==cv_status::no_timeout) {
                std::swap(popped_value, the_queue_.front());
                the_queue_.pop();
                full_cv_.notify_one();
                return true;
            }
            return false;
        }

        // Pop at most nelem items
        template<typename OutIterator>
        inline OutIterator pop_some(OutIterator oi, size_type nelem=size_type(-1))
        {
            LockType lock(the_mutex_);
            if(the_queue_.empty())
            {
                return false;
            }
            nelem=std::min(the_queue_.size(), nelem);
            size_type i=0;
            for(; i<nelem; i++) {
                std::swap(*oi, the_queue_.top());
                the_queue_.pop();
                oi++;
                full_cv_.notify_one();
            }
            return oi;
        }
        
        inline bool empty() const {
            LockType lock(the_mutex_);
            return the_queue_.empty();
        }
        
        inline bool full() const {
            LockType lock(the_mutex_);
            return the_queue_.size() >= capacity_;
        }
        
        inline size_type size() const {
            LockType lock(the_mutex_);
            return the_queue_.size();
        }
        
        inline size_type capacity() const {
            return capacity_;
        }
        
        // Minimal range-based for loop support
        struct iterator : std::iterator<std::input_iterator_tag, T> {
            iterator()
            : queue_(0)
            , popped_(false)
            {}
            
            iterator(this_type *queue)
            : queue_(queue)
            , popped_(false)
            {
                operator++();
            }
            
            iterator(iterator &&other)
            : queue_(other.queue_)
            , value_(std::move(other.value_))
            , popped_(other.popped_)
            {}

            iterator(const iterator &other)=delete;
            iterator &operator=(const iterator &other)=delete;
            
            bool operator!=(const iterator &other) const {
                // Only ended iterators are equal
                return !(ended() && other.ended());
            }
            
            iterator &operator++() {
                popped_=queue_->pop(value_);
                if(!popped_)
                    queue_=0;
                return *this;
            }
            
            value_type &operator*() {
                return value_;
            }
            
            value_type *operator->() {
                return &value_;
            }
            
            bool ended() const {
                return !queue_;
            }
            
            this_type *queue_;
            value_type value_;
            bool popped_;
        };
        
        iterator begin() {
            return iterator(this);
        }
        
        iterator end() const {
            return iterator();
        }
        
    private:
        // Non-copyable, non-movable
        basic_concurrent_queue(const basic_concurrent_queue &)=delete;
        basic_concurrent_queue(basic_concurrent_queue &&)=delete;
        void operator=(const basic_concurrent_queue &)=delete;
        
        bool opened_;
        const size_t capacity_;
        mutable mutex_type the_mutex_;
        CVType full_cv_;
        CVType empty_cv_;
        queue_type the_queue_;
    };

    template<typename T, typename Container=std::deque<T> > using concurrent_queue = basic_concurrent_queue<T, boost::unique_lock<mutex>, condition_variable, Container>;
}}  // End of namespace boost::green_thread

#endif
