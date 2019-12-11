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

#ifndef WebHistoryItem_H
#define WebHistoryItem_H

#include "WebKit.h"

#include <CoreFoundation/CoreFoundation.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
    class HistoryItem;
}

//-----------------------------------------------------------------------------

class WebHistoryItem final : public IWebHistoryItem, IWebHistoryItemPrivate
{
public:
    static WebHistoryItem* createInstance();
    static WebHistoryItem* createInstance(RefPtr<WebCore::HistoryItem>&&);
protected:
    WebHistoryItem(RefPtr<WebCore::HistoryItem>&&);
    ~WebHistoryItem();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebHistoryItem
    virtual HRESULT STDMETHODCALLTYPE initWithURLString(_In_ BSTR urlString, _In_ BSTR title, DATE lastVisited);
    virtual HRESULT STDMETHODCALLTYPE originalURLString(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE URLString(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE title(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE lastVisitedTimeInterval(_Out_ DATE*);
    virtual HRESULT STDMETHODCALLTYPE setAlternateTitle(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE alternateTitle(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE icon(__deref_opt_out HBITMAP*);

    // IWebHistoryItemPrivate
    virtual HRESULT STDMETHODCALLTYPE initFromDictionaryRepresentation(_In_opt_ void* dictionary);
    virtual HRESULT STDMETHODCALLTYPE dictionaryRepresentation(__deref_out_opt void** dictionary);
    virtual HRESULT STDMETHODCALLTYPE visitCount(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE setVisitCount(int);
    virtual HRESULT STDMETHODCALLTYPE hasURLString(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE mergeAutoCompleteHints(_In_opt_ IWebHistoryItem*);
    virtual HRESULT STDMETHODCALLTYPE setLastVisitedTimeInterval(DATE time);
    virtual HRESULT STDMETHODCALLTYPE setTitle(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE RSSFeedReferrer(__deref_out_opt BSTR* url);
    virtual HRESULT STDMETHODCALLTYPE setRSSFeedReferrer(_In_ BSTR url);
    virtual HRESULT STDMETHODCALLTYPE hasPageCache(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setHasPageCache(BOOL);
    virtual HRESULT STDMETHODCALLTYPE target(__deref_out_opt BSTR*);
    virtual HRESULT STDMETHODCALLTYPE isTargetItem(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE children(unsigned* childCount, SAFEARRAY** children);
    virtual HRESULT STDMETHODCALLTYPE lastVisitWasFailure(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setLastVisitWasFailure(BOOL);
    virtual HRESULT STDMETHODCALLTYPE lastVisitWasHTTPNonGet(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setLastVisitWasHTTPNonGet(BOOL);
    virtual HRESULT STDMETHODCALLTYPE redirectURLs(_COM_Outptr_opt_ IEnumVARIANT**);
    virtual HRESULT STDMETHODCALLTYPE visitedWithTitle(_In_ BSTR title, BOOL increaseVisitCount);
    virtual HRESULT STDMETHODCALLTYPE getDailyVisitCounts(_Out_ int* number, __deref_out_opt int** counts);
    virtual HRESULT STDMETHODCALLTYPE getWeeklyVisitCounts(_Out_ int* number, __deref_out_opt int** counts);
    virtual HRESULT STDMETHODCALLTYPE recordInitialVisit();

    // WebHistoryItem
    WebCore::HistoryItem* historyItem() const;

protected:
    ULONG m_refCount { 0 };

    RefPtr<WebCore::HistoryItem> m_historyItem;
    WTF::String m_alternateTitle;
};

#endif
