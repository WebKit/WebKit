/*
 * Copyright (C) 2012 Motorola Mobility Inc.
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PublicURLManager.h"

#include "URLRegistry.h"
#include <wtf/URL.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

std::unique_ptr<PublicURLManager> PublicURLManager::create(ScriptExecutionContext* context)
{
    auto publicURLManager = makeUnique<PublicURLManager>(context);
    publicURLManager->suspendIfNeeded();
    return publicURLManager;
}

PublicURLManager::PublicURLManager(ScriptExecutionContext* context)
    : ActiveDOMObject(context)
    , m_isStopped(false)
{
}

void PublicURLManager::registerURL(const URL& url, URLRegistrable& registrable)
{
    if (m_isStopped)
        return;

    RegistryURLMap::iterator found = m_registryToURL.add(&registrable.registry(), URLSet()).iterator;
    found->key->registerURL(*scriptExecutionContext(), url, registrable);
    found->value.add(url.string());
}

void PublicURLManager::revoke(const URL& url)
{
    for (auto& registry : m_registryToURL) {
        if (registry.value.contains(url.string())) {
            registry.key->unregisterURL(url);
            registry.value.remove(url.string());
            break;
        }
    }
}

void PublicURLManager::stop()
{
    if (m_isStopped)
        return;

    m_isStopped = true;
    for (auto& registry : m_registryToURL) {
        for (auto& url : registry.value)
            registry.key->unregisterURL(URL({ }, url));
    }

    m_registryToURL.clear();
}

const char* PublicURLManager::activeDOMObjectName() const
{
    return "PublicURLManager";
}

} // namespace WebCore
