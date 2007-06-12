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

#ifndef WebScrollBar_h
#define WebScrollBar_h

#include "IWebScrollBarDelegatePrivate.h"
#include "IWebScrollBarPrivate.h"

#include <wtf/RefPtr.h>
#include <wtf/OwnPtr.h>

#pragma warning(push, 0)
#include <WebCore/COMPtr.h>
#include <WebCore/ScrollBar.h>
#pragma warning(pop)

namespace WebCore {
class PlatformScrollbar;
}

using namespace WebCore;

class WebScrollBar : public IWebScrollBarPrivate, ScrollbarClient
{
public:
    static WebScrollBar* createInstance();
protected:
    WebScrollBar();
    ~WebScrollBar();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IWebScrollBarPrivate
    virtual HRESULT STDMETHODCALLTYPE init( 
        /* [in] */ IWebScrollBarDelegatePrivate* delegate,
        /* [in] */ OLE_HANDLE containingWindow,
        /* [in] */ WebScrollBarOrientation orientation,
        /* [in] */ WebScrollBarControlSize controlSize);
    
    virtual HRESULT STDMETHODCALLTYPE setEnabled( 
        /* [in] */ BOOL enabled);
    
    virtual HRESULT STDMETHODCALLTYPE setSteps( 
        /* [in] */ int lineStep,
        /* [in] */ int pageStep);
    
    virtual HRESULT STDMETHODCALLTYPE setProportion( 
        /* [in] */ int visibleSize,
        /* [in] */ int totalSize);
    
    virtual HRESULT STDMETHODCALLTYPE setRect( 
        /* [in] */ RECT bounds);
    
    virtual HRESULT STDMETHODCALLTYPE setValue( 
        /* [in] */ int value);
    
    virtual HRESULT STDMETHODCALLTYPE value( 
        /* [retval][out] */ int* value);
   
    virtual HRESULT STDMETHODCALLTYPE paint( 
        /* [in] */ HDC dc,
        /* [in] */ RECT damageRect);
    
    virtual HRESULT STDMETHODCALLTYPE frameGeometry( 
        /* [retval][out] */ RECT* bounds);
    
    virtual HRESULT STDMETHODCALLTYPE width( 
        /* [retval][out] */ int* w);
    
    virtual HRESULT STDMETHODCALLTYPE height( 
        /* [retval][out] */ int* h);
    
    virtual HRESULT STDMETHODCALLTYPE requestedWidth( 
        /* [retval][out] */ int* w);
    
    virtual HRESULT STDMETHODCALLTYPE requestedHeight( 
        /* [retval][out] */ int* h);

    virtual HRESULT STDMETHODCALLTYPE handleMouseEvent( 
        /* [in] */ OLE_HANDLE window,
        /* [in] */ UINT msg,
        /* [in] */ WPARAM wParam,
        /* [in] */ LPARAM lParam);
    
    virtual HRESULT STDMETHODCALLTYPE scroll( 
        /* [in] */ WebScrollDirection direction,
        /* [in] */ WebScrollGranularity granularity,
        /* [in] */ float multiplier);

protected:
    // ScrollbarClient
    virtual void valueChanged(Scrollbar*);

    // Used to obtain a window clip rect.
    virtual IntRect windowClipRect() const;

    ULONG m_refCount;
    RefPtr<WebCore::PlatformScrollbar> m_scrollBar;
    COMPtr<IWebScrollBarDelegatePrivate> m_delegate;
};

#endif
