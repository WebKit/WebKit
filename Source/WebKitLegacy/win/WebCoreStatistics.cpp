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

#include "WebKitDLL.h"
#include "WebCoreStatistics.h"

#include "COMPropertyBag.h"
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/MemoryStatistics.h>
#include <WebCore/CommonVM.h>
#include <WebCore/DOMWindow.h>
#include <WebCore/FontCache.h>
#include <WebCore/GCController.h>
#include <WebCore/GlyphPage.h>
#include <WebCore/JSDOMWindow.h>
#include <WebCore/PageCache.h>
#include <WebCore/PageConsoleClient.h>
#include <WebCore/RenderView.h>
#include <WebCore/SharedBuffer.h>

using namespace JSC;
using namespace WebCore;

// WebCoreStatistics ---------------------------------------------------------------------------

WebCoreStatistics::WebCoreStatistics()
{
    gClassCount++;
    gClassNameCount().add("WebCoreStatistics");
}

WebCoreStatistics::~WebCoreStatistics()
{
    gClassCount--;
    gClassNameCount().remove("WebCoreStatistics");
}

WebCoreStatistics* WebCoreStatistics::createInstance()
{
    WebCoreStatistics* instance = new WebCoreStatistics();
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT WebCoreStatistics::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<WebCoreStatistics*>(this);
    else if (IsEqualGUID(riid, IID_IWebCoreStatistics))
        *ppvObject = static_cast<WebCoreStatistics*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebCoreStatistics::AddRef()
{
    return ++m_refCount;
}

ULONG WebCoreStatistics::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebCoreStatistics ------------------------------------------------------------------------------

HRESULT WebCoreStatistics::javaScriptObjectsCount(_Out_ UINT* count)
{
    if (!count)
        return E_POINTER;

    JSLockHolder lock(commonVM());
    *count = (UINT)commonVM().heap.objectCount();
    return S_OK;
}

HRESULT WebCoreStatistics::javaScriptGlobalObjectsCount(_Out_ UINT* count)
{
    if (!count)
        return E_POINTER;

    JSLockHolder lock(commonVM());
    *count = (UINT)commonVM().heap.globalObjectCount();
    return S_OK;
}

HRESULT WebCoreStatistics::javaScriptProtectedObjectsCount(_Out_ UINT* count)
{
    if (!count)
        return E_POINTER;

    JSLockHolder lock(commonVM());
    *count = (UINT)commonVM().heap.protectedObjectCount();
    return S_OK;
}

HRESULT WebCoreStatistics::javaScriptProtectedGlobalObjectsCount(_Out_ UINT* count)
{
    if (!count)
        return E_POINTER;

    JSLockHolder lock(commonVM());
    *count = (UINT)commonVM().heap.protectedGlobalObjectCount();
    return S_OK;
}

HRESULT WebCoreStatistics::javaScriptProtectedObjectTypeCounts(_COM_Outptr_opt_ IPropertyBag2** typeNamesAndCounts)
{
    if (!typeNamesAndCounts)
        return E_POINTER;

    JSLockHolder lock(commonVM());
    std::unique_ptr<TypeCountSet> jsObjectTypeNames(commonVM().heap.protectedObjectTypeCounts());
    typedef TypeCountSet::const_iterator Iterator;
    Iterator end = jsObjectTypeNames->end();
    HashMap<String, int> typeCountMap;
    for (Iterator current = jsObjectTypeNames->begin(); current != end; ++current)
        typeCountMap.set(current->key, current->value);

    COMPtr<IPropertyBag2> results(AdoptCOM, COMPropertyBag<int>::createInstance(typeCountMap));
    results.copyRefTo(typeNamesAndCounts);
    return S_OK;
}

HRESULT WebCoreStatistics::javaScriptObjectTypeCounts(_COM_Outptr_opt_ IPropertyBag2** typeNamesAndCounts)
{
    if (!typeNamesAndCounts)
        return E_POINTER;

    JSLockHolder lock(commonVM());
    std::unique_ptr<TypeCountSet> jsObjectTypeNames(commonVM().heap.objectTypeCounts());
    typedef TypeCountSet::const_iterator Iterator;
    Iterator end = jsObjectTypeNames->end();
    HashMap<String, int> typeCountMap;
    for (Iterator current = jsObjectTypeNames->begin(); current != end; ++current)
        typeCountMap.set(current->key, current->value);

    COMPtr<IPropertyBag2> results(AdoptCOM, COMPropertyBag<int>::createInstance(typeCountMap));
    results.copyRefTo(typeNamesAndCounts);
    return S_OK;
}

HRESULT WebCoreStatistics::iconPageURLMappingCount(_Out_ UINT* count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT)0;
    return S_OK;
}

