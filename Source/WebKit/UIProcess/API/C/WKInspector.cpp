/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WKInspector.h"

#if !PLATFORM(IOS_FAMILY)

#include "WKAPICast.h"
#include "WebFrameProxy.h"
#include "WebInspectorUIProxy.h"
#include "WebPageProxy.h"

using namespace WebKit;

WKTypeID WKInspectorGetTypeID()
{
    return toAPI(WebInspectorUIProxy::APIType);
}

WKPageRef WKInspectorGetPage(WKInspectorRef inspectorRef)
{
    return toAPI(protect(protect(toImpl(inspectorRef))->inspectedPage()).get());
}

bool WKInspectorIsConnected(WKInspectorRef inspectorRef)
{
    return protect(toImpl(inspectorRef))->isConnected();
}

bool WKInspectorIsVisible(WKInspectorRef inspectorRef)
{
    return protect(toImpl(inspectorRef))->isVisible();
}

bool WKInspectorIsFront(WKInspectorRef inspectorRef)
{
    return protect(toImpl(inspectorRef))->isFront();
}

void WKInspectorConnect(WKInspectorRef inspectorRef)
{
    protect(toImpl(inspectorRef))->connect();
}

void WKInspectorShow(WKInspectorRef inspectorRef)
{
    protect(toImpl(inspectorRef))->show();
}

void WKInspectorHide(WKInspectorRef inspectorRef)
{
    protect(toImpl(inspectorRef))->hide();
}

void WKInspectorClose(WKInspectorRef inspectorRef)
{
    protect(toImpl(inspectorRef))->close();
}

void WKInspectorShowConsole(WKInspectorRef inspectorRef)
{
    protect(toImpl(inspectorRef))->showConsole();
}

void WKInspectorShowResources(WKInspectorRef inspectorRef)
{
    protect(toImpl(inspectorRef))->showResources();
}

void WKInspectorShowMainResourceForFrame(WKInspectorRef inspectorRef, WKFrameRef frameRef)
{
    protect(toImpl(inspectorRef))->showMainResourceForFrame(protect(toImpl(frameRef))->frameID());
}

bool WKInspectorIsAttached(WKInspectorRef inspectorRef)
{
    return protect(toImpl(inspectorRef))->isAttached();
}

void WKInspectorAttach(WKInspectorRef inspectorRef)
{
    Ref inspector = *toImpl(inspectorRef);
    inspector->attach(inspector->attachmentSide());
}

void WKInspectorDetach(WKInspectorRef inspectorRef)
{
    protect(toImpl(inspectorRef))->detach();
}

bool WKInspectorIsProfilingPage(WKInspectorRef inspectorRef)
{
    return protect(toImpl(inspectorRef))->isProfilingPage();
}

void WKInspectorTogglePageProfiling(WKInspectorRef inspectorRef)
{
    protect(toImpl(inspectorRef))->togglePageProfiling();
}

bool WKInspectorIsElementSelectionActive(WKInspectorRef inspectorRef)
{
    return protect(toImpl(inspectorRef))->isElementSelectionActive();
}

void WKInspectorToggleElementSelection(WKInspectorRef inspectorRef)
{
    protect(toImpl(inspectorRef))->toggleElementSelection();
}

#endif // !PLATFORM(IOS_FAMILY)
