/*
 * Copyright (C) 2009-2010 Google Inc. All rights reserved.
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

#ifndef GenericTask_h
#define GenericTask_h

#include "CrossThreadCopier.h"
#include "ScriptExecutionContext.h"
#include <memory>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/TypeTraits.h>

namespace WebCore {

// Traits for the GenericTask.
template<typename T> struct GenericTaskTraits {
    typedef const T& ParamType;
};

template<typename T> struct GenericTaskTraits<T*> {
    typedef T* ParamType;
};

template<typename T> struct GenericTaskTraits<PassRefPtr<T> > {
    typedef PassRefPtr<T> ParamType;
};

template<typename T> struct GenericTaskTraits<PassOwnPtr<T> > {
    typedef PassOwnPtr<T> ParamType;
};

template<typename P1, typename MP1>
class GenericTask1 : public ScriptExecutionContext::Task {
public:
    typedef void (*Method)(ScriptExecutionContext*, MP1);
    typedef GenericTask1<P1, MP1> GenericTask;
    typedef typename GenericTaskTraits<P1>::ParamType Param1;

    static PassOwnPtr<GenericTask> create(Method method, Param1 parameter1)
    {
        return new GenericTask(method, parameter1);
    }

private:
    GenericTask1(Method method, Param1 parameter1)
        : m_method(method)
        , m_parameter1(parameter1)
    {
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        (*m_method)(context, m_parameter1);
    }

private:
    Method m_method;
    P1 m_parameter1;
};

template<typename P1, typename MP1, typename P2, typename MP2>
class GenericTask2 : public ScriptExecutionContext::Task {
public:
    typedef void (*Method)(ScriptExecutionContext*, MP1, MP2);
    typedef GenericTask2<P1, MP1, P2, MP2> GenericTask;
    typedef typename GenericTaskTraits<P1>::ParamType Param1;
    typedef typename GenericTaskTraits<P2>::ParamType Param2;

    static PassOwnPtr<GenericTask> create(Method method, Param1 parameter1, Param2 parameter2)
    {
        return new GenericTask(method, parameter1, parameter2);
    }

private:
    GenericTask2(Method method, Param1 parameter1, Param2 parameter2)
        : m_method(method)
        , m_parameter1(parameter1)
        , m_parameter2(parameter2)
    {
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        (*m_method)(context, m_parameter1, m_parameter2);
    }

private:
    Method m_method;
    P1 m_parameter1;
    P2 m_parameter2;
};

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3>
class GenericTask3 : public ScriptExecutionContext::Task {
public:
    typedef void (*Method)(ScriptExecutionContext*, MP1, MP2, MP3);
    typedef GenericTask3<P1, MP1, P2, MP2, P3, MP3> GenericTask;
    typedef typename GenericTaskTraits<P1>::ParamType Param1;
    typedef typename GenericTaskTraits<P2>::ParamType Param2;
    typedef typename GenericTaskTraits<P3>::ParamType Param3;

    static PassOwnPtr<GenericTask> create(Method method, Param1 parameter1, Param2 parameter2, Param3 parameter3)
    {
        return new GenericTask(method, parameter1, parameter2, parameter3);
    }

private:
    GenericTask3(Method method, Param1 parameter1, Param2 parameter2, Param3 parameter3)
        : m_method(method)
        , m_parameter1(parameter1)
        , m_parameter2(parameter2)
        , m_parameter3(parameter3)
    {
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        (*m_method)(context, m_parameter1, m_parameter2, m_parameter3);
    }

private:
    Method m_method;
    P1 m_parameter1;
    P2 m_parameter2;
    P3 m_parameter3;
};

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4>
class GenericTask4 : public ScriptExecutionContext::Task {
public:
    typedef void (*Method)(ScriptExecutionContext*, MP1, MP2, MP3, MP4);
    typedef GenericTask4<P1, MP1, P2, MP2, P3, MP3, P4, MP4> GenericTask;
    typedef typename GenericTaskTraits<P1>::ParamType Param1;
    typedef typename GenericTaskTraits<P2>::ParamType Param2;
    typedef typename GenericTaskTraits<P3>::ParamType Param3;
    typedef typename GenericTaskTraits<P4>::ParamType Param4;

    static PassOwnPtr<GenericTask> create(Method method, Param1 parameter1, Param2 parameter2, Param3 parameter3, Param4 parameter4)
    {
        return new GenericTask(method, parameter1, parameter2, parameter3, parameter4);
    }

private:
    GenericTask4(Method method, Param1 parameter1, Param2 parameter2, Param3 parameter3, Param4 parameter4)
        : m_method(method)
        , m_parameter1(parameter1)
        , m_parameter2(parameter2)
        , m_parameter3(parameter3)
        , m_parameter4(parameter4)
    {
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        (*m_method)(context, m_parameter1, m_parameter2, m_parameter3, m_parameter4);
    }

private:
    Method m_method;
    P1 m_parameter1;
    P2 m_parameter2;
    P3 m_parameter3;
    P4 m_parameter4;
};

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5>
class GenericTask5 : public ScriptExecutionContext::Task {
public:
    typedef void (*Method)(ScriptExecutionContext*, MP1, MP2, MP3, MP4, MP5);
    typedef GenericTask5<P1, MP1, P2, MP2, P3, MP3, P4, MP4, P5, MP5> GenericTask;
    typedef typename GenericTaskTraits<P1>::ParamType Param1;
    typedef typename GenericTaskTraits<P2>::ParamType Param2;
    typedef typename GenericTaskTraits<P3>::ParamType Param3;
    typedef typename GenericTaskTraits<P4>::ParamType Param4;
    typedef typename GenericTaskTraits<P5>::ParamType Param5;

    static PassOwnPtr<GenericTask> create(Method method, Param1 parameter1, Param2 parameter2, Param3 parameter3, Param4 parameter4, Param5 parameter5)
    {
        return new GenericTask(method, parameter1, parameter2, parameter3, parameter4, parameter5);
    }

private:
    GenericTask5(Method method, Param1 parameter1, Param2 parameter2, Param3 parameter3, Param4 parameter4, Param5 parameter5)
        : m_method(method)
        , m_parameter1(parameter1)
        , m_parameter2(parameter2)
        , m_parameter3(parameter3)
        , m_parameter4(parameter4)
        , m_parameter5(parameter5)
    {
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        (*m_method)(context, m_parameter1, m_parameter2, m_parameter3, m_parameter4, m_parameter5);
    }

private:
    Method m_method;
    P1 m_parameter1;
    P2 m_parameter2;
    P3 m_parameter3;
    P4 m_parameter4;
    P5 m_parameter5;
};

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5, typename P6, typename MP6>
class GenericTask6 : public ScriptExecutionContext::Task {
public:
    typedef void (*Method)(ScriptExecutionContext*, MP1, MP2, MP3, MP4, MP5, MP6);
    typedef GenericTask6<P1, MP1, P2, MP2, P3, MP3, P4, MP4, P5, MP5, P6, MP6> GenericTask;
    typedef typename GenericTaskTraits<P1>::ParamType Param1;
    typedef typename GenericTaskTraits<P2>::ParamType Param2;
    typedef typename GenericTaskTraits<P3>::ParamType Param3;
    typedef typename GenericTaskTraits<P4>::ParamType Param4;
    typedef typename GenericTaskTraits<P5>::ParamType Param5;
    typedef typename GenericTaskTraits<P6>::ParamType Param6;

    static PassOwnPtr<GenericTask> create(Method method, Param1 parameter1, Param2 parameter2, Param3 parameter3, Param4 parameter4, Param5 parameter5, Param6 parameter6)
    {
        return new GenericTask(method, parameter1, parameter2, parameter3, parameter4, parameter5, parameter6);
    }

private:
    GenericTask6(Method method, Param1 parameter1, Param2 parameter2, Param3 parameter3, Param4 parameter4, Param5 parameter5, Param6 parameter6)
        : m_method(method)
        , m_parameter1(parameter1)
        , m_parameter2(parameter2)
        , m_parameter3(parameter3)
        , m_parameter4(parameter4)
        , m_parameter5(parameter5)
        , m_parameter6(parameter6)
    {
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        (*m_method)(context, m_parameter1, m_parameter2, m_parameter3, m_parameter4, m_parameter5, m_parameter6);
    }

private:
    Method m_method;
    P1 m_parameter1;
    P2 m_parameter2;
    P3 m_parameter3;
    P4 m_parameter4;
    P5 m_parameter5;
    P6 m_parameter6;
};

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5, typename P6, typename MP6, typename P7, typename MP7>
class GenericTask7 : public ScriptExecutionContext::Task {
public:
    typedef void (*Method)(ScriptExecutionContext*, MP1, MP2, MP3, MP4, MP5, MP6, MP7);
    typedef GenericTask7<P1, MP1, P2, MP2, P3, MP3, P4, MP4, P5, MP5, P6, MP6, P7, MP7> GenericTask;
    typedef typename GenericTaskTraits<P1>::ParamType Param1;
    typedef typename GenericTaskTraits<P2>::ParamType Param2;
    typedef typename GenericTaskTraits<P3>::ParamType Param3;
    typedef typename GenericTaskTraits<P4>::ParamType Param4;
    typedef typename GenericTaskTraits<P5>::ParamType Param5;
    typedef typename GenericTaskTraits<P6>::ParamType Param6;
    typedef typename GenericTaskTraits<P7>::ParamType Param7;

    static PassOwnPtr<GenericTask> create(Method method, Param1 parameter1, Param2 parameter2, Param3 parameter3, Param4 parameter4, Param5 parameter5, Param6 parameter6, Param7 parameter7)
    {
        return new GenericTask(method, parameter1, parameter2, parameter3, parameter4, parameter5, parameter6, parameter7);
    }

private:
    GenericTask7(Method method, Param1 parameter1, Param2 parameter2, Param3 parameter3, Param4 parameter4, Param5 parameter5, Param6 parameter6, Param7 parameter7)
        : m_method(method)
        , m_parameter1(parameter1)
        , m_parameter2(parameter2)
        , m_parameter3(parameter3)
        , m_parameter4(parameter4)
        , m_parameter5(parameter5)
        , m_parameter6(parameter6)
        , m_parameter7(parameter7)
    {
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        (*m_method)(context, m_parameter1, m_parameter2, m_parameter3, m_parameter4, m_parameter5, m_parameter6, m_parameter7);
    }

private:
    Method m_method;
    P1 m_parameter1;
    P2 m_parameter2;
    P3 m_parameter3;
    P4 m_parameter4;
    P5 m_parameter5;
    P6 m_parameter6;
    P7 m_parameter7;
};

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5, typename P6, typename MP6, typename P7, typename MP7, typename P8, typename MP8>
class GenericTask8 : public ScriptExecutionContext::Task {
public:
    typedef void (*Method)(ScriptExecutionContext*, MP1, MP2, MP3, MP4, MP5, MP6, MP7, MP8);
    typedef GenericTask8<P1, MP1, P2, MP2, P3, MP3, P4, MP4, P5, MP5, P6, MP6, P7, MP7, P8, MP8> GenericTask;
    typedef typename GenericTaskTraits<P1>::ParamType Param1;
    typedef typename GenericTaskTraits<P2>::ParamType Param2;
    typedef typename GenericTaskTraits<P3>::ParamType Param3;
    typedef typename GenericTaskTraits<P4>::ParamType Param4;
    typedef typename GenericTaskTraits<P5>::ParamType Param5;
    typedef typename GenericTaskTraits<P6>::ParamType Param6;
    typedef typename GenericTaskTraits<P7>::ParamType Param7;
    typedef typename GenericTaskTraits<P8>::ParamType Param8;
    
    static PassOwnPtr<GenericTask> create(Method method, Param1 parameter1, Param2 parameter2, Param3 parameter3, Param4 parameter4, Param5 parameter5, Param6 parameter6, Param7 parameter7, Param8 parameter8)
    {
        return new GenericTask(method, parameter1, parameter2, parameter3, parameter4, parameter5, parameter6, parameter7, parameter8);
    }
    
private:
    GenericTask8(Method method, Param1 parameter1, Param2 parameter2, Param3 parameter3, Param4 parameter4, Param5 parameter5, Param6 parameter6, Param7 parameter7, Param8 parameter8)
    : m_method(method)
    , m_parameter1(parameter1)
    , m_parameter2(parameter2)
    , m_parameter3(parameter3)
    , m_parameter4(parameter4)
    , m_parameter5(parameter5)
    , m_parameter6(parameter6)
    , m_parameter7(parameter7)
    , m_parameter8(parameter8)
    {
    }
    
    virtual void performTask(ScriptExecutionContext* context)
    {
        (*m_method)(context, m_parameter1, m_parameter2, m_parameter3, m_parameter4, m_parameter5, m_parameter6, m_parameter7, m_parameter8);
    }
    
private:
    Method m_method;
    P1 m_parameter1;
    P2 m_parameter2;
    P3 m_parameter3;
    P4 m_parameter4;
    P5 m_parameter5;
    P6 m_parameter6;
    P7 m_parameter7;
    P8 m_parameter8;
};

template<typename P1, typename MP1>
PassOwnPtr<ScriptExecutionContext::Task> createCallbackTask(
    void (*method)(ScriptExecutionContext*, MP1),
    const P1& parameter1)
{
    return GenericTask1<typename CrossThreadCopier<P1>::Type, MP1>::create(
        method,
        CrossThreadCopier<P1>::copy(parameter1));
}

template<typename P1, typename MP1, typename P2, typename MP2>
PassOwnPtr<ScriptExecutionContext::Task> createCallbackTask(
    void (*method)(ScriptExecutionContext*, MP1, MP2),
    const P1& parameter1, const P2& parameter2)
{
    return GenericTask2<typename CrossThreadCopier<P1>::Type, MP1, typename CrossThreadCopier<P2>::Type, MP2>::create(
        method,
        CrossThreadCopier<P1>::copy(parameter1), CrossThreadCopier<P2>::copy(parameter2));
}

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3>
PassOwnPtr<ScriptExecutionContext::Task> createCallbackTask(
    void (*method)(ScriptExecutionContext*, MP1, MP2, MP3),
    const P1& parameter1, const P2& parameter2, const P3& parameter3)
{
    return GenericTask3<typename CrossThreadCopier<P1>::Type, MP1, typename CrossThreadCopier<P2>::Type, MP2, typename CrossThreadCopier<P3>::Type, MP3>::create(
        method,
        CrossThreadCopier<P1>::copy(parameter1), CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3));
}

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4>
PassOwnPtr<ScriptExecutionContext::Task> createCallbackTask(
    void (*method)(ScriptExecutionContext*, MP1, MP2, MP3, MP4),
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4)
{
    return GenericTask4<typename CrossThreadCopier<P1>::Type, MP1, typename CrossThreadCopier<P2>::Type, MP2, typename CrossThreadCopier<P3>::Type, MP3,
        typename CrossThreadCopier<P4>::Type, MP4>::create(
            method,
            CrossThreadCopier<P1>::copy(parameter1), CrossThreadCopier<P2>::copy(parameter2),
            CrossThreadCopier<P3>::copy(parameter3), CrossThreadCopier<P4>::copy(parameter4));
}

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5>
PassOwnPtr<ScriptExecutionContext::Task> createCallbackTask(
    void (*method)(ScriptExecutionContext*, MP1, MP2, MP3, MP4, MP5),
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5)
{
    return GenericTask5<typename CrossThreadCopier<P1>::Type, MP1, typename CrossThreadCopier<P2>::Type, MP2, typename CrossThreadCopier<P3>::Type, MP3,
        typename CrossThreadCopier<P4>::Type, MP4, typename CrossThreadCopier<P5>::Type, MP5>::create(
            method,
            CrossThreadCopier<P1>::copy(parameter1), CrossThreadCopier<P2>::copy(parameter2),
            CrossThreadCopier<P3>::copy(parameter3), CrossThreadCopier<P4>::copy(parameter4),
            CrossThreadCopier<P5>::copy(parameter5));
}

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5, typename P6, typename MP6>
PassOwnPtr<ScriptExecutionContext::Task> createCallbackTask(
    void (*method)(ScriptExecutionContext*, MP1, MP2, MP3, MP4, MP5, MP6),
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5, const P6& parameter6)
{
    return GenericTask6<typename CrossThreadCopier<P1>::Type, MP1, typename CrossThreadCopier<P2>::Type, MP2, typename CrossThreadCopier<P3>::Type, MP3,
        typename CrossThreadCopier<P4>::Type, MP4, typename CrossThreadCopier<P5>::Type, MP5, typename CrossThreadCopier<P6>::Type, MP6>::create(
            method,
            CrossThreadCopier<P1>::copy(parameter1), CrossThreadCopier<P2>::copy(parameter2),
            CrossThreadCopier<P3>::copy(parameter3), CrossThreadCopier<P4>::copy(parameter4),
            CrossThreadCopier<P5>::copy(parameter5), CrossThreadCopier<P6>::copy(parameter6));
}

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5, typename P6, typename MP6, typename P7, typename MP7>
PassOwnPtr<ScriptExecutionContext::Task> createCallbackTask(
    void (*method)(ScriptExecutionContext*, MP1, MP2, MP3, MP4, MP5, MP6, MP7),
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5, const P6& parameter6, const P7& parameter7)
{
    return GenericTask7<typename CrossThreadCopier<P1>::Type, MP1, typename CrossThreadCopier<P2>::Type, MP2, typename CrossThreadCopier<P3>::Type, MP3,
        typename CrossThreadCopier<P4>::Type, MP4, typename CrossThreadCopier<P5>::Type, MP5, typename CrossThreadCopier<P6>::Type, MP6,
        typename CrossThreadCopier<P7>::Type, MP7>::create(
            method,
            CrossThreadCopier<P1>::copy(parameter1), CrossThreadCopier<P2>::copy(parameter2),
            CrossThreadCopier<P3>::copy(parameter3), CrossThreadCopier<P4>::copy(parameter4),
            CrossThreadCopier<P5>::copy(parameter5), CrossThreadCopier<P6>::copy(parameter6),
            CrossThreadCopier<P7>::copy(parameter7));
}

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5, typename P6, typename MP6, typename P7, typename MP7, typename P8, typename MP8>
PassOwnPtr<ScriptExecutionContext::Task> createCallbackTask(
                                                            void (*method)(ScriptExecutionContext*, MP1, MP2, MP3, MP4, MP5, MP6, MP7, MP8),
                                                            const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5, const P6& parameter6, const P7& parameter7, const P8& parameter8)
{
    return GenericTask8<typename CrossThreadCopier<P1>::Type, MP1, typename CrossThreadCopier<P2>::Type, MP2, typename CrossThreadCopier<P3>::Type, MP3,
    typename CrossThreadCopier<P4>::Type, MP4, typename CrossThreadCopier<P5>::Type, MP5, typename CrossThreadCopier<P6>::Type, MP6,
    typename CrossThreadCopier<P7>::Type, MP7, typename CrossThreadCopier<P8>::Type, MP8>::create(
                                                       method,
                                                       CrossThreadCopier<P1>::copy(parameter1), CrossThreadCopier<P2>::copy(parameter2),
                                                       CrossThreadCopier<P3>::copy(parameter3), CrossThreadCopier<P4>::copy(parameter4),
                                                       CrossThreadCopier<P5>::copy(parameter5), CrossThreadCopier<P6>::copy(parameter6),
                                                       CrossThreadCopier<P7>::copy(parameter7), CrossThreadCopier<P8>::copy(parameter8));
}

} // namespace WebCore

#endif // GenericTask_h
