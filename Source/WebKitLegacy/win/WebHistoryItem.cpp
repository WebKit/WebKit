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

#include "WebKitDLL.h"
#include "WebHistoryItem.h"

#include "COMEnumVariant.h"
#include "MarshallingHelpers.h"
#include "WebKit.h"
#include <WebCore/BString.h>
#include <WebCore/COMPtr.h>
#include <WebCore/HistoryItem.h>
#include <wtf/RetainPtr.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>

using namespace WebCore;

// WebHistoryItem ----------------------------------------------------------------

static HashMap<HistoryItem*, WebHistoryItem*>& historyItemWrappers()
{
    static HashMap<HistoryItem*, WebHistoryItem*> staticHistoryItemWrappers;
    return staticHistoryItemWrappers;
}

WebHistoryItem::WebHistoryItem(RefPtr<HistoryItem>&& historyItem)
    : m_historyItem(WTFMove(historyItem))
{
    ASSERT(!historyItemWrappers().contains(m_historyItem.get()));
    historyItemWrappers().set(m_historyItem.get(), this);

    gClassCount++;
    gClassNameCount().add("WebHistoryItem");
}

WebHistoryItem::~WebHistoryItem()
{
    ASSERT(historyItemWrappers().contains(m_historyItem.get()));
    historyItemWrappers().remove(m_historyItem.get());

    gClassCount--;
    gClassNameCount().remove("WebHistoryItem");
}

WebHistoryItem* WebHistoryItem::createInstance()
{
    WebHistoryItem* instance = new WebHistoryItem(HistoryItem::create());
    instance->AddRef();
    return instance;
}

WebHistoryItem* WebHistoryItem::createInstance(RefPtr<HistoryItem>&& historyItem)
{
    WebHistoryItem* instance;

    instance = historyItemWrappers().get(historyItem.get());

    if (!instance)
        instance = new WebHistoryItem(WTFMove(historyItem));

    instance->AddRef();
    return instance;
}

// IWebHistoryItemPrivate -----------------------------------------------------

static CFStringRef urlKey = CFSTR("");
static CFStringRef titleKey = CFSTR("title");
static CFStringRef lastVisitWasFailureKey = CFSTR("lastVisitWasFailure");
static CFStringRef redirectURLsKey = CFSTR("redirectURLs");

HRESULT WebHistoryItem::initFromDictionaryRepresentation(_In_opt_ void* dictionary)
{
    CFDictionaryRef dictionaryRef = (CFDictionaryRef) dictionary;

    CFStringRef urlStringRef = (CFStringRef) CFDictionaryGetValue(dictionaryRef, urlKey);
    if (urlStringRef && CFGetTypeID(urlStringRef) != CFStringGetTypeID())
        return E_FAIL;

    CFStringRef titleRef = (CFStringRef) CFDictionaryGetValue(dictionaryRef, titleKey);
    if (titleRef && CFGetTypeID(titleRef) != CFStringGetTypeID())
        return E_FAIL;

    CFBooleanRef lastVisitWasFailureRef = static_cast<CFBooleanRef>(CFDictionaryGetValue(dictionaryRef, lastVisitWasFailureKey));
    if (lastVisitWasFailureRef && CFGetTypeID(lastVisitWasFailureRef) != CFBooleanGetTypeID())
        return E_FAIL;
    bool lastVisitWasFailure = lastVisitWasFailureRef && CFBooleanGetValue(lastVisitWasFailureRef);

    std::unique_ptr<Vector<String>> redirectURLsVector;
    if (CFArrayRef redirectURLsRef = static_cast<CFArrayRef>(CFDictionaryGetValue(dictionaryRef, redirectURLsKey))) {
        CFIndex size = CFArrayGetCount(redirectURLsRef);
        redirectURLsVector = std::make_unique<Vector<String>>(size);
        for (CFIndex i = 0; i < size; ++i)
            (*redirectURLsVector)[i] = String(static_cast<CFStringRef>(CFArrayGetValueAtIndex(redirectURLsRef, i)));
    }

    historyItemWrappers().remove(m_historyItem.get());
    m_historyItem = HistoryItem::create(urlStringRef, titleRef);
    historyItemWrappers().set(m_historyItem.get(), this);

    if (lastVisitWasFailure)
        m_historyItem->setLastVisitWasFailure(true);

    return S_OK;
}

