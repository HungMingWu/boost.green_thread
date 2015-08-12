//
//  task_object.hpp
//  Boost.GreenThread
//
// Copyright 2015 Chen Xu
// Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_GREEN_THREAD_FUTURE_DETAIL_TASK_OBJECT_HPP
#define BOOST_GREEN_THREAD_FUTURE_DETAIL_TASK_OBJECT_HPP

#include <boost/config.hpp>
#include <boost/throw_exception.hpp>

#include <boost/green_thread/future/detail/task_base.hpp>

namespace boost { namespace green_thread { namespace detail {
    template<typename Fn, typename Allocator, typename R, typename ...Args>
    class task_object : public task_base<R, Args...>
    {
        typedef task_object<Fn, Allocator, R, Args...> this_type;
    public:
        typedef typename Allocator::template rebind<this_type>::other allocator_t;
        
        task_object(Fn &&fn, const allocator_t &alloc)
        : task_base<R, Args...>()
        , fn_(std::forward< Fn >(fn))
        , alloc_(alloc)
        {}
        
        void run(Args&&... args) {
            try {
                this->set_value(fn_(std::forward<Args>(args)...));
            } catch(...) {
                this->set_exception(std::current_exception());
            }
        }
        
    protected:
        void deallocate_future() { destroy_(alloc_, this); }
        
    private:
        Fn fn_;
        allocator_t alloc_;
        
        static void destroy_(allocator_t &alloc, task_object *p) {
            alloc.destroy(p);
            alloc.deallocate(p, 1);
        }
    };
    
    template<typename Fn, typename Allocator, typename ...Args>
    class task_object<Fn, Allocator, void, Args...> : public task_base<void, Args...> {
        typedef task_object<Fn, Allocator, void, Args...> this_type;
    public:
        typedef typename Allocator::template rebind<this_type>::other allocator_t;
        
        task_object(Fn &&fn, const allocator_t &alloc)
        : task_base<void>()
        , fn_(std::forward<Fn>(fn))
        , alloc_(alloc)
        {}
        
        void run(Args&&... args) {
            try {
                fn_(std::forward<Args>(args)...);
                this->set_value();
            } catch(...) {
                this->set_exception(std::current_exception());
            }
        }
        
    protected:
        void deallocate_future() { destroy_(alloc_, this); }
        
    private:
        Fn fn_;
        allocator_t alloc_;
        
        static void destroy_(allocator_t &alloc, task_object *p) {
            alloc.destroy(p);
            alloc.deallocate(p, 1);
        }
    };
}}} // End of namespace boost::green_thread::detail

#endif
