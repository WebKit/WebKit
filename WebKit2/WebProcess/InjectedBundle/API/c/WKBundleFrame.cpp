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

#include "WKBundleFrame.h"
#include "WKBundleFramePrivate.h"

#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebFrame.h"
#include <WebCore/Frame.h>

using namespace WebCore;
using namespace WebKit;

WKTypeID WKBundleFrameGetTypeID()
{
    return toRef(WebFrame::APIType);
}

bool WKBundleFrameIsMainFrame(WKBundleFrameRef frameRef)
{
    return toWK(frameRef)->isMainFrame();
}

WKURLRef WKBundleFrameCopyURL(WKBundleFrameRef frameRef)
{
    return toCopiedURLRef(toWK(frameRef)->url());
}

WKArrayRef WKBundleFrameCopyChildFrames(WKBundleFrameRef frameRef)
{
    return toRef(toWK(frameRef)->childFrames().releaseRef());    
}

unsigned WKBundleFrameGetNumberOfActiveAnimations(WKBundleFrameRef frameRef)
{
    return toWK(frameRef)->numberOfActiveAnimations();
}

bool WKBundleFramePauseAnimationOnElementWithId(WKBundleFrameRef frameRef, WKStringRef name, WKStringRef elementID, double time)
{
    return toWK(frameRef)->pauseAnimationOnElementWithId(toWK(name)->string(), toWK(elementID)->string(), time);
}

JSGlobalContextRef WKBundleFrameGetJavaScriptContext(WKBundleFrameRef frameRef)
{
    return toWK(frameRef)->jsContext();
}

JSGlobalContextRef WKBundleFrameGetJavaScriptContextForWorld(WKBundleFrameRef frameRef, WKBundleScriptWorldRef worldRef)
{
    return toWK(frameRef)->jsContextForWorld(toWK(worldRef));
}

JSValueRef WKBundleFrameGetJavaScriptWrapperForNodeForWorld(WKBundleFrameRef frameRef, WKBundleNodeHandleRef nodeHandleRef, WKBundleScriptWorldRef worldRef)
{
    return toWK(frameRef)->jsWrapperForWorld(toWK(nodeHandleRef), toWK(worldRef));
}

JSValueRef WKBundleFrameGetJavaScriptWrapperForRangeForWorld(WKBundleFrameRef frameRef, WKBundleRangeHandleRef rangeHandleRef, WKBundleScriptWorldRef worldRef)
{
    return toWK(frameRef)->jsWrapperForWorld(toWK(rangeHandleRef), toWK(worldRef));
}

WKStringRef WKBundleFrameCopyName(WKBundleFrameRef frameRef)
{
    return toCopiedRef(toWK(frameRef)->name());
}

JSValueRef WKBundleFrameGetComputedStyleIncludingVisitedInfo(WKBundleFrameRef frameRef, JSObjectRef element)
{
    return toWK(frameRef)->computedStyleIncludingVisitedInfo(element);
}

WKStringRef WKBundleFrameCopyCounterValue(WKBundleFrameRef frameRef, JSObjectRef element)
{
    return toCopiedRef(toWK(frameRef)->counterValue(element));
}

WKStringRef WKBundleFrameCopyMarkerText(WKBundleFrameRef frameRef, JSObjectRef element)
{
    return toCopiedRef(toWK(frameRef)->markerText(element));
}

WKStringRef WKBundleFrameCopyInnerText(WKBundleFrameRef frameRef)
{
    return toCopiedRef(toWK(frameRef)->innerText());
}

unsigned WKBundleFrameGetPendingUnloadCount(WKBundleFrameRef frameRef)
{
    return toWK(frameRef)->pendingUnloadCount();
}
