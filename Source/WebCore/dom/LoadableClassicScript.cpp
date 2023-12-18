/*
 * Copyright (C) 2016-2017 Apple, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LoadableClassicScript.h"

#include "DefaultResourceLoadPriority.h"
#include "Element.h"
#include "FetchIdioms.h"
#include "LoadableImportMap.h"
#include "LoadableScriptError.h"
#include "ScriptElement.h"
#include "ScriptSourceCode.h"
#include "SubresourceIntegrity.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringImpl.h>

namespace WebCore {

LoadableNonModuleScriptBase::LoadableNonModuleScriptBase(const AtomString& nonce, const AtomString& integrity, ReferrerPolicy policy, RequestPriority fetchPriorityHint,  const AtomString& crossOriginMode, const String& charset, const AtomString& initiatorType, bool isInUserAgentShadowTree, bool isAsync)
    : LoadableScript(nonce, policy, fetchPriorityHint, crossOriginMode, charset, initiatorType, isInUserAgentShadowTree)
    , m_integrity(integrity)
    , m_isAsync(isAsync)
{
}

LoadableNonModuleScriptBase::~LoadableNonModuleScriptBase()
{
    if (CachedResourceHandle cachedScript = m_cachedScript)
        cachedScript->removeClient(*this);
}

bool LoadableNonModuleScriptBase::isLoaded() const
{
    ASSERT(m_cachedScript);
    return m_cachedScript->isLoaded();
}

bool LoadableNonModuleScriptBase::hasError() const
{
    ASSERT(m_cachedScript);
    if (m_error)
        return true;

    return m_cachedScript->errorOccurred();
}

std::optional<LoadableScript::Error> LoadableNonModuleScriptBase::takeError()
{
    ASSERT(m_cachedScript);
    if (m_error)
        return std::exchange(m_error, { });

    if (m_cachedScript->errorOccurred())
        return Error { ErrorType::Fetch, { }, { } };

    return std::nullopt;
}

bool LoadableNonModuleScriptBase::wasCanceled() const
{
    ASSERT(m_cachedScript);
    return m_cachedScript->wasCanceled();
}

void LoadableNonModuleScriptBase::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&)
{
    ASSERT(m_cachedScript);
    if (resource.resourceError().isAccessControl()) {
        static NeverDestroyed<String> consoleMessage(MAKE_STATIC_STRING_IMPL("Cross-origin script load denied by Cross-Origin Resource Sharing policy."));
        m_error = Error {
            ErrorType::CrossOriginLoad,
            ConsoleMessage {
                MessageSource::JS,
                MessageLevel::Error,
                consoleMessage
            },
            { }
        };
    }

    CachedResourceHandle cachedScript = m_cachedScript;
    if (!m_error && !isScriptAllowedByNosniff(cachedScript->response())) {
        m_error = Error {
            ErrorType::Nosniff,
            ConsoleMessage {
                MessageSource::Security,
                MessageLevel::Error,
                makeString("Refused to execute ", cachedScript->url().stringCenterEllipsizedToLength(), " as script because \"X-Content-Type-Options: nosniff\" was given and its Content-Type is not a script MIME type.")
            },
            { }
        };
    }

    if (!m_error && shouldBlockResponseDueToMIMEType(cachedScript->response(), cachedScript->options().destination)) {
        m_error = Error {
            ErrorType::MIMEType,
            ConsoleMessage {
                MessageSource::Security,
                MessageLevel::Error,
                makeString("Refused to execute ", cachedScript->url().stringCenterEllipsizedToLength(), " as script because ", cachedScript->response().mimeType(), " is not a script MIME type.")
            },
            { }
        };
    }

    if (!m_error && !resource.errorOccurred() && !matchIntegrityMetadata(resource, m_integrity)) {
        m_error = Error {
            ErrorType::FailedIntegrityCheck,
            ConsoleMessage { MessageSource::Security, MessageLevel::Error, makeString("Cannot load script ", integrityMismatchDescription(resource, m_integrity)) },
            { }
        };
    }

    notifyClientFinished();
}

bool LoadableNonModuleScriptBase::load(Document& document, const URL& sourceURL)
{
    ASSERT(!m_cachedScript);

    auto priority = [&]() -> std::optional<ResourceLoadPriority> {
        if (isAsync())
            return DefaultResourceLoadPriority::asyncScript;
        // Use default.
        return { };
    };

    m_weakDocument = document;
    CachedResourceHandle cachedScript = requestScriptWithCache(document, sourceURL, crossOriginMode(), String { integrity() }, priority());
    m_cachedScript = cachedScript;
    if (!cachedScript)
        return false;
    cachedScript->addClient(*this);
    return true;
}

Ref<LoadableClassicScript> LoadableClassicScript::create(const AtomString& nonce, const AtomString& integrityMetadata, ReferrerPolicy policy, RequestPriority fetchPriorityHint, const AtomString& crossOriginMode, const String& charset, const AtomString& initiatorType, bool isInUserAgentShadowTree, bool isAsync)
{
    return adoptRef(*new LoadableClassicScript(nonce, integrityMetadata, policy, fetchPriorityHint, crossOriginMode, charset, initiatorType, isInUserAgentShadowTree, isAsync));
}

LoadableClassicScript::LoadableClassicScript(const AtomString& nonce, const AtomString& integrity, ReferrerPolicy policy, RequestPriority fetchPriorityHint, const AtomString& crossOriginMode, const String& charset, const AtomString& initiatorType, bool isInUserAgentShadowTree, bool isAsync)
    : LoadableNonModuleScriptBase(nonce, integrity, policy, fetchPriorityHint, crossOriginMode, charset, initiatorType, isInUserAgentShadowTree, isAsync)
{
}

void LoadableClassicScript::execute(ScriptElement& scriptElement)
{
    ASSERT(!m_error);
    scriptElement.executeClassicScript(ScriptSourceCode(protectedCachedScript().get(), JSC::SourceProviderSourceType::Program, *this));
}

}
