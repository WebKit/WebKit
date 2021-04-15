/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DefaultResourceLoadPriority.h"

namespace WebCore {

ResourceLoadPriority DefaultResourceLoadPriority::forResourceType(CachedResource::Type type)
{
    switch (type) {
    case CachedResource::Type::MainResource:
        return ResourceLoadPriority::VeryHigh;
    case CachedResource::Type::CSSStyleSheet:
    case CachedResource::Type::Script:
        return ResourceLoadPriority::High;
    case CachedResource::Type::SVGFontResource:
    case CachedResource::Type::MediaResource:
    case CachedResource::Type::FontResource:
    case CachedResource::Type::RawResource:
    case CachedResource::Type::Icon:
        return ResourceLoadPriority::Medium;
    case CachedResource::Type::ImageResource:
        return ResourceLoadPriority::Low;
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
        return ResourceLoadPriority::High;
#endif
    case CachedResource::Type::SVGDocumentResource:
        return ResourceLoadPriority::Low;
    case CachedResource::Type::Beacon:
    case CachedResource::Type::Ping:
        return ResourceLoadPriority::VeryLow;
    case CachedResource::Type::LinkPrefetch:
        return ResourceLoadPriority::VeryLow;
#if ENABLE(VIDEO)
    case CachedResource::Type::TextTrackResource:
        return ResourceLoadPriority::Low;
#endif
#if ENABLE(MODEL_ELEMENT)
    case CachedResource::Type::ModelResource:
        return ResourceLoadPriority::Medium;
#endif
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
        return ResourceLoadPriority::Low;
#endif
    }
    ASSERT_NOT_REACHED();
    return ResourceLoadPriority::Low;
}

}
