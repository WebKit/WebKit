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

#if ENABLE(INSPECTOR)

#include "WKAPICast.h"
#include "WebInspectorProxy.h"

using namespace WebKit;

WKTypeID WKInspectorGetTypeID()
{
    return toAPI(WebInspectorProxy::APIType);
}

WKPageRef WKInspectorGetPage(WKInspectorRef inspectorRef)
{
    return toAPI(toImpl(inspectorRef)->page());
}

bool WKInspectorIsVisible(WKInspectorRef inspectorRef)
{
    return toImpl(inspectorRef)->isVisible();
}

void WKInspectorShow(WKInspectorRef inspectorRef)
{
    toImpl(inspectorRef)->show();
}

void WKInspectorClose(WKInspectorRef inspectorRef)
{
    toImpl(inspectorRef)->close();
}

void WKInspectorShowConsole(WKInspectorRef inspectorRef)
{
    toImpl(inspectorRef)->showConsole();
}

bool WKInspectorIsAttached(WKInspectorRef inspectorRef)
{
    return toImpl(inspectorRef)->isAttached();
}

void WKInspectorAttach(WKInspectorRef inspectorRef)
{
    toImpl(inspectorRef)->attach();
}

void WKInspectorDetach(WKInspectorRef inspectorRef)
{
    toImpl(inspectorRef)->detach();
}

bool WKInspectorIsDebuggingJavaScript(WKInspectorRef inspectorRef)
{
    return toImpl(inspectorRef)->isDebuggingJavaScript();
}

void WKInspectorToggleJavaScriptDebugging(WKInspectorRef inspectorRef)
{
    toImpl(inspectorRef)->toggleJavaScriptDebugging();
}

bool WKInspectorIsProfilingJavaScript(WKInspectorRef inspectorRef)
{
    return toImpl(inspectorRef)->isProfilingJavaScript();
}

void WKInspectorToggleJavaScriptProfiling(WKInspectorRef inspectorRef)
{
    toImpl(inspectorRef)->toggleJavaScriptProfiling();
}

bool WKInspectorIsProfilingPage(WKInspectorRef inspectorRef)
{
    return toImpl(inspectorRef)->isProfilingPage();
}

void WKInspectorTogglePageProfiling(WKInspectorRef inspectorRef)
{
    toImpl(inspectorRef)->togglePageProfiling();
}

#endif // ENABLE(INSPECTOR)
