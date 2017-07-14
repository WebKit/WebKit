/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "WKMediaSessionMetadata.h"

#include "WKAPICast.h"
#include "WebMediaSessionMetadata.h"

using namespace WebKit;

WKTypeID WKMediaSessionMetadataGetTypeID()
{
#if ENABLE(MEDIA_SESSION)
    return toAPI(WebMediaSessionMetadata::APIType);
#else
    return toAPI(API::Object::Type::Null);
#endif
}

WKStringRef WKMediaSessionMetadataCopyTitle(WKMediaSessionMetadataRef metadata)
{
#if ENABLE(MEDIA_SESSION)
    return toCopiedAPI(toImpl(metadata)->title());
#else
    UNUSED_PARAM(metadata);
    return nullptr;
#endif
}

WKStringRef WKMediaSessionMetadataCopyArtist(WKMediaSessionMetadataRef metadata)
{
#if ENABLE(MEDIA_SESSION)
    return toCopiedAPI(toImpl(metadata)->artist());
#else
    UNUSED_PARAM(metadata);
    return nullptr;
#endif
}

WKStringRef WKMediaSessionMetadataCopyAlbum(WKMediaSessionMetadataRef metadata)
{
#if ENABLE(MEDIA_SESSION)
    return toCopiedAPI(toImpl(metadata)->album());
#else
    UNUSED_PARAM(metadata);
    return nullptr;
#endif
}

WKURLRef WKMediaSessionMetadataCopyArtworkURL(WKMediaSessionMetadataRef metadata)
{
#if ENABLE(MEDIA_SESSION)
    return toCopiedURLAPI(toImpl(metadata)->artworkURL());
#else
    UNUSED_PARAM(metadata);
    return nullptr;
#endif
}
