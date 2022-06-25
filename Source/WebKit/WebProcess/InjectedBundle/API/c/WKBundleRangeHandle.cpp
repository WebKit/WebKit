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
#include "WKBundleRangeHandle.h"
#include "WKBundleRangeHandlePrivate.h"

#include "InjectedBundleRangeHandle.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebImage.h"
#include <WebCore/IntRect.h>

WKTypeID WKBundleRangeHandleGetTypeID()
{
    return WebKit::toAPI(WebKit::InjectedBundleRangeHandle::APIType);
}

WKBundleRangeHandleRef WKBundleRangeHandleCreate(JSContextRef contextRef, JSObjectRef objectRef)
{
    RefPtr<WebKit::InjectedBundleRangeHandle> rangeHandle = WebKit::InjectedBundleRangeHandle::getOrCreate(contextRef, objectRef);
    return toAPI(rangeHandle.leakRef());
}

WKRect WKBundleRangeHandleGetBoundingRectInWindowCoordinates(WKBundleRangeHandleRef rangeHandleRef)
{
    WebCore::IntRect boundingRect = WebKit::toImpl(rangeHandleRef)->boundingRectInWindowCoordinates();
    return WKRectMake(boundingRect.x(), boundingRect.y(), boundingRect.width(), boundingRect.height());
}

WKImageRef WKBundleRangeHandleCopySnapshotWithOptions(WKBundleRangeHandleRef rangeHandleRef, WKSnapshotOptions options)
{
    RefPtr<WebKit::WebImage> image = WebKit::toImpl(rangeHandleRef)->renderedImage(WebKit::toSnapshotOptions(options));
    return toAPI(image.leakRef());
}
