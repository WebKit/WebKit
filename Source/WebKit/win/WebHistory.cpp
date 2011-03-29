/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "WebKitDLL.h"
#include "WebHistory.h"

#include "CFDictionaryPropertyBag.h"
#include "MemoryStream.h"
#include "WebKit.h"
#include "MarshallingHelpers.h"
#include "WebHistoryItem.h"
#include "WebKit.h"
#include "WebNotificationCenter.h"
#include "WebPreferences.h"
#include <CoreFoundation/CoreFoundation.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/HistoryPropertyList.h>
#include <WebCore/KURL.h>
#include <WebCore/PageGroup.h>
#include <WebCore/SharedBuffer.h>
#include <functional>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

using namespace WebCore;
using namespace std;

CFStringRef DatesArrayKey = CFSTR("WebHistoryDates");
CFStringRef FileVersionKey = CFSTR("WebHistoryFileVersion");

#define currentFileVersion 1

class WebHistoryWriter : public HistoryPropertyListWriter {
public:
    WebHistoryWriter(const WebHistory::DateToEntriesMap&);

private:
    virtual void writeHistoryItems(BinaryPropertyListObjectStream&);

    const WebHistory::DateToEntriesMap& m_entriesByDate;
    Vector<WebHistory::DateKey> m_dateKeys;
};

WebHistoryWriter::WebHistoryWriter(const WebHistory::DateToEntriesMap& entriesByDate)
    : m_entriesByDate(entriesByDate)
{
    copyKeysToVector(m_entriesByDate, m_dateKeys);
    sort(m_dateKeys.begin(), m_dateKeys.end());
}

void WebHistoryWriter::writeHistoryItems(BinaryPropertyListObjectStream& stream)
{
    for (int dateIndex = m_dateKeys.size() - 1; dateIndex >= 0; --dateIndex) {
        // get the entries for that date
        CFArrayRef entries = m_entriesByDate.get(m_dateKeys[dateIndex]).get();
        CFIndex entriesCount = CFArrayGetCount(entries);
        for (CFIndex j = entriesCount - 1; j >= 0; --j) {
            IWebHistoryItem* item = (IWebHistoryItem*) CFArrayGetValueAtIndex(entries, j);
            COMPtr<WebHistoryItem> webItem(Query, item);
            if (!webItem)
                continue;

            writeHistoryItem(stream, webItem->historyItem());
        }
    }
}

static bool areEqualOrClose(double d1, double d2)
{
    double diff = d1-d2;
    return (diff < .000001 && diff > -.000001);
}

static COMPtr<CFDictionaryPropertyBag> createUserInfoFromArray(BSTR notificationStr, CFArrayRef arrayItem)
{
    RetainPtr<CFMutableDictionaryRef> dictionary(AdoptCF, 
        CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    RetainPtr<CFStringRef> key(AdoptCF, MarshallingHelpers::BSTRToCFStringRef(notificationStr));
    CFDictionaryAddValue(dictionary.get(), key.get(), arrayItem);

    COMPtr<CFDictionaryPropertyBag> result = CFDictionaryPropertyBag::createInstance();
    result->setDictionary(dictionary.get());
    return result;
}

static COMPtr<CFDictionaryPropertyBag> createUserInfoFromHistoryItem(BSTR notificationStr, IWebHistoryItem* item)
{
    // reference counting of item added to the array is managed by the CFArray value callbacks
    RetainPtr<CFArrayRef> itemList(AdoptCF, CFArrayCreate(0, (const void**) &item, 1, &MarshallingHelpers::kIUnknownArrayCallBacks));
    COMPtr<CFDictionaryPropertyBag> info = createUserInfoFromArray(notificationStr, itemList.get());
    return info;
}

// WebHistory -----------------------------------------------------------------

WebHistory::WebHistory()
: m_refCount(0)
, m_preferences(0)
{
    gClassCount++;
    gClassNameCount.add("WebHistory");

    m_entriesByURL.adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &MarshallingHelpers::kIUnknownDictionaryValueCallBacks));

    m_preferences = WebPreferences::sharedStandardPreferences();
}

WebHistory::~WebHistory()
{
    gClassCount--;
    gClassNameCount.remove("WebHistory");
}

WebHistory* WebHistory::createInstance()
{
    WebHistory* instance = new WebHistory();
    instance->AddRef();
    return instance;
}

