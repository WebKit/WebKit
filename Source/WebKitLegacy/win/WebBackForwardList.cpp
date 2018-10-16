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

#include "WebKitDLL.h"
#include "WebBackForwardList.h"

#include "BackForwardList.h"
#include "WebFrame.h"
#include "WebKit.h"
#include "WebPreferences.h"

#include <WebCore/COMPtr.h>
#include <WebCore/HistoryItem.h>

using std::min;
using namespace WebCore;

// WebBackForwardList ----------------------------------------------------------------

// FIXME: Instead of this we could just create a class derived from BackForwardList
// with a pointer to a WebBackForwardList in it.
static HashMap<BackForwardList*, WebBackForwardList*>& backForwardListWrappers()
{
    static HashMap<BackForwardList*, WebBackForwardList*> staticBackForwardListWrappers;
    return staticBackForwardListWrappers;
}

WebBackForwardList::WebBackForwardList(RefPtr<BackForwardList>&& backForwardList)
    : m_backForwardList(WTFMove(backForwardList))
{
    ASSERT(!backForwardListWrappers().contains(m_backForwardList.get()));
    backForwardListWrappers().set(m_backForwardList.get(), this);

    gClassCount++;
    gClassNameCount().add("WebBackForwardList");
}

WebBackForwardList::~WebBackForwardList()
{
    ASSERT(m_backForwardList->closed());

    ASSERT(backForwardListWrappers().contains(m_backForwardList.get()));
    backForwardListWrappers().remove(m_backForwardList.get());

    gClassCount--;
    gClassNameCount().remove("WebBackForwardList");
}

