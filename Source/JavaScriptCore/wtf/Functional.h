/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef WTF_Functional_h
#define WTF_Functional_h

#include "Assertions.h"
#include "PassRefPtr.h"
#include "RefPtr.h"
#include "ThreadSafeRefCounted.h"

namespace WTF {

// Functional.h provides a very simple way to bind a function pointer and arguments together into a function object
// that can be stored, copied and invoked, similar to how boost::bind and std::bind in C++11.
// The implementation is currently very simple, but the goal is to replace WorkItem in WebKit2 and make it easier to
// package up and invoke function calls inside WebCore.

// A FunctionWrapper is a class template that can wrap a function pointer or a member function pointer and
// provide a unified interface for calling that function.
template<typename> class FunctionWrapper;

template<typename R> class FunctionWrapper<R (*)()> {
public:
    typedef R ResultType;

    explicit FunctionWrapper(R (*function)())
        : m_function(function)
    {
    }

    R operator()()
    {
        return m_function();
    }

private:
    R (*m_function)();
};

class FunctionImplBase : public ThreadSafeRefCounted<FunctionImplBase> {
public:
    virtual ~FunctionImplBase() { }
};

template<typename>
class FunctionImpl;

template<typename R>
class FunctionImpl<R ()> : public FunctionImplBase {
public:
    virtual R operator()() = 0;
};

template<typename FunctionWrapper, typename FunctionType>
class BoundFunctionImpl;

template<typename FunctionWrapper> class BoundFunctionImpl<FunctionWrapper, typename FunctionWrapper::ResultType ()> : public FunctionImpl<typename FunctionWrapper::ResultType ()> {
public:
    explicit BoundFunctionImpl(FunctionWrapper functionWrapper)
        : m_functionWrapper(functionWrapper)
    {
    }

    virtual typename FunctionWrapper::ResultType operator()()
    {
        return m_functionWrapper();
    }

private:
    FunctionWrapper m_functionWrapper;
};

class FunctionBase {
public:
    bool isNull() const
    {
        return !m_impl;
    }

protected:
    FunctionBase()
    {
    }

    explicit FunctionBase(PassRefPtr<FunctionImplBase> impl)
        : m_impl(impl)
    {
    }

    template<typename FunctionType> FunctionImpl<FunctionType>* impl() const
    { 
        return static_cast<FunctionImpl<FunctionType>*>(m_impl.get());
    }

private:
    RefPtr<FunctionImplBase> m_impl;
};

template<typename> class Function;

template<typename R>
class Function<R ()> : public FunctionBase {
public:
    Function()
    {
    }

    Function(PassRefPtr<FunctionImpl<R ()> > impl)
        : FunctionBase(impl)
    {
    }

    R operator()()
    {
        ASSERT(!isNull());

        return impl<R ()>()->operator()();
    }
};

template<typename FunctionType>
Function<typename FunctionWrapper<FunctionType>::ResultType ()> bind(FunctionType function)
{
    return Function<typename FunctionWrapper<FunctionType>::ResultType ()>(adoptRef(new BoundFunctionImpl<FunctionWrapper<FunctionType>, typename FunctionWrapper<FunctionType>::ResultType ()>(FunctionWrapper<FunctionType>(function))));
}

}

using WTF::Function;
using WTF::bind;

#endif // WTF_Functional_h
