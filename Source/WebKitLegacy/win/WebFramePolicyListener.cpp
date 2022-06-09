/*
 * Copyright (C) 2007, 2013, 2015 Apple Inc.  All rights reserved.
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
#include "WebFramePolicyListener.h"

#include "WebFrameLoaderClient.h"
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameLoaderClient.h>

using namespace WebCore;

// WebFramePolicyListener ----------------------------------------------------------------

WebFramePolicyListener::WebFramePolicyListener(RefPtr<Frame>&& frame)
    : m_frame(WTFMove(frame))
{
    gClassCount++;
    gClassNameCount().add("WebFramePolicyListener");
}

WebFramePolicyListener::~WebFramePolicyListener()
{
    gClassCount--;
    gClassNameCount().remove("WebFramePolicyListener");
}

WebFramePolicyListener* WebFramePolicyListener::createInstance(RefPtr<Frame>&& frame)
{
    WebFramePolicyListener* instance = new WebFramePolicyListener(WTFMove(frame));
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT WebFramePolicyListener::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebPolicyDecisionListener*>(this);
    else if (IsEqualGUID(riid, IID_IWebPolicyDecisionListener))
        *ppvObject = static_cast<IWebPolicyDecisionListener*>(this);
    else if (IsEqualGUID(riid, IID_IWebFormSubmissionListener))
        *ppvObject = static_cast<IWebFormSubmissionListener*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebFramePolicyListener::AddRef()
{
    return ++m_refCount;
}

ULONG WebFramePolicyListener::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebPolicyDecisionListener ------------------------------------------------------------

HRESULT WebFramePolicyListener::use()
{
    receivedPolicyDecision(PolicyAction::Use);
    return S_OK;
}

HRESULT WebFramePolicyListener::download()
{
    receivedPolicyDecision(PolicyAction::Download);
    return S_OK;
}

HRESULT WebFramePolicyListener::ignore()
{
    receivedPolicyDecision(PolicyAction::Ignore);
    return S_OK;
}

// IWebFormSubmissionListener ------------------------------------------------------------

HRESULT WebFramePolicyListener::continueSubmit()
{
    receivedPolicyDecision(PolicyAction::Use);
    return S_OK;
}

// WebFramePolicyListener ----------------------------------------------------------------
void WebFramePolicyListener::receivedPolicyDecision(PolicyAction action)
{
    RefPtr<Frame> frame = WTFMove(m_frame);
    if (frame)
        static_cast<WebFrameLoaderClient&>(frame->loader().client()).receivedPolicyDecision(action);
}

void WebFramePolicyListener::invalidate()
{
    m_frame = nullptr;
}

