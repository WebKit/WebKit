/*
 * Copyright (C) 2013, 2015, 2016 Apple Inc. All rights reserved.
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

#include <wtf/CrossThreadCopier.h>
#include <wtf/NoncopyableFunction.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

class CrossThreadTask {
    template<class T>
    friend CrossThreadTask createCrossThreadTask(T&, void (T::*)());
    template<class T, class P1, class MP1>
    friend CrossThreadTask createCrossThreadTask(T&, void (T::*)(MP1), const P1&);
    template<class T, class P1, class MP1, class P2, class MP2>
    friend CrossThreadTask createCrossThreadTask(T&, void (T::*)(MP1, MP2), const P1&, const P2&);
    template<class T, class P1, class MP1, class P2, class MP2, class P3, class MP3>
    friend CrossThreadTask createCrossThreadTask(T&, void (T::*)(MP1, MP2, MP3), const P1&, const P2&, const P3&);
    template<class T, class P1, class MP1, class P2, class MP2, class P3, class MP3, class P4, class MP4>
    friend CrossThreadTask createCrossThreadTask(T&, void (T::*)(MP1, MP2, MP3, MP4), const P1&, const P2&, const P3&, const P4&);
    template<class T, class P1, class MP1, class P2, class MP2, class P3, class MP3, class P4, class MP4, class P5, class MP5>
    friend CrossThreadTask createCrossThreadTask(T&, void (T::*)(MP1, MP2, MP3, MP4, MP5), const P1&, const P2&, const P3&, const P4&, const P5&);
    template<class T, class P1, class MP1, class P2, class MP2, class P3, class MP3, class P4, class MP4, class P5, class MP5, class P6, class MP6>
    friend CrossThreadTask createCrossThreadTask(T&, void (T::*)(MP1, MP2, MP3, MP4, MP5, MP6), const P1&, const P2&, const P3&, const P4&, const P5&, const P6&);
    template<class T, class P1, class MP1, class P2, class MP2, class P3, class MP3, class P4, class MP4, class P5, class MP5, class P6, class MP6, class P7, class MP7>
    friend CrossThreadTask createCrossThreadTask(T&, void (T::*)(MP1, MP2, MP3, MP4, MP5, MP6, MP7), const P1&, const P2&, const P3&, const P4&, const P5&, const P6&, const P7&);
    template<class T, class P1, class MP1, class P2, class MP2, class P3, class MP3, class P4, class MP4, class P5, class MP5, class P6, class MP6, class P7, class MP7, class P8, class MP8>
    friend CrossThreadTask createCrossThreadTask(T&, void (T::*)(MP1, MP2, MP3, MP4, MP5, MP6, MP7, MP8), const P1&, const P2&, const P3&, const P4&, const P5&, const P6&, const P7&, const P8&);
public:
    CrossThreadTask() = default;

    CrossThreadTask(NoncopyableFunction<void ()>&& taskFunction)
        : m_taskFunction(WTFMove(taskFunction))
    {
        ASSERT(m_taskFunction);
    }

    void performTask()
    {
        m_taskFunction();
    }

protected:
    NoncopyableFunction<void ()> m_taskFunction;
};

template<typename T>
CrossThreadTask createCrossThreadTask(
    T& callee,
    void (T::*method)())
{
    return CrossThreadTask([callee = &callee, method]() mutable {
        (callee->*method)();
    });
}

template<typename T, typename P1, typename MP1>
CrossThreadTask createCrossThreadTask(
    T& callee,
    void (T::*method)(MP1),
    const P1& parameter1)
{
    return CrossThreadTask([callee = &callee, method,
        p1 = CrossThreadCopier<P1>::copy(parameter1)]() mutable {
        (callee->*method)(p1);
    });
}

template<typename T, typename P1, typename MP1, typename P2, typename MP2>
CrossThreadTask createCrossThreadTask(
    T& callee,
    void (T::*method)(MP1, MP2),
    const P1& parameter1,
    const P2& parameter2)
{
    return CrossThreadTask([callee = &callee, method,
        p1 = CrossThreadCopier<P1>::copy(parameter1),
        p2 = CrossThreadCopier<P2>::copy(parameter2)]() mutable {
        (callee->*method)(p1, p2);
    });
}

template<typename T, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3>
CrossThreadTask createCrossThreadTask(
    T& callee,
    void (T::*method)(MP1, MP2, MP3),
    const P1& parameter1,
    const P2& parameter2,
    const P3& parameter3)
{
    return CrossThreadTask([callee = &callee, method,
        p1 = CrossThreadCopier<P1>::copy(parameter1),
        p2 = CrossThreadCopier<P2>::copy(parameter2),
        p3 = CrossThreadCopier<P3>::copy(parameter3)]() mutable {
        (callee->*method)(p1, p2, p3);
    });
}

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3>
CrossThreadTask createCrossThreadTask(
    void (*method)(MP1, MP2, MP3),
    const P1& parameter1,
    const P2& parameter2,
    const P3& parameter3)
{
    return CrossThreadTask([method,
        p1 = CrossThreadCopier<P1>::copy(parameter1),
        p2 = CrossThreadCopier<P2>::copy(parameter2),
        p3 = CrossThreadCopier<P3>::copy(parameter3)]() mutable {
        method(p1, p2, p3);
    });
}

template<typename T, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4>
CrossThreadTask createCrossThreadTask(
    T& callee,
    void (T::*method)(MP1, MP2, MP3, MP4),
    const P1& parameter1,
    const P2& parameter2,
    const P3& parameter3,
    const P4& parameter4)
{
    return CrossThreadTask([callee = &callee, method,
        p1 = CrossThreadCopier<P1>::copy(parameter1),
        p2 = CrossThreadCopier<P2>::copy(parameter2),
        p3 = CrossThreadCopier<P3>::copy(parameter3),
        p4 = CrossThreadCopier<P4>::copy(parameter4)]() mutable {
        (callee->*method)(p1, p2, p3, p4);
    });
}

template<typename T, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5>
CrossThreadTask createCrossThreadTask(
    T& callee,
    void (T::*method)(MP1, MP2, MP3, MP4, MP5),
    const P1& parameter1,
    const P2& parameter2,
    const P3& parameter3,
    const P4& parameter4,
    const P5& parameter5)
{
    return CrossThreadTask([callee = &callee, method,
        p1 = CrossThreadCopier<P1>::copy(parameter1),
        p2 = CrossThreadCopier<P2>::copy(parameter2),
        p3 = CrossThreadCopier<P3>::copy(parameter3),
        p4 = CrossThreadCopier<P4>::copy(parameter4),
        p5 = CrossThreadCopier<P5>::copy(parameter5)]() mutable {
        (callee->*method)(p1, p2, p3, p4, p5);
    });
}

template<typename T, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5, typename P6, typename MP6>
CrossThreadTask createCrossThreadTask(
    T& callee,
    void (T::*method)(MP1, MP2, MP3, MP4, MP5, MP6),
    const P1& parameter1,
    const P2& parameter2,
    const P3& parameter3,
    const P4& parameter4,
    const P5& parameter5,
    const P6& parameter6)
{
    return CrossThreadTask([callee = &callee, method,
        p1 = CrossThreadCopier<P1>::copy(parameter1),
        p2 = CrossThreadCopier<P2>::copy(parameter2),
        p3 = CrossThreadCopier<P3>::copy(parameter3),
        p4 = CrossThreadCopier<P4>::copy(parameter4),
        p5 = CrossThreadCopier<P5>::copy(parameter5),
        p6 = CrossThreadCopier<P6>::copy(parameter6)]() mutable {
        (callee->*method)(p1, p2, p3, p4, p5, p6);
    });
}

template<typename T, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5, typename P6, typename MP6, typename P7, typename MP7>
CrossThreadTask createCrossThreadTask(
    T& callee,
    void (T::*method)(MP1, MP2, MP3, MP4, MP5, MP6, MP7),
    const P1& parameter1,
    const P2& parameter2,
    const P3& parameter3,
    const P4& parameter4,
    const P5& parameter5,
    const P6& parameter6,
    const P7& parameter7)
{
    return CrossThreadTask([callee = &callee, method,
        p1 = CrossThreadCopier<P1>::copy(parameter1),
        p2 = CrossThreadCopier<P2>::copy(parameter2),
        p3 = CrossThreadCopier<P3>::copy(parameter3),
        p4 = CrossThreadCopier<P4>::copy(parameter4),
        p5 = CrossThreadCopier<P5>::copy(parameter5),
        p6 = CrossThreadCopier<P6>::copy(parameter6),
        p7 = CrossThreadCopier<P7>::copy(parameter7)]() mutable {
        (callee->*method)(p1, p2, p3, p4, p5, p6, p7);
    });
}

template<typename T, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5, typename P6, typename MP6, typename P7, typename MP7, typename P8, typename MP8>
CrossThreadTask createCrossThreadTask(
    T& callee,
    void (T::*method)(MP1, MP2, MP3, MP4, MP5, MP6, MP7, MP8),
    const P1& parameter1,
    const P2& parameter2,
    const P3& parameter3,
    const P4& parameter4,
    const P5& parameter5,
    const P6& parameter6,
    const P7& parameter7,
    const P8& parameter8)
{
    return CrossThreadTask([callee = &callee, method,
        p1 = CrossThreadCopier<P1>::copy(parameter1),
        p2 = CrossThreadCopier<P2>::copy(parameter2),
        p3 = CrossThreadCopier<P3>::copy(parameter3),
        p4 = CrossThreadCopier<P4>::copy(parameter4),
        p5 = CrossThreadCopier<P5>::copy(parameter5),
        p6 = CrossThreadCopier<P6>::copy(parameter6),
        p7 = CrossThreadCopier<P7>::copy(parameter7),
        p8 = CrossThreadCopier<P8>::copy(parameter8)]() mutable {
        (callee->*method)(p1, p2, p3, p4, p5, p6, p7, p8);
    });
}

} // namespace WTF

using WTF::CrossThreadTask;
using WTF::createCrossThreadTask;
