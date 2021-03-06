/* Copyright (c) 2012, University of Pisa - Consorzio Nazionale Interuniversitario
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

    // static empty helper structure
    //

    namespace
    {
        const std::tuple<> none{};
    }


    namespace details
    {
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

        // seq trick for expanding tuples when calling
        // functions
        //

        template<int ...>
        struct seq { };

        template<int N, int ...S>
        struct gens : gens<N-1, N-1, S...> { };

        template<int ...S>
        struct gens<0, S...> {
            typedef seq<S...> type;
        };


        // allocate, construct and destroy...
        //

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
        void destroy(Tp &t, size_t s, std::integral_constant<size_t, 0>)
        {
            typedef typename
            std::remove_pointer<
                typename std::tuple_element<0, Tp>::type>::type current_type;

            for(size_t i=0; i < s; i++)
                (std::get<0>(t)+i)->~current_type();
        }
        template <size_t N, typename Tp>
        static inline
        void destroy(Tp &t, size_t s, std::integral_constant<size_t,N>)
        {
            typedef typename
            std::remove_pointer<
                typename std::tuple_element<N, Tp>::type>::type current_type;

            for(size_t i=0; i < s; i++)
                (std::get<N>(t)+i)->~current_type();

            destroy(t, s, std::integral_constant<size_t, N-1>());
        }

        template <typename Tp, typename Tuple, int ... S>
        static inline
        void construct(Tp &r, Tp const &t, size_t offset, std::integral_constant<size_t, 0>, Tuple && packs, seq<S...>)
        {
            typedef typename std::remove_pointer<
                typename std::tuple_element<0, Tp>::type>::type current_type;
            auto ptr = (std::get<0>(t)+offset);

            new (ptr) current_type(std::get<S>(std::get<0>(packs))...);

            std::get<0>(r) = ptr;
        }
        template <size_t N, typename Tp, typename Tuple, int ... S>
        static inline
        void construct(Tp &r, Tp const &t, size_t offset, std::integral_constant<size_t, N>, Tuple && packs, seq<S...>)
        {
            typedef typename std::remove_pointer<
                typename std::tuple_element<N, Tp>::type>::type current_type;
            auto ptr = (std::get<N>(t)+offset);

            new (ptr) current_type(std::get<S>(std::get<N>(packs))...);

            std::get<N>(r) = ptr;
            construct(r, t, offset, std::integral_constant<size_t, N-1>(), std::forward<Tuple>(packs),
                        typename gens<
                            std::tuple_size<
                                typename std::decay<
                                    typename std::tuple_element<N-1, Tuple>::type
                                >::type
                            >::value
                        >::type());
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

    // helper functions ala std::get<>:
    // get a pointer by name: ie. auto iph = get<iphdr>(s);
    //

    template <typename T, typename ...Ts>
    inline auto get(slice<Ts...> &s) -> decltype(std::get<details::type_index<T, Ts...>::value>(s.tuple_))
    {
        return std::get<details::type_index<T, Ts...>::value>(s.tuple_);
    }

    template <typename T, typename ...Ts>
    inline auto get(slice<Ts...> const &s) -> decltype(std::get<details::type_index<T, Ts...>::value>(s.tuple_))
    {
        return std::get<details::type_index<T, Ts...>::value>(s.tuple_);
    }

    // helper function for shared_ptr<slice<Ts...>>:
    //

    template <size_t N, typename ...Ts>
    inline auto get(std::shared_ptr<slice<Ts...>> &s)
    -> decltype(mem::get<N>(*s))
    {
        return mem::get<N>(*s);
    }

    template <typename T, typename ...Ts>
    inline auto get(std::shared_ptr<slice<Ts...>> &s)
    -> decltype(mem::get<T>(*s))
    {
        return mem::get<T>(*s);
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
            details::destroy(layer_.tuple_, index_, std::integral_constant<size_t, sizeof...(Ts)-1>());

#ifdef MSLICE_USE_MMAP
            munmap(mem_, sizeof_mem<M, Ts...>());
#else
            free(mem_);
#endif
        }

        slice_manager(const slice_manager &) = delete;
        slice_manager& operator=(const slice_manager &) = delete;

        template <typename ...Xs>
        slice_type *
        alloc(Xs && ...packs)
        {
#ifndef NDEBUG
            if (index_ == M)
                throw std::runtime_error("slice_manager<Ts...>::alloc() overflow");
#endif
            details::construct(slice_[index_].tuple_, layer_.tuple_, index_, std::integral_constant<size_t, sizeof...(Ts)-1>(),
                               std::forward_as_tuple(std::forward<Xs>(packs)...),
                               typename details::gens<
                                    std::tuple_size<
                                        typename std::decay<
                                            typename std::tuple_element<sizeof...(Ts)-1,
                                                decltype(std::forward_as_tuple(std::forward<Xs>(packs)...))
                                            >::type
                                        >::type
                                    >::value
                                >::type());

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


    ///////////////////////////////
    // basic_slice_allocator class

    template <size_t Ns, typename ...Ts>
    struct basic_slice_allocator
    {
        typedef slice<Ts...> slice_type;

        basic_slice_allocator()
        : manager_(new slice_manager<Ns, Ts...>())
        {}

        ~basic_slice_allocator() = default;

        template <typename ...Xs>
        std::shared_ptr<slice_type>
        new_slice(Xs && ... packs)
        {
            reset_manager();
            auto p = manager_->alloc(std::forward<Xs>(packs)...);
            return std::shared_ptr<slice_type>(manager_, p);
        }

        template <typename T, typename ...Xs>
        std::shared_ptr<T>
        new_shared(Xs && ... args)
        {
            reset_manager();
            auto p = manager_->alloc(std::forward_as_tuple(std::forward<Xs>(args)...));
            return std::shared_ptr<T>(manager_, mem::get<T>(*p));
        }

    private:

        void reset_manager()
        {
            if (manager_->size() == Ns)
                manager_.reset(new slice_manager<Ns, Ts...>());
        }

        std::shared_ptr<slice_manager<Ns, Ts...>> manager_;
    };


    template <typename ...Ts>
    using slice_allocator = basic_slice_allocator<131072, Ts...>;


} // namespace mem

