/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef EdenGCActivityCallback_h
#define EdenGCActivityCallback_h

#include "GCActivityCallback.h"

namespace JSC {

class EdenGCActivityCallback : public GCActivityCallback {
public:
    EdenGCActivityCallback(Heap*);

    virtual void doCollection() override;

protected:
#if USE(CF)
    EdenGCActivityCallback(Heap* heap, CFRunLoopRef runLoop)
        : GCActivityCallback(heap, runLoop)
    {
    }
#endif

    virtual double lastGCLength() override;
    virtual double gcTimeSlice(size_t bytes) override;
    virtual double deathRate() override;
};

inline PassRefPtr<GCActivityCallback> GCActivityCallback::createEdenTimer(Heap* heap)
{
    return s_shouldCreateGCTimer ? adoptRef(new EdenGCActivityCallback(heap)) : nullptr;
}

} // namespace JSC

#endif // EdenGCActivityCallback_h
