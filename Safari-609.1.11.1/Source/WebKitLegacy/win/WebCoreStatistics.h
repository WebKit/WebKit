/*
 * Copyright (C) 2008, 2014-2015 Apple Inc. All Rights Reserved.
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

#ifndef WebCoreStatistics_h
#define WebCoreStatistics_h

#include "WebKit.h"

class WebCoreStatistics final : public IWebCoreStatistics {
public:
    static WebCoreStatistics* createInstance();
protected:
    WebCoreStatistics();
    ~WebCoreStatistics();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebCoreStatistics
    virtual HRESULT STDMETHODCALLTYPE javaScriptObjectsCount(_Out_ UINT*);
    virtual HRESULT STDMETHODCALLTYPE javaScriptGlobalObjectsCount(_Out_ UINT*);
    virtual HRESULT STDMETHODCALLTYPE javaScriptProtectedObjectsCount(_Out_ UINT*);
    virtual HRESULT STDMETHODCALLTYPE javaScriptProtectedGlobalObjectsCount(_Out_ UINT*);
    virtual HRESULT STDMETHODCALLTYPE javaScriptProtectedObjectTypeCounts(_COM_Outptr_opt_ IPropertyBag2** typeNamesAndCounts);
    virtual HRESULT STDMETHODCALLTYPE iconPageURLMappingCount(_Out_ UINT*);
    virtual HRESULT STDMETHODCALLTYPE iconRetainedPageURLCount(_Out_ UINT*);
    virtual HRESULT STDMETHODCALLTYPE iconRecordCount(_Out_ UINT*);
    virtual HRESULT STDMETHODCALLTYPE iconsWithDataCount(_Out_ UINT*);
    virtual HRESULT STDMETHODCALLTYPE cachedFontDataCount(_Out_ UINT*);
    virtual HRESULT STDMETHODCALLTYPE cachedFontDataInactiveCount(_Out_ UINT*);
    virtual HRESULT STDMETHODCALLTYPE purgeInactiveFontData();
    virtual HRESULT STDMETHODCALLTYPE glyphPageCount(_Out_ UINT*);
    virtual HRESULT STDMETHODCALLTYPE javaScriptObjectTypeCounts(_COM_Outptr_opt_ IPropertyBag2** typeNamesAndCounts);
    virtual HRESULT STDMETHODCALLTYPE garbageCollectJavaScriptObjects();
    virtual HRESULT STDMETHODCALLTYPE garbageCollectJavaScriptObjectsOnAlternateThreadForDebugging(BOOL waitUntilDone);
    virtual HRESULT STDMETHODCALLTYPE setJavaScriptGarbageCollectorTimerEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE shouldPrintExceptions(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setShouldPrintExceptions(BOOL);
    virtual HRESULT STDMETHODCALLTYPE startIgnoringWebCoreNodeLeaks();
    virtual HRESULT STDMETHODCALLTYPE stopIgnoringWebCoreNodeLeaks();
    virtual HRESULT STDMETHODCALLTYPE memoryStatistics(_COM_Outptr_opt_ IPropertyBag** statistics);
    virtual HRESULT STDMETHODCALLTYPE returnFreeMemoryToSystem();
    virtual HRESULT STDMETHODCALLTYPE cachedPageCount(_Out_ INT*);
    virtual HRESULT STDMETHODCALLTYPE cachedFrameCount(_Out_ INT*);

protected:
    ULONG m_refCount { 0 };
};

#endif
