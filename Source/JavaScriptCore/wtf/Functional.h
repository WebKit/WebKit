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

// Helper class template to determine whether a given type has ref and deref member functions
// with the right type signature.
template<typename T> class HasRefAndDeref {
    typedef char YesType;
    struct NoType {
        char padding[8];
    };

    template<typename U, U, U> struct TypeChecker { };

    template<typename U>
    static YesType refAndDerefCheck(TypeChecker<void (U::*)(), &U::ref, &U::deref>*);

    template<typename U>
    static NoType refAndDerefCheck(...);

public:
    static const bool value = sizeof(refAndDerefCheck<T>(0)) == sizeof(YesType);
};

// A FunctionWrapper is a class template that can wrap a function pointer or a member function pointer and
// provide a unified interface for calling that function.
template<typename> class FunctionWrapper;

template<typename R> class FunctionWrapper<R (*)()> {
public:
    typedef R ResultType;
    static const bool shouldRefFirstParameter = false;

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

template<typename R, typename P0> class FunctionWrapper<R (*)(P0)> {
public:
    typedef R ResultType;
    static const bool shouldRefFirstParameter = false;

    explicit FunctionWrapper(R (*function)(P0))
        : m_function(function)
    {
    }

    R operator()(P0 p0)
    {
        return m_function(p0);
    }

private:
    R (*m_function)(P0);
};

template<typename R, typename P0, typename P1> class FunctionWrapper<R (*)(P0, P1)> {
public:
    typedef R ResultType;
    static const bool shouldRefFirstParameter = false;

    explicit FunctionWrapper(R (*function)(P0, P1))
        : m_function(function)
    {
    }

    R operator()(P0 p0, P1 p1)
    {
        return m_function(p0, p1);
    }

private:
    R (*m_function)(P0, P1);
};

template<typename R, typename C> class FunctionWrapper<R (C::*)()> {
public:
    typedef R ResultType;
    static const bool shouldRefFirstParameter = HasRefAndDeref<C>::value;

    explicit FunctionWrapper(R (C::*function)())
        : m_function(function)
    {
    }

    R operator()(C* c)
    {
        return (c->*m_function)();
    }

private:
    R (C::*m_function)();
};

template<typename R, typename C, typename P0> class FunctionWrapper<R (C::*)(P0)> {
public:
    typedef R ResultType;
    static const bool shouldRefFirstParameter = HasRefAndDeref<C>::value;

    explicit FunctionWrapper(R (C::*function)(P0))
        : m_function(function)
    {
    }

    R operator()(C* c, P0 p0)
    {
        return (c->*m_function)(p0);
    }

private:
    R (C::*m_function)(P0);
};

template<typename T, bool shouldRefAndDeref> struct RefAndDeref {
    static void ref(T) { }
    static void deref(T) { }
};

template<typename T> struct RefAndDeref<T*, true> {
    static void ref(T* t) { t->ref(); }
    static void deref(T* t) { t->deref(); }
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

template<typename FunctionWrapper, typename R> class BoundFunctionImpl<FunctionWrapper, R ()> : public FunctionImpl<typename FunctionWrapper::ResultType ()> {
public:
    explicit BoundFunctionImpl(FunctionWrapper functionWrapper)
        : m_functionWrapper(functionWrapper)
    {
    }

    virtual R operator()()
    {
        return m_functionWrapper();
    }

private:
    FunctionWrapper m_functionWrapper;
};

template<typename FunctionWrapper, typename R, typename P0> class BoundFunctionImpl<FunctionWrapper, R (P0)> : public FunctionImpl<typename FunctionWrapper::ResultType ()> {

public:
    BoundFunctionImpl(FunctionWrapper functionWrapper, const P0& p0)
        : m_functionWrapper(functionWrapper)
        , m_p0(p0)
    {
        RefAndDeref<P0, FunctionWrapper::shouldRefFirstParameter>::ref(m_p0);
    }

    ~BoundFunctionImpl()
    {
        RefAndDeref<P0, FunctionWrapper::shouldRefFirstParameter>::deref(m_p0);
    }

    virtual R operator()()
    {
        return m_functionWrapper(m_p0);
    }

private:
    FunctionWrapper m_functionWrapper;
    P0 m_p0;
};

template<typename FunctionWrapper, typename R, typename P0, typename P1> class BoundFunctionImpl<FunctionWrapper, R (P0, P1)> : public FunctionImpl<typename FunctionWrapper::ResultType ()> {
public:
    BoundFunctionImpl(FunctionWrapper functionWrapper, const P0& p0, const P1& p1)
        : m_functionWrapper(functionWrapper)
        , m_p0(p0)
        , m_p1(p1)
    {
        RefAndDeref<P0, FunctionWrapper::shouldRefFirstParameter>::ref(m_p0);
    }
    
    ~BoundFunctionImpl()
    {
        RefAndDeref<P0, FunctionWrapper::shouldRefFirstParameter>::deref(m_p0);
    }

    virtual typename FunctionWrapper::ResultType operator()()
    {
        return m_functionWrapper(m_p0, m_p1);
    }

private:
    FunctionWrapper m_functionWrapper;
    P0 m_p0;
    P1 m_p1;
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

template<typename FunctionType, typename A1>
Function<typename FunctionWrapper<FunctionType>::ResultType ()> bind(FunctionType function, const A1& a1)
{
    return Function<typename FunctionWrapper<FunctionType>::ResultType ()>(adoptRef(new BoundFunctionImpl<FunctionWrapper<FunctionType>, typename FunctionWrapper<FunctionType>::ResultType (A1)>(FunctionWrapper<FunctionType>(function), a1)));
}

template<typename FunctionType, typename A1, typename A2>
Function<typename FunctionWrapper<FunctionType>::ResultType ()> bind(FunctionType function, const A1& a1, const A2& a2)
{
    return Function<typename FunctionWrapper<FunctionType>::ResultType ()>(adoptRef(new BoundFunctionImpl<FunctionWrapper<FunctionType>, typename FunctionWrapper<FunctionType>::ResultType (A1, A2)>(FunctionWrapper<FunctionType>(function), a1, a2)));
}

}

using WTF::Function;
using WTF::bind;

#endif // WTF_Functional_h