HRESULT WebHistory::postNotification(NotificationType notifyType, IPropertyBag* userInfo /*=0*/)
{
    IWebNotificationCenter* nc = WebNotificationCenter::defaultCenterInternal();
    HRESULT hr = nc->postNotificationName(getNotificationString(notifyType), static_cast<IWebHistory*>(this), userInfo);
    if (FAILED(hr))
        return hr;

    return S_OK;
}

BSTR WebHistory::getNotificationString(NotificationType notifyType)
{
    static BSTR keys[6] = {0};
    if (!keys[0]) {
        keys[0] = SysAllocString(WebHistoryItemsAddedNotification);
        keys[1] = SysAllocString(WebHistoryItemsRemovedNotification);
        keys[2] = SysAllocString(WebHistoryAllItemsRemovedNotification);
        keys[3] = SysAllocString(WebHistoryLoadedNotification);
        keys[4] = SysAllocString(WebHistoryItemsDiscardedWhileLoadingNotification);
        keys[5] = SysAllocString(WebHistorySavedNotification);
    }
    return keys[notifyType];
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebHistory::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, CLSID_WebHistory))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebHistory*>(this);
    else if (IsEqualGUID(riid, IID_IWebHistory))
        *ppvObject = static_cast<IWebHistory*>(this);
    else if (IsEqualGUID(riid, IID_IWebHistoryPrivate))
        *ppvObject = static_cast<IWebHistoryPrivate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebHistory::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebHistory::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebHistory ----------------------------------------------------------------

static inline COMPtr<WebHistory>& sharedHistoryStorage()
{
    DEFINE_STATIC_LOCAL(COMPtr<WebHistory>, sharedHistory, ());
    return sharedHistory;
}

WebHistory* WebHistory::sharedHistory()
{
    return sharedHistoryStorage().get();
}

