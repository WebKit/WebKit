/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCTimeSource_h
#define CCTimeSource_h

#include <wtf/RefCounted.h>

namespace WebCore {

class CCThread;

class CCTimeSourceClient {
public:
    virtual void onTimerTick() = 0;

protected:
    virtual ~CCTimeSourceClient() { }
};

// An generic interface for getting a reliably-ticking timesource of
// a specified rate.
//
// Be sure to call setActive(false) before releasing your reference to the
// timer, or it will keep on ticking!
class CCTimeSource : public RefCounted<CCTimeSource> {
public:
    virtual ~CCTimeSource() { }
    virtual void setClient(CCTimeSourceClient*) = 0;
    virtual void setActive(bool) = 0;
    virtual bool active() const = 0;
    virtual void setTimebaseAndInterval(double timebase, double intervalSeconds) = 0;
    virtual double lastTickTime() = 0;
    virtual double nextTickTime() = 0;
};

}
#endif // CCSmoothedTimer_h
