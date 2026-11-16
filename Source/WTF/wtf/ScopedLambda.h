/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include <concepts>
#include <type_traits>
#include <wtf/Compiler.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/Nonmovable.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

// You can use ScopedLambda to efficiently pass lambdas without allocating memory or requiring
// template specialization of the callee. The callee should be declared as:
//
// void foo(const ScopedLambda<MyThings* (int, Stuff&)>&);
//
// The caller just passes a lambda, which implicitly converts:
//
// foo([&] (int x, Stuff& y) -> MyThings* { blah });
//
// Note that this relies on foo() not escaping the lambda. A ScopedLambda points to its functor
// rather than owning it, so in the call above the lambda is only valid until the end of the full
// expression (hence the name ScopedLambda). To hold a ScopedLambda in a variable, keep the lambda in
// a variable as well so that it outlives the ScopedLambda pointing at it:
//
// auto myLambda = [&] (int x, Stuff& y) -> MyThings* { blah };
// ScopedLambda<MyThings* (int, Stuff&)> myScopedLambda = myLambda;

template<typename FunctionType> class ScopedLambda;
template<typename ResultType, typename... ArgumentTypes>
class ScopedLambda<ResultType(ArgumentTypes...)> final {
    WTF_FORBID_HEAP_ALLOCATION;
    WTF_MAKE_NONMOVABLE(ScopedLambda);
public:
    ScopedLambda(ResultType (*impl)(void* arg, ArgumentTypes...) = nullptr, void* arg = nullptr)
        : m_impl(impl)
        , m_arg(arg)
    {
    }


    template<typename Functor>
    requires (!std::same_as<std::remove_cvref_t<Functor>, std::nullptr_t>
        && !std::same_as<std::remove_cvref_t<Functor>, ScopedLambda>
        && Invocable<Functor, ResultType(ArgumentTypes...)>)
    ScopedLambda(Functor&& functor LIFETIME_BOUND)
        : m_impl([] (void* argument, ArgumentTypes... arguments) -> ResultType {
            auto& boundFunctor = *static_cast<std::remove_reference_t<Functor>*>(argument);
            // A functor may return a value that the signature discards, which Invocable permits.
            if constexpr (std::is_void_v<ResultType>)
                boundFunctor(arguments...);
            else
                return boundFunctor(arguments...);
        })
        , m_arg(const_cast<void*>(static_cast<const void*>(std::addressof(functor))))
    {
    }

    template<typename... PassedArgumentTypes>
    ResultType operator()(PassedArgumentTypes&&... arguments) const
    {
        return m_impl(m_arg, std::forward<PassedArgumentTypes>(arguments)...);
    }

private:
    ResultType (*m_impl)(void* arg, ArgumentTypes...);
    void *m_arg;
};

} // namespace WTF

using WTF::ScopedLambda;
