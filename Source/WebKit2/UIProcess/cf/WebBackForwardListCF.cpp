/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
#include "WebBackForwardList.h"
#include "Logging.h"
#include <wtf/RetainPtr.h>
#include <CoreFoundation/CoreFoundation.h>

using namespace WebCore;

namespace WebKit {

static uint64_t generateWebBackForwardItemID()
{
    // These IDs exist in the UIProcess for items created by the UIProcess.
    // The IDs generated here need to never collide with the IDs created in WebBackForwardListProxy in the WebProcess.
    // We accomplish this by starting from 2, and only ever using even ids.
    static uint64_t uniqueHistoryItemID = 0;
    uniqueHistoryItemID += 2;
    return uniqueHistoryItemID;
}

DEFINE_STATIC_GETTER(CFStringRef, SessionHistoryCurrentIndexKey, (CFSTR("SessionHistoryCurrentIndex")));
DEFINE_STATIC_GETTER(CFStringRef, SessionHistoryEntriesKey, (CFSTR("SessionHistoryEntries")));
DEFINE_STATIC_GETTER(CFStringRef, SessionHistoryEntryTitleKey, (CFSTR("SessionHistoryEntryTitle")));
DEFINE_STATIC_GETTER(CFStringRef, SessionHistoryEntryURLKey, (CFSTR("SessionHistoryEntryURL")));
DEFINE_STATIC_GETTER(CFStringRef, SessionHistoryEntryOriginalURLKey, (CFSTR("SessionHistoryEntryOriginalURL")));
DEFINE_STATIC_GETTER(CFStringRef, SessionHistoryEntryDataKey, (CFSTR("SessionHistoryEntryData")));

CFDictionaryRef WebBackForwardList::createCFDictionaryRepresentation(WebPageProxy::WebPageProxySessionStateFilterCallback filter, void* context) const
{
    ASSERT(m_current == NoCurrentItemIndex || m_current < m_entries.size());

    RetainPtr<CFMutableArrayRef> entries(AdoptCF, CFArrayCreateMutable(0, m_entries.size(), &kCFTypeArrayCallBacks));

    // We may need to update the current index to account for entries that are filtered by the callback.
    int currentIndex = m_current;

    for (size_t i = 0; i < m_entries.size(); ++i) {
        RefPtr<WebURL> webURL = WebURL::create(m_entries[i]->url());
        if (filter && !filter(toAPI(m_page), WKPageGetSessionHistoryURLValueType(), toURLRef(m_entries[i]->originalURL().impl()), context)) {
            if (i <= static_cast<size_t>(m_current))
                currentIndex--;
            continue;
        }
        
        RetainPtr<CFStringRef> url(AdoptCF, m_entries[i]->url().createCFString());
        RetainPtr<CFStringRef> title(AdoptCF, m_entries[i]->title().createCFString());
        RetainPtr<CFStringRef> originalURL(AdoptCF, m_entries[i]->originalURL().createCFString());
        RetainPtr<CFDataRef> entryData(AdoptCF, CFDataCreate(kCFAllocatorDefault, m_entries[i]->backForwardData().data(), m_entries[i]->backForwardData().size()));
        
        const void* keys[4] = { SessionHistoryEntryURLKey(), SessionHistoryEntryTitleKey(), SessionHistoryEntryOriginalURLKey(), SessionHistoryEntryDataKey() };
        const void* values[4] = { url.get(), title.get(), originalURL.get(), entryData.get() };

        RetainPtr<CFDictionaryRef> entryDictionary(AdoptCF, CFDictionaryCreate(0, keys, values, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFArrayAppendValue(entries.get(), entryDictionary.get());
    }

    ASSERT(currentIndex < CFArrayGetCount(entries.get()));
    RetainPtr<CFNumberRef> currentIndexNumber(AdoptCF, CFNumberCreate(0, kCFNumberIntType, &currentIndex));

    const void* keys[2] = { SessionHistoryCurrentIndexKey(), SessionHistoryEntriesKey() };
    const void* values[2] = { currentIndexNumber.get(), entries.get() };

    return CFDictionaryCreate(0, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

bool WebBackForwardList::restoreFromCFDictionaryRepresentation(CFDictionaryRef dictionary)
{
    CFNumberRef cfIndex = (CFNumberRef)CFDictionaryGetValue(dictionary, SessionHistoryCurrentIndexKey());
    if (!cfIndex || CFGetTypeID(cfIndex) != CFNumberGetTypeID()) {
        LOG(SessionState, "WebBackForwardList dictionary representation does not have a valid current index");
        return false;
    }

    CFIndex currentIndex;
    if (!CFNumberGetValue(cfIndex, kCFNumberCFIndexType, &currentIndex)) {
        LOG(SessionState, "WebBackForwardList dictionary representation does not have a valid integer current index");
        return false;
    }

    CFArrayRef cfEntries = (CFArrayRef)CFDictionaryGetValue(dictionary, SessionHistoryEntriesKey());
    if (!cfEntries || CFGetTypeID(cfEntries) != CFArrayGetTypeID()) {
        LOG(SessionState, "WebBackForwardList dictionary representation does not have a valid list of entries");
        return false;
    }

    CFIndex size = CFArrayGetCount(cfEntries);
    if (currentIndex != static_cast<CFIndex>(NoCurrentItemIndex) && currentIndex >= size) {
        LOG(SessionState, "WebBackForwardList dictionary representation contains an invalid current index (%ld) for the number of entries (%ld)", currentIndex, size);
        return false;
    }

    if (currentIndex == static_cast<CFIndex>(NoCurrentItemIndex) && size) {
        LOG(SessionState, "WebBackForwardList dictionary representation says there is no current item index, but there is a list of %ld entries - this is bogus", size);
        return false;
    }
    
    BackForwardListItemVector newEntries;
    newEntries.reserveCapacity(size);
    for (CFIndex i = 0; i < size; ++i) {
        CFDictionaryRef entryDictionary = (CFDictionaryRef)CFArrayGetValueAtIndex(cfEntries, i);
        if (!entryDictionary || CFGetTypeID(entryDictionary) != CFDictionaryGetTypeID()) {
            LOG(SessionState, "WebBackForwardList entry array does not have a valid entry at index %i", (int)i);
            return false;
        }
        
        CFStringRef entryURL = (CFStringRef)CFDictionaryGetValue(entryDictionary, SessionHistoryEntryURLKey());
        if (!entryURL || CFGetTypeID(entryURL) != CFStringGetTypeID()) {
            LOG(SessionState, "WebBackForwardList entry at index %i does not have a valid URL", (int)i);
            return false;
        }

        CFStringRef entryTitle = (CFStringRef)CFDictionaryGetValue(entryDictionary, SessionHistoryEntryTitleKey());
        if (!entryTitle || CFGetTypeID(entryTitle) != CFStringGetTypeID()) {
            LOG(SessionState, "WebBackForwardList entry at index %i does not have a valid title", (int)i);
            return false;
        }

        CFStringRef originalURL = (CFStringRef)CFDictionaryGetValue(entryDictionary, SessionHistoryEntryOriginalURLKey());
        if (!originalURL || CFGetTypeID(originalURL) != CFStringGetTypeID()) {
            LOG(SessionState, "WebBackForwardList entry at index %i does not have a valid original URL", (int)i);
            return false;
        }

        CFDataRef backForwardData = (CFDataRef)CFDictionaryGetValue(entryDictionary, SessionHistoryEntryDataKey());
        if (!backForwardData || CFGetTypeID(backForwardData) != CFDataGetTypeID()) {
            LOG(SessionState, "WebBackForwardList entry at index %i does not have back/forward data", (int)i);
            return false;
        }
        
        newEntries.append(WebBackForwardListItem::create(originalURL, entryURL, entryTitle, CFDataGetBytePtr(backForwardData), CFDataGetLength(backForwardData), generateWebBackForwardItemID()));
    }
    
    m_current = currentIndex;
    m_entries = newEntries;

    return true;
}

} // namespace WebKit
