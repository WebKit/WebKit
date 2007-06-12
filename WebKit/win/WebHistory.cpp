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
#include "IWebURLResponse.h"
#include "MarshallingHelpers.h"
#include "WebHistoryItem.h"
#include "WebKit.h"
#include "WebNotificationCenter.h"
#include "WebPreferences.h"
#include <CoreFoundation/CoreFoundation.h>
#include <WebCore/WebCoreHistory.h>
#pragma warning( push, 0 )
#include <wtf/Vector.h>
#pragma warning( pop )

CFStringRef DatesArrayKey = CFSTR("WebHistoryDates");
CFStringRef FileVersionKey = CFSTR("WebHistoryFileVersion");

const IID IID_IWebHistoryPrivate = { 0x3de04e59, 0x93f9, 0x4369, { 0x8b, 0x43, 0x97, 0x64, 0x58, 0xd7, 0xe3, 0x19 } };

#define currentFileVersion 1

class _WebCoreHistoryProvider : public WebCore::WebCoreHistoryProvider {
public:
    _WebCoreHistoryProvider(IWebHistory* history);
    ~_WebCoreHistoryProvider();

    virtual bool containsItemForURLLatin1(const char* latin1, unsigned int length);
    virtual bool containsItemForURLUnicode(const UChar* unicode, unsigned int length);

private:
    IWebHistory* m_history;
    IWebHistoryPrivate* m_historyPrivate;
};

static bool areEqualOrClose(double d1, double d2)
{
    double diff = d1-d2;
    return (diff < .000001 && diff > -.000001);
}

static CFDictionaryPropertyBag* createUserInfoFromArray(BSTR notificationStr, CFArrayRef arrayItem)
{
    RetainPtr<CFMutableDictionaryRef> dictionary(AdoptCF, 
        CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    RetainPtr<CFStringRef> key(AdoptCF, MarshallingHelpers::BSTRToCFStringRef(notificationStr));
    CFDictionaryAddValue(dictionary.get(), key.get(), arrayItem);

    CFDictionaryPropertyBag* result = CFDictionaryPropertyBag::createInstance();
    result->setDictionary(dictionary.get());
    return result;
}

static CFDictionaryPropertyBag* createUserInfoFromHistoryItem(BSTR notificationStr, IWebHistoryItem* item)
{
    // reference counting of item added to the array is managed by the CFArray value callbacks
    RetainPtr<CFArrayRef> itemList(AdoptCF, CFArrayCreate(0, (const void**) &item, 1, &MarshallingHelpers::kIUnknownArrayCallBacks));
    CFDictionaryPropertyBag* info = createUserInfoFromArray(notificationStr, itemList.get());
    return info;
}

static void releaseUserInfo(CFDictionaryPropertyBag* userInfo)
{
    // free the dictionary
    userInfo->setDictionary(0);
    int result = userInfo->Release();
    (void)result;
    ASSERT(result == 0);   // make sure no one else holds a reference to the userInfo.
}

// WebHistory -----------------------------------------------------------------

IWebHistory* WebHistory::m_optionalSharedHistory = 0;

WebHistory::WebHistory()
: m_refCount(0)
, m_preferences(0)
{
    gClassCount++;

    m_entriesByURL.adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &MarshallingHelpers::kIUnknownDictionaryValueCallBacks));
    m_datesWithEntries.adoptCF(CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));
    m_entriesByDate.adoptCF(CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));

    COMPtr<WebPreferences> tempPrefs(AdoptCOM, WebPreferences::createInstance());
    IWebPreferences* standardPrefs;
    tempPrefs->standardPreferences(&standardPrefs);
    m_preferences.adoptRef(static_cast<WebPreferences*>(standardPrefs)); // should be safe, since there's no setter for standardPrefs
}

