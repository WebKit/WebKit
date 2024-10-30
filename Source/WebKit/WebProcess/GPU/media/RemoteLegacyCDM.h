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

#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "RemoteLegacyCDMIdentifier.h"
#include <WebCore/LegacyCDMPrivate.h>
#include <WebCore/MediaPlayerIdentifier.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class Settings;
}

namespace WebKit {

class RemoteLegacyCDMFactory;
class RemoteLegacyCDMSession;


class RemoteLegacyCDM final
    : public WebCore::CDMPrivateInterface
    , public CanMakeWeakPtr<RemoteLegacyCDM> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLegacyCDM);
public:
    RemoteLegacyCDM(RemoteLegacyCDMFactory&, RemoteLegacyCDMIdentifier);
    virtual ~RemoteLegacyCDM();

    bool supportsMIMEType(const String&) const final;
    RefPtr<WebCore::LegacyCDMSession> createSession(WebCore::LegacyCDMSessionClient&) final;
    void setPlayerId(std::optional<WebCore::MediaPlayerIdentifier>);

    void ref() const final;
    void deref() const final;

private:
    Ref<RemoteLegacyCDMFactory> protectedFactory() const;

    WeakRef<RemoteLegacyCDMFactory> m_factory;
    RemoteLegacyCDMIdentifier m_identifier;
};

}

#endif
