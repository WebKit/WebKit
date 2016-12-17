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

#include "CDM.h"
#include "CDMPrivate.h"
#include "MediaKeysRequirement.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class MockCDMFactory : public RefCounted<MockCDMFactory>, private CDMFactory {
public:
    static Ref<MockCDMFactory> create() { return adoptRef(*new MockCDMFactory); }
    ~MockCDMFactory();

    const Vector<String>& supportedDataTypes() const { return m_supportedDataTypes; }
    void setSupportedDataTypes(Vector<String>&& types) { m_supportedDataTypes = WTFMove(types); }

    const Vector<String>& supportedRobustness() const { return m_supportedRobustness; }
    void setSupportedRobustness(Vector<String>&& robustness) { m_supportedRobustness = WTFMove(robustness); }

    MediaKeysRequirement distinctiveIdentifiersRequirement() const { return m_distinctiveIdentifiersRequirement; }
    void setDistinctiveIdentifiersRequirement(MediaKeysRequirement requirement) { m_distinctiveIdentifiersRequirement = requirement; }

    MediaKeysRequirement persistentStateRequirement() const { return m_persistentStateRequirement; }
    void setPersistentStateRequirement(MediaKeysRequirement requirement) { m_persistentStateRequirement = requirement; }

    void unregister();

private:
    MockCDMFactory();
    std::unique_ptr<CDMPrivate> createCDM(CDM&) final;
    bool supportsKeySystem(const String&) final;

    MediaKeysRequirement m_distinctiveIdentifiersRequirement { MediaKeysRequirement::Optional };
    MediaKeysRequirement m_persistentStateRequirement { MediaKeysRequirement::Optional };
    Vector<String> m_supportedDataTypes;
    Vector<String> m_supportedRobustness;
    bool m_registered { true };
    WeakPtrFactory<MockCDMFactory> m_weakPtrFactory;
};

class MockCDM : public CDMPrivate {
public:
    MockCDM(WeakPtr<MockCDMFactory>);

private:
    bool supportsInitDataType(const String&) final;
    bool supportsConfiguration(const MediaKeySystemConfiguration&) final;
    bool supportsConfigurationWithRestrictions(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&) final;
    bool supportsSessionTypeWithConfiguration(MediaKeySessionType&, const MediaKeySystemConfiguration&) final;
    bool supportsRobustness(const String&) final;
    MediaKeysRequirement distinctiveIdentifiersRequirement(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&) final;
    MediaKeysRequirement persistentStateRequirement(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&) final;
    bool distinctiveIdentifiersAreUniquePerOriginAndClearable(const MediaKeySystemConfiguration&) final;

    WeakPtr<MockCDMFactory> m_factory;
};

}

#endif