HRESULT WebHistoryItem::dictionaryRepresentation(__deref_out_opt void** dictionary)
{
    CFDictionaryRef* dictionaryRef = (CFDictionaryRef*) dictionary;

    size_t keyCount = 0;
    CFTypeRef keys[9];
    CFTypeRef values[9];

    if (!m_historyItem->urlString().isEmpty()) {
        keys[keyCount] = urlKey;
        values[keyCount] = m_historyItem->urlString().createCFString().leakRef();
        ++keyCount;
    }

    if (!m_historyItem->title().isEmpty()) {
        keys[keyCount] = titleKey;
        values[keyCount] = m_historyItem->title().createCFString().leakRef();
        ++keyCount;
    }

    if (m_historyItem->lastVisitWasFailure()) {
        keys[keyCount] = lastVisitWasFailureKey;
        values[keyCount] = CFRetain(kCFBooleanTrue);
        ++keyCount;
    }

    *dictionaryRef = CFDictionaryCreate(0, keys, values, keyCount, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    for (int i = 0; i < keyCount; ++i)
        CFRelease(values[i]);

    return S_OK;
}

HRESULT WebHistoryItem::hasURLString(_Out_ BOOL* hasURL)
{
    if (!hasURL)
        return E_POINTER;
    *hasURL = m_historyItem->urlString().isEmpty() ? FALSE : TRUE;
    return S_OK;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT WebHistoryItem::visitCount(_Out_ int* count)
{
    if (!count)
        return E_POINTER;
    return E_NOTIMPL;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT WebHistoryItem::setVisitCount(int count)
{
    return E_NOTIMPL;

}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT WebHistoryItem::mergeAutoCompleteHints(_In_opt_ IWebHistoryItem*)
{
    return E_NOTIMPL;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT WebHistoryItem::setLastVisitedTimeInterval(DATE time)
{
    return E_NOTIMPL;
}

HRESULT WebHistoryItem::setTitle(_In_ BSTR title)
{
    m_historyItem->setTitle(String(title, SysStringLen(title)));

    return S_OK;
}

HRESULT WebHistoryItem::RSSFeedReferrer(__deref_out_opt BSTR* url)
{
    if (!url)
        return E_POINTER;

    BString str(m_historyItem->referrer());
    *url = str.release();

    return S_OK;
}

HRESULT WebHistoryItem::setRSSFeedReferrer(_In_ BSTR url)
{
    m_historyItem->setReferrer(String(url, SysStringLen(url)));

    return S_OK;
}

HRESULT WebHistoryItem::hasPageCache(_Out_ BOOL* hasCache)
{
    // FIXME - TODO
    ASSERT_NOT_REACHED();
    if (!hasCache)
        return E_POINTER;
    *hasCache = FALSE;
    return E_NOTIMPL;
}

HRESULT WebHistoryItem::setHasPageCache(BOOL /*hasCache*/)
{
    // FIXME - TODO
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT WebHistoryItem::target(__deref_out_opt BSTR* target)
{
    if (!target) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *target = BString(m_historyItem->target()).release();
    return S_OK;
}

HRESULT WebHistoryItem::isTargetItem(_Out_ BOOL* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = m_historyItem->isTargetItem() ? TRUE : FALSE;
    return S_OK;
}

HRESULT WebHistoryItem::children(unsigned* outChildCount, SAFEARRAY** outChildren)
{
    if (!outChildCount || !outChildren) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *outChildCount = 0;
    *outChildren = 0;

    const auto& coreChildren = m_historyItem->children();
    if (coreChildren.isEmpty())
        return S_OK;
    size_t childCount = coreChildren.size();

    SAFEARRAY* children = SafeArrayCreateVector(VT_UNKNOWN, 0, static_cast<ULONG>(childCount));
    if (!children)
        return E_OUTOFMEMORY;

    for (unsigned i = 0; i < childCount; ++i) {
        COMPtr<WebHistoryItem> item(AdoptCOM, WebHistoryItem::createInstance(const_cast<HistoryItem*>(coreChildren[i].ptr())));
        if (!item) {
            SafeArrayDestroy(children);
            return E_OUTOFMEMORY;
        }

        LONG longI = i;
        HRESULT hr = SafeArrayPutElement(children, &longI, item.get());
        if (FAILED(hr)) {
            SafeArrayDestroy(children);
            return hr;
        }
    }

    *outChildCount = static_cast<unsigned>(childCount);
    *outChildren = children;
    return S_OK;

}

HRESULT WebHistoryItem::lastVisitWasFailure(_Out_ BOOL* wasFailure)
{
    if (!wasFailure) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *wasFailure = m_historyItem->lastVisitWasFailure();
    return S_OK;
}

HRESULT WebHistoryItem::setLastVisitWasFailure(BOOL wasFailure)
{
    m_historyItem->setLastVisitWasFailure(wasFailure);
    return S_OK;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT WebHistoryItem::lastVisitWasHTTPNonGet(_Out_ BOOL* HTTPNonGet)
{
    if (!HTTPNonGet)
        return E_POINTER;
    *HTTPNonGet = FALSE;
    return E_NOTIMPL;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT WebHistoryItem::setLastVisitWasHTTPNonGet(BOOL)
{
    return E_NOTIMPL;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT WebHistoryItem::redirectURLs(_COM_Outptr_opt_ IEnumVARIANT** urls)
{
    return E_NOTIMPL;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT WebHistoryItem::visitedWithTitle(_In_ BSTR title, BOOL increaseVisitCount)
{
    return E_NOTIMPL;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT WebHistoryItem::getDailyVisitCounts(_Out_ int* number, __deref_out_opt int** counts)
{
    if (!number || !counts)
        return E_POINTER;
    return E_NOTIMPL;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT WebHistoryItem::getWeeklyVisitCounts(_Out_ int* number, __deref_out_opt int** counts)
{
    if (!number || !counts)
        return E_POINTER;
    return E_NOTIMPL;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT WebHistoryItem::recordInitialVisit()
{
    // FIXME: This function should be removed from the IWebHistoryItem interface.
    return E_NOTIMPL;
}

// IUnknown -------------------------------------------------------------------

HRESULT WebHistoryItem::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, __uuidof(WebHistoryItem)))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebHistoryItem*>(this);
    else if (IsEqualGUID(riid, IID_IWebHistoryItem))
        *ppvObject = static_cast<IWebHistoryItem*>(this);
    else if (IsEqualGUID(riid, IID_IWebHistoryItemPrivate))
        *ppvObject = static_cast<IWebHistoryItemPrivate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebHistoryItem::AddRef()
{
    return ++m_refCount;
}

ULONG WebHistoryItem::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebHistoryItem -------------------------------------------------------------

HRESULT WebHistoryItem::initWithURLString(_In_ BSTR urlString, _In_ BSTR title, DATE lastVisited)
{
    historyItemWrappers().remove(m_historyItem.get());
    m_historyItem = HistoryItem::create(String(urlString, SysStringLen(urlString)), String(title, SysStringLen(title)));
    historyItemWrappers().set(m_historyItem.get(), this);

    return S_OK;
}

HRESULT WebHistoryItem::originalURLString(__deref_opt_out BSTR* url)
{
    if (!url)
        return E_POINTER;

    BString str = m_historyItem->originalURLString();
    *url = str.release();
    return S_OK;
}

HRESULT WebHistoryItem::URLString(__deref_opt_out BSTR* url)
{
    if (!url)
        return E_POINTER;

    BString str = m_historyItem->urlString();
    *url = str.release();
    return S_OK;
}

HRESULT WebHistoryItem::title(__deref_opt_out BSTR* pageTitle)
{
    if (!pageTitle)
        return E_POINTER;

    BString str(m_historyItem->title());
    *pageTitle = str.release();
    return S_OK;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT WebHistoryItem::lastVisitedTimeInterval(_Out_ DATE* lastVisited)
{
    if (!lastVisited)
        return E_POINTER;
    return E_NOTIMPL;
}

HRESULT WebHistoryItem::setAlternateTitle(_In_ BSTR title)
{
    m_alternateTitle = String(title, SysStringLen(title));
    return S_OK;
}

HRESULT WebHistoryItem::alternateTitle(__deref_opt_out BSTR* title)
{
    if (!title) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *title = BString(m_alternateTitle).release();
    return S_OK;
}

HRESULT WebHistoryItem::icon(__deref_opt_out HBITMAP* hBitmap)
{
    ASSERT_NOT_REACHED();
    if (!hBitmap)
        return E_POINTER;
    return E_NOTIMPL;
}

// WebHistoryItem -------------------------------------------------------------

HistoryItem* WebHistoryItem::historyItem() const
{
    return m_historyItem.get();
}
