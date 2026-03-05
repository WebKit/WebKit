/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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
#include <wtf/TZoneMalloc.h>

OBJC_CLASS AVContentKeyRequest;

namespace WebCore {

struct FourCC;

class CDMFactoryFairPlayStreaming final : public CDMFactory {
    WTF_MAKE_TZONE_ALLOCATED(CDMFactoryFairPlayStreaming);
public:
    static CDMFactoryFairPlayStreaming& singleton();

    virtual ~CDMFactoryFairPlayStreaming();

    // Do nothing since this is a singleton object.
    void ref() const final { }
    void deref() const final { }

    std::unique_ptr<CDMPrivate> createCDM(const String& keySystem, const String& mediaKeysHashSalt, const CDMPrivateClient&) override;
    bool supportsKeySystem(const String&) override;

private:
    friend class NeverDestroyed<CDMFactoryFairPlayStreaming>;
    CDMFactoryFairPlayStreaming();
};

class CDMPrivateFairPlayStreaming final : public CDMPrivate {
    WTF_MAKE_TZONE_ALLOCATED(CDMPrivateFairPlayStreaming);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(CDMPrivateFairPlayStreaming);
public:
    CDMPrivateFairPlayStreaming(const String& mediaKeysHashSalt, const CDMPrivateClient&);
    virtual ~CDMPrivateFairPlayStreaming();

#if !RELEASE_LOG_DISABLED
    void setLogIdentifier(uint64_t logIdentifier) final { m_logIdentifier = logIdentifier; }
    const Logger& logger() const { return m_logger; };
    uint64_t logIdentifier() const { return m_logIdentifier; }
    ASCIILiteral logClassName() const { return "CDMPrivateFairPlayStreaming"_s; }
#endif

    Vector<String> supportedInitDataTypes() const override;
    bool supportsConfiguration(const CDMKeySystemConfiguration&) const override;
    bool supportsConfigurationWithRestrictions(const CDMKeySystemConfiguration&, const CDMRestrictions&) const override;
    bool supportsSessionTypeWithConfiguration(const CDMSessionType&, const CDMKeySystemConfiguration&) const override;
    Vector<String> supportedRobustnesses() const override;
    CDMRequirement distinctiveIdentifiersRequirement(const CDMKeySystemConfiguration&, const CDMRestrictions&) const override;
    CDMRequirement persistentStateRequirement(const CDMKeySystemConfiguration&, const CDMRestrictions&) const override;
    bool distinctiveIdentifiersAreUniquePerOriginAndClearable(const CDMKeySystemConfiguration&) const override;
    RefPtr<CDMInstance> createInstance() override;
    void loadAndInitialize() override;
    bool supportsServerCertificates() const override;
    bool supportsSessions() const override;
    bool supportsInitData(const String&, const SharedBuffer&) const override;
    RefPtr<SharedBuffer> sanitizeResponse(const SharedBuffer&) const override;
    std::optional<String> sanitizeSessionId(const String&) const override;

    static const String& sinfName();
    static std::optional<Vector<Ref<SharedBuffer>>> extractKeyIDsSinf(const SharedBuffer&);
    static RefPtr<SharedBuffer> sanitizeSinf(const SharedBuffer&);

    static const String& skdName();
    static std::optional<Vector<Ref<SharedBuffer>>> extractKeyIDsSkd(const SharedBuffer&);
    static RefPtr<SharedBuffer> sanitizeSkd(const SharedBuffer&);

#if HAVE(FAIRPLAYSTREAMING_MTPS_INITDATA)
    static const String& mptsName();
    static std::optional<Vector<Ref<SharedBuffer>>> extractKeyIDsMpts(const SharedBuffer&);
    static RefPtr<SharedBuffer> sanitizeMpts(const SharedBuffer&);
    static const Vector<Ref<SharedBuffer>>& mptsKeyIDs();
#endif

    static const Vector<FourCC>& validFairPlayStreamingSchemes();

#if HAVE(AVCONTENTKEYSESSION)
    static Vector<Ref<SharedBuffer>> keyIDsForRequest(AVContentKeyRequest *);
#endif

    const String& mediaKeysHashSalt() const { return m_mediaKeysHashSalt; }

private:
    String m_mediaKeysHashSalt;

#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    uint64_t m_logIdentifier { 0 };
#endif
};

}

#endif // ENABLE(ENCRYPTED_MEDIA)
