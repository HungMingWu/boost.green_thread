//
//  fss.hpp
//  Boost.GreenThread
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// (C) Copyright 2007-8 Anthony Williams
// (C) Copyright 2015 Chen Xu
//

#ifndef BOOST_GREEN_THREAD_FSS_HPP
#define BOOST_GREEN_THREAD_FSS_HPP

#include <memory>
#include <boost/green_thread/detail/config.hpp>

namespace boost { namespace green_thread {
    namespace detail {
        struct BOOST_GREEN_THREAD_DECL tss_cleanup_function{
            virtual ~tss_cleanup_function() {}
            virtual void operator()(void* data)=0;
        };
        
        template<typename T>
        inline T* heap_new()
        { return new T(); }
        
        template<typename T>
        inline void heap_delete(T* data)
        { delete data; }
        
        template<typename T>
        struct do_heap_delete {
            void operator()(T* data) const
            { detail::heap_delete(data); }
        };

        void set_tss_data(void const* key,std::shared_ptr<tss_cleanup_function> func,void* fss_data,bool cleanup_existing);
        void* get_tss_data(void const* key);
    }   // End of namespace detail

    /**
     * Thread local storage allows multi-thread applications to have a separate instance of a given data item for each thread.
     */
    template <typename T>
    class thread_specific_ptr {
    public:
        typedef T element_type;
        
        /**
         * Construct a `thread_specific_ptr` object for storing a pointer
         * to an object of type `T` specific to each thread. The default
         * delete-based cleanup function will be used to destroy any
         * thread-local objects when `reset()` is called, or the thread
         * exits.
         */
        thread_specific_ptr()
        : cleanup(detail::heap_new<delete_data>(),
                  detail::do_heap_delete<delete_data>())
        {}
        
        /**
         * Construct a `thread_specific_ptr` object for storing a pointer
         * to an object of type `T` specific to each thread. The supplied
         * cleanup_function will be used to destroy any thread-local
         * objects when `reset()` is called, or the thread exits.
         */
        explicit thread_specific_ptr(void (*cleanup_function)(T*)) {
            if(cleanup_function) {
                cleanup.reset(detail::heap_new<run_custom_cleanup_function>(cleanup_function),
                              detail::do_heap_delete<run_custom_cleanup_function>());
            }
        }
        
        /**
         * Calls `this->reset()` to clean up the associated value for the
         * current thread, and destroys `*this`.
         */
        ~thread_specific_ptr() {
            detail::set_tss_data(this,std::shared_ptr<detail::tss_cleanup_function>(),0,true);
        }
        
        /**
         * Returns the pointer associated with the current thread.
         */
        T* get() const {
            return static_cast<T*>(detail::get_tss_data(this));
        }
        
        /**
         * Returns `this->get()`
         */
        T* operator->() const {
            return get();
        }
        
        /**
         * Returns `*(this->get())`
         */
        T& operator*() const {
            return *get();
        }
        
        /**
         * Return `this->get()` and store `NULL` as the pointer associated
         * with the current thread without invoking the cleanup function.
         */
        T* release() {
            T* const temp=get();
            detail::set_tss_data(this,std::shared_ptr<detail::tss_cleanup_function>(),0,false);
            return temp;
        }
        
        /**
         * If `this->get()!=new_value` and `this->get()` is non-NULL, invoke
         * `delete this->get()` or `cleanup_function(this->get())` as appropriate.
         * Store `new_value` as the pointer associated with the current thread.
         */
        void reset(T* new_value=0) {
            T* const current_value=get();
            if(current_value!=new_value) {
                detail::set_tss_data(this,cleanup,new_value,true);
            }
        }
        
    private:
        /// non-copyable
        thread_specific_ptr(thread_specific_ptr&)=delete;
        thread_specific_ptr& operator=(thread_specific_ptr&)=delete;
        
        struct delete_data: detail::tss_cleanup_function {
            void operator()(void* data) {
                delete static_cast<T*>(data);
            }
        };
        
        struct run_custom_cleanup_function: detail::tss_cleanup_function {
            void (*cleanup_function)(T*);
            
            explicit run_custom_cleanup_function(void (*cleanup_function_)(T*)):
            cleanup_function(cleanup_function_)
            {}
            
            void operator()(void* data) {
                cleanup_function(static_cast<T*>(data));
            }
        };
        
        std::shared_ptr<detail::tss_cleanup_function> cleanup;
    };
}}  // End of namespace boost::green_thread

#endif
