/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ViewClientEfl.h"

#include "EwkView.h"
#include "WKView.h"

using namespace EwkViewCallbacks;

namespace WebKit {

EwkView* ViewClientEfl::toEwkView(const void* clientInfo)
{
    return static_cast<ViewClientEfl*>(const_cast<void*>(clientInfo))->m_view;
}

void ViewClientEfl::viewNeedsDisplay(WKViewRef, WKRect, const void* clientInfo)
{
    toEwkView(clientInfo)->scheduleUpdateDisplay();
}

void ViewClientEfl::didChangeContentsSize(WKViewRef, WKSize size, const void* clientInfo)
{
    EwkView* ewkView = toEwkView(clientInfo);
    ewkView->scheduleUpdateDisplay();
    ewkView->smartCallback<ContentsSizeChanged>().call(size);
}

ViewClientEfl::ViewClientEfl(EwkView* view)
    : m_view(view)
{
    ASSERT(m_view);

    WKViewClient viewClient;
    memset(&viewClient, 0, sizeof(WKViewClient));
    viewClient.version = kWKViewClientCurrentVersion;
    viewClient.clientInfo = this;
    viewClient.didChangeContentsSize = didChangeContentsSize;
    viewClient.viewNeedsDisplay = viewNeedsDisplay;

    WKViewSetViewClient(m_view->wkView(), &viewClient);
}

ViewClientEfl::~ViewClientEfl()
{
    WKViewSetViewClient(m_view->wkView(), 0);
}

} // namespace WebKit
