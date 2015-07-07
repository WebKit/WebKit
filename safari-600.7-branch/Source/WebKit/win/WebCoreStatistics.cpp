/*
 * Copyright (C) 2008, 2014 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "WebKitDLL.h"
#include "WebCoreStatistics.h"

#include "COMPropertyBag.h"
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/MemoryStatistics.h>
#include <WebCore/FontCache.h>
#include <WebCore/GCController.h>
#include <WebCore/GlyphPageTreeNode.h>
#include <WebCore/IconDatabase.h>
#include <WebCore/JSDOMWindow.h>
#include <WebCore/PageCache.h>
#include <WebCore/PageConsole.h>
#include <WebCore/RenderView.h>
#include <WebCore/SharedBuffer.h>

using namespace JSC;
using namespace WebCore;

// WebCoreStatistics ---------------------------------------------------------------------------

WebCoreStatistics::WebCoreStatistics()
: m_refCount(0)
{
    gClassCount++;
    gClassNameCount.add("WebCoreStatistics");
}

WebCoreStatistics::~WebCoreStatistics()
{
    gClassCount--;
    gClassNameCount.remove("WebCoreStatistics");
}

WebCoreStatistics* WebCoreStatistics::createInstance()
{
    WebCoreStatistics* instance = new WebCoreStatistics();
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebCoreStatistics::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<WebCoreStatistics*>(this);
    else if (IsEqualGUID(riid, IID_IWebCoreStatistics))
        *ppvObject = static_cast<WebCoreStatistics*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebCoreStatistics::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebCoreStatistics::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebCoreStatistics ------------------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebCoreStatistics::javaScriptObjectsCount( 
    /* [retval][out] */ UINT* count)
{
    if (!count)
        return E_POINTER;

    JSLockHolder lock(JSDOMWindow::commonVM());
    *count = (UINT)JSDOMWindow::commonVM().heap.objectCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::javaScriptGlobalObjectsCount( 
    /* [retval][out] */ UINT* count)
{
    if (!count)
        return E_POINTER;

    JSLockHolder lock(JSDOMWindow::commonVM());
    *count = (UINT)JSDOMWindow::commonVM().heap.globalObjectCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::javaScriptProtectedObjectsCount( 
    /* [retval][out] */ UINT* count)
{
    if (!count)
        return E_POINTER;

    JSLockHolder lock(JSDOMWindow::commonVM());
    *count = (UINT)JSDOMWindow::commonVM().heap.protectedObjectCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::javaScriptProtectedGlobalObjectsCount( 
    /* [retval][out] */ UINT* count)
{
    if (!count)
        return E_POINTER;

    JSLockHolder lock(JSDOMWindow::commonVM());
    *count = (UINT)JSDOMWindow::commonVM().heap.protectedGlobalObjectCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::javaScriptProtectedObjectTypeCounts( 
    /* [retval][out] */ IPropertyBag2** typeNamesAndCounts)
{
    JSLockHolder lock(JSDOMWindow::commonVM());
    OwnPtr<TypeCountSet> jsObjectTypeNames(JSDOMWindow::commonVM().heap.protectedObjectTypeCounts());
    typedef TypeCountSet::const_iterator Iterator;
    Iterator end = jsObjectTypeNames->end();
    HashMap<String, int> typeCountMap;
    for (Iterator current = jsObjectTypeNames->begin(); current != end; ++current)
        typeCountMap.set(current->key, current->value);

    COMPtr<IPropertyBag2> results(AdoptCOM, COMPropertyBag<int>::createInstance(typeCountMap));
    results.copyRefTo(typeNamesAndCounts);
    return S_OK;
}

HRESULT WebCoreStatistics::javaScriptObjectTypeCounts(IPropertyBag2** typeNamesAndCounts)
{
    if (!typeNamesAndCounts)
        return E_POINTER;

    JSLockHolder lock(JSDOMWindow::commonVM());
    OwnPtr<TypeCountSet> jsObjectTypeNames(JSDOMWindow::commonVM().heap.objectTypeCounts());
    typedef TypeCountSet::const_iterator Iterator;
    Iterator end = jsObjectTypeNames->end();
    HashMap<String, int> typeCountMap;
    for (Iterator current = jsObjectTypeNames->begin(); current != end; ++current)
        typeCountMap.set(current->key, current->value);

    COMPtr<IPropertyBag2> results(AdoptCOM, COMPropertyBag<int>::createInstance(typeCountMap));
    results.copyRefTo(typeNamesAndCounts);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::iconPageURLMappingCount( 
    /* [retval][out] */ UINT* count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT) iconDatabase().pageURLMappingCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::iconRetainedPageURLCount( 
    /* [retval][out] */ UINT *count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT) iconDatabase().retainedPageURLCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::iconRecordCount( 
    /* [retval][out] */ UINT *count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT) iconDatabase().iconRecordCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::iconsWithDataCount( 
    /* [retval][out] */ UINT *count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT) iconDatabase().iconRecordCountWithData();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::cachedFontDataCount( 
    /* [retval][out] */ UINT *count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT) fontCache().fontDataCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::cachedFontDataInactiveCount( 
    /* [retval][out] */ UINT *count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT) fontCache().inactiveFontDataCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::purgeInactiveFontData(void)
{
    fontCache().purgeInactiveFontData();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::glyphPageCount( 
    /* [retval][out] */ UINT *count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT) GlyphPageTreeNode::treeGlyphPageCount();
    return S_OK;
}

HRESULT WebCoreStatistics::garbageCollectJavaScriptObjects()
{
    gcController().garbageCollectNow();
    return S_OK;
}

HRESULT WebCoreStatistics::garbageCollectJavaScriptObjectsOnAlternateThreadForDebugging(BOOL waitUntilDone)
{
    gcController().garbageCollectOnAlternateThreadForDebugging(waitUntilDone);
    return S_OK;
}

HRESULT WebCoreStatistics::setJavaScriptGarbageCollectorTimerEnabled(BOOL enable)
{
    gcController().setJavaScriptGarbageCollectorTimerEnabled(enable);
    return S_OK;
}

HRESULT WebCoreStatistics::shouldPrintExceptions(BOOL* shouldPrint)
{
    if (!shouldPrint)
        return E_POINTER;

    JSLockHolder lock(JSDOMWindow::commonVM());
    *shouldPrint = PageConsole::shouldPrintExceptions();
    return S_OK;
}

HRESULT WebCoreStatistics::setShouldPrintExceptions(BOOL print)
{
    JSLockHolder lock(JSDOMWindow::commonVM());
    PageConsole::setShouldPrintExceptions(print);
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

HRESULT WebCoreStatistics::memoryStatistics(IPropertyBag** statistics)
{
    if (!statistics)
        return E_POINTER;

    WTF::FastMallocStatistics fastMallocStatistics = WTF::fastMallocStatistics();

    JSLockHolder lock(JSDOMWindow::commonVM());
    unsigned long long heapSize = JSDOMWindow::commonVM().heap.size();
    unsigned long long heapFree = JSDOMWindow::commonVM().heap.capacity() - heapSize;
    GlobalMemoryStatistics globalMemoryStats = globalMemoryStatistics();

    HashMap<String, unsigned long long, CaseFoldingHash> fields;
    fields.add("FastMallocReservedVMBytes", static_cast<unsigned long long>(fastMallocStatistics.reservedVMBytes));
    fields.add("FastMallocCommittedVMBytes", static_cast<unsigned long long>(fastMallocStatistics.committedVMBytes));
    fields.add("FastMallocFreeListBytes", static_cast<unsigned long long>(fastMallocStatistics.freeListBytes));
    fields.add("JavaScriptHeapSize", heapSize);
    fields.add("JavaScriptFreeSize", heapFree);
    fields.add("JavaScriptStackSize", static_cast<unsigned long long>(globalMemoryStats.stackBytes));
    fields.add("JavaScriptJITSize", static_cast<unsigned long long>(globalMemoryStats.JITBytes));

    *statistics = COMPropertyBag<unsigned long long, String, CaseFoldingHash>::adopt(fields);

    return S_OK;
}

HRESULT WebCoreStatistics::returnFreeMemoryToSystem()
{
    WTF::releaseFastMallocFreeMemory();
    return S_OK;
}

HRESULT WebCoreStatistics::cachedPageCount(INT* count)
{
    if (!count)
        return E_POINTER;

    *count = pageCache()->pageCount();
    return S_OK;
}

HRESULT WebCoreStatistics::cachedFrameCount(INT* count)
{
    if (!count)
        return E_POINTER;

    *count = pageCache()->frameCount();
    return S_OK;
}
