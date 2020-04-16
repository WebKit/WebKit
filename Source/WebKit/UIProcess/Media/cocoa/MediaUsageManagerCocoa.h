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

#if ENABLE(MEDIA_USAGE)

#include "MediaUsageManager.h"

OBJC_CLASS USVideoUsage;

namespace WebKit {

class MediaUsageManagerCocoa : public MediaUsageManager {
    WTF_MAKE_FAST_ALLOCATED;
public:
    MediaUsageManagerCocoa() = default;
    virtual ~MediaUsageManagerCocoa();

private:
    void reset() final;
    void addMediaSession(WebCore::MediaSessionIdentifier, const String&, const URL&) final;
    void updateMediaUsage(WebCore::MediaSessionIdentifier, const WebCore::MediaUsageInfo&) final;
    void removeMediaSession(WebCore::MediaSessionIdentifier) final;

    struct SessionMediaUsage {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;

        SessionMediaUsage(WebCore::MediaSessionIdentifier identifier, const String& bundleIdentifier, const URL& pageURL)
            : identifier(identifier)
            , bundleIdentifier(bundleIdentifier)
            , pageURL(pageURL)
        {
        }

        WebCore::MediaSessionIdentifier identifier;
        String bundleIdentifier;
        URL pageURL;
        Optional<WebCore::MediaUsageInfo> mediaUsageInfo;
        RetainPtr<USVideoUsage> usageTracker;
    };

    HashMap<WebCore::MediaSessionIdentifier, std::unique_ptr<SessionMediaUsage>> m_mediaSessions;
};

} // namespace WebKit

#endif // ENABLE(MEDIA_USAGE)

