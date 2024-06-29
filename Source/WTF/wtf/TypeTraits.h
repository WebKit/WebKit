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

// SFINAE depends on overload resolution. We indicate the overload we'd prefer
// (if it can compile) using a higher priorty type (int), and the overload
// to fall back to using a lower priority type (long). 0 can convert to int
// or long, so we can trigger overload resolution using 0. C++ is awesome!
#define SFINAE_OVERLOAD 0
#define SFINAE_OVERLOAD_DEFAULT long
#define SFINAE_OVERLOAD_PREFERRED int

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

// HasRefPtrMethods implementation
namespace detail {

template<typename>
struct SFINAE1True : std::true_type { };

template<class T>
static auto HasRefPtrMethodsTest(SFINAE_OVERLOAD_PREFERRED) -> SFINAE1True<decltype(static_cast<std::remove_cv_t<T>*>(nullptr)->ref(), static_cast<std::remove_cv_t<T>*>(nullptr)->deref())>;
template<class>
static auto HasRefPtrMethodsTest(SFINAE_OVERLOAD_DEFAULT) -> std::false_type;

} // namespace detail

template<class T>
struct HasRefPtrMethods : decltype(detail::HasRefPtrMethodsTest<T>(SFINAE_OVERLOAD)) { };

// HasCheckedPtrMethods implementation
namespace detail {

template<class T>
static auto HasCheckedPtrMethodsTest(SFINAE_OVERLOAD_PREFERRED) -> SFINAE1True<decltype(static_cast<std::remove_cv_t<T>*>(nullptr)->incrementPtrCount(), static_cast<std::remove_cv_t<T>*>(nullptr)->decrementPtrCount())>;
template<class>
static auto HasCheckedPtrMethodsTest(SFINAE_OVERLOAD_DEFAULT) -> std::false_type;

} // namespace detail

template<class T>
struct HasCheckedPtrMethods : decltype(detail::HasCheckedPtrMethodsTest<T>(SFINAE_OVERLOAD)) { };

// HasIsolatedCopy()
namespace detail {

// FIXME: This test is incorrectly false for RefCounted objects because
// substitution for std::declval<T>() fails when the constructor is private.
template<class T>
static auto HasIsolatedCopyTest(SFINAE_OVERLOAD_PREFERRED) -> SFINAE1True<decltype(std::declval<T>().isolatedCopy())>;
template<class>
static auto HasIsolatedCopyTest(SFINAE_OVERLOAD_DEFAULT) -> std::false_type;

} // namespace detail

template<class T>
struct HasIsolatedCopy : decltype(detail::HasIsolatedCopyTest<T>(SFINAE_OVERLOAD)) { };

// LooksLikeRCSerialDispatcher implementation
namespace detail {

template <bool b, typename>
struct SFINAE1If : std::integral_constant<bool, b> { };

template <bool b, class T>
static auto LooksLikeRCSerialDispatcherTest(SFINAE_OVERLOAD_PREFERRED)
    -> SFINAE1If<b, decltype(std::declval<T>().ref(), std::declval<T>().deref())>;

template <bool, typename>
static auto LooksLikeRCSerialDispatcherTest(SFINAE_OVERLOAD_DEFAULT) -> std::false_type;

} // namespace detail

template <class T>
struct LooksLikeRCSerialDispatcher : decltype(detail::LooksLikeRCSerialDispatcherTest<std::is_base_of_v<SerialFunctionDispatcher, T>, T>(SFINAE_OVERLOAD)) { };

class NativePromiseBase;
class ConvertibleToNativePromise;

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

template <typename T>
struct IsExpected : std::false_type { };

template <typename T, typename E>
struct IsExpected<Expected<T, E>> : std::true_type { };

} // namespace NTF