WebBackForwardList* WebBackForwardList::createInstance(RefPtr<BackForwardList>&& backForwardList)
{
    WebBackForwardList* instance = backForwardListWrappers().get(backForwardList.get());

    if (!instance)
        instance = new WebBackForwardList(WTFMove(backForwardList));

    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT WebBackForwardList::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebBackForwardList*>(this);
    else if (IsEqualGUID(riid, IID_IWebBackForwardList))
        *ppvObject = static_cast<IWebBackForwardList*>(this);
    else if (IsEqualGUID(riid, IID_IWebBackForwardListPrivate))
        *ppvObject = static_cast<IWebBackForwardListPrivate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebBackForwardList::AddRef()
{
    return ++m_refCount;
}

ULONG WebBackForwardList::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebBackForwardList ---------------------------------------------------------

HRESULT WebBackForwardList::addItem(_In_opt_ IWebHistoryItem* item)
{
    COMPtr<WebHistoryItem> webHistoryItem;
 
    if (!item || FAILED(item->QueryInterface(&webHistoryItem)))
        return E_FAIL;
 
    m_backForwardList->addItem(*webHistoryItem->historyItem());
    return S_OK;
}

HRESULT WebBackForwardList::goBack()
{
    m_backForwardList->goBack();
    return S_OK;
}

HRESULT WebBackForwardList::goForward()
{
    m_backForwardList->goForward();
    return S_OK;
}

HRESULT WebBackForwardList::goToItem(_In_opt_ IWebHistoryItem* item)
{
    COMPtr<WebHistoryItem> webHistoryItem;
 
    if (!item || FAILED(item->QueryInterface(&webHistoryItem)))
        return E_FAIL;

    if (auto item = webHistoryItem->historyItem())
        m_backForwardList->goToItem(*item);

    return S_OK;
}

HRESULT WebBackForwardList::backItem(_COM_Outptr_opt_ IWebHistoryItem** item)
{
    if (!item)
        return E_POINTER;
    *item = nullptr;
    HistoryItem* historyItem = m_backForwardList->backItem();

    if (!historyItem)
        return E_FAIL;

    *item = WebHistoryItem::createInstance(historyItem);
    return S_OK;
}

HRESULT WebBackForwardList::currentItem(_COM_Outptr_opt_ IWebHistoryItem** item)
{
    if (!item)
        return E_POINTER;
    *item = nullptr;

    HistoryItem* historyItem = m_backForwardList->currentItem();
    if (!historyItem)
        return E_FAIL;

    *item = WebHistoryItem::createInstance(historyItem);
    return S_OK;
}

HRESULT WebBackForwardList::forwardItem(_COM_Outptr_opt_ IWebHistoryItem** item)
{
    if (!item)
        return E_POINTER;
    *item = nullptr;

    HistoryItem* historyItem = m_backForwardList->forwardItem();
    if (!historyItem)
        return E_FAIL;

    *item = WebHistoryItem::createInstance(historyItem);
    return S_OK;
}

HRESULT WebBackForwardList::backListWithLimit(int limit, _Out_ int* listCount,
    __deref_inout_opt IWebHistoryItem** list)
{
    Vector<Ref<HistoryItem>> historyItemVector;
    m_backForwardList->backListWithLimit(limit, historyItemVector);

    *listCount = static_cast<int>(historyItemVector.size());

    if (!list)
        return E_NOT_SUFFICIENT_BUFFER;

    for (unsigned i = 0; i < historyItemVector.size(); i++)
        list[i] = WebHistoryItem::createInstance(historyItemVector[i].ptr());

    return S_OK;
}

HRESULT WebBackForwardList::forwardListWithLimit(int limit, _Out_ int* listCount,
    __deref_inout_opt IWebHistoryItem** list)
{
    Vector<Ref<HistoryItem>> historyItemVector;
    m_backForwardList->forwardListWithLimit(limit, historyItemVector);

    *listCount = static_cast<int>(historyItemVector.size());

    if (!list)
        return E_NOT_SUFFICIENT_BUFFER;
    
    for (unsigned i = 0; i < historyItemVector.size(); i++)
        list[i] = WebHistoryItem::createInstance(historyItemVector[i].ptr());

    return S_OK;
}

HRESULT WebBackForwardList::capacity(_Out_ int* result)
{
    if (!result)
        return E_POINTER;

    *result = m_backForwardList->capacity();
    return S_OK;
}

HRESULT WebBackForwardList::setCapacity(int size)
{
    if (size < 0)
        return E_FAIL;
    
    m_backForwardList->setCapacity(size);
    return S_OK;
}

HRESULT WebBackForwardList::backListCount(_Out_ int* count)
{
    if (!count)
        return E_POINTER;

    *count = m_backForwardList->backListCount();
    return S_OK;
}

HRESULT WebBackForwardList::forwardListCount(_Out_ int* count)
{
    if (!count)
        return E_POINTER;

    *count = m_backForwardList->forwardListCount();
    return S_OK;
}

HRESULT WebBackForwardList::containsItem(_In_opt_ IWebHistoryItem* item, _Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;

    COMPtr<WebHistoryItem> webHistoryItem;

    if (!item || FAILED(item->QueryInterface(&webHistoryItem)))
        return E_FAIL;

    *result = m_backForwardList->containsItem(webHistoryItem->historyItem());
    return S_OK;
}

HRESULT WebBackForwardList::itemAtIndex(int index, _COM_Outptr_opt_ IWebHistoryItem** item)
{
    if (!item)
        return E_POINTER;
    *item = nullptr;

    HistoryItem* historyItem = m_backForwardList->itemAtIndex(index);
    if (!historyItem)
        return E_FAIL;
 
    *item = WebHistoryItem::createInstance(historyItem);
    return S_OK;
}

// IWebBackForwardListPrivate --------------------------------------------------------

HRESULT WebBackForwardList::removeItem(_In_opt_ IWebHistoryItem* item)
{
    COMPtr<WebHistoryItem> webHistoryItem;
 
    if (!item || FAILED(item->QueryInterface(&webHistoryItem)))
        return E_FAIL;
 
    m_backForwardList->removeItem(webHistoryItem->historyItem());
    return S_OK;
}
