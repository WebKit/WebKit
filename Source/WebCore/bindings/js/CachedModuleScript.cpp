/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
#include "CachedModuleScript.h"

#include "CachedModuleScriptClient.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "ScriptController.h"
#include "ScriptModuleLoader.h"
#include "ScriptRunner.h"
#include "ScriptSourceCode.h"

namespace WebCore {

Ref<CachedModuleScript> CachedModuleScript::create()
{
    return adoptRef(*new CachedModuleScript());
}

void CachedModuleScript::load(Document& document, const URL& rootURL, CachedScriptFetcher& scriptFetcher)
{
    if (auto* frame = document.frame())
        frame->script().loadModuleScript(*this, rootURL.string(), scriptFetcher);
}

void CachedModuleScript::load(Document& document, const ScriptSourceCode& sourceCode, CachedScriptFetcher& scriptFetcher)
{
    if (auto* frame = document.frame())
        frame->script().loadModuleScript(*this, sourceCode, scriptFetcher);
}

void CachedModuleScript::notifyLoadCompleted(UniquedStringImpl& moduleKey)
{
    m_moduleKey = &moduleKey;
    notifyClientFinished();
}

void CachedModuleScript::notifyLoadFailed(LoadableScript::Error&& error)
{
    m_error = WTFMove(error);
    notifyClientFinished();
}

void CachedModuleScript::notifyLoadWasCanceled()
{
    m_wasCanceled = true;
    notifyClientFinished();
}

void CachedModuleScript::notifyClientFinished()
{
    m_isLoaded = true;

    Ref<CachedModuleScript> protectedThis(*this);

    Vector<CachedModuleScriptClient*> clients;
    copyToVector(m_clients, clients);
    for (auto& client : clients)
        client->notifyFinished(*this);
}

void CachedModuleScript::addClient(CachedModuleScriptClient& client)
{
    m_clients.add(&client);
    if (!isLoaded())
        return;
    Ref<CachedModuleScript> protectedThis(*this);
    client.notifyFinished(*this);
}

void CachedModuleScript::removeClient(CachedModuleScriptClient& client)
{
    m_clients.remove(&client);
}

}
