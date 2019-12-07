/*
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
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

#include "APIArray.h"
#include "APIDictionary.h"
#include "WebContextClient.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include <wtf/WallTime.h>

namespace WebKit {
using namespace WebCore;

static const Seconds plugInAutoStartExpirationTimeThreshold { 30 * 24 * 60 * 60 };

PlugInAutoStartProvider::PlugInAutoStartProvider(WebProcessPool* processPool)
    : m_processPool(processPool)
{
    m_hashToOriginMap.add(PAL::SessionID::defaultSessionID(), HashToOriginMap());
    m_autoStartTable.add(PAL::SessionID::defaultSessionID(), AutoStartTable());
}

static WallTime expirationTimeFromNow()
{
    return WallTime::now() + plugInAutoStartExpirationTimeThreshold;
}

void PlugInAutoStartProvider::addAutoStartOriginHash(const String& pageOrigin, unsigned plugInOriginHash, PAL::SessionID sessionID)
{
    auto sessionIterator = m_hashToOriginMap.find(sessionID);
    if (sessionIterator == m_hashToOriginMap.end()) {
        if (m_hashToOriginMap.get(PAL::SessionID::defaultSessionID()).contains(plugInOriginHash))
            return;
        sessionIterator = m_hashToOriginMap.set(sessionID, HashToOriginMap()).iterator;
    } else if (sessionIterator->value.contains(plugInOriginHash) || m_hashToOriginMap.get(PAL::SessionID::defaultSessionID()).contains(plugInOriginHash))
        return;

    AutoStartTable::iterator it = m_autoStartTable.add(sessionID, AutoStartTable()).iterator->value.add(pageOrigin, PlugInAutoStartOriginMap()).iterator;

    WallTime expirationTime = expirationTimeFromNow();
    it->value.set(plugInOriginHash, expirationTime);
    sessionIterator->value.set(plugInOriginHash, pageOrigin);

    m_processPool->sendToAllProcessesForSession(Messages::WebProcess::DidAddPlugInAutoStartOriginHash(plugInOriginHash, expirationTime), sessionID);

    if (!sessionID.isEphemeral())
        m_processPool->client().plugInAutoStartOriginHashesChanged(m_processPool);
}

PlugInAutoStartOriginMap PlugInAutoStartProvider::autoStartOriginHashesCopy(PAL::SessionID sessionID) const
{
    PlugInAutoStartOriginMap map;

    auto it = m_autoStartTable.find(sessionID);
    if (it != m_autoStartTable.end()) {
        for (const auto& keyOriginHash : it->value) {
            for (const auto& originHash : keyOriginHash.value)
                map.set(originHash.key, originHash.value);
        }
    }
    return map;
}

Ref<API::Dictionary> PlugInAutoStartProvider::autoStartOriginsTableCopy() const
{
    API::Dictionary::MapType map;

    WallTime now = WallTime::now();
    for (const auto& stringOriginHash : m_autoStartTable.get(PAL::SessionID::defaultSessionID())) {
        API::Dictionary::MapType hashMap;
        for (const auto& originHash : stringOriginHash.value) {
            if (now <= originHash.value)
                hashMap.set(String::number(originHash.key), API::Double::create(originHash.value.secondsSinceEpoch().seconds()));
        }
        if (hashMap.size())
            map.set(stringOriginHash.key, API::Dictionary::create(WTFMove(hashMap)));
    }

    return API::Dictionary::create(WTFMove(map));
}

void PlugInAutoStartProvider::setAutoStartOriginsTable(API::Dictionary& table)
{
    setAutoStartOriginsTableWithItemsPassingTest(table, [](WallTime) {
        return true;
    });
}

void PlugInAutoStartProvider::setAutoStartOriginsFilteringOutEntriesAddedAfterTime(API::Dictionary& table, WallTime time)
{
    WallTime adjustedTimestamp = time + plugInAutoStartExpirationTimeThreshold;
    setAutoStartOriginsTableWithItemsPassingTest(table, [adjustedTimestamp](WallTime expirationTimestamp) {
        return adjustedTimestamp > expirationTimestamp;
    });
}

void PlugInAutoStartProvider::setAutoStartOriginsTableWithItemsPassingTest(API::Dictionary& table, WTF::Function<bool(WallTime expirationTimestamp)>&& isExpirationTimeAcceptable)
{
    ASSERT(isExpirationTimeAcceptable);

    m_hashToOriginMap.clear();
    m_autoStartTable.clear();
    HashMap<unsigned, WallTime> hashMap;
    auto& hashToOriginMap = m_hashToOriginMap.add(PAL::SessionID::defaultSessionID(), HashToOriginMap()).iterator->value;
    AutoStartTable& ast = m_autoStartTable.add(PAL::SessionID::defaultSessionID(), AutoStartTable()).iterator->value;

    for (auto& strDict : table.map()) {
        PlugInAutoStartOriginMap hashes;
        for (auto& hashTime : static_cast<API::Dictionary*>(strDict.value.get())->map()) {
            bool ok;
            unsigned hash = hashTime.key.toUInt(&ok);
            if (!ok)
                continue;

            if (hashTime.value->type() != API::Double::APIType)
                continue;

            WallTime expirationTime = WallTime::fromRawSeconds(static_cast<API::Double*>(hashTime.value.get())->value());
            if (!isExpirationTimeAcceptable(expirationTime))
                continue;

            hashes.set(hash, expirationTime);
            hashMap.set(hash, expirationTime);
            hashToOriginMap.set(hash, strDict.key);
        }

        if (!hashes.isEmpty())
            ast.set(strDict.key, hashes);
    }

    m_processPool->sendToAllProcessesForSession(Messages::WebProcess::ResetPlugInAutoStartOriginHashes(hashMap), PAL::SessionID::defaultSessionID());
}

void PlugInAutoStartProvider::setAutoStartOriginsArray(API::Array& originList)
{
    m_autoStartOrigins.clear();
    for (auto string : originList.elementsOfType<API::String>())
        m_autoStartOrigins.append(string->string());
}

void PlugInAutoStartProvider::didReceiveUserInteraction(unsigned plugInOriginHash, PAL::SessionID sessionID)
{
    auto sessionIterator = m_hashToOriginMap.find(sessionID);
    HashToOriginMap::const_iterator it;
    bool contains = false;
    if (sessionIterator != m_hashToOriginMap.end()) {
        it = sessionIterator->value.find(plugInOriginHash);
        contains = it != sessionIterator->value.end();
    }
    if (!contains) {
        sessionIterator = m_hashToOriginMap.find(PAL::SessionID::defaultSessionID());
        it = sessionIterator->value.find(plugInOriginHash);
        if (it == sessionIterator->value.end()) {
            ASSERT_NOT_REACHED();
            return;
        }
    }

    WallTime newExpirationTime = expirationTimeFromNow();
    m_autoStartTable.add(sessionID, AutoStartTable()).iterator->value.add(it->value, PlugInAutoStartOriginMap()).iterator->value.set(plugInOriginHash, newExpirationTime);
    m_processPool->sendToAllProcessesForSession(Messages::WebProcess::DidAddPlugInAutoStartOriginHash(plugInOriginHash, newExpirationTime), sessionID);
    m_processPool->client().plugInAutoStartOriginHashesChanged(m_processPool);
}

} // namespace WebKit
