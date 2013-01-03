/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "PlugInAutoStartProvider.h"

#include "WebContext.h"
#include "WebContextClient.h"
#include "WebProcessMessages.h"

using namespace WebCore;

static const double plugInAutoStartExpirationTimeThreshold = 30 * 24 * 60;

namespace WebKit {

PlugInAutoStartProvider::PlugInAutoStartProvider(WebContext* context)
    : m_context(context)
{
}

static double expirationTimeFromNow()
{
    return currentTime() + plugInAutoStartExpirationTimeThreshold;
}

void PlugInAutoStartProvider::addAutoStartOrigin(const String& pageOrigin, unsigned plugInOriginHash)
{
    if (m_autoStartHashes.contains(plugInOriginHash))
        return;
    
    AutoStartTable::iterator it = m_autoStartTable.find(pageOrigin);
    if (it == m_autoStartTable.end())
        it = m_autoStartTable.add(pageOrigin, HashMap<unsigned, double>()).iterator;

    double expirationTime = expirationTimeFromNow();
    it->value.set(plugInOriginHash, expirationTime);
    m_autoStartHashes.set(plugInOriginHash, pageOrigin);
    
    m_context->sendToAllProcesses(Messages::WebProcess::DidAddPlugInAutoStartOrigin(plugInOriginHash, expirationTime));
    m_context->client().plugInAutoStartOriginHashesChanged(m_context);
}

HashMap<unsigned, double> PlugInAutoStartProvider::autoStartOriginsCopy() const
{
    HashMap<unsigned, double> copyMap;
    AutoStartTable::const_iterator end = m_autoStartTable.end();
    for (AutoStartTable::const_iterator it = m_autoStartTable.begin(); it != end; ++it) {
        HashMap<unsigned, double>::const_iterator mapEnd = it->value.end();
        for (HashMap<unsigned, double>::const_iterator mapIt = it->value.begin(); mapIt != mapEnd; ++mapIt)
            copyMap.set(mapIt->key, mapIt->value);
    }
    return copyMap;
}

PassRefPtr<ImmutableDictionary> PlugInAutoStartProvider::autoStartOriginsTableCopy() const
{
    ImmutableDictionary::MapType map;
    AutoStartTable::const_iterator end = m_autoStartTable.end();
    double now = currentTime();
    for (AutoStartTable::const_iterator it = m_autoStartTable.begin(); it != end; ++it) {
        ImmutableDictionary::MapType hashMap;
        HashMap<unsigned, double>::const_iterator valueEnd = it->value.end();
        for (HashMap<unsigned, double>::const_iterator valueIt = it->value.begin(); valueIt != valueEnd; ++valueIt) {
            if (now > valueIt->value)
                continue;
            hashMap.set(String::number(valueIt->key), WebDouble::create(valueIt->value));
        }

        if (hashMap.size())
            map.set(it->key, ImmutableDictionary::adopt(hashMap));
    }

    return ImmutableDictionary::adopt(map);
}

void PlugInAutoStartProvider::setAutoStartOriginsTable(ImmutableDictionary& table)
{
    m_autoStartTable.clear();
    m_autoStartHashes.clear();
    HashMap<unsigned, double> hashMap;

    ImmutableDictionary::MapType::const_iterator end = table.map().end();
    for (ImmutableDictionary::MapType::const_iterator it = table.map().begin(); it != end; ++it) {
        HashMap<unsigned, double> hashes;
        ImmutableDictionary* hashesForPage = static_cast<ImmutableDictionary*>(it->value.get());
        ImmutableDictionary::MapType::const_iterator hashEnd = hashesForPage->map().end();
        for (ImmutableDictionary::MapType::const_iterator hashIt = hashesForPage->map().begin(); hashIt != hashEnd; ++hashIt) {
            bool ok;
            unsigned hash = hashIt->key.toUInt(&ok);
            if (!ok)
                continue;

            if (hashIt->value->type() != WebDouble::APIType)
                continue;

            double expirationTime = static_cast<WebDouble*>(hashIt->value.get())->value();
            hashes.set(hash, expirationTime);
            hashMap.set(hash, expirationTime);
            m_autoStartHashes.set(hash, it->key);
        }

        m_autoStartTable.set(it->key, hashes);
    }

    m_context->sendToAllProcesses(Messages::WebProcess::ResetPlugInAutoStartOrigins(hashMap));
}

void PlugInAutoStartProvider::didReceiveUserInteraction(unsigned plugInOriginHash)
{
    HashMap<unsigned, String>::const_iterator it = m_autoStartHashes.find(plugInOriginHash);
    if (it == m_autoStartHashes.end()) {
        ASSERT_NOT_REACHED();
        return;
    }

    double newExpirationTime = expirationTimeFromNow();
    m_autoStartTable.find(it->value)->value.set(plugInOriginHash, newExpirationTime);
    m_context->sendToAllProcesses(Messages::WebProcess::DidAddPlugInAutoStartOrigin(plugInOriginHash, newExpirationTime));
    m_context->client().plugInAutoStartOriginHashesChanged(m_context);
}

} // namespace WebKit
