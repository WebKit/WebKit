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

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "LegacyCDMSession.h"
#include <JavaScriptCore/Uint8Array.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class LegacyCDM;
class CDMPrivateInterface;
class MediaPlayer;

using CreateCDM = WTF::Function<std::unique_ptr<CDMPrivateInterface>(LegacyCDM*)>;
using CDMSupportsKeySystem = WTF::Function<bool(const String&)>;
using CDMSupportsKeySystemAndMimeType = WTF::Function<bool(const String&, const String&)>;

class LegacyCDMClient {
public:
    virtual ~LegacyCDMClient() = default;

    virtual RefPtr<MediaPlayer> cdmMediaPlayer(const LegacyCDM*) const = 0;
};

class WEBCORE_EXPORT LegacyCDM final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit LegacyCDM(const String& keySystem);

    enum CDMErrorCode { NoError, UnknownError, ClientError, ServiceError, OutputError, HardwareChangeError, DomainError };
    static bool supportsKeySystem(const String&);
    static bool keySystemSupportsMimeType(const String& keySystem, const String& mimeType);
    static std::unique_ptr<LegacyCDM> create(const String& keySystem);
    static void registerCDMFactory(CreateCDM&&, CDMSupportsKeySystem&&, CDMSupportsKeySystemAndMimeType&&);
    ~LegacyCDM();

    static void resetFactories();
    static void clearFactories();

    bool supportsMIMEType(const String&) const;
    std::unique_ptr<LegacyCDMSession> createSession(LegacyCDMSessionClient&);

    const String& keySystem() const { return m_keySystem; }

    LegacyCDMClient* client() const { return m_client; }
    void setClient(LegacyCDMClient* client) { m_client = client; }

    RefPtr<MediaPlayer> mediaPlayer() const;
    CDMPrivateInterface* cdmPrivate() const { return m_private.get(); }

private:
    String m_keySystem;
    std::unique_ptr<CDMPrivateInterface> m_private;
    LegacyCDMClient* m_client;
};

}

#endif // ENABLE(LEGACY_ENCRYPTED_MEDIA)
