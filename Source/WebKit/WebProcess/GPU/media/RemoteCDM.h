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

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)

#include "RemoteCDMConfiguration.h"
#include "RemoteCDMFactory.h"
#include "RemoteCDMIdentifier.h"
#include <WebCore/CDMPrivate.h>
#include <wtf/RefCounted.h>

namespace WebKit {

class RemoteCDM final : public WebCore::CDMPrivate {
public:
    static std::unique_ptr<RemoteCDM> create(WeakPtr<RemoteCDMFactory>&&, RemoteCDMIdentifier&&, RemoteCDMConfiguration&&);
    virtual ~RemoteCDM() = default;

private:
    RemoteCDM(WeakPtr<RemoteCDMFactory>&&, RemoteCDMIdentifier&&, RemoteCDMConfiguration&&);

    void getSupportedConfiguration(WebCore::CDMKeySystemConfiguration&& candidateConfiguration, LocalStorageAccess, SupportedConfigurationCallback&&) final;

    bool supportsConfiguration(const WebCore::CDMKeySystemConfiguration&) const final;
    bool supportsConfigurationWithRestrictions(const WebCore::CDMKeySystemConfiguration&, const WebCore::CDMRestrictions&) const final;
    bool supportsSessionTypeWithConfiguration(const WebCore::CDMSessionType&, const WebCore::CDMKeySystemConfiguration&) const final;
    bool supportsInitData(const AtomString&, const WebCore::SharedBuffer&) const final;
    WebCore::CDMRequirement distinctiveIdentifiersRequirement(const WebCore::CDMKeySystemConfiguration&, const WebCore::CDMRestrictions&) const final;
    WebCore::CDMRequirement persistentStateRequirement(const WebCore::CDMKeySystemConfiguration&, const WebCore::CDMRestrictions&) const final;
    bool distinctiveIdentifiersAreUniquePerOriginAndClearable(const WebCore::CDMKeySystemConfiguration&) const final;
    RefPtr<WebCore::CDMInstance> createInstance() final;
    void loadAndInitialize() final;
    RefPtr<WebCore::SharedBuffer> sanitizeResponse(const WebCore::SharedBuffer&) const final;
    std::optional<String> sanitizeSessionId(const String&) const final;

    Vector<AtomString> supportedInitDataTypes() const final { return m_configuration.supportedInitDataTypes; }
    Vector<AtomString> supportedRobustnesses() const final { return m_configuration.supportedRobustnesses; }
    bool supportsServerCertificates() const final { return m_configuration.supportsServerCertificates; }
    bool supportsSessions() const final { return m_configuration.supportsSessions; }

    WeakPtr<RemoteCDMFactory> m_factory;
    RemoteCDMIdentifier m_identifier;
    RemoteCDMConfiguration m_configuration;
};

}

#endif
