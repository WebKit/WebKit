/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "WebHistoryItem.h"

#include "COMEnumVariant.h"
#include "MarshallingHelpers.h"
#include "WebKit.h"
#include <WebCore/BString.h>
#include <WebCore/COMPtr.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/URL.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/CString.h>

using namespace WebCore;

// WebHistoryItem ----------------------------------------------------------------

static HashMap<HistoryItem*, WebHistoryItem*>& historyItemWrappers()
{
    static HashMap<HistoryItem*, WebHistoryItem*> staticHistoryItemWrappers;
    return staticHistoryItemWrappers;
}

WebHistoryItem::WebHistoryItem(PassRefPtr<HistoryItem> historyItem)
: m_refCount(0)
, m_historyItem(historyItem)
{
    ASSERT(!historyItemWrappers().contains(m_historyItem.get()));
    historyItemWrappers().set(m_historyItem.get(), this);

    gClassCount++;
    gClassNameCount.add("WebHistoryItem");
}

WebHistoryItem::~WebHistoryItem()
{
    ASSERT(historyItemWrappers().contains(m_historyItem.get()));
    historyItemWrappers().remove(m_historyItem.get());

    gClassCount--;
    gClassNameCount.remove("WebHistoryItem");
}

WebHistoryItem* WebHistoryItem::createInstance()
{
    WebHistoryItem* instance = new WebHistoryItem(HistoryItem::create());
    instance->AddRef();
    return instance;
}

WebHistoryItem* WebHistoryItem::createInstance(PassRefPtr<HistoryItem> historyItem)
{
    WebHistoryItem* instance;

    instance = historyItemWrappers().get(historyItem.get());

    if (!instance)
        instance = new WebHistoryItem(historyItem);

    instance->AddRef();
    return instance;
}

// IWebHistoryItemPrivate -----------------------------------------------------

static CFStringRef urlKey = CFSTR("");
static CFStringRef titleKey = CFSTR("title");
static CFStringRef lastVisitWasFailureKey = CFSTR("lastVisitWasFailure");
static CFStringRef redirectURLsKey = CFSTR("redirectURLs");

