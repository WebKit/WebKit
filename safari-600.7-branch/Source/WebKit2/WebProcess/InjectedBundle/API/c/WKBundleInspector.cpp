/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "WKBundleInspector.h"

#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebInspector.h"

using namespace WebCore;
using namespace WebKit;

WKTypeID WKBundleInspectorGetTypeID()
{
#if ENABLE(INSPECTOR)
    return toAPI(WebInspector::APIType);
#else
    return toAPI(API::Object::Type::Null);
#endif
}

void WKBundleInspectorShow(WKBundleInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    toImpl(inspectorRef)->show();
#else
    UNUSED_PARAM(inspectorRef);
#endif
}

void WKBundleInspectorClose(WKBundleInspectorRef inspectorRef)
{
#if ENABLE(INSPECTOR)
    toImpl(inspectorRef)->close();
#else
    UNUSED_PARAM(inspectorRef);
#endif
}

void WKBundleInspectorEvaluateScriptForTest(WKBundleInspectorRef inspectorRef, WKStringRef script)
{
#if ENABLE(INSPECTOR)
    toImpl(inspectorRef)->evaluateScriptForTest(toWTFString(script));
#else
    UNUSED_PARAM(script);
    UNUSED_PARAM(inspectorRef);
#endif
}

void WKBundleInspectorSetPageProfilingEnabled(WKBundleInspectorRef inspectorRef, bool enabled)
{
#if ENABLE(INSPECTOR)
    if (enabled)
        toImpl(inspectorRef)->startPageProfiling();
    else
        toImpl(inspectorRef)->stopPageProfiling();
#else
    UNUSED_PARAM(enabled);
    UNUSED_PARAM(inspectorRef);
#endif
}
