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

#include "FetchIdioms.h"
#include "ScriptElement.h"
#include "ScriptSourceCode.h"
#include "SubresourceIntegrity.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringImpl.h>

namespace WebCore {

Ref<LoadableClassicScript> LoadableClassicScript::create(const String& nonce, const String& integrityMetadata, ReferrerPolicy policy, const String& crossOriginMode, const String& charset, const AtomString& initiatorName, bool isInUserAgentShadowTree)
{
    return adoptRef(*new LoadableClassicScript(nonce, integrityMetadata, policy, crossOriginMode, charset, initiatorName, isInUserAgentShadowTree));
}

LoadableClassicScript::~LoadableClassicScript()
{
    if (m_cachedScript)
        m_cachedScript->removeClient(*this);
}

bool LoadableClassicScript::isLoaded() const
{
    ASSERT(m_cachedScript);
    return m_cachedScript->isLoaded();
}

Optional<LoadableScript::Error> LoadableClassicScript::error() const
{
    ASSERT(m_cachedScript);
    if (m_error)
        return m_error;

    if (m_cachedScript->errorOccurred())
        return Error { ErrorType::CachedScript, WTF::nullopt };

    return WTF::nullopt;
}

bool LoadableClassicScript::wasCanceled() const
{
    ASSERT(m_cachedScript);
    return m_cachedScript->wasCanceled();
}

void LoadableClassicScript::notifyFinished(CachedResource& resource)
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
            }
        };
    }

    if (!m_error && !isScriptAllowedByNosniff(m_cachedScript->response())) {
        m_error = Error {
            ErrorType::Nosniff,
            ConsoleMessage {
                MessageSource::Security,
                MessageLevel::Error,
                makeString("Refused to execute ", m_cachedScript->url().stringCenterEllipsizedToLength(), " as script because \"X-Content-Type: nosniff\" was given and its Content-Type is not a script MIME type.")
            }
        };
    }

    if (!m_error && shouldBlockResponseDueToMIMEType(m_cachedScript->response(), m_cachedScript->options().destination)) {
        m_error = Error {
            ErrorType::MIMEType,
            ConsoleMessage {
                MessageSource::Security,
                MessageLevel::Error,
                makeString("Refused to execute ", m_cachedScript->url().stringCenterEllipsizedToLength(), " as script because ", m_cachedScript->response().mimeType(), " is not a script MIME type.")
            }
        };
    }

    if (!m_error && !resource.errorOccurred() && !matchIntegrityMetadata(resource, m_integrity)) {
        m_error = Error {
            ErrorType::FailedIntegrityCheck,
            ConsoleMessage { MessageSource::Security, MessageLevel::Error, makeString("Cannot load script ", integrityMismatchDescription(resource, m_integrity)) }
        };
    }

    notifyClientFinished();
}

void LoadableClassicScript::execute(ScriptElement& scriptElement)
{
    ASSERT(!error());
    scriptElement.executeClassicScript(ScriptSourceCode(m_cachedScript.get(), JSC::SourceProviderSourceType::Program, *this));
}

bool LoadableClassicScript::load(Document& document, const URL& sourceURL)
{
    ASSERT(!m_cachedScript);
    m_cachedScript = requestScriptWithCache(document, sourceURL, crossOriginMode(), String { m_integrity });
    if (!m_cachedScript)
        return false;
    m_cachedScript->addClient(*this);
    return true;
}

}
