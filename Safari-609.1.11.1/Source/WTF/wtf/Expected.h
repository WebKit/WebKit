/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// Implementation of Library Fundamentals v3's std::expected, as described here: http://wg21.link/p0323r4

#pragma once

/*
    expected synopsis

namespace std {
namespace experimental {
inline namespace fundamentals_v3 {
    // ?.?.4, Expected for object types
    template <class T, class E>
        class expected;

    // ?.?.5, Expected specialization for void
    template <class E>
        class expected<void,E>;

    // ?.?.6, unexpect tag
    struct unexpect_t {
       unexpect_t() = default;
    };
    inline constexpr unexpect_t unexpect{};

    // ?.?.7, class bad_expected_access
    template <class E>
       class bad_expected_access;

    // ?.?.8, Specialization for void.
    template <>
       class bad_expected_access<void>;

    // ?.?.9, Expected relational operators
    template <class T, class E>
        constexpr bool operator==(const expected<T, E>&, const expected<T, E>&);
    template <class T, class E>
        constexpr bool operator!=(const expected<T, E>&, const expected<T, E>&);

    // ?.?.10, Comparison with T
    template <class T, class E>
      constexpr bool operator==(const expected<T, E>&, const T&);
    template <class T, class E>
      constexpr bool operator==(const T&, const expected<T, E>&);
    template <class T, class E>
      constexpr bool operator!=(const expected<T, E>&, const T&);
    template <class T, class E>
      constexpr bool operator!=(const T&, const expected<T, E>&);

    // ?.?.10, Comparison with unexpected<E>
    template <class T, class E>
      constexpr bool operator==(const expected<T, E>&, const unexpected<E>&);
    template <class T, class E>
      constexpr bool operator==(const unexpected<E>&, const expected<T, E>&);
    template <class T, class E>
      constexpr bool operator!=(const expected<T, E>&, const unexpected<E>&);
    template <class T, class E>
      constexpr bool operator!=(const unexpected<E>&, const expected<T, E>&);

    // ?.?.11, Specialized algorithms
    void swap(expected<T, E>&, expected<T, E>&) noexcept(see below);

    template <class T, class E>
    class expected
    {
    public:
        typedef T value_type;
        typedef E error_type;
        typedef unexpected<E> unexpected_type;
    
        template <class U>
            struct rebind {
            using type = expected<U, error_type>;
          };
    
        // ?.?.4.1, constructors
        constexpr expected();
        constexpr expected(const expected&);
        constexpr expected(expected&&) noexcept(see below);
        template <class U, class G>
            EXPLICIT constexpr expected(const expected<U, G>&);
        template <class U, class G>
            EXPLICIT constexpr expected(expected<U, G>&&);
    
        template <class U = T>
            EXPLICIT constexpr expected(U&& v);
    
        template <class... Args>
            constexpr explicit expected(in_place_t, Args&&...);
        template <class U, class... Args>
            constexpr explicit expected(in_place_t, initializer_list<U>, Args&&...);
        template <class G = E>
            constexpr expected(unexpected<G> const&);
        template <class G = E>
            constexpr expected(unexpected<G> &&);
        template <class... Args>
            constexpr explicit expected(unexpect_t, Args&&...);
        template <class U, class... Args>
            constexpr explicit expected(unexpect_t, initializer_list<U>, Args&&...);
    
        // ?.?.4.2, destructor
        ~expected();
    
        // ?.?.4.3, assignment
        expected& operator=(const expected&);
        expected& operator=(expected&&) noexcept(see below);
        template <class U = T> expected& operator=(U&&);
        template <class G = E>
            expected& operator=(const unexpected<G>&);
        template <class G = E>
            expected& operator=(unexpected<G>&&) noexcept(see below);
    
        template <class... Args>
            void emplace(Args&&...);
        template <class U, class... Args>
            void emplace(initializer_list<U>, Args&&...);
    
        // ?.?.4.4, swap
        void swap(expected&) noexcept(see below);
    
        // ?.?.4.5, observers
        constexpr const T* operator ->() const;
        constexpr T* operator ->();
        constexpr const T& operator *() const&;
        constexpr T& operator *() &;
        constexpr const T&& operator *() const &&;
        constexpr T&& operator *() &&;
        constexpr explicit operator bool() const noexcept;
        constexpr bool has_value() const noexcept;
        constexpr const T& value() const&;
        constexpr T& value() &;
        constexpr const T&& value() const &&;
        constexpr T&& value() &&;
        constexpr const E& error() const&;
        constexpr E& error() &;
        constexpr const E&& error() const &&;
        constexpr E&& error() &&;
        template <class U>
            constexpr T value_or(U&&) const&;
        template <class U>
            T value_or(U&&) &&;
    
    private:
        bool has_val; // exposition only
        union
        {
            value_type val; // exposition only
            unexpected_type unexpect; // exposition only
        };
    };

}}}

*/

