/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDMFactory.h"
#include "CDMPrivate.h"

namespace WebCore {

struct FourCC;

class CDMFactoryFairPlayStreaming final : public CDMFactory {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static CDMFactoryFairPlayStreaming& singleton();

    virtual ~CDMFactoryFairPlayStreaming();

    std::unique_ptr<CDMPrivate> createCDM(const String&, const CDMPrivateClient&) override;
    bool supportsKeySystem(const String&) override;

private:
    friend class NeverDestroyed<CDMFactoryFairPlayStreaming>;
    CDMFactoryFairPlayStreaming();
};

class CDMPrivateFairPlayStreaming final : public CDMPrivate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CDMPrivateFairPlayStreaming(const CDMPrivateClient&);
    virtual ~CDMPrivateFairPlayStreaming();

#if !RELEASE_LOG_DISABLED
    void setLogIdentifier(const void* logIdentifier) final { m_logIdentifier = logIdentifier; }
    const Logger& logger() const { return m_logger; };
    const void* logIdentifier() const { return m_logIdentifier; }
    const char* logClassName() const { return "CDMPrivateFairPlayStreaming"; }
#endif

    Vector<AtomString> supportedInitDataTypes() const override;
    bool supportsConfiguration(const CDMKeySystemConfiguration&) const override;
    bool supportsConfigurationWithRestrictions(const CDMKeySystemConfiguration&, const CDMRestrictions&) const override;
    bool supportsSessionTypeWithConfiguration(const CDMSessionType&, const CDMKeySystemConfiguration&) const override;
    Vector<AtomString> supportedRobustnesses() const override;
    CDMRequirement distinctiveIdentifiersRequirement(const CDMKeySystemConfiguration&, const CDMRestrictions&) const override;
    CDMRequirement persistentStateRequirement(const CDMKeySystemConfiguration&, const CDMRestrictions&) const override;
    bool distinctiveIdentifiersAreUniquePerOriginAndClearable(const CDMKeySystemConfiguration&) const override;
    RefPtr<CDMInstance> createInstance() override;
    void loadAndInitialize() override;
    bool supportsServerCertificates() const override;
    bool supportsSessions() const override;
    bool supportsInitData(const AtomString&, const FragmentedSharedBuffer&) const override;
    RefPtr<FragmentedSharedBuffer> sanitizeResponse(const FragmentedSharedBuffer&) const override;
    std::optional<String> sanitizeSessionId(const String&) const override;

    static const AtomString& sinfName();
    static std::optional<Vector<Ref<FragmentedSharedBuffer>>> extractKeyIDsSinf(const FragmentedSharedBuffer&);
    static RefPtr<FragmentedSharedBuffer> sanitizeSinf(const FragmentedSharedBuffer&);

    static const AtomString& skdName();
    static std::optional<Vector<Ref<FragmentedSharedBuffer>>> extractKeyIDsSkd(const FragmentedSharedBuffer&);
    static RefPtr<FragmentedSharedBuffer> sanitizeSkd(const FragmentedSharedBuffer&);

    static const Vector<FourCC>& validFairPlayStreamingSchemes();

private:
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier { nullptr };
#endif
};

}

#endif // ENABLE(ENCRYPTED_MEDIA)
