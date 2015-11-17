/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef ScopedLambda_h
#define ScopedLambda_h

namespace WTF {

// You can use ScopedLambda to efficiently pass lambdas without allocating memory or requiring
// template specialization of the callee. The callee should be declared as:
//
// void foo(const ScopedLambda<MyThings* (int, Stuff&)>&);
//
// The caller just does:
//
// void foo(scopedLambda<MyThings* (int, Stuff&)>([&] (int x, Stuff& y) -> MyThings* { blah }));
//
// Note that this relies on foo() not escaping the lambda. The lambda is only valid while foo() is
// on the stack - hence the name ScopedLambda.

template<typename FunctionType> class ScopedLambda;
template<typename ResultType, typename... ArgumentTypes>
class ScopedLambda<ResultType (ArgumentTypes...)> {
public:
    ScopedLambda(ResultType (*impl)(void* arg, ArgumentTypes...) = nullptr, void* arg = nullptr)
        : m_impl(impl)
        , m_arg(arg)
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

template<typename FunctionType, typename Functor> class ScopedLambdaFunctor;
template<typename ResultType, typename... ArgumentTypes, typename Functor>
class ScopedLambdaFunctor<ResultType (ArgumentTypes...), Functor> : public ScopedLambda<ResultType (ArgumentTypes...)> {
public:
    template<typename PassedFunctor>
    ScopedLambdaFunctor(PassedFunctor&& functor)
        : ScopedLambda<ResultType (ArgumentTypes...)>(implFunction, this)
        , m_functor(std::forward<PassedFunctor>(functor))
    {
    }

private:
    static ResultType implFunction(void* argument, ArgumentTypes... arguments)
    {
        return static_cast<ScopedLambdaFunctor*>(argument)->m_functor(arguments...);
    }

    Functor m_functor;
};

template<typename FunctionType, typename Functor>
ScopedLambdaFunctor<FunctionType, Functor> scopedLambda(Functor&& functor)
{
    return ScopedLambdaFunctor<FunctionType, Functor>(std::forward<Functor>(functor));
}

} // namespace WTF

using WTF::ScopedLambda;
using WTF::scopedLambda;

#endif // ScopedLambda_h

