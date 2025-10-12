/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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
#include "WKWebArchiveRef.h"

#include "APIArray.h"
#include "APIData.h"
#include "APIWebArchive.h"
#include "APIWebArchiveResource.h"
#include "InjectedBundleRangeHandle.h"
#include "WKBundleAPICast.h"
#include "WKSharedAPICast.h"
#include <WebCore/Range.h>
#include <WebCore/SimpleRange.h>

WKTypeID WKWebArchiveGetTypeID()
{
    return WebKit::toAPI(API::WebArchive::APIType);
}

WKWebArchiveRef WKWebArchiveCreate(WKWebArchiveResourceRef mainResourceRef, WKArrayRef subresourcesRef, WKArrayRef subframeArchivesRef)
{
    auto webArchive = API::WebArchive::create(WebKit::toProtectedImpl(mainResourceRef).get(), WebKit::toImpl(subresourcesRef), WebKit::toImpl(subframeArchivesRef));
    return WebKit::toAPILeakingRef(WTFMove(webArchive));
}

WKWebArchiveRef WKWebArchiveCreateWithData(WKDataRef dataRef)
{
    auto webArchive = API::WebArchive::create(WebKit::toProtectedImpl(dataRef).get());
    return WebKit::toAPILeakingRef(WTFMove(webArchive));
}

WKWebArchiveRef WKWebArchiveCreateFromRange(WKBundleRangeHandleRef rangeHandleRef)
{
    Ref webArchive = API::WebArchive::create(makeSimpleRange(WebKit::toProtectedImpl(rangeHandleRef)->coreRange()));
    return WebKit::toAPILeakingRef(WTFMove(webArchive));
}

WKWebArchiveResourceRef WKWebArchiveCopyMainResource(WKWebArchiveRef webArchiveRef)
{
    RefPtr<API::WebArchiveResource> mainResource = WebKit::toProtectedImpl(webArchiveRef)->mainResource();
    return WebKit::toAPILeakingRef(WTFMove(mainResource));
}

WKArrayRef WKWebArchiveCopySubresources(WKWebArchiveRef webArchiveRef)
{
    RefPtr<API::Array> subresources = WebKit::toProtectedImpl(webArchiveRef)->subresources();
    return WebKit::toAPILeakingRef(WTFMove(subresources));
}

WKArrayRef WKWebArchiveCopySubframeArchives(WKWebArchiveRef webArchiveRef)
{
    RefPtr<API::Array> subframeArchives = WebKit::toProtectedImpl(webArchiveRef)->subframeArchives();
    return WebKit::toAPILeakingRef(WTFMove(subframeArchives));
}

WKDataRef WKWebArchiveCopyData(WKWebArchiveRef webArchiveRef)
{
    return WebKit::toAPILeakingRef(WebKit::toProtectedImpl(webArchiveRef)->data());
}
