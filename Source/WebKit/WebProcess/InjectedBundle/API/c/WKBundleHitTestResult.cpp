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
#include "WKBundleHitTestResult.h"

#include "InjectedBundleHitTestResult.h"
#include "InjectedBundleNodeHandle.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebFrame.h"
#include "WebImage.h"
#include <WebCore/ScriptExecutionContext.h>

WKTypeID WKBundleHitTestResultGetTypeID()
{
    return WebKit::toAPI(WebKit::InjectedBundleHitTestResult::APIType);
}

WKBundleNodeHandleRef WKBundleHitTestResultCopyNodeHandle(WKBundleHitTestResultRef hitTestResultRef)
{
    RefPtr<WebKit::InjectedBundleNodeHandle> nodeHandle = WebKit::toProtectedImpl(hitTestResultRef)->nodeHandle();
    SUPPRESS_UNCOUNTED_ARG return toAPI(nodeHandle.leakRef());
}

WKBundleNodeHandleRef WKBundleHitTestResultCopyURLElementHandle(WKBundleHitTestResultRef hitTestResultRef)
{
    RefPtr<WebKit::InjectedBundleNodeHandle> urlElementNodeHandle = WebKit::toProtectedImpl(hitTestResultRef)->urlElementHandle();
    SUPPRESS_UNCOUNTED_ARG return toAPI(urlElementNodeHandle.leakRef());
}

WKBundleFrameRef WKBundleHitTestResultGetFrame(WKBundleHitTestResultRef)
{
    return nullptr;
}

WKBundleFrameRef WKBundleHitTestResultGetTargetFrame(WKBundleHitTestResultRef)
{
    return nullptr;
}

WKURLRef WKBundleHitTestResultCopyAbsoluteImageURL(WKBundleHitTestResultRef hitTestResultRef)
{
    return WebKit::toCopiedURLAPI(WebKit::toProtectedImpl(hitTestResultRef)->absoluteImageURL());
}

WKURLRef WKBundleHitTestResultCopyAbsolutePDFURL(WKBundleHitTestResultRef hitTestResultRef)
{
    return WebKit::toCopiedURLAPI(WebKit::toProtectedImpl(hitTestResultRef)->absolutePDFURL());
}

WKURLRef WKBundleHitTestResultCopyAbsoluteLinkURL(WKBundleHitTestResultRef hitTestResultRef)
{
    return WebKit::toCopiedURLAPI(WebKit::toProtectedImpl(hitTestResultRef)->absoluteLinkURL());
}

WKURLRef WKBundleHitTestResultCopyAbsoluteMediaURL(WKBundleHitTestResultRef hitTestResultRef)
{
    return WebKit::toCopiedURLAPI(WebKit::toProtectedImpl(hitTestResultRef)->absoluteMediaURL());
}

bool WKBundleHitTestResultMediaIsInFullscreen(WKBundleHitTestResultRef hitTestResultRef)
{
    return WebKit::toProtectedImpl(hitTestResultRef)->mediaIsInFullscreen();
}

bool WKBundleHitTestResultMediaHasAudio(WKBundleHitTestResultRef hitTestResultRef)
{
    return WebKit::toProtectedImpl(hitTestResultRef)->mediaHasAudio();
}

bool WKBundleHitTestResultIsDownloadableMedia(WKBundleHitTestResultRef hitTestResultRef)
{
    return WebKit::toProtectedImpl(hitTestResultRef)->isDownloadableMedia();
}

WKBundleHitTestResultMediaType WKBundleHitTestResultGetMediaType(WKBundleHitTestResultRef hitTestResultRef)
{
    return toAPI(WebKit::toProtectedImpl(hitTestResultRef)->mediaType());
}

WKRect WKBundleHitTestResultGetImageRect(WKBundleHitTestResultRef hitTestResultRef)
{
    return WebKit::toAPI(WebKit::toProtectedImpl(hitTestResultRef)->imageRect());
}

WKImageRef WKBundleHitTestResultCopyImage(WKBundleHitTestResultRef)
{
    return nullptr;
}

bool WKBundleHitTestResultGetIsSelected(WKBundleHitTestResultRef hitTestResultRef)
{
    return WebKit::toProtectedImpl(hitTestResultRef)->isSelected();
}

WKStringRef WKBundleHitTestResultCopyLinkLabel(WKBundleHitTestResultRef hitTestResultRef)
{
    return WebKit::toCopiedAPI(WebKit::toProtectedImpl(hitTestResultRef)->linkLabel());
}

WKStringRef WKBundleHitTestResultCopyLinkTitle(WKBundleHitTestResultRef hitTestResultRef)
{
    return WebKit::toCopiedAPI(WebKit::toProtectedImpl(hitTestResultRef)->linkTitle());
}

WKStringRef WKBundleHitTestResultCopyLinkSuggestedFilename(WKBundleHitTestResultRef hitTestResultRef)
{
    return WebKit::toCopiedAPI(WebKit::toProtectedImpl(hitTestResultRef)->linkSuggestedFilename());
}
