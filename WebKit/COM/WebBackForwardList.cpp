/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "WebKitDLL.h"
#include "WebBackForwardList.h"

// WebBackForwardList ----------------------------------------------------------------

WebBackForwardList::WebBackForwardList()
: m_refCount(0)
, m_position(-1)
, m_maximumSize(100) // typically set by browser app
{
    gClassCount++;
}

WebBackForwardList::~WebBackForwardList()
{
    Vector<WebHistoryItem*>::iterator end = m_list.end();
    for (Vector<WebHistoryItem*>::iterator it = m_list.begin(); it != end; ++it)
        (*it)->Release();

    gClassCount--;
}

WebBackForwardList* WebBackForwardList::createInstance()
{
    WebBackForwardList* instance = new WebBackForwardList();
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebBackForwardList::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebBackForwardList*>(this);
    else if (IsEqualGUID(riid, IID_IWebBackForwardList))
        *ppvObject = static_cast<IWebBackForwardList*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebBackForwardList::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebBackForwardList::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebBackForwardList ---------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebBackForwardList::addItem( 
    /* [in] */ IWebHistoryItem* item)
{
    if (!m_maximumSize)
        return S_FALSE;

    if (!item)
        return E_FAIL;

    // Toss anything in the forward list
    unsigned int end = (unsigned int) m_list.size();
    unsigned int clearItem = m_position + 1;
    if (clearItem < end) {
        Vector<WebHistoryItem*>::iterator it = &m_list.at(clearItem);
        for (unsigned int i=clearItem; i<end; i++) {
            (*it)->Release();
            m_list.remove(clearItem);
        }
    }

    // Toss the first item if the list is getting too big, as long as we're not using it
    if (m_list.size() == m_maximumSize && m_position != 0) {
        m_list.remove(0);
        m_position--;
    }

    m_position++;
    m_list.append(static_cast<WebHistoryItem*>(item));
    item->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::goBack( void)
{
    if (m_position < 1)
        return E_FAIL;

    m_position--;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::goForward( void)
{
    if (m_position >= (int)m_list.size()-1)
        return E_FAIL;

    m_position++;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::goToItem( 
    /* [in] */ IWebHistoryItem* item)
{
    Vector<WebHistoryItem*>::iterator end = m_list.end();
    int newPosition = 0;
    for (Vector<WebHistoryItem*>::iterator it = m_list.begin(); it != end; ++it, ++newPosition) {
        if (item == *it) {
            m_position = newPosition;
            return S_OK;
        }
    }

    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::backItem( 
    /* [retval][out] */ IWebHistoryItem** item)
{
    *item = 0;
    if (m_position > 0) {
        *item = m_list[m_position-1];
        (*item)->AddRef();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::currentItem( 
    /* [retval][out] */ IWebHistoryItem** item)
{
    *item = 0;
    if (m_list.size() > 0) {
        *item = m_list[m_position];
        (*item)->AddRef();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::forwardItem( 
    /* [retval][out] */ IWebHistoryItem** item)
{
    *item = 0;
    if (m_position < (int)m_list.size()-1) {
        *item = m_list[m_position+1];
        (*item)->AddRef();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::backListWithLimit( 
    /* [in] */ int /*limit*/,
    /* [out] */ int* /*listCount*/,
    /* [retval][out] */ IWebHistoryItem*** /*list*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::forwardListWithLimit( 
    /* [in] */ int /*limit*/,
    /* [out] */ int* /*listCount*/,
    /* [retval][out] */ IWebHistoryItem*** /*list*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::capacity( 
    /* [retval][out] */ int* result)
{
    *result = (int)m_maximumSize;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::setCapacity( 
    /* [in] */ int size)
{
    if (size < 0)
        return E_FAIL;
    
    m_maximumSize = size;

    if (size == 0)
        m_position = 0;
    else if (m_position >= size)
        m_position = size-1;

    unsigned int uSize = (unsigned int) size;
    unsigned int end = (unsigned int) m_list.size();
    if (uSize < end) {
        Vector<WebHistoryItem*>::iterator it = &m_list.at(size);
        for (unsigned int i=uSize; i<end; i++) {
            (*it)->Release();
            m_list.remove(size);
        }
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::backListCount( 
    /* [retval][out] */ int* count)
{
    *count = m_position;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::forwardListCount( 
    /* [retval][out] */ int* sizecount)
{
    int listSize = (int) m_list.size();

    *sizecount = (listSize > 0) ? (listSize-m_position-1) : 0;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::containsItem( 
    /* [in] */ IWebHistoryItem* item,
    /* [retval][out] */ BOOL* result)
{
    *result = FALSE;
    Vector<WebHistoryItem*>::iterator end = m_list.end();
    for (Vector<WebHistoryItem*>::iterator it = m_list.begin(); it != end; ++it) {
        if (item == *it)
            *result = TRUE;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::itemAtIndex( 
    /* [in] */ int index,
    /* [retval][out] */ IWebHistoryItem** item)
{
    *item = 0;
    if (index < 0 || index > (int)m_list.size())
        return E_FAIL;

    *item = m_list.at(index);
    (*item)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::setPageCacheSize( 
    /* [in] */ UINT /*size*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::pageCacheSize( 
    /* [retval][out] */ UINT* /*size*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
