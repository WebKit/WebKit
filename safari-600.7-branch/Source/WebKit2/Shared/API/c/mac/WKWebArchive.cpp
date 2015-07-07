/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "WKWebArchive.h"

#include "APIArray.h"
#include "APIData.h"
#include "APIWebArchive.h"
#include "APIWebArchiveResource.h"
#include "InjectedBundleRangeHandle.h"
#include "WKBundleAPICast.h"
#include "WKSharedAPICast.h"

using namespace WebKit;

WKTypeID WKWebArchiveGetTypeID()
{
    return toAPI(API::WebArchive::APIType);
}

WKWebArchiveRef WKWebArchiveCreate(WKWebArchiveResourceRef mainResourceRef, WKArrayRef subresourcesRef, WKArrayRef subframeArchivesRef)
{
    RefPtr<API::WebArchive> webArchive = API::WebArchive::create(toImpl(mainResourceRef), toImpl(subresourcesRef), toImpl(subframeArchivesRef));
    return toAPI(webArchive.release().leakRef());
}

WKWebArchiveRef WKWebArchiveCreateWithData(WKDataRef dataRef)
{
    RefPtr<API::WebArchive> webArchive = API::WebArchive::create(toImpl(dataRef));
    return toAPI(webArchive.release().leakRef());
}

WKWebArchiveRef WKWebArchiveCreateFromRange(WKBundleRangeHandleRef rangeHandleRef)
{
    RefPtr<API::WebArchive> webArchive = API::WebArchive::create(toImpl(rangeHandleRef)->coreRange());
    return toAPI(webArchive.release().leakRef());
}

WKWebArchiveResourceRef WKWebArchiveCopyMainResource(WKWebArchiveRef webArchiveRef)
{
    RefPtr<API::WebArchiveResource> mainResource = toImpl(webArchiveRef)->mainResource();
    return toAPI(mainResource.release().leakRef());
}

WKArrayRef WKWebArchiveCopySubresources(WKWebArchiveRef webArchiveRef)
{
    RefPtr<API::Array> subresources = toImpl(webArchiveRef)->subresources();
    return toAPI(subresources.release().leakRef());
}

WKArrayRef WKWebArchiveCopySubframeArchives(WKWebArchiveRef webArchiveRef)
{
    RefPtr<API::Array> subframeArchives = toImpl(webArchiveRef)->subframeArchives();
    return toAPI(subframeArchives.release().leakRef());
}

WKDataRef WKWebArchiveCopyData(WKWebArchiveRef webArchiveRef)
{
    return toAPI(toImpl(webArchiveRef)->data().leakRef());
}
