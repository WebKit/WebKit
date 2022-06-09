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
#include "LoadableScript.h"

#include "CachedResourceLoader.h"
#include "CachedScript.h"
#include "ContentSecurityPolicy.h"
#include "Document.h"
#include "LoadableScriptClient.h"
#include "Settings.h"

namespace WebCore {

void LoadableScript::addClient(LoadableScriptClient& client)
{
    m_clients.add(&client);
    if (isLoaded()) {
        Ref<LoadableScript> protectedThis(*this);
        client.notifyFinished(*this);
    }
}

void LoadableScript::removeClient(LoadableScriptClient& client)
{
    m_clients.remove(&client);
}

void LoadableScript::notifyClientFinished()
{
    RefPtr<LoadableScript> protectedThis(this);

    Vector<LoadableScriptClient*> vector;
    for (auto& pair : m_clients)
        vector.append(pair.key);
    for (auto& client : vector)
        client->notifyFinished(*this);
}

}