WebHistory::~WebHistory()
{
    gClassCount--;
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

HRESULT STDMETHODCALLTYPE WebHistory::optionalSharedHistory( 
    /* [retval][out] */ IWebHistory** history)
{
    *history = m_optionalSharedHistory;
    if (m_optionalSharedHistory)
        m_optionalSharedHistory->AddRef();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::setOptionalSharedHistory( 
    /* [in] */ IWebHistory* history)
{
    if (m_optionalSharedHistory) {
        m_optionalSharedHistory->Release();
        m_optionalSharedHistory = 0;        
    }

    _WebCoreHistoryProvider* coreHistory = 0;
    m_optionalSharedHistory = history;
    if (history) {
        history->AddRef();
        coreHistory = new _WebCoreHistoryProvider(history);
    }
    WebCore::WebCoreHistory::setHistoryProvider(coreHistory);

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
        CFDictionaryPropertyBag* userInfo = createUserInfoFromArray(getNotificationString(kWebHistoryItemsDiscardedWhileLoadingNotification), discardedItems.get());
        hr = postNotification(kWebHistoryItemsDiscardedWhileLoadingNotification, userInfo);
        releaseUserInfo(userInfo);
        if (FAILED(hr))
            goto exit;
    }

exit:
    if (succeeded)
        *succeeded = SUCCEEDED(hr);
    return hr;
}

CFDictionaryRef createHistoryListFromStream(CFReadStreamRef stream, CFPropertyListFormat format)
{
    __try // FIXME- <rdar://4754295> Prevent crash when reading corrupt plists for the seed.  Real fix is to figure out why CF$UID plist entries get written out by CF occasionally.
    {
        return (CFDictionaryRef)CFPropertyListCreateFromStream(0, stream, 0, kCFPropertyListImmutable, &format, 0);
    }
    __except(1) // FIXME- <rdar://4754295> Prevent crash when reading corrupt plists for the seed.  Real fix is to figure out why CF$UID plist entries get written out by CF occasionally.
    {
        return 0;
    }
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
                addItem(item.get()); // ref is added inside addItem
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

    // FIXME:  Correctly report error when new API is ready.
    if (error)
        *error = 0;

    RetainPtr<CFWriteStreamRef> stream(AdoptCF, CFWriteStreamCreateWithFile(kCFAllocatorDefault, url));
    if (!stream) 
        return E_FAIL;

    CFMutableArrayRef rawEntries;
    hr = datesArray(&rawEntries);
    if (FAILED(hr))
        return hr;
    RetainPtr<CFMutableArrayRef> entries(AdoptCF, rawEntries);

    // create the outer dictionary
    CFTypeRef keys[2];
    CFTypeRef values[2];
    keys[0]   = DatesArrayKey;
    values[0] = entries.get();
    keys[1]   = FileVersionKey;

    int version = currentFileVersion;
    RetainPtr<CFNumberRef> versionCF(AdoptCF, CFNumberCreate(0, kCFNumberIntType, &version));
    values[1] = versionCF.get();

    RetainPtr<CFDictionaryRef> dictionary(AdoptCF, 
        CFDictionaryCreate(0, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    if (!CFWriteStreamOpen(stream.get())) 
        return E_FAIL;

    if (!CFPropertyListWriteToStream(dictionary.get(), stream.get(), kCFPropertyListXMLFormat_v1_0, 0))
        hr = E_FAIL;
 
    CFWriteStreamClose(stream.get());

    return hr;
}

HRESULT WebHistory::datesArray(CFMutableArrayRef* datesArray)
{
    HRESULT hr = S_OK;

    RetainPtr<CFMutableArrayRef> result(AdoptCF, CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));
    
    // for each date with entries
    int dateCount = CFArrayGetCount(m_entriesByDate.get());
    for (int i = 0; i < dateCount; ++i) {
        // get the entries for that date
        CFArrayRef entries = (CFArrayRef)CFArrayGetValueAtIndex(m_entriesByDate.get(), i);
        int entriesCount = CFArrayGetCount(entries);
        for (int j = entriesCount - 1; j >= 0; --j) {
            IWebHistoryItem* item = (IWebHistoryItem*) CFArrayGetValueAtIndex(entries, j);
            IWebHistoryItemPrivate* webHistoryItem;
            hr = item->QueryInterface(IID_IWebHistoryItemPrivate, (void**)&webHistoryItem);
            if (FAILED(hr))
                return E_FAIL;
            
            CFDictionaryRef itemDict;
            hr = webHistoryItem->dictionaryRepresentation((void**)&itemDict);
            webHistoryItem->Release();
            if (FAILED(hr))
                return E_FAIL;

            CFArrayAppendValue(result.get(), itemDict);
            CFRelease(itemDict);
        }
    }

    if (SUCCEEDED(hr))
        *datesArray = result.releaseRef();
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
        hr = addItem(items[i]);
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
    CFArrayRemoveAllValues(m_entriesByDate.get());
    CFArrayRemoveAllValues(m_datesWithEntries.get());
    CFDictionaryRemoveAllValues(m_entriesByURL.get());

    return postNotification(kWebHistoryAllItemsRemovedNotification);
}

HRESULT STDMETHODCALLTYPE WebHistory::orderedLastVisitedDays( 
    /* [out][in] */ int* count,
    /* [in] */ DATE* calendarDates)
{
    int dateCount = CFArrayGetCount(m_datesWithEntries.get());
    if (!calendarDates) {
        *count = dateCount;
        return S_OK;
    }

    if (*count < dateCount) {
        *count = dateCount;
        return E_FAIL;
    }

    *count = dateCount;
    for (int i = 0; i < dateCount; i++) {
        CFNumberRef absoluteTimeNumberRef = (CFNumberRef)CFArrayGetValueAtIndex(m_datesWithEntries.get(), i);
        CFAbsoluteTime absoluteTime;
        if (!CFNumberGetValue(absoluteTimeNumberRef, kCFNumberDoubleType, &absoluteTime))
            return E_FAIL;
        calendarDates[i] = MarshallingHelpers::CFAbsoluteTimeToDATE(absoluteTime);
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::orderedItemsLastVisitedOnDay( 
    /* [out][in] */ int* count,
    /* [in] */ IWebHistoryItem** items,
    /* [in] */ DATE calendarDate)
{
    int index;
    if (!findIndex(&index, MarshallingHelpers::DATEToCFAbsoluteTime(calendarDate))) {
        *count = 0;
        return 0;
    }

    CFArrayRef entries = (CFArrayRef)CFArrayGetValueAtIndex(m_entriesByDate.get(), index);
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
        if (!item)
            return E_FAIL;
        item->AddRef();
        items[newCount-i-1] = item; // reverse when inserting to get the list sorted oldest to newest
    }

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

    CFDictionaryPropertyBag* userInfo = createUserInfoFromHistoryItem(
        getNotificationString(kWebHistoryItemsRemovedNotification), entry);
    hr = postNotification(kWebHistoryItemsRemovedNotification, userInfo);
    releaseUserInfo(userInfo);

    return hr;
}

HRESULT WebHistory::addItem(IWebHistoryItem* entry)
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

    CFDictionaryPropertyBag* userInfo = createUserInfoFromHistoryItem(
        getNotificationString(kWebHistoryItemsAddedNotification), entry);
    hr = postNotification(kWebHistoryItemsAddedNotification, userInfo);
    releaseUserInfo(userInfo);

    return hr;
}

HRESULT WebHistory::addItemForURL(BSTR url, BSTR title)
{
    COMPtr<WebHistoryItem> item(AdoptCOM, WebHistoryItem::createInstance());
    if (!item)
        return E_FAIL;

    SYSTEMTIME currentTime;
    GetSystemTime(&currentTime);
    DATE lastVisited;
    if (!SystemTimeToVariantTime(&currentTime, &lastVisited))
        return E_FAIL;

    HRESULT hr = item->initWithURLString(url, title, lastVisited);
    if (FAILED(hr))
        return hr;

    hr = addItem(item.get());
    if (FAILED(hr))
        return hr;

    return item->setLastVisitedTimeInterval(lastVisited); // also increments visitedCount
}

HRESULT WebHistory::itemForURLString(
    /* [in] */ CFStringRef urlString,
    /* [retval][out] */ IWebHistoryItem** item)
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

HRESULT WebHistory::containsItemForURLString(
    /* [in] */ void* urlCFString,
    /* [retval][out] */ BOOL* contains)
{
    IWebHistoryItem* item = 0;
    HRESULT hr;
    if (SUCCEEDED(hr = itemForURLString((CFStringRef)urlCFString, &item))) {
        *contains = TRUE;
        // itemForURLString refs the returned item, so we need to balance that 
        item->Release();
    } else
        *contains = FALSE;

    return hr;
}

HRESULT WebHistory::removeItemForURLString(CFStringRef urlString)
{
    IWebHistoryItem* entry = (IWebHistoryItem*) CFDictionaryGetValue(m_entriesByURL.get(), urlString);
    if (!entry) 
        return E_FAIL;

    HRESULT hr = removeItemFromDateCaches(entry);
    CFDictionaryRemoveValue(m_entriesByURL.get(), urlString);

    return hr;
}

HRESULT WebHistory::addItemToDateCaches(IWebHistoryItem* entry)
{
    HRESULT hr = S_OK;

    DATE lastVisitedCOMTime;
    entry->lastVisitedTimeInterval(&lastVisitedCOMTime);
    CFAbsoluteTime lastVisitedDate = timeToDate(MarshallingHelpers::DATEToCFAbsoluteTime(lastVisitedCOMTime));
    
    int dateIndex;
    if (findIndex(&dateIndex, lastVisitedDate)) {
        // other entries already exist for this date
        hr = insertItem(entry, dateIndex);
    } else {
        // no other entries exist for this date
        RetainPtr<CFNumberRef> lastVisitedDateRef(AdoptCF, CFNumberCreate(0, kCFNumberDoubleType, &lastVisitedDate));
        CFArrayInsertValueAtIndex(m_datesWithEntries.get(), dateIndex, lastVisitedDateRef.get());
        RetainPtr<CFMutableArrayRef> entryArray(AdoptCF, 
            CFArrayCreateMutable(0, 0, &MarshallingHelpers::kIUnknownArrayCallBacks));
        CFArrayAppendValue(entryArray.get(), entry);
        CFArrayInsertValueAtIndex(m_entriesByDate.get(), dateIndex, entryArray.get());
    }

    return hr;
}

HRESULT WebHistory::removeItemFromDateCaches(IWebHistoryItem* entry)
{
    HRESULT hr = S_OK;

    DATE lastVisitedCOMTime;
    entry->lastVisitedTimeInterval(&lastVisitedCOMTime);
    CFAbsoluteTime lastVisitedDate = timeToDate(MarshallingHelpers::DATEToCFAbsoluteTime(lastVisitedCOMTime));

    int dateIndex;
    if (!findIndex(&dateIndex, lastVisitedDate))
        return E_FAIL;

    CFMutableArrayRef entriesForDate = (CFMutableArrayRef) CFArrayGetValueAtIndex(m_entriesByDate.get(), dateIndex);
    CFIndex count = CFArrayGetCount(entriesForDate);
    for (int i = count - 1; i >= 0; --i) {
        if ((IWebHistoryItem*)CFArrayGetValueAtIndex(entriesForDate, i) == entry)
            CFArrayRemoveValueAtIndex(entriesForDate, i);
    }

    // remove this date entirely if there are no other entries on it
    if (CFArrayGetCount(entriesForDate) == 0) {
        CFArrayRemoveValueAtIndex(m_entriesByDate.get(), dateIndex);
        CFArrayRemoveValueAtIndex(m_datesWithEntries.get(), dateIndex);
    }

    return hr;
}

// Returns whether the day is already in the list of days,
// and fills in *index with the found or proposed index.
bool WebHistory::findIndex(int* index, CFAbsoluteTime forDay)
{
    CFAbsoluteTime forDayInDays = timeToDate(forDay);

    //FIXME: just does linear search through days; inefficient if many days
    int count = CFArrayGetCount(m_datesWithEntries.get());
    for (*index = 0; *index < count; ++*index) {
        CFNumberRef entryTimeNumberRef = (CFNumberRef) CFArrayGetValueAtIndex(m_datesWithEntries.get(), *index);
        CFAbsoluteTime entryTime;
        CFNumberGetValue(entryTimeNumberRef, kCFNumberDoubleType, &entryTime);
        CFAbsoluteTime entryInDays = timeToDate(entryTime);
        if (areEqualOrClose(forDayInDays, entryInDays))
            return true;
        else if (forDayInDays > entryInDays)
            return false;
    }
    return false;
}

HRESULT WebHistory::insertItem(IWebHistoryItem* entry, int dateIndex)
{
    HRESULT hr = S_OK;

    if (!entry)
        return E_FAIL;
    if (dateIndex < 0 || dateIndex >= CFArrayGetCount(m_entriesByDate.get()))
        return E_FAIL;

    //FIXME: just does linear search through entries; inefficient if many entries for this date
    DATE entryTime;
    entry->lastVisitedTimeInterval(&entryTime);
    CFMutableArrayRef entriesForDate = (CFMutableArrayRef) CFArrayGetValueAtIndex(m_entriesByDate.get(), dateIndex);
    int count = CFArrayGetCount(entriesForDate);
    // optimized for inserting oldest to youngest
    int index;
    for (index = 0; index < count; ++index) {
        IWebHistoryItem* indEntry = (IWebHistoryItem*) CFArrayGetValueAtIndex(entriesForDate, index);
        DATE indTime;
        hr = indEntry->lastVisitedTimeInterval(&indTime);
        if (FAILED(hr))
            return hr;
        if (entryTime < indTime)
            break;
    }    
    CFArrayInsertValueAtIndex(entriesForDate, index, entry);
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

IWebHistoryPrivate* WebHistory::optionalSharedHistoryInternal()
{
    if (!m_optionalSharedHistory)
        return 0;

    IWebHistoryPrivate* historyPrivate;
    if (FAILED(m_optionalSharedHistory->QueryInterface(IID_IWebHistoryPrivate, (void**)&historyPrivate)))
        return 0;

    historyPrivate->Release(); // don't add an additional ref for this internal call
    return historyPrivate;
}

// _WebCoreHistoryProvider ----------------------------------------------------------------

_WebCoreHistoryProvider::_WebCoreHistoryProvider(IWebHistory* history)
    : m_history(history)
    , m_historyPrivate(0)
{
}

_WebCoreHistoryProvider::~_WebCoreHistoryProvider()
{
}

static inline bool matchLetter(char c, char lowercaseLetter)
{
    return (c | 0x20) == lowercaseLetter;
}

static inline bool matchUnicodeLetter(UniChar c, UniChar lowercaseLetter)
{
    return (c | 0x20) == lowercaseLetter;
}

bool _WebCoreHistoryProvider::containsItemForURLLatin1(const char* latin1, unsigned int length)
{
    const int bufferSize = 2048;
    const char *latin1Str = latin1;
    char staticStrBuffer[bufferSize];
    char *strBuffer = 0;
    bool needToAddSlash = false;

    if (length >= 6 &&
        matchLetter(latin1[0], 'h') &&
        matchLetter(latin1[1], 't') &&
        matchLetter(latin1[2], 't') &&
        matchLetter(latin1[3], 'p') &&
        (latin1[4] == ':' 
        || (matchLetter(latin1[4], 's') && latin1[5] == ':'))) {
            int pos = latin1[4] == ':' ? 5 : 6;
            // skip possible initial two slashes
            if (latin1[pos] == '/' && latin1[pos + 1] == '/') {
                pos += 2;
            }

            const char* nextSlash = strchr(latin1 + pos, '/');
            if (!nextSlash)
                needToAddSlash = true;
    }

    if (needToAddSlash) {
        if (length + 1 <= bufferSize)
            strBuffer = staticStrBuffer;
        else
            strBuffer = (char*)malloc(length + 2);
        memcpy(strBuffer, latin1, length + 1);
        strBuffer[length] = '/';
        strBuffer[length+1] = '\0';
        length++;

        latin1Str = strBuffer;
    }

    if (!m_historyPrivate) {
        if (SUCCEEDED(m_history->QueryInterface(IID_IWebHistoryPrivate, (void**)&m_historyPrivate))) {
            // don't hold a ref - we're owned by IWebHistory/IWebHistoryPrivate
            m_historyPrivate->Release();
        } else {
            if (strBuffer != staticStrBuffer)
                free(strBuffer);
            m_historyPrivate = 0;
            return false;
        }
    }
    
    CFStringRef str = CFStringCreateWithCStringNoCopy(NULL, latin1Str, kCFStringEncodingWindowsLatin1, kCFAllocatorNull);
    BOOL result = FALSE;
    m_historyPrivate->containsItemForURLString((void*)str, &result);
    CFRelease(str);

    if (strBuffer != staticStrBuffer)
        free(strBuffer);

    return !!result;
}

bool _WebCoreHistoryProvider::containsItemForURLUnicode(const UChar* unicode, unsigned int length)
{
    const int bufferSize = 1024;
    const UChar *unicodeStr = unicode;
    UChar staticStrBuffer[bufferSize];
    UChar *strBuffer = 0;
    bool needToAddSlash = false;

    if (length >= 6 &&
        matchUnicodeLetter(unicode[0], 'h') &&
        matchUnicodeLetter(unicode[1], 't') &&
        matchUnicodeLetter(unicode[2], 't') &&
        matchUnicodeLetter(unicode[3], 'p') &&
        (unicode[4] == ':' 
        || (matchUnicodeLetter(unicode[4], 's') && unicode[5] == ':'))) {

            unsigned pos = unicode[4] == ':' ? 5 : 6;

            // skip possible initial two slashes
            if (pos + 1 < length && unicode[pos] == '/' && unicode[pos + 1] == '/')
                pos += 2;

            while (pos < length && unicode[pos] != '/')
                pos++;

            if (pos == length)
                needToAddSlash = true;
    }

    if (needToAddSlash) {
        if (length + 1 <= bufferSize)
            strBuffer = staticStrBuffer;
        else
            strBuffer = (UChar*)malloc(sizeof(UChar) * (length + 1));
        memcpy(strBuffer, unicode, 2 * length);
        strBuffer[length] = '/';
        length++;

        unicodeStr = strBuffer;
    }

    if (!m_historyPrivate) {
        if (SUCCEEDED(m_history->QueryInterface(IID_IWebHistoryPrivate, (void**)&m_historyPrivate))) {
            // don't hold a ref - we're owned by IWebHistory/IWebHistoryPrivate
            m_historyPrivate->Release();
        } else {
            if (strBuffer != staticStrBuffer)
                free(strBuffer);
            m_historyPrivate = 0;
            return false;
        }
    }

    CFStringRef str = CFStringCreateWithCharactersNoCopy(NULL, (const UniChar*)unicodeStr, length, kCFAllocatorNull);
    BOOL result = FALSE;
    m_historyPrivate->containsItemForURLString((void*)str, &result);
    CFRelease(str);

    if (strBuffer != staticStrBuffer)
        free(strBuffer);

    return !!result;
}
