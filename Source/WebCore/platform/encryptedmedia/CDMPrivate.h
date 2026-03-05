/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <WebCore/CDMInstance.h>
#include <WebCore/CDMRequirement.h>
#include <WebCore/CDMSessionType.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

#if !RELEASE_LOG_DISABLED
namespace WTF {
class Logger;
}
#endif

namespace WebCore {

struct CDMKeySystemConfiguration;
struct CDMMediaCapability;
struct CDMRestrictions;
class SharedBuffer;

enum class CDMPrivateLocalStorageAccess : bool {
    NotAllowed,
    Allowed,
};

class CDMPrivateClient {
public:
    virtual ~CDMPrivateClient() = default;

#if !RELEASE_LOG_DISABLED
    virtual const Logger& logger() const = 0;
#endif
};

class CDMPrivate : public CanMakeWeakPtr<CDMPrivate>, public CanMakeCheckedPtr<CDMPrivate> {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(CDMPrivate, WEBCORE_EXPORT);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(CDMPrivate);
public:
    WEBCORE_EXPORT virtual ~CDMPrivate();

#if !RELEASE_LOG_DISABLED
    virtual void setLogIdentifier(uint64_t) { };
#endif

    using LocalStorageAccess = CDMPrivateLocalStorageAccess;

    using SupportedConfigurationCallback = Function<void(std::optional<CDMKeySystemConfiguration>)>;
    WEBCORE_EXPORT virtual void getSupportedConfiguration(CDMKeySystemConfiguration&& candidateConfiguration, LocalStorageAccess, SupportedConfigurationCallback&&);

    virtual Vector<String> supportedInitDataTypes() const = 0;
    virtual bool supportsConfiguration(const CDMKeySystemConfiguration&) const = 0;
    virtual bool supportsConfigurationWithRestrictions(const CDMKeySystemConfiguration&, const CDMRestrictions&) const = 0;
    virtual bool supportsSessionTypeWithConfiguration(const CDMSessionType&, const CDMKeySystemConfiguration&) const = 0;
    virtual Vector<String> supportedRobustnesses() const = 0;
    virtual CDMRequirement distinctiveIdentifiersRequirement(const CDMKeySystemConfiguration&, const CDMRestrictions&) const = 0;
    virtual CDMRequirement persistentStateRequirement(const CDMKeySystemConfiguration&, const CDMRestrictions&) const = 0;
    virtual bool distinctiveIdentifiersAreUniquePerOriginAndClearable(const CDMKeySystemConfiguration&) const = 0;
    virtual RefPtr<CDMInstance> createInstance() = 0;
    virtual void loadAndInitialize() = 0;
    virtual bool supportsServerCertificates() const = 0;
    virtual bool supportsSessions() const = 0;
    virtual bool supportsInitData(const String&, const SharedBuffer&) const = 0;
    virtual RefPtr<SharedBuffer> sanitizeResponse(const SharedBuffer&) const = 0;
    virtual std::optional<String> sanitizeSessionId(const String&) const = 0;

protected:
    WEBCORE_EXPORT CDMPrivate();
    static bool isPersistentType(CDMSessionType);

    enum class ConfigurationStatus {
        Supported,
        NotSupported,
        ConsentDenied,
    };

    enum class ConsentStatus {
        ConsentDenied,
        InformUser,
        Allowed,
    };

    enum class AudioVideoType {
        Audio,
        Video,
    };

    void doSupportedConfigurationStep(CDMKeySystemConfiguration&& candidateConfiguration, CDMRestrictions&&, LocalStorageAccess, SupportedConfigurationCallback&&);
    std::optional<CDMKeySystemConfiguration> getSupportedConfiguration(const CDMKeySystemConfiguration& candidateConfiguration, CDMRestrictions&, LocalStorageAccess);
    std::optional<Vector<CDMMediaCapability>> getSupportedCapabilitiesForAudioVideoType(AudioVideoType, const Vector<CDMMediaCapability>& requestedCapabilities, const CDMKeySystemConfiguration& partialConfiguration, CDMRestrictions&);

    using ConsentStatusCallback = Function<void(ConsentStatus, CDMKeySystemConfiguration&&, CDMRestrictions&&)>;
    void getConsentStatus(CDMKeySystemConfiguration&& accumulatedConfiguration, CDMRestrictions&&, LocalStorageAccess, ConsentStatusCallback&&);
};

}

#endif
