/*
 * Copyright (C) 2006-2007, 2014-2015 Apple Inc.  All rights reserved.
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

#include "WebKitDLL.h"
#include "WebHistory.h"

#include "MemoryStream.h"
#include "WebKit.h"
#include "MarshallingHelpers.h"
#include "WebHistoryItem.h"
#include "WebKit.h"
#include "WebNotificationCenter.h"
#include "WebPreferences.h"
#include "WebVisitedLinkStore.h"
#include <WebCore/BString.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/SharedBuffer.h>
#include <functional>
#include <wtf/DateMath.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>

#if USE(CF)
#include "CFDictionaryPropertyBag.h"
#include <CoreFoundation/CoreFoundation.h>
#else
#include "COMPropertyBag.h"
#endif

using namespace WebCore;

static COMPtr<IPropertyBag> createUserInfoFromArray(BSTR notificationStr, IWebHistoryItem** data, size_t size)
{
#if USE(CF)
    RetainPtr<CFArrayRef> arrayItem = adoptCF(CFArrayCreate(kCFAllocatorDefault, (const void**)data, size, &MarshallingHelpers::kIUnknownArrayCallBacks));

    RetainPtr<CFMutableDictionaryRef> dictionary = adoptCF(
        CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    auto key = MarshallingHelpers::BSTRToCFStringRef(notificationStr);
    CFDictionaryAddValue(dictionary.get(), key.get(), arrayItem.get());

    COMPtr<CFDictionaryPropertyBag> result = CFDictionaryPropertyBag::createInstance();
    result->setDictionary(dictionary.get());
    return COMPtr<IPropertyBag>(AdoptCOM, result.leakRef());
#else
    Vector<COMPtr<IWebHistoryItem>, 1> arrayItem;
    arrayItem.reserveInitialCapacity(size);
    for (size_t i = 0; i < size; ++i)
        arrayItem.uncheckedAppend(data[i]);

    HashMap<String, Vector<COMPtr<IWebHistoryItem>>> dictionary;
    String key(notificationStr, ::SysStringLen(notificationStr));
    dictionary.set(key, WTFMove(arrayItem));
    return COMPtr<IPropertyBag>(AdoptCOM, COMPropertyBag<Vector<COMPtr<IWebHistoryItem>>>::adopt(dictionary));
#endif
}

static inline COMPtr<IPropertyBag> createUserInfoFromHistoryItem(BSTR notificationStr, IWebHistoryItem* item)
{
    return createUserInfoFromArray(notificationStr, &item, 1);
}

// WebHistory -----------------------------------------------------------------

WebHistory::WebHistory()
{
    gClassCount++;
    gClassNameCount().add("WebHistory"_s);

    m_preferences = WebPreferences::sharedStandardPreferences();
}

WebHistory::~WebHistory()
{
    gClassCount--;
    gClassNameCount().remove("WebHistory"_s);
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

HRESULT WebHistory::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
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

ULONG WebHistory::AddRef()
{
    return ++m_refCount;
}

ULONG WebHistory::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebHistory ----------------------------------------------------------------

static COMPtr<WebHistory>& sharedHistoryStorage()
{
    static NeverDestroyed<COMPtr<WebHistory>> sharedHistory;
    return sharedHistory;
}

WebHistory* WebHistory::sharedHistory()
{
    return sharedHistoryStorage().get();
}

HRESULT WebHistory::optionalSharedHistory(_COM_Outptr_opt_ IWebHistory** history)
{
    if (!history)
        return E_POINTER;
    *history = sharedHistory();
    if (*history)
        (*history)->AddRef();
    return S_OK;
}

HRESULT WebHistory::setOptionalSharedHistory(_In_opt_ IWebHistory* history)
{
    if (sharedHistoryStorage() == history)
        return S_OK;
    sharedHistoryStorage().query(history);
    WebVisitedLinkStore::setShouldTrackVisitedLinks(sharedHistoryStorage());
    WebVisitedLinkStore::removeAllVisitedLinks();
    return S_OK;
}

HRESULT WebHistory::unused1()
{
    ASSERT_NOT_REACHED();
    return E_FAIL;
}

HRESULT WebHistory::unused2()
{
    ASSERT_NOT_REACHED();
    return E_FAIL;
}

HRESULT WebHistory::addItems(int itemCount, __deref_in_ecount_opt(itemCount) IWebHistoryItem** items)
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

HRESULT WebHistory::removeItems(int itemCount, __deref_in_ecount_opt(itemCount) IWebHistoryItem** items)
{
    HRESULT hr;
    for (int i = 0; i < itemCount; ++i) {
        hr = removeItem(items[i]);
        if (FAILED(hr))
            return hr;
    }

    return S_OK;
}

HRESULT WebHistory::removeAllItems()
{
    Vector<IWebHistoryItem*> itemsVector;
    itemsVector.reserveInitialCapacity(m_entriesByURL.size());
    for (auto it = m_entriesByURL.begin(); it != m_entriesByURL.end(); ++it)
        itemsVector.append(it->value.get());
    COMPtr<IPropertyBag> userInfo = createUserInfoFromArray(getNotificationString(kWebHistoryAllItemsRemovedNotification), itemsVector.data(), itemsVector.size());

    m_entriesByURL.clear();

    WebVisitedLinkStore::removeAllVisitedLinks();

    return postNotification(kWebHistoryAllItemsRemovedNotification, userInfo.get());
}

// FIXME: This function should be removed from the IWebHistory interface.
HRESULT WebHistory::orderedLastVisitedDays(_Inout_ int* count, _In_ DATE* calendarDates)
{
    return E_NOTIMPL;
}

// FIXME: This function should be removed from the IWebHistory interface.
HRESULT WebHistory::orderedItemsLastVisitedOnDay(_Inout_ int* count, __deref_in_opt IWebHistoryItem** items, DATE calendarDate)
{
    return E_NOTIMPL;
}

HRESULT WebHistory::allItems(_Inout_ int* count, __deref_opt_out IWebHistoryItem** items)
{
    int entriesByURLCount = m_entriesByURL.size();

    if (!items) {
        *count = entriesByURLCount;
        return S_OK;
    }

    if (*count < entriesByURLCount) {
        *count = entriesByURLCount;
        return E_NOT_SUFFICIENT_BUFFER;
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
    WebVisitedLinkStore::setShouldTrackVisitedLinks(visitedLinkTrackingEnabled);
    return S_OK;
}

HRESULT WebHistory::removeAllVisitedLinks()
{
    WebVisitedLinkStore::removeAllVisitedLinks();
    return S_OK;
}

HRESULT WebHistory::setHistoryItemLimit(int limit)
{
    if (!m_preferences)
        return E_FAIL;
    return m_preferences->setHistoryItemLimit(limit);
}

HRESULT WebHistory::historyItemLimit(_Out_ int* limit)
{
    if (!limit)
        return E_POINTER;
    *limit = 0;
    if (!m_preferences)
        return E_FAIL;
    return m_preferences->historyItemLimit(limit);
}

HRESULT WebHistory::setHistoryAgeInDaysLimit(int limit)
{
    if (!m_preferences)
        return E_FAIL;
    return m_preferences->setHistoryAgeInDaysLimit(limit);
}

HRESULT WebHistory::historyAgeInDaysLimit(_Out_ int* limit)
{
    if (!limit)
        return E_POINTER;
    *limit = 0;
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
    if (urlString.isEmpty())
        return E_FAIL;

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
    if (urlString.isEmpty())
        return E_FAIL;

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
    const String& urlString = url.string();
    if (urlString.isEmpty())
        return;

    IWebHistoryItem* entry = m_entriesByURL.get(urlString);
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

        if (FAILED(entry->initWithURLString(BString(urlString), BString(title), lastVisited)))
            return;
        
        m_entriesByURL.set(urlString, entry);
    }

    COMPtr<IWebHistoryItemPrivate> entryPrivate(Query, entry);
    if (!entryPrivate)
        return;

    entryPrivate->setLastVisitWasFailure(wasFailure);

    COMPtr<WebHistoryItem> item(Query, entry);

    COMPtr<IPropertyBag> userInfo = createUserInfoFromHistoryItem(
        getNotificationString(kWebHistoryItemsAddedNotification), entry);
    postNotification(kWebHistoryItemsAddedNotification, userInfo.get());
}

HRESULT WebHistory::itemForURL(_In_ BSTR urlBStr, _COM_Outptr_opt_ IWebHistoryItem** item)
{
    if (!item)
        return E_POINTER;
    *item = nullptr;

    String urlString(urlBStr, SysStringLen(urlBStr));
    if (urlString.isEmpty())
        return E_FAIL;

    auto it = m_entriesByURL.find(urlString);
    if (it == m_entriesByURL.end())
        return E_FAIL;

    it->value.copyRefTo(item);
    return S_OK;
}

HRESULT WebHistory::removeItemForURLString(const WTF::String& urlString)
{
    if (urlString.isEmpty())
        return E_FAIL;

    auto it = m_entriesByURL.find(urlString);
    if (it == m_entriesByURL.end())
        return E_FAIL;

    if (!m_entriesByURL.size())
        WebVisitedLinkStore::removeAllVisitedLinks();

    return S_OK;
}

COMPtr<IWebHistoryItem> WebHistory::itemForURLString(const String& urlString) const
{
    if (urlString.isEmpty())
        return nullptr;
    return m_entriesByURL.get(urlString);
}

void WebHistory::addVisitedLinksToVisitedLinkStore(WebVisitedLinkStore& visitedLinkStore)
{
    for (auto& url : m_entriesByURL.keys())
        visitedLinkStore.addVisitedLink(url);
}
