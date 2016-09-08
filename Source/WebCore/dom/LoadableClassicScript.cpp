/*
 * Copyright (C) 2016 Apple, Inc. All Rights Reserved.
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

#include "ScriptElement.h"
#include "ScriptSourceCode.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringImpl.h>

namespace WebCore {

Ref<LoadableClassicScript> LoadableClassicScript::create(CachedResourceHandle<CachedScript>&& cachedScript, const String& crossOriginMode, SecurityOrigin& securityOrigin)
{
    ASSERT(cachedScript);
    auto script = adoptRef(*new LoadableClassicScript(WTFMove(cachedScript), crossOriginMode, securityOrigin));
    cachedScript->addClient(script.ptr());
    return script;
}

LoadableClassicScript::LoadableClassicScript(CachedResourceHandle<CachedScript>&& cachedScript, const String& crossOriginMode, SecurityOrigin& securityOrigin)
    : m_cachedScript(cachedScript)
    , m_securityOrigin(securityOrigin)
    , m_requestUsesAccessControl(!crossOriginMode.isNull())
{
}

LoadableClassicScript::~LoadableClassicScript()
{
    m_cachedScript->removeClient(this);
}

bool LoadableClassicScript::isLoaded() const
{
    return m_cachedScript->isLoaded();
}

Optional<LoadableScript::Error> LoadableClassicScript::wasErrored() const
{
    if (m_error)
        return m_error;

    if (m_cachedScript->errorOccurred())
        return Error { ErrorType::CachedScript, Nullopt };

    return Nullopt;
}

bool LoadableClassicScript::wasCanceled() const
{
    return m_cachedScript->wasCanceled();
}

void LoadableClassicScript::notifyFinished(CachedResource*)
{
    if (!m_error && m_requestUsesAccessControl && !m_cachedScript->passesSameOriginPolicyCheck(m_securityOrigin.get())) {
        static NeverDestroyed<String> consoleMessage(ASCIILiteral("Cross-origin script load denied by Cross-Origin Resource Sharing policy."));
        m_error = Error {
            ErrorType::CrossOriginLoad,
            ConsoleMessage {
                MessageSource::JS,
                MessageLevel::Error,
                consoleMessage
            }
        };
    }

#if ENABLE(NOSNIFF)
    if (!m_error && !m_cachedScript->mimeTypeAllowedByNosniff()) {
        m_error = Error {
            ErrorType::Nosniff,
            ConsoleMessage {
                MessageSource::Security,
                MessageLevel::Error,
                makeString(
                    "Refused to execute script from '", m_cachedScript->url().stringCenterEllipsizedToLength()
                    "' because its MIME type ('", m_cachedScript->mimeType(), "') is not executable, and strict MIME type checking is enabled.")
            }
        };
    }
#endif

    notifyClientFinished();
}

void LoadableClassicScript::execute(ScriptElement& scriptElement)
{
    ASSERT(!wasErrored());
    scriptElement.executeScript(ScriptSourceCode(m_cachedScript.get()));
}

}
