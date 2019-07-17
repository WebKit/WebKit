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
#include "LoadableModuleScript.h"

#include "Document.h"
#include "Frame.h"
#include "ModuleFetchParameters.h"
#include "ScriptController.h"
#include "ScriptElement.h"

namespace WebCore {

Ref<LoadableModuleScript> LoadableModuleScript::create(const String& nonce, const String& integrity, ReferrerPolicy policy, const String& crossOriginMode, const String& charset, const AtomString& initiatorName, bool isInUserAgentShadowTree)
{
    return adoptRef(*new LoadableModuleScript(nonce, integrity, policy, crossOriginMode, charset, initiatorName, isInUserAgentShadowTree));
}

LoadableModuleScript::LoadableModuleScript(const String& nonce, const String& integrity, ReferrerPolicy policy, const String& crossOriginMode, const String& charset, const AtomString& initiatorName, bool isInUserAgentShadowTree)
    : LoadableScript(nonce, policy, crossOriginMode, charset, initiatorName, isInUserAgentShadowTree)
    , m_parameters(ModuleFetchParameters::create(integrity))
{
}

LoadableModuleScript::~LoadableModuleScript() = default;

bool LoadableModuleScript::isLoaded() const
{
    return m_isLoaded;
}

Optional<LoadableScript::Error> LoadableModuleScript::error() const
{
    return m_error;
}

bool LoadableModuleScript::wasCanceled() const
{
    return m_wasCanceled;
}

void LoadableModuleScript::notifyLoadCompleted(UniquedStringImpl& moduleKey)
{
    m_moduleKey = &moduleKey;
    m_isLoaded = true;
    notifyClientFinished();
}

void LoadableModuleScript::notifyLoadFailed(LoadableScript::Error&& error)
{
    m_error = WTFMove(error);
    m_isLoaded = true;
    notifyClientFinished();
}

void LoadableModuleScript::notifyLoadWasCanceled()
{
    m_wasCanceled = true;
    m_isLoaded = true;
    notifyClientFinished();
}

void LoadableModuleScript::execute(ScriptElement& scriptElement)
{
    scriptElement.executeModuleScript(*this);
}

void LoadableModuleScript::load(Document& document, const URL& rootURL)
{
    if (auto* frame = document.frame())
        frame->script().loadModuleScript(*this, rootURL.string(), m_parameters.copyRef());
}

void LoadableModuleScript::load(Document& document, const ScriptSourceCode& sourceCode)
{
    if (auto* frame = document.frame())
        frame->script().loadModuleScript(*this, sourceCode);
}

}
