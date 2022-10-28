/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)

#include "MediaDescriptionInfo.h"

namespace WebKit {

class RemoteMediaDescription : public WebCore::MediaDescription {
public:
    static Ref<RemoteMediaDescription> create(const MediaDescriptionInfo& descriptionInfo)
    {
        return adoptRef(*new RemoteMediaDescription(descriptionInfo));
    }

    virtual ~RemoteMediaDescription() = default;

    AtomString codec() const final { return m_codec; }
    bool isVideo() const final { return m_isVideo; }
    bool isAudio() const final { return m_isAudio; }
    bool isText() const final { return m_isText;}

private:
    RemoteMediaDescription(const MediaDescriptionInfo& descriptionInfo)
        : m_codec(descriptionInfo.m_codec)
        , m_isVideo(descriptionInfo.m_isVideo)
        , m_isAudio(descriptionInfo.m_isAudio)
        , m_isText(descriptionInfo.m_isText)
    {
    }

    AtomString m_codec;
    bool m_isVideo { false };
    bool m_isAudio { false };
    bool m_isText { false };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