HRESULT STDMETHODCALLTYPE WebHistoryItem::initFromDictionaryRepresentation(void* dictionary)
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

    if (redirectURLsVector.get())
        m_historyItem->setRedirectURLs(WTF::move(redirectURLsVector));

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::dictionaryRepresentation(void** dictionary)
{
    CFDictionaryRef* dictionaryRef = (CFDictionaryRef*) dictionary;

    int keyCount = 0;
    CFTypeRef keys[9];
    CFTypeRef values[9];

    if (!m_historyItem->urlString().isEmpty()) {
        keys[keyCount] = urlKey;
        values[keyCount++] = m_historyItem->urlString().createCFString().leakRef();
    }

    if (!m_historyItem->title().isEmpty()) {
        keys[keyCount] = titleKey;
        values[keyCount++] = m_historyItem->title().createCFString().leakRef();
    }

    if (m_historyItem->lastVisitWasFailure()) {
        keys[keyCount] = lastVisitWasFailureKey;
        values[keyCount++] = CFRetain(kCFBooleanTrue);
    }

    if (Vector<String>* redirectURLs = m_historyItem->redirectURLs()) {
        size_t size = redirectURLs->size();
        ASSERT(size);
        CFStringRef* items = new CFStringRef[size];
        for (size_t i = 0; i < size; ++i)
            items[i] = redirectURLs->at(i).createCFString().leakRef();
        CFArrayRef result = CFArrayCreate(0, (const void**)items, size, &kCFTypeArrayCallBacks);
        for (size_t i = 0; i < size; ++i)
            CFRelease(items[i]);
        delete[] items;

        keys[keyCount] = redirectURLsKey;
        values[keyCount++] = result;
    }

    *dictionaryRef = CFDictionaryCreate(0, keys, values, keyCount, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    for (int i = 0; i < keyCount; ++i)
        CFRelease(values[i]);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::hasURLString(BOOL *hasURL)
{
    *hasURL = m_historyItem->urlString().isEmpty() ? FALSE : TRUE;
    return S_OK;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT STDMETHODCALLTYPE WebHistoryItem::visitCount(int *count)
{
    return E_NOTIMPL;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT STDMETHODCALLTYPE WebHistoryItem::setVisitCount(int count)
{
    return E_NOTIMPL;

}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT STDMETHODCALLTYPE WebHistoryItem::mergeAutoCompleteHints(IWebHistoryItem* otherItem)
{
    return E_NOTIMPL;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT STDMETHODCALLTYPE WebHistoryItem::setLastVisitedTimeInterval(DATE time)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::setTitle(BSTR title)
{
    m_historyItem->setTitle(String(title, SysStringLen(title)));

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::RSSFeedReferrer(BSTR* url)
{
    BString str(m_historyItem->referrer());
    *url = str.release();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::setRSSFeedReferrer(BSTR url)
{
    m_historyItem->setReferrer(String(url, SysStringLen(url)));

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::hasPageCache(BOOL* /*hasCache*/)
{
    // FIXME - TODO
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::setHasPageCache(BOOL /*hasCache*/)
{
    // FIXME - TODO
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::target(BSTR* target)
{
    if (!target) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *target = BString(m_historyItem->target()).release();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::isTargetItem(BOOL* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = m_historyItem->isTargetItem() ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::children(unsigned* outChildCount, SAFEARRAY** outChildren)
{
    if (!outChildCount || !outChildren) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *outChildCount = 0;
    *outChildren = 0;

    const HistoryItemVector& coreChildren = m_historyItem->children();
    if (coreChildren.isEmpty())
        return S_OK;
    size_t childCount = coreChildren.size();

    SAFEARRAY* children = SafeArrayCreateVector(VT_UNKNOWN, 0, static_cast<ULONG>(childCount));
    if (!children)
        return E_OUTOFMEMORY;

    for (unsigned i = 0; i < childCount; ++i) {
        COMPtr<WebHistoryItem> item(AdoptCOM, WebHistoryItem::createInstance(coreChildren[i]));
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

HRESULT STDMETHODCALLTYPE WebHistoryItem::lastVisitWasFailure(BOOL* wasFailure)
{
    if (!wasFailure) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *wasFailure = m_historyItem->lastVisitWasFailure();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::setLastVisitWasFailure(BOOL wasFailure)
{
    m_historyItem->setLastVisitWasFailure(wasFailure);
    return S_OK;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT STDMETHODCALLTYPE WebHistoryItem::lastVisitWasHTTPNonGet(BOOL* HTTPNonGet)
{
    return E_NOTIMPL;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT STDMETHODCALLTYPE WebHistoryItem::setLastVisitWasHTTPNonGet(BOOL HTTPNonGet)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::redirectURLs(IEnumVARIANT** urls)
{
    if (!urls) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    Vector<String>* urlVector = m_historyItem->redirectURLs();
    if (!urlVector) {
        *urls = 0;
        return S_OK;
    }

    COMPtr<COMEnumVariant<Vector<String> > > enumVariant(AdoptCOM, COMEnumVariant<Vector<String> >::createInstance(*urlVector));
    *urls = enumVariant.leakRef();

    return S_OK;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT STDMETHODCALLTYPE WebHistoryItem::visitedWithTitle(BSTR title, BOOL increaseVisitCount)
{
    return E_NOTIMPL;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT STDMETHODCALLTYPE WebHistoryItem::getDailyVisitCounts(int* number, int** counts)
{
    return E_NOTIMPL;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT STDMETHODCALLTYPE WebHistoryItem::getWeeklyVisitCounts(int* number, int** counts)
{
    return E_NOTIMPL;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT STDMETHODCALLTYPE WebHistoryItem::recordInitialVisit()
{
    // FIXME: This function should be removed from the IWebHistoryItem interface.
    return E_NOTIMPL;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebHistoryItem::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
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

ULONG STDMETHODCALLTYPE WebHistoryItem::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebHistoryItem::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebHistoryItem -------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebHistoryItem::initWithURLString(
    /* [in] */ BSTR urlString,
    /* [in] */ BSTR title,
    /* [in] */ DATE lastVisited)
{
    historyItemWrappers().remove(m_historyItem.get());
    m_historyItem = HistoryItem::create(String(urlString, SysStringLen(urlString)), String(title, SysStringLen(title)));
    historyItemWrappers().set(m_historyItem.get(), this);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::originalURLString( 
    /* [retval][out] */ BSTR* url)
{
    if (!url)
        return E_POINTER;

    BString str = m_historyItem->originalURLString();
    *url = str.release();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::URLString( 
    /* [retval][out] */ BSTR* url)
{
    if (!url)
        return E_POINTER;

    BString str = m_historyItem->urlString();
    *url = str.release();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::title( 
    /* [retval][out] */ BSTR* pageTitle)
{
    if (!pageTitle)
        return E_POINTER;

    BString str(m_historyItem->title());
    *pageTitle = str.release();
    return S_OK;
}

// FIXME: This function should be removed from the IWebHistoryItem interface.
HRESULT STDMETHODCALLTYPE WebHistoryItem::lastVisitedTimeInterval( 
    /* [retval][out] */ DATE* lastVisited)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::setAlternateTitle( 
    /* [in] */ BSTR title)
{
    m_alternateTitle = String(title, SysStringLen(title));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::alternateTitle( 
    /* [retval][out] */ BSTR* title)
{
    if (!title) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *title = BString(m_alternateTitle).release();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::icon(/* [out, retval] */ HBITMAP* /*hBitmap*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// WebHistoryItem -------------------------------------------------------------

HistoryItem* WebHistoryItem::historyItem() const
{
    return m_historyItem.get();
}