#include <cstdlib>
#include <initializer_list>
#include <type_traits>
#include <utility>
#include <wtf/Assertions.h>
#include <wtf/Compiler.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Unexpected.h>

namespace std {
namespace experimental {
inline namespace fundamentals_v3 {

struct unexpected_t {
    unexpected_t() = default;
};
#if __cplusplus < 201703L
#define __EXPECTED_INLINE_VARIABLE static const
#else
#define __EXPECTED_INLINE_VARIABLE inline
#endif

__EXPECTED_INLINE_VARIABLE constexpr unexpected_t unexpect { };

template<class E> class bad_expected_access;

template<>
class bad_expected_access<void> : public std::exception {
public:
    explicit bad_expected_access() { }
};

template<class E>
class bad_expected_access : public bad_expected_access<void> {
public:
    explicit bad_expected_access(E val) : val(val) { }
    virtual const char* what() const noexcept override { return std::exception::what(); }
    E& error() & { return val; }
    const E& error() const& { return val; }
    E&& error() && { return std::move(val); }
    const E&&  error() const&& { return std::move(val); }

private:
    E val;
};

namespace __expected_detail {

#if COMPILER_SUPPORTS(EXCEPTIONS)
#define __EXPECTED_THROW(__exception) (throw __exception)
#else
inline NO_RETURN_DUE_TO_CRASH void __expected_terminate() { RELEASE_ASSERT_NOT_REACHED(); }
#define __EXPECTED_THROW(...) __expected_detail::__expected_terminate()
#endif

__EXPECTED_INLINE_VARIABLE constexpr enum class value_tag_t { } value_tag { };
__EXPECTED_INLINE_VARIABLE constexpr enum class error_tag_t { } error_tag { };

template<class T, std::enable_if_t<std::is_trivially_destructible<T>::value>* = nullptr> void destroy(T&) { }
template<class T, std::enable_if_t<!std::is_trivially_destructible<T>::value && (std::is_class<T>::value || std::is_union<T>::value)>* = nullptr> void destroy(T& t) { t.~T(); }

template<class T, class E>
union constexpr_storage {
    typedef T value_type;
    typedef E error_type;
    typedef unexpected<E> unexpected_type;
    char dummy;
    value_type val;
    error_type err;
    constexpr constexpr_storage() : dummy() { }
    constexpr constexpr_storage(value_tag_t) : val() { }
    constexpr constexpr_storage(error_tag_t) : err() { }
    template<typename U = T>
    constexpr constexpr_storage(value_tag_t, U&& v) : val(std::forward<U>(v)) { }
    template<typename U = E>
    constexpr constexpr_storage(error_tag_t, U&& e) : err(std::forward<U>(e)) { }
    ~constexpr_storage() = default;
};

template<class T, class E>
union storage {
    typedef T value_type;
    typedef E error_type;
    typedef unexpected<E> unexpected_type;
    char dummy;
    value_type val;
    error_type err;
    constexpr storage() : dummy() { }
    constexpr storage(value_tag_t) : val() { }
    constexpr storage(error_tag_t) : err() { }
    constexpr storage(value_tag_t, const value_type& val) : val(val) { }
    constexpr storage(value_tag_t, value_type&& val) : val(std::forward<value_type>(val)) { }
    constexpr storage(error_tag_t, const error_type& err) : err(err) { }
    constexpr storage(error_tag_t, error_type&& err) : err(std::forward<error_type>(err)) { }
    ~storage() { }
};

template<class E>
union constexpr_storage<void, E> {
    typedef void value_type;
    typedef E error_type;
    typedef unexpected<E> unexpected_type;
    char dummy;
    error_type err;
    constexpr constexpr_storage() : dummy() { }
    constexpr constexpr_storage(value_tag_t) : dummy() { }
    constexpr constexpr_storage(error_tag_t) : err() { }
    constexpr constexpr_storage(error_tag_t, const error_type& e) : err(e) { }
    ~constexpr_storage() = default;
};

template<class E>
union storage<void, E> {
    typedef void value_type;
    typedef E error_type;
    typedef unexpected<E> unexpected_type;
    char dummy;
    error_type err;
    constexpr storage() : dummy() { }
    constexpr storage(value_tag_t) : dummy() { }
    constexpr storage(error_tag_t) : err() { }
    constexpr storage(error_tag_t, const error_type& err) : err(err) { }
    constexpr storage(error_tag_t, error_type&& err) : err(std::forward<error_type>(err)) { }
    ~storage() { }
};

template<class T, class E>
struct constexpr_base {
    typedef T value_type;
    typedef E error_type;
    typedef unexpected<E> unexpected_type;
    constexpr_storage<value_type, error_type> s;
    bool has;
    constexpr constexpr_base() : s(), has(true) { }
    constexpr constexpr_base(value_tag_t tag) : s(tag), has(true) { }
    constexpr constexpr_base(error_tag_t tag) : s(tag), has(false) { }
    template<typename U = T>
    constexpr constexpr_base(value_tag_t tag, U&& val) : s(tag, std::forward<U>(val)), has(true) { }
    template<typename U = E>
    constexpr constexpr_base(error_tag_t tag, U&& err) : s(tag, std::forward<U>(err)), has(false) { }
    ~constexpr_base() = default;
};

template<class T, class E>
struct base {
    typedef T value_type;
    typedef E error_type;
    typedef unexpected<E> unexpected_type;
    storage<value_type, error_type> s;
    bool has;
    constexpr base() : s(), has(true) { }
    constexpr base(value_tag_t tag) : s(tag), has(true) { }
    constexpr base(error_tag_t tag) : s(tag), has(false) { }
    constexpr base(value_tag_t tag, const value_type& val) : s(tag, val), has(true) { }
    constexpr base(value_tag_t tag, value_type&& val) : s(tag, std::forward<value_type>(val)), has(true) { }
    constexpr base(error_tag_t tag, const error_type& err) : s(tag, err), has(false) { }
    constexpr base(error_tag_t tag, error_type&& err) : s(tag, std::forward<error_type>(err)), has(false) { }
    base(const base& o)
        : has(o.has)
    {
        if (has)
            ::new (std::addressof(s.val)) value_type(o.s.val);
        else
            ::new (std::addressof(s.err)) error_type(o.s.err);
    }
    base(base&& o)
        : has(o.has)
    {
        if (has)
            ::new (std::addressof(s.val)) value_type(std::move(o.s.val));
        else
            ::new (std::addressof(s.err)) error_type(std::move(o.s.err));
    }
    ~base()
    {
        if (has)
            destroy(s.val);
        else
            destroy(s.err);
    }
};

template<class E>
struct constexpr_base<void, E> {
    typedef void value_type;
    typedef E error_type;
    typedef unexpected<E> unexpected_type;
    constexpr_storage<value_type, error_type> s;
    bool has;
    constexpr constexpr_base() : s(), has(true) { }
    constexpr constexpr_base(value_tag_t tag) : s(tag), has(true) { }
    constexpr constexpr_base(error_tag_t tag) : s(tag), has(false) { }
    constexpr constexpr_base(error_tag_t tag, const error_type& err) : s(tag, err), has(false) { }
    constexpr constexpr_base(error_tag_t tag, error_type&& err) : s(tag, std::forward<error_type>(err)), has(false) { }
    ~constexpr_base() = default;
};

template<class E>
struct base<void, E> {
    typedef void value_type;
    typedef E error_type;
    typedef unexpected<E> unexpected_type;
    storage<value_type, error_type> s;
    bool has;
    constexpr base() : s(), has(true) { }
    constexpr base(value_tag_t tag) : s(tag), has(true) { }
    constexpr base(error_tag_t tag) : s(tag), has(false) { }
    constexpr base(error_tag_t tag, const error_type& err) : s(tag, err), has(false) { }
    constexpr base(error_tag_t tag, error_type&& err) : s(tag, std::forward<error_type>(err)), has(false) { }
    base(const base& o)
        : has(o.has)
    {
        if (!has)
            ::new (std::addressof(s.err)) error_type(o.s.err);
    }
    base(base&& o)
        : has(o.has)
    {
        if (!has)
            ::new (std::addressof(s.err)) error_type(std::move(o.s.err));
    }
    ~base()
    {
        if (!has)
            destroy(s.err);
    }
};

template<class T, class E>
using base_select = typename std::conditional<
    ((std::is_void<T>::value || std::is_trivially_destructible<T>::value)
        && std::is_trivially_destructible<E>::value),
    constexpr_base<typename std::remove_const<T>::type, typename std::remove_const<E>::type>,
    base<typename std::remove_const<T>::type, typename std::remove_const<E>::type>
>::type;

} // namespace __expected_detail

template<class T, class E>
class expected : private __expected_detail::base_select<T, E> {
    WTF_MAKE_FAST_ALLOCATED;
    typedef __expected_detail::base_select<T, E> base;

public:
    typedef typename base::value_type value_type;
    typedef typename base::error_type error_type;
    typedef typename base::unexpected_type unexpected_type;

private:
    typedef expected<value_type, error_type> type;

public:
    template<class U> struct rebind {
        using type = expected<U, error_type>;
    };

    constexpr expected() : base(__expected_detail::value_tag) { }
    expected(const expected&) = default;
    expected(expected&&) = default;

    constexpr expected(const value_type& e) : base(__expected_detail::value_tag, e) { }
    constexpr expected(value_type&& e) : base(__expected_detail::value_tag, std::forward<value_type>(e)) { }
    template<class... Args> constexpr explicit expected(std::in_place_t, Args&&... args) : base(__expected_detail::value_tag, value_type(std::forward<Args>(args)...)) { }
    // template<class U, class... Args> constexpr explicit expected(in_place_t, std::initializer_list<U>, Args&&...);
    constexpr expected(const unexpected_type& u) : base(__expected_detail::error_tag, u.value()) { }
    constexpr expected(unexpected_type&& u) : base(__expected_detail::error_tag, std::forward<unexpected_type>(u).value()) { }
    template<class Err> constexpr expected(const unexpected<Err>& u) : base(__expected_detail::error_tag, u.value()) { }
    template<class Err> constexpr expected(unexpected<Err>&& u) : base(__expected_detail::error_tag, std::forward<Err>(u.value())) { }
    template<class... Args> constexpr explicit expected(unexpected_t, Args&&... args) : base(__expected_detail::value_tag, unexpected_type(std::forward<Args>(args)...)) { }
    // template<class U, class... Args> constexpr explicit expected(unexpected_t, std::initializer_list<U>, Args&&...);

    ~expected() = default;

    expected& operator=(const expected& e) { type(e).swap(*this); return *this; }
    expected& operator=(expected&& e) { type(std::move(e)).swap(*this); return *this; }
    template<class U> expected& operator=(U&& u) { type(std::move(u)).swap(*this); return *this; }
    expected& operator=(const unexpected_type& u) { type(u).swap(*this); return *this; }
    expected& operator=(unexpected_type&& u) { type(std::move(u)).swap(*this); return *this; }
    // template<class... Args> void emplace(Args&&...);
    // template<class U, class... Args> void emplace(std::initializer_list<U>, Args&&...);

    void swap(expected& o)
    {
        using std::swap;
        if (base::has && o.has)
            swap(base::s.val, o.s.val);
        else if (base::has && !o.has) {
            error_type e(std::move(o.s.err));
            __expected_detail::destroy(o.s.err);
            ::new (std::addressof(o.s.val)) value_type(std::move(base::s.val));
            __expected_detail::destroy(base::s.val);
            ::new (std::addressof(base::s.err)) error_type(std::move(e));
            swap(base::has, o.has);
        } else if (!base::has && o.has) {
            value_type v(std::move(o.s.val));
            __expected_detail::destroy(o.s.val);
            ::new (std::addressof(o.s.err)) error_type(std::move(base::s.err));
            __expected_detail::destroy(base::s.err);
            ::new (std::addressof(base::s.val)) value_type(std::move(v));
            swap(base::has, o.has);
        } else
            swap(base::s.err, o.s.err);
    }

    constexpr const value_type* operator->() const { return &base::s.val; }
    value_type* operator->() { return &base::s.val; }
    constexpr const value_type& operator*() const & { return base::s.val; }
    value_type& operator*() & { return base::s.val; }
    constexpr const value_type&& operator*() const && { return std::move(base::s.val); }
    constexpr value_type&& operator*() && { return std::move(base::s.val); }
    constexpr explicit operator bool() const { return base::has; }
    constexpr bool has_value() const { return base::has; }
    constexpr const value_type& value() const & { return base::has ? base::s.val : (__EXPECTED_THROW(bad_expected_access<error_type>(base::s.err)), base::s.val); }
    constexpr value_type& value() & { return base::has ? base::s.val : (__EXPECTED_THROW(bad_expected_access<error_type>(base::s.err)), base::s.val); }
    constexpr const value_type&& value() const && { return std::move(base::has ? base::s.val : (__EXPECTED_THROW(bad_expected_access<error_type>(base::s.err)), base::s.val)); }
    constexpr value_type&& value() && { return std::move(base::has ? base::s.val : (__EXPECTED_THROW(bad_expected_access<error_type>(base::s.err)), base::s.val)); }
    constexpr const error_type& error() const & { return !base::has ? base::s.err : (__EXPECTED_THROW(bad_expected_access<void>()), base::s.err); }
    error_type& error() & { return !base::has ? base::s.err : (__EXPECTED_THROW(bad_expected_access<void>()), base::s.err); }
    constexpr error_type&& error() && { return std::move(!base::has ? base::s.err : (__EXPECTED_THROW(bad_expected_access<void>()), base::s.err)); }
    constexpr const error_type&& error() const && { return std::move(!base::has ? base::s.err : (__EXPECTED_THROW(bad_expected_access<void>()), base::s.err)); }
    template<class U> constexpr value_type value_or(U&& u) const & { return base::has ? **this : static_cast<value_type>(std::forward<U>(u)); }
    template<class U> value_type value_or(U&& u) && { return base::has ? std::move(**this) : static_cast<value_type>(std::forward<U>(u)); }
};

template<class E>
class expected<void, E> : private __expected_detail::base_select<void, E> {
    typedef __expected_detail::base_select<void, E> base;

public:
    typedef typename base::value_type value_type;
    typedef typename base::error_type error_type;
    typedef typename base::unexpected_type unexpected_type;

private:
    typedef expected<value_type, error_type> type;

public:
    template<class U> struct rebind {
        using type = expected<U, error_type>;
    };

    constexpr expected() : base(__expected_detail::value_tag) { }
    expected(const expected&) = default;
    expected(expected&&) = default;
    // constexpr explicit expected(in_place_t);
    constexpr expected(unexpected_type const& u) : base(__expected_detail::error_tag, u.value()) { }
    constexpr expected(unexpected_type&& u) : base(__expected_detail::error_tag, std::forward<unexpected_type>(u).value()) { }
    template<class Err> constexpr expected(unexpected<Err> const& u) : base(__expected_detail::error_tag, u.value()) { }

    ~expected() = default;

    expected& operator=(const expected& e) { type(e).swap(*this); return *this; }
    expected& operator=(expected&& e) { type(std::move(e)).swap(*this); return *this; }
    expected& operator=(const unexpected_type& u) { type(u).swap(*this); return *this; } // Not in the current paper.
    expected& operator=(unexpected_type&& u) { type(std::move(u)).swap(*this); return *this; } // Not in the current paper.
    // void emplace();

    void swap(expected& o)
    {
        using std::swap;
        if (base::has && o.has) {
            // Do nothing.
        } else if (base::has && !o.has) {
            error_type e(std::move(o.s.err));
            ::new (std::addressof(base::s.err)) error_type(e);
            swap(base::has, o.has);
        } else if (!base::has && o.has) {
            ::new (std::addressof(o.s.err)) error_type(std::move(base::s.err));
            swap(base::has, o.has);
        } else
            swap(base::s.err, o.s.err);
    }

    constexpr explicit operator bool() const { return base::has; }
    constexpr bool has_value() const { return base::has; }
    void value() const { !base::has ? __EXPECTED_THROW(bad_expected_access<void>()) : void(); }
    constexpr const E& error() const & { return !base::has ? base::s.err : (__EXPECTED_THROW(bad_expected_access<void>()), base::s.err); }
    E& error() & { return !base::has ? base::s.err : (__EXPECTED_THROW(bad_expected_access<void>()), base::s.err); }
    constexpr E&& error() && { return std::move(!base::has ? base::s.err : (__EXPECTED_THROW(bad_expected_access<void>()), base::s.err)); }
};

template<class T, class E> constexpr bool operator==(const expected<T, E>& x, const expected<T, E>& y) { return bool(x) == bool(y) && (x ? x.value() == y.value() : x.error() == y.error()); }
template<class T, class E> constexpr bool operator!=(const expected<T, E>& x, const expected<T, E>& y) { return !(x == y); }

template<class E> constexpr bool operator==(const expected<void, E>& x, const expected<void, E>& y) { return bool(x) == bool(y) && (x ? true : x.error() == y.error()); }

template<class T, class E> constexpr bool operator==(const expected<T, E>& x, const T& y) { return x ? *x == y : false; }
template<class T, class E> constexpr bool operator==(const T& x, const expected<T, E>& y) { return y ? x == *y : false; }
template<class T, class E> constexpr bool operator!=(const expected<T, E>& x, const T& y) { return x ? *x != y : true; }
template<class T, class E> constexpr bool operator!=(const T& x, const expected<T, E>& y) { return y ? x != *y : true; }

template<class T, class E> constexpr bool operator==(const expected<T, E>& x, const unexpected<E>& y) { return x ? false : x.error() == y.value(); }
template<class T, class E> constexpr bool operator==(const unexpected<E>& x, const expected<T, E>& y) { return y ? false : x.value() == y.error(); }
template<class T, class E> constexpr bool operator!=(const expected<T, E>& x, const unexpected<E>& y) { return x ? true : x.error() != y.value(); }
template<class T, class E> constexpr bool operator!=(const unexpected<E>& x, const expected<T, E>& y) { return y ? true : x.value() != y.error(); }

template<typename T, typename E> void swap(expected<T, E>& x, expected<T, E>& y) { x.swap(y); }

}}} // namespace std::experimental::fundamentals_v3

__EXPECTED_INLINE_VARIABLE constexpr auto& unexpect = std::experimental::unexpect;
template<class T, class E> using Expected = std::experimental::expected<T, E>;
