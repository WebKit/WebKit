/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef MediaSourcePrivateClient_h
#define MediaSourcePrivateClient_h

#if ENABLE(MEDIA_SOURCE)

#include "PlatformTimeRanges.h"
#include <wtf/Logger.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class MediaSourcePrivate;

class MediaSourcePrivateClient : public RefCounted<MediaSourcePrivateClient> {
public:
    virtual ~MediaSourcePrivateClient() = default;

    virtual void setPrivateAndOpen(Ref<MediaSourcePrivate>&&) = 0;
    virtual MediaTime duration() const = 0;
    virtual std::unique_ptr<PlatformTimeRanges> buffered() const = 0;
    virtual void seekToTime(const MediaTime&) = 0;
#if USE(GSTREAMER)
    virtual void monitorSourceBuffers() = 0;
#endif

#if !RELEASE_LOG_DISABLED
    virtual void setLogIdentifier(const void*) = 0;
#endif

    enum class RendererType { Audio, Video };
    virtual void failedToCreateRenderer(RendererType) = 0;
};

}

#endif // ENABLE(MEDIA_SOURCE)

#endif