HRESULT WebCoreStatistics::iconRetainedPageURLCount(_Out_ UINT *count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT)0;
    return S_OK;
}

HRESULT WebCoreStatistics::iconRecordCount(_Out_ UINT *count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT)0;
    return S_OK;
}

HRESULT WebCoreStatistics::iconsWithDataCount(_Out_ UINT *count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT)0;
    return S_OK;
}

HRESULT WebCoreStatistics::cachedFontDataCount(_Out_ UINT *count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT) FontCache::singleton().fontCount();
    return S_OK;
}

HRESULT WebCoreStatistics::cachedFontDataInactiveCount(_Out_ UINT *count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT) FontCache::singleton().inactiveFontCount();
    return S_OK;
}

HRESULT WebCoreStatistics::purgeInactiveFontData(void)
{
    FontCache::singleton().purgeInactiveFontData();
    return S_OK;
}

HRESULT WebCoreStatistics::glyphPageCount(_Out_ UINT *count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT) GlyphPage::count();
    return S_OK;
}

HRESULT WebCoreStatistics::garbageCollectJavaScriptObjects()
{
    GCController::singleton().garbageCollectNow();
    return S_OK;
}

HRESULT WebCoreStatistics::garbageCollectJavaScriptObjectsOnAlternateThreadForDebugging(BOOL waitUntilDone)
{
    GCController::singleton().garbageCollectOnAlternateThreadForDebugging(waitUntilDone);
    return S_OK;
}

HRESULT WebCoreStatistics::setJavaScriptGarbageCollectorTimerEnabled(BOOL enable)
{
    GCController::singleton().setJavaScriptGarbageCollectorTimerEnabled(enable);
    return S_OK;
}

HRESULT WebCoreStatistics::shouldPrintExceptions(_Out_ BOOL* shouldPrint)
{
    if (!shouldPrint)
        return E_POINTER;

    JSLockHolder lock(commonVM());
    *shouldPrint = PageConsoleClient::shouldPrintExceptions();
    return S_OK;
}

HRESULT WebCoreStatistics::setShouldPrintExceptions(BOOL print)
{
    JSLockHolder lock(commonVM());
    PageConsoleClient::setShouldPrintExceptions(print);
    return S_OK;
}

HRESULT WebCoreStatistics::startIgnoringWebCoreNodeLeaks()
{
    WebCore::Node::startIgnoringLeaks();
    return S_OK;
}

HRESULT WebCoreStatistics::stopIgnoringWebCoreNodeLeaks()
{
    WebCore::Node::startIgnoringLeaks();
    return S_OK;
}

HRESULT WebCoreStatistics::memoryStatistics(_COM_Outptr_opt_ IPropertyBag** statistics)
{
    if (!statistics)
        return E_POINTER;

    WTF::FastMallocStatistics fastMallocStatistics = WTF::fastMallocStatistics();

    JSLockHolder lock(commonVM());
    unsigned long long heapSize = commonVM().heap.size();
    unsigned long long heapFree = commonVM().heap.capacity() - heapSize;
    GlobalMemoryStatistics globalMemoryStats = globalMemoryStatistics();

    HashMap<String, unsigned long long, ASCIICaseInsensitiveHash> fields;
    fields.add("FastMallocReservedVMBytes", static_cast<unsigned long long>(fastMallocStatistics.reservedVMBytes));
    fields.add("FastMallocCommittedVMBytes", static_cast<unsigned long long>(fastMallocStatistics.committedVMBytes));
    fields.add("FastMallocFreeListBytes", static_cast<unsigned long long>(fastMallocStatistics.freeListBytes));
    fields.add("JavaScriptHeapSize", heapSize);
    fields.add("JavaScriptFreeSize", heapFree);
    fields.add("JavaScriptStackSize", static_cast<unsigned long long>(globalMemoryStats.stackBytes));
    fields.add("JavaScriptJITSize", static_cast<unsigned long long>(globalMemoryStats.JITBytes));

    *statistics = COMPropertyBag<unsigned long long, String, ASCIICaseInsensitiveHash>::adopt(fields);

    return S_OK;
}

HRESULT WebCoreStatistics::returnFreeMemoryToSystem()
{
    WTF::releaseFastMallocFreeMemory();
    return S_OK;
}

HRESULT WebCoreStatistics::cachedPageCount(_Out_ INT* count)
{
    if (!count)
        return E_POINTER;

    *count = PageCache::singleton().pageCount();
    return S_OK;
}

HRESULT WebCoreStatistics::cachedFrameCount(_Out_ INT* count)
{
    if (!count)
        return E_POINTER;

    *count = PageCache::singleton().frameCount();
    return S_OK;
}
