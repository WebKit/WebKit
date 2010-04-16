/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
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

#ifndef FileThreadTask_h
#define FileThreadTask_h

#include "CrossThreadCopier.h"
#include "FileThread.h"
#include <memory>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/TypeTraits.h>

namespace WebCore {

// Traits for the Task.
template<typename T> struct FileThreadTaskTraits {
    typedef const T& ParamType;
};

template<typename T> struct FileThreadTaskTraits<T*> {
    typedef T* ParamType;
};

template<typename T> struct FileThreadTaskTraits<PassRefPtr<T> > {
    typedef PassRefPtr<T> ParamType;
};

template<typename T> struct FileThreadTaskTraits<PassOwnPtr<T> > {
    typedef PassOwnPtr<T> ParamType;
};

class FileThreadTask0 : public FileThread::Task {
public:
    typedef void (FileStream::*Method)();
    typedef FileThreadTask0 FileThreadTask;

    static PassOwnPtr<FileThreadTask> create(FileStream* stream, Method method)
    {
        return new FileThreadTask(stream, method);
    }

private:
    FileThreadTask0(FileStream* stream, Method method)
        : FileThread::Task(stream)
        , m_method(method)
    {
    }

    virtual void performTask()
    {
        (*stream().*m_method)();
    }

private:
    Method m_method;
};

template<typename P1, typename MP1>
class FileThreadTask1 : public FileThread::Task {
public:
    typedef void (FileStream::*Method)(MP1);
    typedef FileThreadTask1<P1, MP1> FileThreadTask;
    typedef typename FileThreadTaskTraits<P1>::ParamType Param1;

    static PassOwnPtr<FileThreadTask> create(FileStream* stream, Method method, Param1 parameter1)
    {
        return new FileThreadTask(stream, method, parameter1);
    }

private:
    FileThreadTask1(FileStream* stream, Method method, Param1 parameter1)
        : FileThread::Task(stream)
        , m_method(method)
        , m_parameter1(parameter1)
    {
    }

    virtual void performTask()
    {
        (*stream().*m_method)(m_parameter1);
    }

private:
    Method m_method;
    P1 m_parameter1;
};

template<typename P1, typename MP1, typename P2, typename MP2>
class FileThreadTask2 : public FileThread::Task {
public:
    typedef void (FileStream::*Method)(MP1, MP2);
    typedef FileThreadTask2<P1, MP1, P2, MP2> FileThreadTask;
    typedef typename FileThreadTaskTraits<P1>::ParamType Param1;
    typedef typename FileThreadTaskTraits<P2>::ParamType Param2;

    static PassOwnPtr<FileThreadTask> create(FileStream* stream, Method method, Param1 parameter1, Param2 parameter2)
    {
        return new FileThreadTask(stream, method, parameter1, parameter2);
    }

private:
    FileThreadTask2(FileStream* stream, Method method, Param1 parameter1, Param2 parameter2)
        : FileThread::Task(stream)
        , m_method(method)
        , m_parameter1(parameter1)
        , m_parameter2(parameter2)
    {
    }

    virtual void performTask()
    {
        (*stream().*m_method)(m_parameter1, m_parameter2);
    }

private:
    Method m_method;
    P1 m_parameter1;
    P2 m_parameter2;
};

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3>
class FileThreadTask3 : public FileThread::Task {
public:
    typedef void (FileStream::*Method)(MP1, MP2, MP3);
    typedef FileThreadTask3<P1, MP1, P2, MP2, P3, MP3> FileThreadTask;
    typedef typename FileThreadTaskTraits<P1>::ParamType Param1;
    typedef typename FileThreadTaskTraits<P2>::ParamType Param2;
    typedef typename FileThreadTaskTraits<P3>::ParamType Param3;

    static PassOwnPtr<FileThreadTask> create(FileStream* stream, Method method, Param1 parameter1, Param2 parameter2, Param3 parameter3)
    {
        return new FileThreadTask(stream, method, parameter1, parameter2, parameter3);
    }

private:
    FileThreadTask3(FileStream* stream, Method method, Param1 parameter1, Param2 parameter2, Param3 parameter3)
        : FileThread::Task(stream)
        , m_method(method)
        , m_parameter1(parameter1)
        , m_parameter2(parameter2)
        , m_parameter3(parameter3)
    {
    }

    virtual void performTask()
    {
        (*stream().*m_method)(m_parameter1, m_parameter2, m_parameter3);
    }

private:
    Method m_method;
    P1 m_parameter1;
    P2 m_parameter2;
    P3 m_parameter3;
};

PassOwnPtr<FileThread::Task> createFileThreadTask(
    FileStream* const callee,
    void (FileStream::*method)())
{
    return FileThreadTask0::create(
        callee,
        method);
}

template<typename P1, typename MP1>
PassOwnPtr<FileThread::Task> createFileThreadTask(
    FileStream* const callee,
    void (FileStream::*method)(MP1),
    const P1& parameter1)
{
    return FileThreadTask1<typename CrossThreadCopier<P1>::Type, MP1>::create(
        callee,
        method,
        CrossThreadCopier<P1>::copy(parameter1));
}

template<typename P1, typename MP1, typename P2, typename MP2>
PassOwnPtr<FileThread::Task> createFileThreadTask(
    FileStream* const callee,
    void (FileStream::*method)(MP1, MP2),
    const P1& parameter1,
    const P2& parameter2)
{
    return FileThreadTask2<typename CrossThreadCopier<P1>::Type, MP1, typename CrossThreadCopier<P2>::Type, MP2>::create(
        callee,
        method,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2));
}

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3>
PassOwnPtr<FileThread::Task> createFileThreadTask(
    FileStream* const callee,
    void (FileStream::*method)(MP1, MP2, MP3),
    const P1& parameter1,
    const P2& parameter2,
    const P3& parameter3)
{
    return FileThreadTask3<typename CrossThreadCopier<P1>::Type, MP1, typename CrossThreadCopier<P2>::Type, MP2, typename CrossThreadCopier<P3>::Type, MP3>::create(
        callee,
        method,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3));
}

} // namespace WebCore

#endif // FileThreadTask_h
