/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <type_traits>
#include <wtf/Forward.h>
#include <wtf/FunctionDispatcher.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>

namespace WTF {

namespace detail {

// IsRefcountedSmartPointer implementation.
template<typename CVRemoved>
struct IsRefcountedSmartPointerHelper : std::false_type { };

template<typename Pointee>
struct IsRefcountedSmartPointerHelper<RefPtr<Pointee>> : std::true_type { };

template<typename Pointee>
struct IsRefcountedSmartPointerHelper<Ref<Pointee>> : std::true_type { };

} // namespace detail

template<typename T>
struct IsRefcountedSmartPointer : detail::IsRefcountedSmartPointerHelper<std::remove_cv_t<T>> { };

// IsSmartRef implementation
namespace detail {

template<typename CVRemoved>
struct IsSmartRefHelper : std::false_type { };

template<typename Pointee>
struct IsSmartRefHelper<Ref<Pointee>> : std::true_type { };

} // namespace detail

template<typename T>
struct IsSmartRef : detail::IsSmartRefHelper<std::remove_cv_t<T>> { };

// RemoveSmartPointer implementation
namespace detail {

template<typename T, typename CVRemoved>
struct RemoveSmartPointerHelper {
    typedef T type;
};

template<typename T, typename Pointee>
struct RemoveSmartPointerHelper<T, RefPtr<Pointee>> {
    typedef Pointee type;
};

template<typename T, typename Pointee>
struct RemoveSmartPointerHelper<T, Ref<Pointee>> {
    typedef Pointee type;
};

} // namespace detail

template<typename T>
struct RemoveSmartPointer : detail::RemoveSmartPointerHelper<T, std::remove_cv_t<T>> { };

// HasRefCountMethods implementation
namespace detail {

template<typename>
struct SFINAE1True : std::true_type { };

template<class T>
static auto HasRefCountMethodsTest(int) -> SFINAE1True<decltype(std::declval<T>().ref(), std::declval<T>().deref())>;
template<class>
static auto HasRefCountMethodsTest(long) -> std::false_type;

} // namespace detail

template<class T>
struct HasRefCountMethods : decltype(detail::HasRefCountMethodsTest<T>(0)) { };

// HasIsolatedCopy()
namespace detail {

template<class T>
static auto HasIsolatedCopyTest(int) -> SFINAE1True<decltype(std::declval<T>().isolatedCopy())>;
template<class>
static auto HasIsolatedCopyTest(long) -> std::false_type;

} // namespace detail

template<class T>
struct HasIsolatedCopy : decltype(detail::HasIsolatedCopyTest<T>(0)) { };

// LooksLikeRCSerialDispatcher implementation
namespace detail {

template <bool b, typename>
struct SFINAE1If : std::integral_constant<bool, b> { };

template <bool b, class T>
static auto LooksLikeRCSerialDispatcherTest(int)
    -> SFINAE1If<b, decltype(std::declval<T>().ref(), std::declval<T>().deref())>;

template <bool, typename>
static auto LooksLikeRCSerialDispatcherTest(long) -> std::false_type;

} // namespace detail

template <class T>
struct LooksLikeRCSerialDispatcher : decltype(detail::LooksLikeRCSerialDispatcherTest<std::is_base_of_v<SerialFunctionDispatcher, T>, T>(0)) { };

class NativePromiseBase;
class ConvertibleToNativePromise;

// The use of C++20 concepts causes a crash with the current msvc (see webkit.org/b/261598)
#if !COMPILER(MSVC)
template <typename T>
concept IsNativePromise = std::is_base_of<NativePromiseBase, T>::value;

template <typename T>
concept IsConvertibleToNativePromise = std::is_base_of<ConvertibleToNativePromise, T>::value;

template <typename T, typename U>
concept RelatedNativePromise = requires(T, U)
{
    { IsConvertibleToNativePromise<T> };
    { IsConvertibleToNativePromise<U> };
    { std::is_same<typename T::PromiseType, typename U::PromiseType>::value };
};
#else
template <typename T>
constexpr bool IsNativePromise = std::is_base_of<NativePromiseBase, T>::value;

template <typename T>
constexpr bool IsConvertibleToNativePromise = std::is_base_of<ConvertibleToNativePromise, T>::value;

// The test isn't as exhaustive as concept's version.
// It will prevent having more user-friendly compilation error should mix&match of NativePromise is used.
// This will do for now.
template <typename T, typename U>
constexpr bool RelatedNativePromise = IsConvertibleToNativePromise<T> && IsConvertibleToNativePromise<U>;
#endif


template <typename T>
struct IsExpected : std::false_type { };

template <typename T, typename E>
struct IsExpected<Expected<T, E>> : std::true_type { };

} // namespace NTF
