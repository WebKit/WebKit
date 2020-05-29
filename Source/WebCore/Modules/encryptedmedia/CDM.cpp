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
#include "CDM.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDMFactory.h"
#include "CDMPrivate.h"
#include "Document.h"
#include "InitDataRegistry.h"
#include "MediaKeysRequirement.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include "Page.h"
#include "ParsedContentType.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SecurityOriginData.h"
#include "Settings.h"
#include <wtf/FileSystem.h>
#include <wtf/Logger.h>
#include <wtf/LoggerHelper.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

bool CDM::supportsKeySystem(const String& keySystem)
{
    for (auto* factory : CDMFactory::registeredFactories()) {
        if (factory->supportsKeySystem(keySystem))
            return true;
    }
    return false;
}

Ref<CDM> CDM::create(Document& document, const String& keySystem)
{
    return adoptRef(*new CDM(document, keySystem));
}

CDM::CDM(Document& document, const String& keySystem)
    : ContextDestructionObserver(&document)
#if !RELEASE_LOG_DISABLED
    , m_logger(document.logger())
    , m_logIdentifier(LoggerHelper::uniqueLogIdentifier())
#endif
    , m_keySystem(keySystem)
{
    ASSERT(supportsKeySystem(keySystem));
    for (auto* factory : CDMFactory::registeredFactories()) {
        if (factory->supportsKeySystem(keySystem)) {
            m_private = factory->createCDM(keySystem);
#if !RELEASE_LOG_DISABLED
            m_private->setLogger(m_logger, m_logIdentifier);
#endif
            break;
        }
    }
}

CDM::~CDM() = default;

void CDM::getSupportedConfiguration(MediaKeySystemConfiguration&& candidateConfiguration, SupportedConfigurationCallback&& callback)
{
    // https://w3c.github.io/encrypted-media/#get-supported-configuration
    // W3C Editor's Draft 09 November 2016
    // Implemented in CDMPrivate::getSupportedConfiguration()

    Document* document = downcast<Document>(m_scriptExecutionContext);
    if (!document || !m_private) {
        callback(WTF::nullopt);
        return;
    }

    bool isEphemeral = !document->page() || document->page()->sessionID().isEphemeral();

    SecurityOrigin& origin = document->securityOrigin();
    SecurityOrigin& topOrigin = document->topOrigin();
    CDMPrivate::LocalStorageAccess access = !isEphemeral && origin.canAccessLocalStorage(&topOrigin) ? CDMPrivate::LocalStorageAccess::Allowed : CDMPrivate::LocalStorageAccess::NotAllowed;
    m_private->getSupportedConfiguration(WTFMove(candidateConfiguration), access, WTFMove(callback));
}

void CDM::loadAndInitialize()
{
    if (m_private)
        m_private->loadAndInitialize();
}

RefPtr<CDMInstance> CDM::createInstance()
{
    if (!m_private)
        return nullptr;
    auto instance = m_private->createInstance();
    if (instance)
        instance->setStorageDirectory(storageDirectory());
    return instance;
}

bool CDM::supportsServerCertificates() const
{
    return m_private && m_private->supportsServerCertificates();
}

bool CDM::supportsSessions() const
{
    return m_private && m_private->supportsSessions();
}

bool CDM::supportsInitDataType(const AtomString& initDataType) const
{
    return m_private && m_private->supportedInitDataTypes().contains(initDataType);
}

RefPtr<SharedBuffer> CDM::sanitizeInitData(const AtomString& initDataType, const SharedBuffer& initData)
{
    return InitDataRegistry::shared().sanitizeInitData(initDataType, initData);
}

bool CDM::supportsInitData(const AtomString& initDataType, const SharedBuffer& initData)
{
    return m_private && m_private->supportsInitData(initDataType, initData);
}

RefPtr<SharedBuffer> CDM::sanitizeResponse(const SharedBuffer& response)
{
    if (!m_private)
        return nullptr;
    return m_private->sanitizeResponse(response);
}

Optional<String> CDM::sanitizeSessionId(const String& sessionId)
{
    if (!m_private)
        return WTF::nullopt;
    return m_private->sanitizeSessionId(sessionId);
}

String CDM::storageDirectory() const
{
    auto* document = downcast<Document>(scriptExecutionContext());
    if (!document)
        return emptyString();

    auto* page = document->page();
    if (!page || page->usesEphemeralSession())
        return emptyString();

    auto storageDirectory = document->settings().mediaKeysStorageDirectory();
    if (storageDirectory.isEmpty())
        return emptyString();

    return FileSystem::pathByAppendingComponent(storageDirectory, document->securityOrigin().data().databaseIdentifier());
}

}

#endif
