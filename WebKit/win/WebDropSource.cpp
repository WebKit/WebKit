/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "WebDropSource.h"

#include "WebKitDLL.h"
#include "WebView.h"

#include <WebCore/DragActions.h>
#include <WebCore/EventHandler.h>
#include <WebCore/Frame.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformMouseEvent.h>
#include <wtf/CurrentTime.h>

#include <Windows.h>

using namespace WebCore;


HRESULT WebDropSource::createInstance(WebView* webView, IDropSource** result)
{
    if (!webView || !result)
        return E_INVALIDARG;
    *result = new WebDropSource(webView);
    return S_OK;
}

WebDropSource::WebDropSource(WebView* webView)
: m_ref(1)
, m_dropped(false) 
, m_webView(webView)
{
    gClassCount++;
    gClassNameCount.add("WebDropSource");
}

WebDropSource::~WebDropSource()
{
    gClassCount--;
    gClassNameCount.remove("WebDropSource");
}

STDMETHODIMP WebDropSource::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualIID(riid, IID_IUnknown) || 
        IsEqualIID(riid, IID_IDropSource)) {
        *ppvObject = this;
        AddRef();

        return S_OK;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) WebDropSource::AddRef(void)
{
    return InterlockedIncrement(&m_ref);
}

STDMETHODIMP_(ULONG) WebDropSource::Release(void)
{
    long c = InterlockedDecrement(&m_ref);
    if (c == 0)
        delete this;
    return c;
}

PlatformMouseEvent generateMouseEvent(WebView* webView, bool isDrag) {
    POINTL pt;
    ::GetCursorPos((LPPOINT)&pt);
    POINTL localpt = pt;
    HWND viewWindow;
    if (SUCCEEDED(webView->viewWindow((OLE_HANDLE*)&viewWindow)))
        ::ScreenToClient(viewWindow, (LPPOINT)&localpt);
    return PlatformMouseEvent(IntPoint(localpt.x, localpt.y), IntPoint(pt.x, pt.y),
        isDrag ? LeftButton : NoButton, MouseEventMoved, 0, false, false, false, false, currentTime());
}

STDMETHODIMP WebDropSource::QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState)
{
    if (fEscapePressed || !(grfKeyState & (MK_LBUTTON|MK_RBUTTON))) {
        m_dropped = !fEscapePressed;
        return fEscapePressed? DRAGDROP_S_CANCEL : DRAGDROP_S_DROP;
    } else if (Page* page = m_webView->page())
        if (Frame* frame = page->mainFrame()) 
            frame->eventHandler()->dragSourceMovedTo(generateMouseEvent(m_webView.get(), true));

    return S_OK;
}

STDMETHODIMP WebDropSource::GiveFeedback(DWORD)
{
    return DRAGDROP_S_USEDEFAULTCURSORS;
}
