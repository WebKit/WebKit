/*
 * Copyright (C) 2010, 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "JSRunLoopTimer.h"
#include <wtf/RefPtr.h>

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace JSC {

class FullGCActivityCallback;
class Heap;

class JS_EXPORT_PRIVATE GCActivityCallback : public JSRunLoopTimer {
public:
    using Base = JSRunLoopTimer;
    static RefPtr<FullGCActivityCallback> tryCreateFullTimer(Heap*);
    static RefPtr<GCActivityCallback> tryCreateEdenTimer(Heap*);

    GCActivityCallback(Heap*);

    void doWork(VM&) override;

    virtual void doCollection(VM&) = 0;

    void didAllocate(Heap&, size_t);
    void willCollect();
    void cancel();
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    static bool s_shouldCreateGCTimer;

protected:
    virtual Seconds lastGCLength(Heap&) = 0;
    virtual double gcTimeSlice(size_t bytes) = 0;
    virtual double deathRate(Heap&) = 0;

    GCActivityCallback(VM* vm)
        : Base(vm)
        , m_enabled(true)
        , m_delay(s_decade)
    {
    }

    bool m_enabled;

protected:
    void scheduleTimer(Seconds);

private:
    Seconds m_delay;
};

} // namespace JSC
