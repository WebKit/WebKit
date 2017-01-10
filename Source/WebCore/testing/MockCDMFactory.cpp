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

#include "config.h"
#include "MockCDMFactory.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include <runtime/ArrayBuffer.h>
#include <wtf/text/StringView.h>

namespace WebCore {

MockCDMFactory::MockCDMFactory()
    : m_weakPtrFactory(this)
{
    CDM::registerCDMFactory(*this);
}

MockCDMFactory::~MockCDMFactory()
{
    unregister();
}

void MockCDMFactory::unregister()
{
    if (m_registered) {
        CDM::unregisterCDMFactory(*this);
        m_registered = false;
    }
}

bool MockCDMFactory::supportsKeySystem(const String& keySystem)
{
    return equalIgnoringASCIICase(keySystem, "org.webkit.mock");
}

std::unique_ptr<CDMPrivate> MockCDMFactory::createCDM(CDM&)
{
    return std::make_unique<MockCDM>(m_weakPtrFactory.createWeakPtr());
}

MockCDM::MockCDM(WeakPtr<MockCDMFactory> factory)
    : m_factory(WTFMove(factory))
    , m_weakPtrFactory(this)
{
}

bool MockCDM::supportsInitDataType(const String& initDataType)
{
    if (m_factory)
        return m_factory->supportedDataTypes().contains(initDataType);
    return false;
}

bool MockCDM::supportsConfiguration(const MediaKeySystemConfiguration&)
{
    // NOTE: Implement;
    return true;

}

bool MockCDM::supportsConfigurationWithRestrictions(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&)
{
    // NOTE: Implement;
    return true;
}

bool MockCDM::supportsSessionTypeWithConfiguration(MediaKeySessionType&, const MediaKeySystemConfiguration&)
{
    // NOTE: Implement;
    return true;
}

bool MockCDM::supportsRobustness(const String& robustness)
{
    if (m_factory)
        return m_factory->supportedRobustness().contains(robustness);
    return false;
}

MediaKeysRequirement MockCDM::distinctiveIdentifiersRequirement(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&)
{
    if (m_factory)
        return m_factory->distinctiveIdentifiersRequirement();
    return MediaKeysRequirement::Optional;
}

MediaKeysRequirement MockCDM::persistentStateRequirement(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&)
{
    if (m_factory)
        return m_factory->persistentStateRequirement();
    return MediaKeysRequirement::Optional;
}

bool MockCDM::distinctiveIdentifiersAreUniquePerOriginAndClearable(const MediaKeySystemConfiguration&)
{
    // NOTE: Implement;
    return true;
}

std::unique_ptr<CDMInstance> MockCDM::createInstance()
{
    if (m_factory && !m_factory->canCreateInstances())
        return nullptr;
    return std::unique_ptr<CDMInstance>(new MockCDMInstance(m_weakPtrFactory.createWeakPtr()));
}

void MockCDM::loadAndInitialize()
{
    // No-op.
}

bool MockCDM::supportsServerCertificates() const
{
    return m_factory && m_factory->supportsServerCertificates();
}

MockCDMInstance::MockCDMInstance(WeakPtr<MockCDM> cdm)
    : m_cdm(cdm)
{
}

CDMInstance::SuccessValue MockCDMInstance::initializeWithConfiguration(const MediaKeySystemConfiguration& configuration)
{
    if (!m_cdm || !m_cdm->supportsConfiguration(configuration))
        return Failed;

    return Succeeded;
}

CDMInstance::SuccessValue MockCDMInstance::setDistinctiveIdentifiersAllowed(bool distinctiveIdentifiersAllowed)
{
    if (m_distinctiveIdentifiersAllowed == distinctiveIdentifiersAllowed)
        return Succeeded;

    auto* factory = m_cdm ? m_cdm->factory() : nullptr;

    if (!factory || (!distinctiveIdentifiersAllowed && factory->distinctiveIdentifiersRequirement() == MediaKeysRequirement::Required))
        return Failed;

    m_distinctiveIdentifiersAllowed = distinctiveIdentifiersAllowed;
    return Succeeded;
}

CDMInstance::SuccessValue MockCDMInstance::setPersistentStateAllowed(bool persistentStateAllowed)
{
    if (m_persistentStateAllowed == persistentStateAllowed)
        return Succeeded;

    MockCDMFactory* factory = m_cdm ? m_cdm->factory() : nullptr;

    if (!factory || (!persistentStateAllowed && factory->persistentStateRequirement() == MediaKeysRequirement::Required))
        return Failed;

    m_persistentStateAllowed = persistentStateAllowed;
    return Succeeded;
}

CDMInstance::SuccessValue MockCDMInstance::setServerCertificate(ArrayBuffer& certificate)
{
    StringView certificateStringView(static_cast<const LChar*>(certificate.data()), certificate.byteLength());

    if (equalIgnoringASCIICase(certificateStringView, "valid"))
        return Succeeded;
    return Failed;
}

}

#endif
