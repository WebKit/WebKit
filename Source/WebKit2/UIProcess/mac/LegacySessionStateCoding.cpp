/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "LegacySessionStateCoding.h"

#include "APIData.h"
#include "SessionState.h"
#include <wtf/cf/TypeCasts.h>

namespace WebKit {

// Session state keys.
static const uint32_t sessionStateDataVersion = 2;

static const CFStringRef sessionHistoryKey = CFSTR("SessionHistory");
static const CFStringRef provisionalURLKey = CFSTR("ProvisionalURL");

// Session history keys.
static const CFStringRef sessionHistoryVersionKey = CFSTR("SessionHistoryVersion");
static const CFStringRef sessionHistoryCurrentIndexKey = CFSTR("SessionHistoryCurrentIndex");
static const CFStringRef sessionHistoryEntriesKey = CFSTR("SessionHistoryEntries");

LegacySessionStateDecoder::LegacySessionStateDecoder(API::Data* data)
    : m_data(data)
{
}

LegacySessionStateDecoder::~LegacySessionStateDecoder()
{
}

bool LegacySessionStateDecoder::decodeSessionState(SessionState& sessionState) const
{
    if (!m_data)
        return false;

    size_t size = m_data->size();
    const uint8_t* bytes = m_data->bytes();

    if (size < sizeof(uint32_t))
        return false;

    uint32_t versionNumber = (bytes[0] << 24) + (bytes[1] << 16) + (bytes[2] << 8) + bytes[3];

    if (versionNumber != sessionStateDataVersion)
        return false;

    auto data = adoptCF(CFDataCreate(kCFAllocatorDefault, bytes + sizeof(uint32_t), size - sizeof(uint32_t)));

    auto sessionStateDictionary = adoptCF(dynamic_cf_cast<CFDictionaryRef>(CFPropertyListCreateWithData(kCFAllocatorDefault, data.get(), kCFPropertyListImmutable, nullptr, nullptr)));
    if (!sessionStateDictionary)
        return false;

    if (auto backForwardListDictionary = dynamic_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(sessionStateDictionary.get(), sessionHistoryKey))) {
        if (!decodeSessionHistory(backForwardListDictionary, sessionState.backForwardListState))
            return false;
    }

    if (auto provisionalURLString = dynamic_cf_cast<CFStringRef>(CFDictionaryGetValue(sessionStateDictionary.get(), provisionalURLKey))) {
        sessionState.provisionalURL = WebCore::URL(WebCore::URL(), provisionalURLString);
        if (!sessionState.provisionalURL.isValid())
            return false;
    }

    return true;
}

bool LegacySessionStateDecoder::decodeSessionHistory(CFDictionaryRef backForwardListDictionary, BackForwardListState& backForwardListState) const
{
    auto sessionHistoryVersionNumber = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(backForwardListDictionary, sessionHistoryVersionKey));
    if (!sessionHistoryVersionNumber) {
        // Version 0 session history dictionaries did not contain a version number.
        return decodeV0SessionHistory(backForwardListDictionary, backForwardListState);
    }

    CFIndex sessionHistoryVersion;
    if (!CFNumberGetValue(sessionHistoryVersionNumber, kCFNumberCFIndexType, &sessionHistoryVersion))
        return false;

    if (sessionHistoryVersion == 1)
        return decodeV1SessionHistory(backForwardListDictionary, backForwardListState);

    return false;
}

bool LegacySessionStateDecoder::decodeV0SessionHistory(CFDictionaryRef sessionHistoryDictionary, BackForwardListState& backForwardListState) const
{
    // FIXME: Implement.
    return false;
}

bool LegacySessionStateDecoder::decodeV1SessionHistory(CFDictionaryRef sessionHistoryDictionary, BackForwardListState& backForwardListState) const
{
    auto currentIndexNumber = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(sessionHistoryDictionary, sessionHistoryCurrentIndexKey));
    if (!currentIndexNumber) {
        // No current index means the dictionary represents an empty session.
        backForwardListState.currentIndex = 0;
        backForwardListState.items = { };
        return true;
    }

    CFIndex currentIndex;
    if (!CFNumberGetValue(currentIndexNumber, kCFNumberCFIndexType, &currentIndex))
        return false;

    if (currentIndex < 0)
        return false;

    auto historyEntries = dynamic_cf_cast<CFArrayRef>(CFDictionaryGetValue(sessionHistoryDictionary, sessionHistoryEntriesKey));
    if (!historyEntries)
        return false;

    if (!decodeSessionHistoryEntries(historyEntries, backForwardListState.items))
        return false;

    backForwardListState.currentIndex = static_cast<uint32_t>(currentIndex);
    if (backForwardListState.currentIndex >= backForwardListState.items.size())
        return false;

    return true;
}

bool LegacySessionStateDecoder::decodeSessionHistoryEntries(CFArrayRef entriesArray, Vector<PageState>& entries) const
{
    for (CFIndex i = 0, size = CFArrayGetCount(entriesArray); i < size; ++i) {
        auto entryDictionary = dynamic_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(entriesArray, i));
        if (!entryDictionary)
            return false;

        PageState entry;
        if (!decodeSessionHistoryEntry(entryDictionary, entry))
            return false;

        entries.append(std::move(entry));
    }

    return false;
}

bool LegacySessionStateDecoder::decodeSessionHistoryEntry(CFDictionaryRef entryDictionary, PageState& pageState) const
{
    // FIXME: Implement this.
    return false;
}


} // namespace WebKit
