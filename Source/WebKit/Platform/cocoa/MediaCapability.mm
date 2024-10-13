/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "MediaCapability.h"

#if ENABLE(EXTENSION_CAPABILITIES)

#import <BrowserEngineKit/BECapability.h>
#import <WebCore/SecurityOrigin.h>
#import <wtf/text/WTFString.h>

namespace WebKit {

static RetainPtr<BEMediaEnvironment> createMediaEnvironment(const URL& webPageURL)
{
    NSURL *protocolHostAndPortURL = URL { webPageURL.protocolHostAndPort() };
    RELEASE_ASSERT(protocolHostAndPortURL);
    return adoptNS([[BEMediaEnvironment alloc] initWithWebPageURL:protocolHostAndPortURL]);
}

Ref<MediaCapability> MediaCapability::create(URL&& url)
{
    return adoptRef(*new MediaCapability(WTFMove(url)));
}

MediaCapability::MediaCapability(URL&& webPageURL)
    : m_webPageURL { WTFMove(webPageURL) }
    , m_mediaEnvironment { createMediaEnvironment(m_webPageURL) }
{
    setPlatformCapability([BEProcessCapability mediaPlaybackAndCaptureWithEnvironment:m_mediaEnvironment.get()]);
}

bool MediaCapability::isActivatingOrActive() const
{
    switch (m_state) {
    case State::Inactive:
    case State::Deactivating:
        return false;
    case State::Activating:
    case State::Active:
        return true;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

String MediaCapability::environmentIdentifier() const
{
#if USE(EXTENSIONKIT)
    xpc_object_t xpcObject = [m_mediaEnvironment createXPCRepresentation];
    if (!xpcObject)
        return emptyString();
    return String::fromUTF8(xpc_dictionary_get_string(xpcObject, "identifier"));
#endif

    return { };
}

} // namespace WebKit

#endif // ENABLE(EXTENSION_CAPABILITIES)
