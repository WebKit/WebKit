/*
 * Copyright (C) 2006-2008, 2015 Apple Inc. All rights reserved.
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

#ifndef WebHistory_H
#define WebHistory_H

#include "WebKit.h"
#include <WebCore/COMPtr.h>
#include <memory>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>

namespace WebCore {
    class URL;
    class PageGroup;
}

//-----------------------------------------------------------------------------

class WebPreferences;
class WebVisitedLinkStore;

class WebHistory : public IWebHistory, public IWebHistoryPrivate {
public:
    static WebHistory* createInstance();
private:
    WebHistory();
    ~WebHistory();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebHistory
    virtual HRESULT STDMETHODCALLTYPE optionalSharedHistory(_COM_Outptr_opt_ IWebHistory**);
    virtual HRESULT STDMETHODCALLTYPE setOptionalSharedHistory(_In_opt_ IWebHistory*);
    HRESULT STDMETHODCALLTYPE unused1() override;
    HRESULT STDMETHODCALLTYPE unused2() override;
    virtual HRESULT STDMETHODCALLTYPE addItems(int itemCount, __deref_in_ecount_opt(itemCount) IWebHistoryItem**);
    virtual HRESULT STDMETHODCALLTYPE removeItems(int itemCount, __deref_in_ecount_opt(itemCount) IWebHistoryItem**);
    virtual HRESULT STDMETHODCALLTYPE removeAllItems();
    virtual HRESULT STDMETHODCALLTYPE orderedLastVisitedDays(_Inout_ int* count, _In_ DATE* calendarDates);
    virtual HRESULT STDMETHODCALLTYPE orderedItemsLastVisitedOnDay(_Inout_ int* count, __deref_in_opt IWebHistoryItem** items, DATE calendarDate);
    virtual HRESULT STDMETHODCALLTYPE itemForURL(_In_ BSTR url, _COM_Outptr_opt_ IWebHistoryItem**);
    virtual HRESULT STDMETHODCALLTYPE setHistoryItemLimit(int);
    virtual HRESULT STDMETHODCALLTYPE historyItemLimit(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE setHistoryAgeInDaysLimit(int);
    virtual HRESULT STDMETHODCALLTYPE historyAgeInDaysLimit(_Out_ int*);

    // IWebHistoryPrivate
    virtual HRESULT STDMETHODCALLTYPE allItems(_Inout_ int* count, __deref_opt_out IWebHistoryItem** items);
    virtual HRESULT STDMETHODCALLTYPE setVisitedLinkTrackingEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE removeAllVisitedLinks();

    // WebHistory
    static WebHistory* sharedHistory();
    void visitedURL(const WebCore::URL&, const WTF::String& title, const WTF::String& httpMethod, bool wasFailure, bool increaseVisitCount);
    void addVisitedLinksToVisitedLinkStore(WebVisitedLinkStore&);

    COMPtr<IWebHistoryItem> itemForURLString(const WTF::String&) const;

    typedef int64_t DateKey;
    typedef HashMap<DateKey, Vector<COMPtr<IWebHistoryItem>>> DateToEntriesMap;
    typedef HashMap<WTF::String, COMPtr<IWebHistoryItem>> URLToEntriesMap;

private:

    enum NotificationType
    {
        kWebHistoryItemsAddedNotification = 0,
        kWebHistoryItemsRemovedNotification = 1,
        kWebHistoryAllItemsRemovedNotification = 2,
        kWebHistoryLoadedNotification = 3,
        kWebHistoryItemsDiscardedWhileLoadingNotification = 4,
        kWebHistorySavedNotification = 5
    };

    HRESULT postNotification(NotificationType notifyType, IPropertyBag* userInfo = 0);
    HRESULT removeItem(IWebHistoryItem* entry);
    HRESULT addItem(IWebHistoryItem* entry, bool discardDuplicate, bool* added);
    HRESULT removeItemForURLString(const WTF::String& urlString);
    BSTR getNotificationString(NotificationType notifyType);

    ULONG m_refCount { 0 };
    URLToEntriesMap m_entriesByURL;
    COMPtr<WebPreferences> m_preferences;
};

#endif
