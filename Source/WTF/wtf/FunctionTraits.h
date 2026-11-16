/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#include <tuple>
#include <type_traits>
#include <wtf/Platform.h>

namespace WTF {

template<typename T>
struct FunctionTraits;

template<typename Result, typename... Args>
struct FunctionTraits<Result(Args...)> {
    using ResultType = Result;

    static constexpr bool hasResult = !std::is_same<ResultType, void>::value;

    static constexpr std::size_t arity = sizeof...(Args);

    template<std::size_t n>
        requires (n < arity)
    using ArgumentType = typename std::tuple_element<n, std::tuple<Args...>>::type;
    using ArgumentTypes = std::tuple<Args...>;

    static constexpr unsigned cCallArity() { return arity; }
};

#if OS(WINDOWS) && CPU(X86_64)
template<typename Result, typename... Args>
struct FunctionTraits<Result SYSV_ABI(Args...)> : public FunctionTraits<Result(Args...)> {
};
#endif

template<typename Result, typename... Args>
struct FunctionTraits<Result(*)(Args...)> : public FunctionTraits<Result(Args...)> {
};

#if OS(WINDOWS) && CPU(X86_64)
template<typename Result, typename... Args>
struct FunctionTraits<Result SYSV_ABI (*)(Args...)> : public FunctionTraits<Result(Args...)> {
};
#endif

template<typename Result, typename... Args>
struct FunctionTraits<Result(Args...) noexcept> : public FunctionTraits<Result(Args...)> {
};

template<typename Result, typename... Args>
struct FunctionTraits<Result(*)(Args...) noexcept> : public FunctionTraits<Result(Args...)> {
};

} // namespace WTF

using WTF::FunctionTraits;
