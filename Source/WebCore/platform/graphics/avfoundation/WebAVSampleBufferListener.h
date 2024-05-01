/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <CoreMedia/CMTime.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS AVSampleBufferAudioRenderer;
OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_CLASS NSError;
OBJC_CLASS WebAVSampleBufferListenerPrivate;
OBJC_PROTOCOL(WebSampleBufferVideoRendering);

namespace WebCore {
class WebAVSampleBufferListenerClient;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebCore::WebAVSampleBufferListenerClient> : std::true_type { };
}

namespace WebCore {

class WebAVSampleBufferListenerClient : public CanMakeWeakPtr<WebAVSampleBufferListenerClient> {
public:
    virtual ~WebAVSampleBufferListenerClient() = default;
    virtual void videoRendererDidReceiveError(WebSampleBufferVideoRendering *, NSError *) { }
    virtual void videoRendererRequiresFlushToResumeDecodingChanged(WebSampleBufferVideoRendering *, bool) { }
    virtual void videoRendererReadyForDisplayChanged(WebSampleBufferVideoRendering *, bool) { }
    virtual void audioRendererDidReceiveError(AVSampleBufferAudioRenderer *, NSError *) { }
    virtual void audioRendererWasAutomaticallyFlushed(AVSampleBufferAudioRenderer *, const CMTime&) { }
    virtual void outputObscuredDueToInsufficientExternalProtectionChanged(bool) { }
};

class WebAVSampleBufferListener final : public ThreadSafeRefCounted<WebAVSampleBufferListener> {
public:
    static Ref<WebAVSampleBufferListener> create(WebAVSampleBufferListenerClient& client) { return adoptRef(*new WebAVSampleBufferListener(client)); }
    void invalidate();
    void beginObservingVideoRenderer(WebSampleBufferVideoRendering *);
    void stopObservingVideoRenderer(WebSampleBufferVideoRendering *);
    void beginObservingAudioRenderer(AVSampleBufferAudioRenderer *);
    void stopObservingAudioRenderer(AVSampleBufferAudioRenderer *);
private:
    explicit WebAVSampleBufferListener(WebAVSampleBufferListenerClient&);
    RetainPtr<WebAVSampleBufferListenerPrivate> m_private;
};

} // namespace WebCore
