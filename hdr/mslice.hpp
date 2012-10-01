/* Copyright (c) 2011, Consorzio Nazionale Interuniversitario 
 * per le Telecomunicazioni. 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of Interuniversitario per le Telecomunicazioni nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT 
 * HOLDERBE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
 * PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 */

#pragma once

/*
 * Author: Nicola Bonelli <nicola.bonelli@cnit.it>
 */

#include <memory>
#include <tuple>
#include <stdexcept>
#include <type_traits>

#include <cstdlib>

#ifdef MSLICE_USE_MMAP
#include <sys/mman.h>
#endif

namespace mem {

    // details...
    //
    
    namespace details 
    { 
        template <size_t M, typename Tp>
        static inline 
        void allocate(Tp &t, char *mem, std::integral_constant<size_t,0>) 
        {
            typedef typename 
            std::remove_pointer<
                typename std::tuple_element<0, Tp>::type>::type current_type;

            std::get<0>(t) = reinterpret_cast<current_type *>(mem); 
        }
        template <size_t M, size_t N, typename Tp>
        static inline
        void allocate(Tp &t, char *mem, std::integral_constant<size_t, N>) 
        {
            typedef typename 
            std::remove_pointer<
                typename std::tuple_element<N, Tp>::type>::type current_type;

            std::get<N>(t) = reinterpret_cast<current_type *>(mem); 
            allocate<M>(t, mem + sizeof(current_type) * M, std::integral_constant<size_t, N-1>());
        }

        template <typename Tp>
        static inline 
        void 
        destroy(Tp &t, size_t s, std::integral_constant<size_t, 0>) 
        {
            typedef typename 
            std::remove_pointer<
                typename std::tuple_element<0, Tp>::type>::type current_type;
            if (!std::has_trivial_destructor<current_type>::value)
            {
                for(size_t i=0; i < s; i++)
                    (std::get<0>(t)+i)->~current_type();
            }
        }
        template <size_t N, typename Tp>
        static inline
        void destroy(Tp &t, size_t s, std::integral_constant<size_t,N>) 
        {
            typedef typename 
            std::remove_pointer<
                typename std::tuple_element<N, Tp>::type>::type current_type;
            if (!std::has_trivial_destructor<current_type>::value)
            {
                for(size_t i=0; i < s; i++)
                    (std::get<N>(t)+i)->~current_type();
            }
            destroy(t, s, std::integral_constant<size_t, N-1>());
        }

        template <typename Tp, typename ...Ts>
        static inline
        void construct(Tp &r, Tp const &t, size_t offset, std::integral_constant<size_t, 0>, Ts&& ...args) 
        {
            typedef typename std::remove_pointer<
                typename std::tuple_element<0, Tp>::type>::type current_type;
            auto ptr = (std::get<0>(t)+offset);
            if (!std::has_trivial_default_constructor<current_type>::value)
            {
                new (ptr) current_type(std::forward<Ts>(args)...); // only the first type is constructed by passing arguments
            }
            std::get<0>(r) = ptr;
        }
        template <size_t N, typename Tp, typename ...Ts>
        static inline
        void construct(Tp &r, Tp const &t, size_t offset, std::integral_constant<size_t, N>, Ts&& ...args) 
        {    
            typedef typename std::remove_pointer<
                typename std::tuple_element<N, Tp>::type>::type current_type;
            auto ptr = (std::get<N>(t)+offset);
            if (!std::has_trivial_default_constructor<current_type>::value)
            {
                new (ptr) current_type;
            }
            std::get<N>(r) = ptr;
            construct(r, t, offset, std::integral_constant<size_t, N-1>(), std::forward<Ts>(args)...);
        }
    
    } // namespace details


    /////////////////////////////////////////////////////////////////////////
    // slice class, a simple adapter for std::tuple<T0 *, T1 *...>
    //

    template <typename ...Ts>
    struct slice
    {
        typedef std::tuple<typename std::add_pointer<Ts>::type...> tuple_type;
        
        tuple_type tuple_;

        slice() = default;
        slice(slice const &) = default;
        slice& operator=(slice const &) = default;

        template<typename ...Vs>
        explicit slice(Vs && ...args)
        : tuple_(std::forward<Vs>(args)...)
        {}

        template <typename ...Vs>
        slice & operator=(std::tuple<Vs...> const &t)
        {
            tuple_ = t;
            return *this;
        }

        explicit operator tuple_type()
        {
            return tuple_;
        }    
    };

    template <typename Tp> struct slice_size;
    template <typename ...Ts>
    struct slice_size<slice<Ts...>> : std::integral_constant<size_t, sizeof...(Ts)> { };

    // utility to make a slice from a set of pointers
    //
    
    template <typename ...Ts>
    auto make_slice(Ts* ... args)
    -> decltype(slice<Ts...>(args...))
    {
        return slice<Ts...>(args...);
    }

