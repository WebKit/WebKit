/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

#include "MediaKeySessionType.h"
#include "MediaKeySystemConfiguration.h"
#include "MediaKeySystemMediaCapability.h"
#include "MediaKeysRestrictions.h"
#include <WebCore/CDMPrivate.h>
#include <WebCore/ContextDestructionObserver.h>
#include <wtf/Function.h>
#include <wtf/Ref.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if !RELEASE_LOG_DISABLED
namespace WTF {
class Logger;
}
#endif

namespace WebCore {

class CDMFactory;
class CDMInstance;
class CDMPrivate;
class Document;
class ScriptExecutionContext;
class SharedBuffer;

class CDM : public RefCounted<CDM>, public CDMPrivateClient, private ContextDestructionObserver {
public:
    static bool supportsKeySystem(const String&);
    static bool isPersistentType(MediaKeySessionType);

    static Ref<CDM> create(Document&, const String& keySystem, const String& mediaKeysHashSalt);
    ~CDM();

    // ContextDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    using SupportedConfigurationCallback = Function<void(std::optional<MediaKeySystemConfiguration>)>;
    void getSupportedConfiguration(MediaKeySystemConfiguration&& candidateConfiguration, SupportedConfigurationCallback&&);

    const String& keySystem() const { return m_keySystem; }

    void loadAndInitialize();
    RefPtr<CDMInstance> createInstance();
    bool supportsServerCertificates() const;
    bool supportsSessions() const;
    bool supportsInitDataType(const AtomString&) const;

    RefPtr<SharedBuffer> sanitizeInitData(const AtomString& initDataType, const SharedBuffer&);
    bool supportsInitData(const AtomString& initDataType, const SharedBuffer&);

    RefPtr<SharedBuffer> sanitizeResponse(const SharedBuffer&);

    std::optional<String> sanitizeSessionId(const String& sessionId);

    String storageDirectory() const;

    const String& mediaKeysHashSalt() const { return m_mediaKeysHashSalt; }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger; }
    uint64_t logIdentifier() const { return m_logIdentifier; }
#endif

private:
    CDM(Document&, const String& keySystem, const String& mediaKeysHashSalt);

#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
    String m_keySystem;
    String m_mediaKeysHashSalt;
    const std::unique_ptr<CDMPrivate> m_private;
};

}

#endif
