/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef MediaSessionInterruptionProvider_h
#define MediaSessionInterruptionProvider_h

#if ENABLE(MEDIA_SESSION)

#include <wtf/RefCounted.h>

namespace WebCore {

enum class MediaSessionInterruptingCategory {
    Content,
    Transient,
    TransientSolo
};

class MediaSessionInterruptionProviderClient {
public:
    virtual void didReceiveStartOfInterruptionNotification(MediaSessionInterruptingCategory) = 0;
    virtual void didReceiveEndOfInterruptionNotification(MediaSessionInterruptingCategory) = 0;

protected:
    virtual ~MediaSessionInterruptionProviderClient() = default;
};

class MediaSessionInterruptionProvider : public RefCounted<MediaSessionInterruptionProvider> {
public:
    explicit MediaSessionInterruptionProvider(MediaSessionInterruptionProviderClient&);
    virtual ~MediaSessionInterruptionProvider();

    // To be overridden by subclasses.
    virtual void beginListeningForInterruptions() = 0;
    virtual void stopListeningForInterruptions() = 0;

    MediaSessionInterruptionProviderClient& client() const { return *m_client; }

private:
    MediaSessionInterruptionProviderClient* m_client;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_SESSION)

#endif /* MediaSessionInterruptionProvider_h */
