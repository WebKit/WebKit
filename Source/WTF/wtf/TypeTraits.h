/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Fady Farag. All rights reserved.
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

#include <cstddef>
#include <expected>
#include <type_traits>
#include <utility>
#include <wtf/Forward.h>

namespace WTF {

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

template<typename T>
concept HasRefPtrMemberFunctions = requires(std::remove_cv_t<T>* ptr)
{
    ptr->ref();
    ptr->deref();
};

template<typename T>
concept HasWeakPtrFunctions = requires(std::remove_cv_t<T>* ptr)
{
    ptr->weakImpl();
    ptr->weakCount();
};

template<typename T>
concept HasThreadSafeWeakPtrFunctions = requires(std::remove_cv_t<T>* ptr)
{
    ptr->weakRefCount();
};

template<typename T>
concept HasCheckedPtrMemberFunctions = requires(std::remove_cv_t<T>* ptr)
{
    ptr->incrementCheckedPtrCount();
    ptr->decrementCheckedPtrCount();
};

template<typename T>
concept IsCompleteType = requires { sizeof(T); };

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
struct IsExpected<std::expected<T, E>> : std::true_type { };

template <typename... Args>
struct ParameterCountImpl {
    static constexpr std::size_t value = sizeof...(Args);
};

template <typename ReturnType, typename... Args>
constexpr std::size_t parameterCount(ReturnType(*)(Args...))
{
    return ParameterCountImpl<Args...>::value;
}

#if defined(__has_feature)
#if __has_feature(objc_arc)
struct ARCEnabled : std::true_type { };
#else
struct ARCEnabled : std::false_type { };
#endif
#else
struct ARCEnabled : std::false_type { };
#endif

} // namespace NTF
