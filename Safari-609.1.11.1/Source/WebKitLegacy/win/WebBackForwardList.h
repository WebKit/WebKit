/*
 * Copyright (C) 2006-2007, 2015 Apple Inc.  All rights reserved.
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

#pragma once

#include "WebKit.h"

#include "WebHistoryItem.h"

#include <WTF/RefPtr.h>

class BackForwardList;

class WebBackForwardList final : public IWebBackForwardList, IWebBackForwardListPrivate
{
public:
    static WebBackForwardList* createInstance(RefPtr<BackForwardList>&&);
protected:
    WebBackForwardList(RefPtr<BackForwardList>&&);
    ~WebBackForwardList();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebBackForwardList
    virtual HRESULT STDMETHODCALLTYPE addItem(_In_opt_ IWebHistoryItem*);    
    virtual HRESULT STDMETHODCALLTYPE goBack();
    virtual HRESULT STDMETHODCALLTYPE goForward();    
    virtual HRESULT STDMETHODCALLTYPE goToItem(_In_opt_ IWebHistoryItem*);
    virtual HRESULT STDMETHODCALLTYPE backItem(_COM_Outptr_opt_ IWebHistoryItem**);
    virtual HRESULT STDMETHODCALLTYPE currentItem(_COM_Outptr_opt_ IWebHistoryItem**);
    virtual HRESULT STDMETHODCALLTYPE forwardItem(_COM_Outptr_opt_ IWebHistoryItem**);
    virtual HRESULT STDMETHODCALLTYPE backListWithLimit(int limit, _Out_ int* listCount, __deref_inout_opt IWebHistoryItem**);
    virtual HRESULT STDMETHODCALLTYPE forwardListWithLimit(int limit, _Out_ int *listCount, __deref_inout_opt IWebHistoryItem **list);
    virtual HRESULT STDMETHODCALLTYPE capacity(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE setCapacity(int);
    virtual HRESULT STDMETHODCALLTYPE backListCount(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE forwardListCount(_Out_ int*);   
    virtual HRESULT STDMETHODCALLTYPE containsItem(_In_opt_ IWebHistoryItem*, _Out_ BOOL* result);   
    virtual HRESULT STDMETHODCALLTYPE itemAtIndex(int index, _COM_Outptr_opt_ IWebHistoryItem**);
    
    // IWebBackForwardListPrivate
    virtual HRESULT STDMETHODCALLTYPE removeItem(_In_opt_ IWebHistoryItem*);

protected:
    ULONG m_refCount { 0 };
    RefPtr<BackForwardList> m_backForwardList;
};
