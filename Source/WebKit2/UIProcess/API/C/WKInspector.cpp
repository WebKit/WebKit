/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "WKAPICast.h"
#include "WebInspectorProxy.h"

using namespace WebKit;

WKTypeID WKInspectorGetTypeID()
{
#if ENABLE(INSPECTOR)
    return toAPI(WebInspectorProxy::APIType);
#else
    return 0;
#endif
}

WKPageRef WKInspectorGetPage(WKInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    return toAPI(toImpl(inspectorRef)->page());
#else
    return 0;
#endif
}

bool WKInspectorIsVisible(WKInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    return toImpl(inspectorRef)->isVisible();
#else
    return false;
#endif
}

bool WKInspectorIsFront(WKInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    return toImpl(inspectorRef)->isFront();
#else
    return false;
#endif
}

void WKInspectorShow(WKInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    toImpl(inspectorRef)->show();
#endif
}

void WKInspectorClose(WKInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    toImpl(inspectorRef)->close();
#endif
}

void WKInspectorShowConsole(WKInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    toImpl(inspectorRef)->showConsole();
#endif
}

void WKInspectorShowResources(WKInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    toImpl(inspectorRef)->showResources();
#endif
}

void WKInspectorShowMainResourceForFrame(WKInspectorRef inspectorRef, WKFrameRef frameRef)
{
#if ENABLE(INSPECTOR)
    toImpl(inspectorRef)->showMainResourceForFrame(toImpl(frameRef));
#endif
}

bool WKInspectorIsAttached(WKInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    return toImpl(inspectorRef)->isAttached();
#else
    return false;
#endif
}

void WKInspectorAttach(WKInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    toImpl(inspectorRef)->attach();
#endif
}

void WKInspectorDetach(WKInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    toImpl(inspectorRef)->detach();
#endif
}

bool WKInspectorIsDebuggingJavaScript(WKInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    return toImpl(inspectorRef)->isDebuggingJavaScript();
#else
    return false;
#endif
}

void WKInspectorToggleJavaScriptDebugging(WKInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    toImpl(inspectorRef)->toggleJavaScriptDebugging();
#endif
}

bool WKInspectorIsProfilingJavaScript(WKInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    return toImpl(inspectorRef)->isProfilingJavaScript();
#else
    return false;
#endif
}

void WKInspectorToggleJavaScriptProfiling(WKInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    toImpl(inspectorRef)->toggleJavaScriptProfiling();
#endif
}

bool WKInspectorIsProfilingPage(WKInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    return toImpl(inspectorRef)->isProfilingPage();
#else
    return false;
#endif
}

void WKInspectorTogglePageProfiling(WKInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    toImpl(inspectorRef)->togglePageProfiling();
#endif
}
