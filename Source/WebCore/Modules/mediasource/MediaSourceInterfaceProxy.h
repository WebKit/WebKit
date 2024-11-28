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

#if ENABLE(MEDIA_SOURCE)

#include "PlatformTimeRanges.h"
#include <wtf/MediaTime.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class HTMLMediaElement;
class MediaSourcePrivateClient;
class TimeRanges;

class MediaSourceInterfaceProxy
    : public RefCounted<MediaSourceInterfaceProxy>
    , public CanMakeWeakPtr<MediaSourceInterfaceProxy> {

public:
    virtual ~MediaSourceInterfaceProxy() = default;

    virtual RefPtr<MediaSourcePrivateClient> client() const = 0;
    virtual void monitorSourceBuffers() = 0;
    virtual bool isClosed() const = 0;
    virtual MediaTime duration() const = 0;
    virtual PlatformTimeRanges buffered() const = 0;
    virtual Ref<TimeRanges> seekable() const = 0;
    virtual bool isStreamingContent() const = 0;
    virtual bool attachToElement(WeakPtr<HTMLMediaElement>&&) = 0;
    virtual void detachFromElement() = 0;
    virtual void elementIsShuttingDown() = 0;
    virtual void openIfDeferredOpen() = 0;
    virtual bool isManaged() const = 0;
    virtual void setAsSrcObject(bool) = 0;
    virtual void memoryPressure() = 0;
    virtual bool detachable() const = 0;
};

} // namespace WebCore

#endif