HRESULT STDMETHODCALLTYPE WebHistory::optionalSharedHistory( 
    /* [retval][out] */ IWebHistory** history)
{
    *history = sharedHistory();
    if (*history)
        (*history)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::setOptionalSharedHistory( 
    /* [in] */ IWebHistory* history)
{
    if (sharedHistoryStorage() == history)
        return S_OK;
    sharedHistoryStorage().query(history);
    PageGroup::setShouldTrackVisitedLinks(sharedHistoryStorage());
    PageGroup::removeAllVisitedLinks();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::loadFromURL( 
    /* [in] */ BSTR url,
    /* [out] */ IWebError** error,
    /* [retval][out] */ BOOL* succeeded)
{
    HRESULT hr = S_OK;
    RetainPtr<CFMutableArrayRef> discardedItems(AdoptCF, 
        CFArrayCreateMutable(0, 0, &MarshallingHelpers::kIUnknownArrayCallBacks));

    RetainPtr<CFURLRef> urlRef(AdoptCF, MarshallingHelpers::BSTRToCFURLRef(url));

    hr = loadHistoryGutsFromURL(urlRef.get(), discardedItems.get(), error);
    if (FAILED(hr))
        goto exit;

    hr = postNotification(kWebHistoryLoadedNotification);
    if (FAILED(hr))
        goto exit;

    if (CFArrayGetCount(discardedItems.get()) > 0) {
        COMPtr<CFDictionaryPropertyBag> userInfo = createUserInfoFromArray(getNotificationString(kWebHistoryItemsDiscardedWhileLoadingNotification), discardedItems.get());
        hr = postNotification(kWebHistoryItemsDiscardedWhileLoadingNotification, userInfo.get());
        if (FAILED(hr))
            goto exit;
    }

exit:
    if (succeeded)
        *succeeded = SUCCEEDED(hr);
    return hr;
}

static CFDictionaryRef createHistoryListFromStream(CFReadStreamRef stream, CFPropertyListFormat format)
{
    return (CFDictionaryRef)CFPropertyListCreateFromStream(0, stream, 0, kCFPropertyListImmutable, &format, 0);
}

HRESULT WebHistory::loadHistoryGutsFromURL(CFURLRef url, CFMutableArrayRef discardedItems, IWebError** /*error*/) //FIXME
{
    CFPropertyListFormat format = kCFPropertyListBinaryFormat_v1_0 | kCFPropertyListXMLFormat_v1_0;
    HRESULT hr = S_OK;
    int numberOfItemsLoaded = 0;

    RetainPtr<CFReadStreamRef> stream(AdoptCF, CFReadStreamCreateWithFile(0, url));
    if (!stream) 
        return E_FAIL;

    if (!CFReadStreamOpen(stream.get())) 
        return E_FAIL;

    RetainPtr<CFDictionaryRef> historyList(AdoptCF, createHistoryListFromStream(stream.get(), format));
    CFReadStreamClose(stream.get());

    if (!historyList) 
        return E_FAIL;

    CFNumberRef fileVersionObject = (CFNumberRef)CFDictionaryGetValue(historyList.get(), FileVersionKey);
    int fileVersion;
    if (!CFNumberGetValue(fileVersionObject, kCFNumberIntType, &fileVersion)) 
        return E_FAIL;

    if (fileVersion > currentFileVersion) 
        return E_FAIL;
    
    CFArrayRef datesArray = (CFArrayRef)CFDictionaryGetValue(historyList.get(), DatesArrayKey);

    int itemCountLimit;
    hr = historyItemLimit(&itemCountLimit);
    if (FAILED(hr))
        return hr;

    CFAbsoluteTime limitDate;
    hr = ageLimitDate(&limitDate);
    if (FAILED(hr))
        return hr;

    bool ageLimitPassed = false;
    bool itemLimitPassed = false;

    CFIndex itemCount = CFArrayGetCount(datesArray);
    for (CFIndex i = 0; i < itemCount; ++i) {
        CFDictionaryRef itemAsDictionary = (CFDictionaryRef)CFArrayGetValueAtIndex(datesArray, i);
        COMPtr<WebHistoryItem> item(AdoptCOM, WebHistoryItem::createInstance());
        hr = item->initFromDictionaryRepresentation((void*)itemAsDictionary);
        if (FAILED(hr))
            return hr;

        // item without URL is useless; data on disk must have been bad; ignore
        BOOL hasURL;
        hr = item->hasURLString(&hasURL);
        if (FAILED(hr))
            return hr;
        
        if (hasURL) {
            // Test against date limit. Since the items are ordered newest to oldest, we can stop comparing
            // once we've found the first item that's too old.
            if (!ageLimitPassed) {
                DATE lastVisitedTime;
                hr = item->lastVisitedTimeInterval(&lastVisitedTime);
                if (FAILED(hr))
                    return hr;
                if (timeToDate(MarshallingHelpers::DATEToCFAbsoluteTime(lastVisitedTime)) <= limitDate)
                    ageLimitPassed = true;
            }
            if (ageLimitPassed || itemLimitPassed)
                CFArrayAppendValue(discardedItems, item.get());
            else {
                bool added;
                addItem(item.get(), true, &added); // ref is added inside addItem
                if (added)
                    ++numberOfItemsLoaded;
                if (numberOfItemsLoaded == itemCountLimit)
                    itemLimitPassed = true;
            }
        }
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE WebHistory::saveToURL( 
    /* [in] */ BSTR url,
    /* [out] */ IWebError** error,
    /* [retval][out] */ BOOL* succeeded)
{
    HRESULT hr = S_OK;
    RetainPtr<CFURLRef> urlRef(AdoptCF, MarshallingHelpers::BSTRToCFURLRef(url));

    hr = saveHistoryGuts(urlRef.get(), error);

    if (succeeded)
        *succeeded = SUCCEEDED(hr);
    if (SUCCEEDED(hr))
        hr = postNotification(kWebHistorySavedNotification);

    return hr;
}

HRESULT WebHistory::saveHistoryGuts(CFURLRef url, IWebError** error)
{
    HRESULT hr = S_OK;

    // FIXME: Correctly report error when new API is ready.
    if (error)
        *error = 0;

    RetainPtr<CFDataRef> data = this->data();

    RetainPtr<CFWriteStreamRef> stream(AdoptCF, CFWriteStreamCreateWithFile(kCFAllocatorDefault, url));
    if (!stream) 
        return E_FAIL;

    if (!CFWriteStreamOpen(stream.get())) 
        return E_FAIL;

    const UInt8* dataPtr = CFDataGetBytePtr(data.get());
    CFIndex length = CFDataGetLength(data.get());

    while (length) {
        CFIndex bytesWritten = CFWriteStreamWrite(stream.get(), dataPtr, length);
        if (bytesWritten <= 0) {
            hr = E_FAIL;
            break;
        }
        dataPtr += bytesWritten;
        length -= bytesWritten;
    }

    CFWriteStreamClose(stream.get());

    return hr;
}

HRESULT STDMETHODCALLTYPE WebHistory::addItems( 
    /* [in] */ int itemCount,
    /* [in] */ IWebHistoryItem** items)
{
    // There is no guarantee that the incoming entries are in any particular
    // order, but if this is called with a set of entries that were created by
    // iterating through the results of orderedLastVisitedDays and orderedItemsLastVisitedOnDay
    // then they will be ordered chronologically from newest to oldest. We can make adding them
    // faster (fewer compares) by inserting them from oldest to newest.

    HRESULT hr;
    for (int i = itemCount - 1; i >= 0; --i) {
        hr = addItem(items[i], false, 0);
        if (FAILED(hr))
            return hr;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::removeItems( 
    /* [in] */ int itemCount,
    /* [in] */ IWebHistoryItem** items)
{
    HRESULT hr;
    for (int i = 0; i < itemCount; ++i) {
        hr = removeItem(items[i]);
        if (FAILED(hr))
            return hr;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::removeAllItems( void)
{
    m_entriesByDate.clear();
    m_orderedLastVisitedDays.clear();

    CFIndex itemCount = CFDictionaryGetCount(m_entriesByURL.get());
    Vector<IWebHistoryItem*> itemsVector(itemCount);
    CFDictionaryGetKeysAndValues(m_entriesByURL.get(), 0, (const void**)itemsVector.data());
    RetainPtr<CFArrayRef> allItems(AdoptCF, CFArrayCreate(kCFAllocatorDefault, (const void**)itemsVector.data(), itemCount, &MarshallingHelpers::kIUnknownArrayCallBacks));

    CFDictionaryRemoveAllValues(m_entriesByURL.get());

    PageGroup::removeAllVisitedLinks();

    COMPtr<CFDictionaryPropertyBag> userInfo = createUserInfoFromArray(getNotificationString(kWebHistoryAllItemsRemovedNotification), allItems.get());
    return postNotification(kWebHistoryAllItemsRemovedNotification, userInfo.get());
}

HRESULT STDMETHODCALLTYPE WebHistory::orderedLastVisitedDays( 
    /* [out][in] */ int* count,
    /* [in] */ DATE* calendarDates)
{
    int dateCount = m_entriesByDate.size();
    if (!calendarDates) {
        *count = dateCount;
        return S_OK;
    }

    if (*count < dateCount) {
        *count = dateCount;
        return E_FAIL;
    }

    *count = dateCount;
    if (!m_orderedLastVisitedDays) {
        m_orderedLastVisitedDays = adoptArrayPtr(new DATE[dateCount]);
        DateToEntriesMap::const_iterator::Keys end = m_entriesByDate.end().keys();
        int i = 0;
        for (DateToEntriesMap::const_iterator::Keys it = m_entriesByDate.begin().keys(); it != end; ++it, ++i)
            m_orderedLastVisitedDays[i] = MarshallingHelpers::CFAbsoluteTimeToDATE(*it);
        // Use std::greater to sort the days in descending order (i.e., most-recent first).
        sort(m_orderedLastVisitedDays.get(), m_orderedLastVisitedDays.get() + dateCount, greater<DATE>());
    }

    memcpy(calendarDates, m_orderedLastVisitedDays.get(), dateCount * sizeof(DATE));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::orderedItemsLastVisitedOnDay( 
    /* [out][in] */ int* count,
    /* [in] */ IWebHistoryItem** items,
    /* [in] */ DATE calendarDate)
{
    DateKey dateKey;
    if (!findKey(&dateKey, MarshallingHelpers::DATEToCFAbsoluteTime(calendarDate))) {
        *count = 0;
        return 0;
    }

    CFArrayRef entries = m_entriesByDate.get(dateKey).get();
    if (!entries) {
        *count = 0;
        return 0;
    }

    int newCount = CFArrayGetCount(entries);

    if (!items) {
        *count = newCount;
        return S_OK;
    }

    if (*count < newCount) {
        *count = newCount;
        return E_FAIL;
    }

    *count = newCount;
    for (int i = 0; i < newCount; i++) {
        IWebHistoryItem* item = (IWebHistoryItem*)CFArrayGetValueAtIndex(entries, i);
        item->AddRef();
        items[i] = item;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::allItems( 
    /* [out][in] */ int* count,
    /* [out][retval] */ IWebHistoryItem** items)
{
    int entriesByURLCount = CFDictionaryGetCount(m_entriesByURL.get());

    if (!items) {
        *count = entriesByURLCount;
        return S_OK;
    }

    if (*count < entriesByURLCount) {
        *count = entriesByURLCount;
        return E_FAIL;
    }

    *count = entriesByURLCount;
    CFDictionaryGetKeysAndValues(m_entriesByURL.get(), 0, (const void**)items);
    for (int i = 0; i < entriesByURLCount; i++)
        items[i]->AddRef();

    return S_OK;
}

HRESULT WebHistory::data(IStream** stream)
{
    if (!stream)
        return E_POINTER;

    *stream = 0;

    RetainPtr<CFDataRef> historyData = data();
    if (!historyData)
        return S_OK;

    COMPtr<MemoryStream> result = MemoryStream::createInstance(SharedBuffer::wrapCFData(historyData.get()));
    return result.copyRefTo(stream);
}

HRESULT WebHistory::setVisitedLinkTrackingEnabled(BOOL visitedLinkTrackingEnabled)
{
    PageGroup::setShouldTrackVisitedLinks(visitedLinkTrackingEnabled);
    return S_OK;
}

HRESULT WebHistory::removeAllVisitedLinks()
{
    PageGroup::removeAllVisitedLinks();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::setHistoryItemLimit( 
    /* [in] */ int limit)
{
    if (!m_preferences)
        return E_FAIL;
    return m_preferences->setHistoryItemLimit(limit);
}

HRESULT STDMETHODCALLTYPE WebHistory::historyItemLimit( 
    /* [retval][out] */ int* limit)
{
    if (!m_preferences)
        return E_FAIL;
    return m_preferences->historyItemLimit(limit);
}

HRESULT STDMETHODCALLTYPE WebHistory::setHistoryAgeInDaysLimit( 
    /* [in] */ int limit)
{
    if (!m_preferences)
        return E_FAIL;
    return m_preferences->setHistoryAgeInDaysLimit(limit);
}

HRESULT STDMETHODCALLTYPE WebHistory::historyAgeInDaysLimit( 
    /* [retval][out] */ int* limit)
{
    if (!m_preferences)
        return E_FAIL;
    return m_preferences->historyAgeInDaysLimit(limit);
}

HRESULT WebHistory::removeItem(IWebHistoryItem* entry)
{
    HRESULT hr = S_OK;
    BSTR urlBStr = 0;

    hr = entry->URLString(&urlBStr);
    if (FAILED(hr))
        return hr;

    RetainPtr<CFStringRef> urlString(AdoptCF, MarshallingHelpers::BSTRToCFStringRef(urlBStr));
    SysFreeString(urlBStr);

    // If this exact object isn't stored, then make no change.
    // FIXME: Is this the right behavior if this entry isn't present, but another entry for the same URL is?
    // Maybe need to change the API to make something like removeEntryForURLString public instead.
    IWebHistoryItem *matchingEntry = (IWebHistoryItem*)CFDictionaryGetValue(m_entriesByURL.get(), urlString.get());
    if (matchingEntry != entry)
        return E_FAIL;

    hr = removeItemForURLString(urlString.get());
    if (FAILED(hr))
        return hr;

    COMPtr<CFDictionaryPropertyBag> userInfo = createUserInfoFromHistoryItem(
        getNotificationString(kWebHistoryItemsRemovedNotification), entry);
    hr = postNotification(kWebHistoryItemsRemovedNotification, userInfo.get());

    return hr;
}

HRESULT WebHistory::addItem(IWebHistoryItem* entry, bool discardDuplicate, bool* added)
{
    HRESULT hr = S_OK;

    if (!entry)
        return E_FAIL;

    BSTR urlBStr = 0;
    hr = entry->URLString(&urlBStr);
    if (FAILED(hr))
        return hr;

    RetainPtr<CFStringRef> urlString(AdoptCF, MarshallingHelpers::BSTRToCFStringRef(urlBStr));
    SysFreeString(urlBStr);

    COMPtr<IWebHistoryItem> oldEntry((IWebHistoryItem*) CFDictionaryGetValue(
        m_entriesByURL.get(), urlString.get()));
    
    if (oldEntry) {
        if (discardDuplicate) {
            if (added)
                *added = false;
            return S_OK;
        }
        
        removeItemForURLString(urlString.get());

        // If we already have an item with this URL, we need to merge info that drives the
        // URL autocomplete heuristics from that item into the new one.
        IWebHistoryItemPrivate* entryPriv;
        hr = entry->QueryInterface(IID_IWebHistoryItemPrivate, (void**)&entryPriv);
        if (SUCCEEDED(hr)) {
            entryPriv->mergeAutoCompleteHints(oldEntry.get());
            entryPriv->Release();
        }
    }

    hr = addItemToDateCaches(entry);
    if (FAILED(hr))
        return hr;

    CFDictionarySetValue(m_entriesByURL.get(), urlString.get(), entry);

    COMPtr<CFDictionaryPropertyBag> userInfo = createUserInfoFromHistoryItem(
        getNotificationString(kWebHistoryItemsAddedNotification), entry);
    hr = postNotification(kWebHistoryItemsAddedNotification, userInfo.get());

    if (added)
        *added = true;

    return hr;
}

void WebHistory::visitedURL(const KURL& url, const String& title, const String& httpMethod, bool wasFailure, bool increaseVisitCount)
{
    RetainPtr<CFStringRef> urlString(AdoptCF, url.string().createCFString());

    IWebHistoryItem* entry = (IWebHistoryItem*) CFDictionaryGetValue(m_entriesByURL.get(), urlString.get());
    if (entry) {
        COMPtr<IWebHistoryItemPrivate> entryPrivate(Query, entry);
        if (!entryPrivate)
            return;

        // Remove the item from date caches before changing its last visited date.  Otherwise we might get duplicate entries
        // as seen in <rdar://problem/6570573>.
        removeItemFromDateCaches(entry);
        entryPrivate->visitedWithTitle(BString(title), increaseVisitCount);
    } else {
        COMPtr<WebHistoryItem> item(AdoptCOM, WebHistoryItem::createInstance());
        if (!item)
            return;

        entry = item.get();

        SYSTEMTIME currentTime;
        GetSystemTime(&currentTime);
        DATE lastVisited;
        if (!SystemTimeToVariantTime(&currentTime, &lastVisited))
            return;

        if (FAILED(entry->initWithURLString(BString(url.string()), BString(title), lastVisited)))
            return;
        
        item->recordInitialVisit();

        CFDictionarySetValue(m_entriesByURL.get(), urlString.get(), entry);
    }

    addItemToDateCaches(entry);

    COMPtr<IWebHistoryItemPrivate> entryPrivate(Query, entry);
    if (!entryPrivate)
        return;

    entryPrivate->setLastVisitWasFailure(wasFailure);
    if (!httpMethod.isEmpty())
        entryPrivate->setLastVisitWasHTTPNonGet(!equalIgnoringCase(httpMethod, "GET") && url.protocolInHTTPFamily());

    COMPtr<WebHistoryItem> item(Query, entry);
    item->historyItem()->setRedirectURLs(0);

    COMPtr<CFDictionaryPropertyBag> userInfo = createUserInfoFromHistoryItem(
        getNotificationString(kWebHistoryItemsAddedNotification), entry);
    postNotification(kWebHistoryItemsAddedNotification, userInfo.get());
}

HRESULT WebHistory::itemForURLString(
    /* [in] */ CFStringRef urlString,
    /* [retval][out] */ IWebHistoryItem** item) const
{
    if (!item)
        return E_FAIL;
    *item = 0;

    IWebHistoryItem* foundItem = (IWebHistoryItem*) CFDictionaryGetValue(m_entriesByURL.get(), urlString);
    if (!foundItem)
        return E_FAIL;

    foundItem->AddRef();
    *item = foundItem;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::itemForURL( 
    /* [in] */ BSTR url,
    /* [retval][out] */ IWebHistoryItem** item)
{
    RetainPtr<CFStringRef> urlString(AdoptCF, MarshallingHelpers::BSTRToCFStringRef(url));
    return itemForURLString(urlString.get(), item);
}

HRESULT WebHistory::removeItemForURLString(CFStringRef urlString)
{
    IWebHistoryItem* entry = (IWebHistoryItem*) CFDictionaryGetValue(m_entriesByURL.get(), urlString);
    if (!entry) 
        return E_FAIL;

    HRESULT hr = removeItemFromDateCaches(entry);
    CFDictionaryRemoveValue(m_entriesByURL.get(), urlString);

    if (!CFDictionaryGetCount(m_entriesByURL.get()))
        PageGroup::removeAllVisitedLinks();

    return hr;
}

COMPtr<IWebHistoryItem> WebHistory::itemForURLString(const String& urlString) const
{
    RetainPtr<CFStringRef> urlCFString(AdoptCF, urlString.createCFString());
    if (!urlCFString)
        return 0;
    COMPtr<IWebHistoryItem> item;
    if (FAILED(itemForURLString(urlCFString.get(), &item)))
        return 0;
    return item;
}

HRESULT WebHistory::addItemToDateCaches(IWebHistoryItem* entry)
{
    HRESULT hr = S_OK;

    DATE lastVisitedCOMTime;
    entry->lastVisitedTimeInterval(&lastVisitedCOMTime);
    
    DateKey dateKey;
    if (findKey(&dateKey, MarshallingHelpers::DATEToCFAbsoluteTime(lastVisitedCOMTime))) {
        // other entries already exist for this date
        hr = insertItem(entry, dateKey);
    } else {
        ASSERT(!m_entriesByDate.contains(dateKey));
        // no other entries exist for this date
        RetainPtr<CFMutableArrayRef> entryArray(AdoptCF, 
            CFArrayCreateMutable(0, 0, &MarshallingHelpers::kIUnknownArrayCallBacks));
        CFArrayAppendValue(entryArray.get(), entry);
        m_entriesByDate.set(dateKey, entryArray);
        // Clear m_orderedLastVisitedDays so it will be regenerated when next requested.
        m_orderedLastVisitedDays.clear();
    }

    return hr;
}

HRESULT WebHistory::removeItemFromDateCaches(IWebHistoryItem* entry)
{
    HRESULT hr = S_OK;

    DATE lastVisitedCOMTime;
    entry->lastVisitedTimeInterval(&lastVisitedCOMTime);

    DateKey dateKey;
    if (!findKey(&dateKey, MarshallingHelpers::DATEToCFAbsoluteTime(lastVisitedCOMTime)))
        return E_FAIL;

    DateToEntriesMap::iterator found = m_entriesByDate.find(dateKey);
    ASSERT(found != m_entriesByDate.end());
    CFMutableArrayRef entriesForDate = found->second.get();

    CFIndex count = CFArrayGetCount(entriesForDate);
    for (int i = count - 1; i >= 0; --i) {
        if ((IWebHistoryItem*)CFArrayGetValueAtIndex(entriesForDate, i) == entry)
            CFArrayRemoveValueAtIndex(entriesForDate, i);
    }

    // remove this date entirely if there are no other entries on it
    if (CFArrayGetCount(entriesForDate) == 0) {
        m_entriesByDate.remove(found);
        // Clear m_orderedLastVisitedDays so it will be regenerated when next requested.
        m_orderedLastVisitedDays.clear();
    }

    return hr;
}

static void getDayBoundaries(CFAbsoluteTime day, CFAbsoluteTime& beginningOfDay, CFAbsoluteTime& beginningOfNextDay)
{
    RetainPtr<CFTimeZoneRef> timeZone(AdoptCF, CFTimeZoneCopyDefault());
    CFGregorianDate date = CFAbsoluteTimeGetGregorianDate(day, timeZone.get());
    date.hour = 0;
    date.minute = 0;
    date.second = 0;
    beginningOfDay = CFGregorianDateGetAbsoluteTime(date, timeZone.get());
    date.day += 1;
    beginningOfNextDay = CFGregorianDateGetAbsoluteTime(date, timeZone.get());
}

static inline CFAbsoluteTime beginningOfDay(CFAbsoluteTime date)
{
    static CFAbsoluteTime cachedBeginningOfDay = numeric_limits<CFAbsoluteTime>::quiet_NaN();
    static CFAbsoluteTime cachedBeginningOfNextDay;
    if (!(date >= cachedBeginningOfDay && date < cachedBeginningOfNextDay))
        getDayBoundaries(date, cachedBeginningOfDay, cachedBeginningOfNextDay);
    return cachedBeginningOfDay;
}

static inline WebHistory::DateKey dateKey(CFAbsoluteTime date)
{
    // Converting from double (CFAbsoluteTime) to int64_t (WebHistoryDateKey) is
    // safe here because all sensible dates are in the range -2**48 .. 2**47 which
    // safely fits in an int64_t.
    return beginningOfDay(date);
}

// Returns whether the day is already in the list of days,
// and fills in *key with the found or proposed key.
bool WebHistory::findKey(DateKey* key, CFAbsoluteTime forDay)
{
    ASSERT_ARG(key, key);

    *key = dateKey(forDay);
    return m_entriesByDate.contains(*key);
}

HRESULT WebHistory::insertItem(IWebHistoryItem* entry, DateKey dateKey)
{
    ASSERT_ARG(entry, entry);
    ASSERT_ARG(dateKey, m_entriesByDate.contains(dateKey));

    HRESULT hr = S_OK;

    if (!entry)
        return E_FAIL;

    DATE entryTime;
    entry->lastVisitedTimeInterval(&entryTime);
    CFMutableArrayRef entriesForDate = m_entriesByDate.get(dateKey).get();
    unsigned count = CFArrayGetCount(entriesForDate);

    // The entries for each day are stored in a sorted array with the most recent entry first
    // Check for the common cases of the entry being newer than all existing entries or the first entry of the day
    bool isNewerThanAllEntries = false;
    if (count) {
        IWebHistoryItem* item = const_cast<IWebHistoryItem*>(static_cast<const IWebHistoryItem*>(CFArrayGetValueAtIndex(entriesForDate, 0)));
        DATE itemTime;
        isNewerThanAllEntries = SUCCEEDED(item->lastVisitedTimeInterval(&itemTime)) && itemTime < entryTime;
    }
    if (!count || isNewerThanAllEntries) {
        CFArrayInsertValueAtIndex(entriesForDate, 0, entry);
        return S_OK;
    }

    // .. or older than all existing entries
    bool isOlderThanAllEntries = false;
    if (count > 0) {
        IWebHistoryItem* item = const_cast<IWebHistoryItem*>(static_cast<const IWebHistoryItem*>(CFArrayGetValueAtIndex(entriesForDate, count - 1)));
        DATE itemTime;
        isOlderThanAllEntries = SUCCEEDED(item->lastVisitedTimeInterval(&itemTime)) && itemTime >= entryTime;
    }
    if (isOlderThanAllEntries) {
        CFArrayInsertValueAtIndex(entriesForDate, count, entry);
        return S_OK;
    }

    unsigned low = 0;
    unsigned high = count;
    while (low < high) {
        unsigned mid = low + (high - low) / 2;
        IWebHistoryItem* item = const_cast<IWebHistoryItem*>(static_cast<const IWebHistoryItem*>(CFArrayGetValueAtIndex(entriesForDate, mid)));
        DATE itemTime;
        if (FAILED(item->lastVisitedTimeInterval(&itemTime)))
            return E_FAIL;

        if (itemTime >= entryTime)
            low = mid + 1;
        else
            high = mid;
    }

    // low is now the index of the first entry that is older than entryDate
    CFArrayInsertValueAtIndex(entriesForDate, low, entry);
    return S_OK;
}

CFAbsoluteTime WebHistory::timeToDate(CFAbsoluteTime time)
{
    // can't just divide/round since the day boundaries depend on our current time zone
    const double secondsPerDay = 60 * 60 * 24;
    CFTimeZoneRef timeZone = CFTimeZoneCopySystem();
    CFGregorianDate date = CFAbsoluteTimeGetGregorianDate(time, timeZone);
    date.hour = date.minute = 0;
    date.second = 0.0;
    CFAbsoluteTime timeInDays = CFGregorianDateGetAbsoluteTime(date, timeZone);
    if (areEqualOrClose(time - timeInDays, secondsPerDay))
        timeInDays += secondsPerDay;
    return timeInDays;
}

// Return a date that marks the age limit for history entries saved to or
// loaded from disk. Any entry older than this item should be rejected.
HRESULT WebHistory::ageLimitDate(CFAbsoluteTime* time)
{
    // get the current date as a CFAbsoluteTime
    CFAbsoluteTime currentDate = timeToDate(CFAbsoluteTimeGetCurrent());

    CFGregorianUnits ageLimit = {0};
    int historyLimitDays;
    HRESULT hr = historyAgeInDaysLimit(&historyLimitDays);
    if (FAILED(hr))
        return hr;
    ageLimit.days = -historyLimitDays;
    *time = CFAbsoluteTimeAddGregorianUnits(currentDate, CFTimeZoneCopySystem(), ageLimit);
    return S_OK;
}

static void addVisitedLinkToPageGroup(const void* key, const void*, void* context)
{
    CFStringRef url = static_cast<CFStringRef>(key);
    PageGroup* group = static_cast<PageGroup*>(context);

    CFIndex length = CFStringGetLength(url);
    const UChar* characters = reinterpret_cast<const UChar*>(CFStringGetCharactersPtr(url));
    if (characters)
        group->addVisitedLink(characters, length);
    else {
        Vector<UChar, 512> buffer(length);
        CFStringGetCharacters(url, CFRangeMake(0, length), reinterpret_cast<UniChar*>(buffer.data()));
        group->addVisitedLink(buffer.data(), length);
    }
}

void WebHistory::addVisitedLinksToPageGroup(PageGroup& group)
{
    CFDictionaryApplyFunction(m_entriesByURL.get(), addVisitedLinkToPageGroup, &group);
}

RetainPtr<CFDataRef> WebHistory::data() const
{
    if (m_entriesByDate.isEmpty())
        return 0;

    WebHistoryWriter writer(m_entriesByDate);
    writer.writePropertyList();
    return writer.releaseData();
}
