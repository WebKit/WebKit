/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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

#import <wtf/CompletionHandler.h>
#import <wtf/Deque.h>
#import <wtf/FastMalloc.h>
#import <wtf/Lock.h>
#import <wtf/Ref.h>
#import <wtf/ThreadSafeRefCounted.h>

struct WGPUInstanceImpl {
};

namespace WebGPU {

class Adapter;
class PresentationContext;

// https://gpuweb.github.io/gpuweb/#gpu
class Instance : public WGPUInstanceImpl, public ThreadSafeRefCounted<Instance> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<Instance> create(const WGPUInstanceDescriptor&);
    static Ref<Instance> createInvalid()
    {
        return adoptRef(*new Instance());
    }

    ~Instance();

    Ref<PresentationContext> createSurface(const WGPUSurfaceDescriptor&);
    void processEvents();
    void requestAdapter(const WGPURequestAdapterOptions&, CompletionHandler<void(WGPURequestAdapterStatus, Ref<Adapter>&&, String&&)>&& callback);

    bool isValid() const { return m_isValid; }

    // This can be called on a background thread.
    using WorkItem = CompletionHandler<void(void)>;
    void scheduleWork(WorkItem&&);

private:
    Instance(WGPUScheduleWorkBlock);
    Instance();

    // This can be called on a background thread.
    void defaultScheduleWork(WGPUWorkItem&&);

    // This can be used on a background thread.
    Deque<WGPUWorkItem> m_pendingWork WTF_GUARDED_BY_LOCK(m_lock);
    const WGPUScheduleWorkBlock m_scheduleWorkBlock;
    Lock m_lock;
    bool m_isValid { true };
};

} // namespace WebGPU
