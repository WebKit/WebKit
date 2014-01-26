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

#include "MemoryStream.h"
#include "WebKit.h"
#include "MarshallingHelpers.h"
#include "WebHistoryItem.h"
#include "WebKit.h"
#include "WebNotificationCenter.h"
#include "WebPreferences.h"
#include <WebCore/BString.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/URL.h>
#include <WebCore/PageGroup.h>
#include <WebCore/SharedBuffer.h>
#include <functional>
#include <wtf/DateMath.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

#if USE(CF)
#include "CFDictionaryPropertyBag.h"
#include <CoreFoundation/CoreFoundation.h>
#else
#include "COMPropertyBag.h"
#endif

using namespace WebCore;
using namespace std;

static bool areEqualOrClose(double d1, double d2)
{
    double diff = d1-d2;
    return (diff < .000001 && diff > -.000001);
}

static COMPtr<IPropertyBag> createUserInfoFromArray(BSTR notificationStr, IWebHistoryItem** data, size_t size)
{
#if USE(CF)
    RetainPtr<CFArrayRef> arrayItem = adoptCF(CFArrayCreate(kCFAllocatorDefault, (const void**)data, size, &MarshallingHelpers::kIUnknownArrayCallBacks));

    RetainPtr<CFMutableDictionaryRef> dictionary = adoptCF(
        CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    RetainPtr<CFStringRef> key = adoptCF(MarshallingHelpers::BSTRToCFStringRef(notificationStr));
    CFDictionaryAddValue(dictionary.get(), key.get(), arrayItem.get());

    COMPtr<CFDictionaryPropertyBag> result = CFDictionaryPropertyBag::createInstance();
    result->setDictionary(dictionary.get());
    return COMPtr<IPropertyBag>(AdoptCOM, result.leakRef());
#else
    Vector<COMPtr<IWebHistoryItem>, 1> arrayItem;
    arrayItem.reserveInitialCapacity(size);
    for (size_t i = 0; i < size; ++i)
        arrayItem[i] = data[i];

    HashMap<String, Vector<COMPtr<IWebHistoryItem>>> dictionary;
    String key(notificationStr, ::SysStringLen(notificationStr));
    dictionary.set(key, std::move(arrayItem));
    return COMPtr<IPropertyBag>(AdoptCOM, COMPropertyBag<Vector<COMPtr<IWebHistoryItem>>>::adopt(dictionary));
#endif
}

static inline COMPtr<IPropertyBag> createUserInfoFromHistoryItem(BSTR notificationStr, IWebHistoryItem* item)
{
    return createUserInfoFromArray(notificationStr, &item, 1);
}

static inline void addDayToSystemTime(SYSTEMTIME& systemTime)
{
    systemTime.wDay += 1;
    if (systemTime.wDay > 31) {
        systemTime.wDay = 1;
        systemTime.wMonth += 1;
    }
    if (systemTime.wMonth > 12) {
        systemTime.wMonth = 1;
        systemTime.wYear += 1;
    }

    // Convert to and from VariantTime to fix invalid dates like 2001-04-31.
    DATE date = 0.0;
    ::SystemTimeToVariantTime(&systemTime, &date);
    ::VariantTimeToSystemTime(date, &systemTime);
}

static void getDayBoundaries(DATE day, DATE& beginningOfDay, DATE& beginningOfNextDay)
{
    SYSTEMTIME systemTime;
    ::VariantTimeToSystemTime(day, &systemTime);

    SYSTEMTIME beginningLocalTime;
    ::SystemTimeToTzSpecificLocalTime(0, &systemTime, &beginningLocalTime);
    beginningLocalTime.wHour = 0;
    beginningLocalTime.wMinute = 0;
    beginningLocalTime.wSecond = 0;
    beginningLocalTime.wMilliseconds = 0;

    SYSTEMTIME beginningOfNextDayLocalTime = beginningLocalTime;
    addDayToSystemTime(beginningOfNextDayLocalTime);

    SYSTEMTIME beginningSystemTime;
    ::TzSpecificLocalTimeToSystemTime(0, &beginningLocalTime, &beginningSystemTime);
    ::SystemTimeToVariantTime(&beginningSystemTime, &beginningOfDay);

    SYSTEMTIME beginningOfNextDaySystemTime;
    ::TzSpecificLocalTimeToSystemTime(0, &beginningOfNextDayLocalTime, &beginningOfNextDaySystemTime);
    ::SystemTimeToVariantTime(&beginningOfNextDaySystemTime, &beginningOfNextDay);
}

static inline DATE beginningOfDay(DATE date)
{
    static DATE cachedBeginningOfDay = std::numeric_limits<DATE>::quiet_NaN();
    static DATE cachedBeginningOfNextDay;
    if (!(date >= cachedBeginningOfDay && date < cachedBeginningOfNextDay))
        getDayBoundaries(date, cachedBeginningOfDay, cachedBeginningOfNextDay);
    return cachedBeginningOfDay;
}

static inline WebHistory::DateKey dateKey(DATE date)
{
    // Converting from double (DATE) to int64_t (WebHistoryDateKey) is safe
    // here because all sensible dates are in the range -2**48 .. 2**47 which
    // safely fits in an int64_t.
    return beginningOfDay(date) * secondsPerDay;
}

// WebHistory -----------------------------------------------------------------

WebHistory::WebHistory()
: m_refCount(0)
, m_preferences(0)
{
    gClassCount++;
    gClassNameCount.add("WebHistory");

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

HRESULT STDMETHODCALLTYPE WebHistory::unused1()
{
    ASSERT_NOT_REACHED();
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebHistory::unused2()
{
    ASSERT_NOT_REACHED();
    return E_FAIL;
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
    m_orderedLastVisitedDays = nullptr;

    Vector<IWebHistoryItem*> itemsVector;
    itemsVector.reserveInitialCapacity(m_entriesByURL.size());
    for (auto it = m_entriesByURL.begin(); it != m_entriesByURL.end(); ++it)
        itemsVector.append(it->value.get());
    COMPtr<IPropertyBag> userInfo = createUserInfoFromArray(getNotificationString(kWebHistoryAllItemsRemovedNotification), itemsVector.data(), itemsVector.size());

    m_entriesByURL.clear();

    PageGroup::removeAllVisitedLinks();

    return postNotification(kWebHistoryAllItemsRemovedNotification, userInfo.get());
}

// FIXME: This function should be removed from the IWebHistory interface.
HRESULT STDMETHODCALLTYPE WebHistory::orderedLastVisitedDays( 
    /* [out][in] */ int* count,
    /* [in] */ DATE* calendarDates)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebHistory::orderedItemsLastVisitedOnDay( 
    /* [out][in] */ int* count,
    /* [in] */ IWebHistoryItem** items,
    /* [in] */ DATE calendarDate)
{
    auto found = m_entriesByDate.find(dateKey(calendarDate));
    if (found == m_entriesByDate.end()) {
        *count = 0;
        return 0;
    }

    auto& entriesForDate = found->value;
    int newCount = entriesForDate.size();

    if (!items) {
        *count = newCount;
        return S_OK;
    }

    if (*count < newCount) {
        *count = newCount;
        return E_FAIL;
    }

    *count = newCount;
    for (int i = 0; i < newCount; ++i)
        entriesForDate[i].copyRefTo(&items[i]);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::allItems( 
    /* [out][in] */ int* count,
    /* [out][retval] */ IWebHistoryItem** items)
{
    int entriesByURLCount = m_entriesByURL.size();

    if (!items) {
        *count = entriesByURLCount;
        return S_OK;
    }

    if (*count < entriesByURLCount) {
        *count = entriesByURLCount;
        return E_FAIL;
    }

    *count = entriesByURLCount;
    int i = 0;
    for (auto it = m_entriesByURL.begin(); it != m_entriesByURL.end(); ++i, ++it) {
        items[i] = it->value.get();
        items[i]->AddRef();
    }

    return S_OK;
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
    BString urlBStr;

    hr = entry->URLString(&urlBStr);
    if (FAILED(hr))
        return hr;

    String urlString(urlBStr, SysStringLen(urlBStr));

    auto it = m_entriesByURL.find(urlString);
    if (it == m_entriesByURL.end())
        return E_FAIL;

    // If this exact object isn't stored, then make no change.
    // FIXME: Is this the right behavior if this entry isn't present, but another entry for the same URL is?
    // Maybe need to change the API to make something like removeEntryForURLString public instead.
    if (it->value.get() != entry)
        return E_FAIL;

    hr = removeItemForURLString(urlString);
    if (FAILED(hr))
        return hr;

    COMPtr<IPropertyBag> userInfo = createUserInfoFromHistoryItem(
        getNotificationString(kWebHistoryItemsRemovedNotification), entry);
    hr = postNotification(kWebHistoryItemsRemovedNotification, userInfo.get());

    return hr;
}

HRESULT WebHistory::addItem(IWebHistoryItem* entry, bool discardDuplicate, bool* added)
{
    HRESULT hr = S_OK;

    if (!entry)
        return E_FAIL;

    BString urlBStr;
    hr = entry->URLString(&urlBStr);
    if (FAILED(hr))
        return hr;

    String urlString(urlBStr, SysStringLen(urlBStr));

    COMPtr<IWebHistoryItem> oldEntry(m_entriesByURL.get(urlString));

    if (oldEntry) {
        if (discardDuplicate) {
            if (added)
                *added = false;
            return S_OK;
        }

        removeItemForURLString(urlString);

        // If we already have an item with this URL, we need to merge info that drives the
        // URL autocomplete heuristics from that item into the new one.
        IWebHistoryItemPrivate* entryPriv;
        hr = entry->QueryInterface(IID_IWebHistoryItemPrivate, (void**)&entryPriv);
        if (SUCCEEDED(hr)) {
            entryPriv->mergeAutoCompleteHints(oldEntry.get());
            entryPriv->Release();
        }
    }

    m_entriesByURL.set(urlString, entry);

    COMPtr<IPropertyBag> userInfo = createUserInfoFromHistoryItem(
        getNotificationString(kWebHistoryItemsAddedNotification), entry);
    hr = postNotification(kWebHistoryItemsAddedNotification, userInfo.get());

    if (added)
        *added = true;

    return hr;
}

void WebHistory::visitedURL(const URL& url, const String& title, const String& httpMethod, bool wasFailure, bool increaseVisitCount)
{
    IWebHistoryItem* entry = m_entriesByURL.get(url.string()).get();
    if (!entry) {
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
        
        m_entriesByURL.set(url.string(), entry);
    }

    COMPtr<IWebHistoryItemPrivate> entryPrivate(Query, entry);
    if (!entryPrivate)
        return;

    entryPrivate->setLastVisitWasFailure(wasFailure);

    COMPtr<WebHistoryItem> item(Query, entry);
    item->historyItem()->setRedirectURLs(nullptr);

    COMPtr<IPropertyBag> userInfo = createUserInfoFromHistoryItem(
        getNotificationString(kWebHistoryItemsAddedNotification), entry);
    postNotification(kWebHistoryItemsAddedNotification, userInfo.get());
}

HRESULT WebHistory::itemForURL(BSTR url, IWebHistoryItem** item)
{
    if (!item)
        return E_FAIL;
    *item = 0;

    auto it = m_entriesByURL.find(url);
    if (it == m_entriesByURL.end())
        return E_FAIL;

    it->value.copyRefTo(item);
    return S_OK;
}

HRESULT WebHistory::removeItemForURLString(const WTF::String& urlString)
{
    auto it = m_entriesByURL.find(urlString);
    if (it == m_entriesByURL.end())
        return E_FAIL;

    if (!m_entriesByURL.size())
        PageGroup::removeAllVisitedLinks();

    return hr;
}

COMPtr<IWebHistoryItem> WebHistory::itemForURLString(const String& urlString) const
{
    if (!urlString)
        return 0;
    return m_entriesByURL.get(urlString);
}

void WebHistory::addVisitedLinksToPageGroup(PageGroup& group)
{
    for (auto it = m_entriesByURL.begin(); it != m_entriesByURL.end(); ++it) {
        const String& url = it->key;
        group.addVisitedLink(url.characters(), url.length());
    }
}
