/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
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
#include "WebDragSource.h"

#include <WebCore/Cursor.h>
#include <WebCore/DragActions.h>
#include <WebCore/EventHandler.h>
#include <WebCore/Frame.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformMouseEvent.h>
#include <wtf/CurrentTime.h>

using namespace WebCore;

PassRefPtr<WebDragSource> WebDragSource::createInstance()
{
    return adoptRef(new WebDragSource);
}

WebDragSource::WebDragSource()
{
}

HRESULT WebDragSource::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDropSource)) {
        *ppvObject = this;
        AddRef();

        return S_OK;
    }

    return E_NOINTERFACE;
}

ULONG WebDragSource::AddRef(void)
{
    ref();
    return refCount();
}

ULONG WebDragSource::Release(void)
{
    deref();
    return refCount();
}

HRESULT WebDragSource::QueryContinueDrag(BOOL fEscapePressed, DWORD grfState)
{
    if (fEscapePressed)
        return DRAGDROP_S_CANCEL;

    if (grfState & (MK_LBUTTON | MK_RBUTTON))
        return S_OK;

    return DRAGDROP_S_DROP;
}

HRESULT WebDragSource::GiveFeedback(DWORD dwEffect)
{
    return DRAGDROP_S_USEDEFAULTCURSORS;
}