    // helper functions ala std::get<>:
    // get a pointer by index:
    //

    template <size_t N, typename ...Ts>
    inline auto get(slice<Ts...> &s) -> decltype(std::get<N>(s.tuple_))
    {
        return std::get<N>(s.tuple_);
    }

    template <size_t N, typename ...Ts>
    inline auto get(slice<Ts...> const &s) -> decltype(std::get<N>(s.tuple_))
    {
        return std::get<N>(s.tuple_);
    }

    // utility meta-function that return the index of a type in a pack, 
    // the size of the packet otherwise:
    //

    template <typename ... Ts> 
    struct type_index
    {
        enum { value = 0 }; // stop recursion here
    };

    template <typename T, typename T0, typename ... Ts>
    struct type_index<T, T0, Ts...>
    {
        enum { value = 1 + type_index<T, Ts...>::value };
    };

    template <typename T, typename ... Ts>
    struct type_index<T, T, Ts...>
    {
        enum { value = 0 }; // stop recursion here
    };

    // helper functions ala std::get<>:
    // get a pointer by name: ie. auto iph = get<iphdr>(s);
    //
    
    template <typename T, typename ...Ts>
    inline auto get(slice<Ts...> &s) -> decltype(std::get<type_index<T, Ts...>::value>(s.tuple_))
    {
        return std::get<type_index<T, Ts...>::value>(s.tuple_);
    }

    template <typename T, typename ...Ts>
    inline auto get(slice<Ts...> const &s) -> decltype(std::get<type_index<T, Ts...>::value>(s.tuple_))
    {
        return std::get<type_index<T, Ts...>::value>(s.tuple_);
    }

    // recursive functions that calculate the total size of 
    // memory
    //
    
    template <size_t M> 
    constexpr inline 
    size_t sizeof_mem()
    {
        return 0; 
    }
    template <size_t M, typename T, typename ...Ts> 
    constexpr inline 
    size_t sizeof_mem()
    {
        return sizeof(T) * M + sizeof_mem<M, Ts...>(); 
    }

    /////////////////////////////////////////////////////////////
    // slice manager: utility class that manages layers of memory
    //

    template <size_t M, typename ...Ts>
    struct slice_manager
    {
        typedef slice<Ts...> slice_type;
        
        slice_manager()
        : index_(0)
        , layer_()
        , slice_(new slice_type[M])
        {
            mem_ = 
#ifdef MSLICE_USE_MMAP

#ifndef MAP_UNINITIALIZED
#define MAP_UNINITIALIZED 0x4000000
#endif
            mmap(0, sizeof_mem<M, Ts...>(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_UNINITIALIZED , -1, 0);
#else
            malloc(sizeof_mem<M, Ts...>());
#endif
            if (mem_ == nullptr)
                throw std::runtime_error("slice_manager: out of memory");

            details::allocate<M>(layer_.tuple_, static_cast<char *>(mem_), std::integral_constant<size_t, sizeof...(Ts)-1>());
        }

        ~slice_manager()
        {
#ifdef MSLICE_USE_MMAP
            munmap(mem_, sizeof_mem<M, Ts...>());
#else
            free(mem_);
#endif
            details::destroy(layer_.tuple_, index_, std::integral_constant<size_t, sizeof...(Ts)-1>());
        }

        slice_manager(const slice_manager &) = delete;
        slice_manager& operator=(const slice_manager &) = delete;

        template <typename ...Xs>
        slice_type *
        alloc(Xs && ...args) 
        {
#ifndef NDEBUG
            if (index_ == M)
                throw std::runtime_error("slice_manager<Ts...>::alloc() overflow");
#endif
            details::construct(slice_[index_].tuple_, layer_.tuple_, index_, std::integral_constant<size_t, sizeof...(Ts)-1>(), std::forward<Xs>(args)...);
            return &slice_[index_++];
        }

        size_t 
        size() const 
        {
            return index_;
        }

    private:          
        size_t      index_;
        slice_type  layer_;

        std::unique_ptr<slice_type[]> slice_;
        void * mem_;
    };

    /////////////////////////
    // slice_allocator class

    template <typename ...Ts>
    struct slice_allocator
    {
        typedef slice<Ts...> slice_type;
        
        static constexpr size_t Ns = 4096*32;

        slice_allocator()
        : manager_(new slice_manager<Ns, Ts...>())
        {}

        ~slice_allocator() = default;

        template <typename ...Xs>
        std::shared_ptr<slice_type>
        new_slice(Xs && ... args)
        {
            if (manager_->size() == Ns)
                manager_.reset(new slice_manager<Ns, Ts...>());

            auto p = manager_->alloc(std::forward<Xs>(args)...);
            return std::shared_ptr<slice_type>(manager_, p);
        }

    private:
        std::shared_ptr<slice_manager<Ns, Ts...>> manager_;
    };

} // namespace mem

