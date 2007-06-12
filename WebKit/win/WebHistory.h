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

#ifndef WebHistory_H
#define WebHistory_H

#include "IWebHistory.h"

#include "COMPtr.h"
#include <CoreFoundation/CoreFoundation.h>
#include <wtf/RetainPtr.h>

//-----------------------------------------------------------------------------

// {3DE04E59-93F9-4369-8B43-976458D7E319}
DEFINE_GUID(IID_IWebHistoryPrivate, 0x3de04e59, 0x93f9, 0x4369, 0x8b, 0x43, 0x97, 0x64, 0x58, 0xd7, 0xe3, 0x19);

interface IWebHistoryPrivate : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE addItemForURL(
        /* [in] */ BSTR url,
        /* [in] */ BSTR title) = 0;
    virtual HRESULT STDMETHODCALLTYPE containsItemForURLString(
        /* [in] */ void* urlCFString,
        /* [retval][out] */ BOOL* contains) = 0;
};

//-----------------------------------------------------------------------------

class WebPreferences;

class WebHistory : public IWebHistory, IWebHistoryPrivate
{
public:
    static WebHistory* createInstance();
protected:
    WebHistory();
    ~WebHistory();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IWebHistory
    virtual HRESULT STDMETHODCALLTYPE optionalSharedHistory( 
        /* [retval][out] */ IWebHistory** history);
    
    virtual HRESULT STDMETHODCALLTYPE setOptionalSharedHistory( 
        /* [in] */ IWebHistory* history);
    
    virtual HRESULT STDMETHODCALLTYPE loadFromURL( 
        /* [in] */ BSTR url,
        /* [out] */ IWebError** error,
        /* [retval][out] */ BOOL* succeeded);
    
    virtual HRESULT STDMETHODCALLTYPE saveToURL( 
        /* [in] */ BSTR url,
        /* [out] */ IWebError** error,
        /* [retval][out] */ BOOL* succeeded);
    
    virtual HRESULT STDMETHODCALLTYPE addItems( 
        /* [in] */ int itemCount,
        /* [in] */ IWebHistoryItem** items);
    
    virtual HRESULT STDMETHODCALLTYPE removeItems( 
        /* [in] */ int itemCount,
        /* [in] */ IWebHistoryItem** items);
    
    virtual HRESULT STDMETHODCALLTYPE removeAllItems( void);
    
    virtual HRESULT STDMETHODCALLTYPE orderedLastVisitedDays( 
        /* [out][in] */ int* count,
        /* [in] */ DATE* calendarDates);
    
    virtual HRESULT STDMETHODCALLTYPE orderedItemsLastVisitedOnDay( 
        /* [out][in] */ int* count,
        /* [in] */ IWebHistoryItem** items,
        /* [in] */ DATE calendarDate);

    virtual HRESULT STDMETHODCALLTYPE itemForURL( 
        /* [in] */ BSTR url,
        /* [retval][out] */ IWebHistoryItem** item);
    
    virtual HRESULT STDMETHODCALLTYPE setHistoryItemLimit( 
        /* [in] */ int limit);
    
    virtual HRESULT STDMETHODCALLTYPE historyItemLimit( 
        /* [retval][out] */ int* limit);
    
    virtual HRESULT STDMETHODCALLTYPE setHistoryAgeInDaysLimit( 
        /* [in] */ int limit);
    
    virtual HRESULT STDMETHODCALLTYPE historyAgeInDaysLimit( 
        /* [retval][out] */ int* limit);

    // WebHistory
    static IWebHistoryPrivate* optionalSharedHistoryInternal();
    virtual HRESULT STDMETHODCALLTYPE addItemForURL(BSTR url, BSTR title);
    virtual HRESULT STDMETHODCALLTYPE containsItemForURLString(void* urlCFString, BOOL* contains);

protected:
    enum NotificationType
    {
        kWebHistoryItemsAddedNotification = 0,
        kWebHistoryItemsRemovedNotification = 1,
        kWebHistoryAllItemsRemovedNotification = 2,
        kWebHistoryLoadedNotification = 3,
        kWebHistoryItemsDiscardedWhileLoadingNotification = 4,
        kWebHistorySavedNotification = 5
    };

    HRESULT loadHistoryGutsFromURL(CFURLRef url, CFMutableArrayRef discardedItems, IWebError** error);
    HRESULT saveHistoryGuts(CFURLRef url, IWebError** error);
    HRESULT postNotification(NotificationType notifyType, IPropertyBag* userInfo = 0);
    HRESULT removeItem(IWebHistoryItem* entry);
    HRESULT addItem(IWebHistoryItem* entry);
    HRESULT removeItemForURLString(CFStringRef urlString);
    HRESULT addItemToDateCaches(IWebHistoryItem* entry);
    HRESULT removeItemFromDateCaches(IWebHistoryItem* entry);
    HRESULT insertItem(IWebHistoryItem* entry, int dateIndex);
    HRESULT ageLimitDate(CFAbsoluteTime* time);
    HRESULT datesArray(CFMutableArrayRef* datesArray);
    bool findIndex(int* index, CFAbsoluteTime forDay);
    static CFAbsoluteTime timeToDate(CFAbsoluteTime time);
    BSTR getNotificationString(NotificationType notifyType);
    HRESULT itemForURLString(CFStringRef urlString, IWebHistoryItem** item);

protected:
    ULONG m_refCount;
    RetainPtr<CFMutableDictionaryRef> m_entriesByURL;
    RetainPtr<CFMutableArrayRef> m_datesWithEntries;
    RetainPtr<CFMutableArrayRef> m_entriesByDate;
    COMPtr<WebPreferences> m_preferences;
    static IWebHistory* m_optionalSharedHistory;
};

#endif
