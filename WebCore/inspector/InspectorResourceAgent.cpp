/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorResourceAgent.h"

#include "Base64.h"
#include "Cache.h"
#include "CachedResource.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "KURL.h"
#include "SharedBuffer.h"
#include "TextEncoding.h"

#include <wtf/RefPtr.h>

#if ENABLE(INSPECTOR)

namespace WebCore {

bool InspectorResourceAgent::resourceContent(Document* document, const KURL& url, String* result)
{
    if (!document)
        return false;

    String textEncodingName;
    RefPtr<SharedBuffer> buffer = InspectorResourceAgent::resourceData(document, url, &textEncodingName);

    if (buffer) {
        TextEncoding encoding(textEncodingName);
        if (!encoding.isValid())
            encoding = WindowsLatin1Encoding();
        *result = encoding.decode(buffer->data(), buffer->size());
        return true;
    }

    return false;
}

bool InspectorResourceAgent::resourceContentBase64(Document* document, const KURL& url, String* result)
{
    Vector<char> out;
    String textEncodingName;
    RefPtr<SharedBuffer> data = InspectorResourceAgent::resourceData(document, url, &textEncodingName);
    if (!data) {
        *result = String();
        return false;
    }

    base64Encode(data->buffer(), out);
    *result = String(out.data(), out.size());
    return true;
}

PassRefPtr<SharedBuffer> InspectorResourceAgent::resourceData(Document* document, const KURL& url, String* textEncodingName)
{
    FrameLoader* frameLoader = document->frame()->loader();
    DocumentLoader* loader = frameLoader->documentLoader();
    if (equalIgnoringFragmentIdentifier(url, loader->url())) {
        *textEncodingName = document->inputEncoding();
        return frameLoader->documentLoader()->mainResourceData();
    }

    CachedResource* cachedResource = InspectorResourceAgent::cachedResource(document, url);
    if (!cachedResource)
        return 0;

    if (cachedResource->isPurgeable()) {
        // If the resource is purgeable then make it unpurgeable to get
        // get its data. This might fail, in which case we return an
        // empty String.
        // FIXME: should we do something else in the case of a purged
        // resource that informs the user why there is no data in the
        // inspector?
        if (!cachedResource->makePurgeable(false))
            return 0;
    }

    *textEncodingName = cachedResource->encoding();
    return cachedResource->data();
}

InspectorResource::Type InspectorResourceAgent::cachedResourceType(Document* document, const KURL& url)
{
    CachedResource* cachedResource = InspectorResourceAgent::cachedResource(document, url);
    if (!cachedResource)
        return InspectorResource::Other;

    switch (cachedResource->type()) {
    case CachedResource::ImageResource:
        return InspectorResource::Image;
    case CachedResource::FontResource:
        return InspectorResource::Font;
    case CachedResource::CSSStyleSheet:
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
#endif
        return InspectorResource::Stylesheet;
    case CachedResource::Script:
        return InspectorResource::Script;
    default:
        return InspectorResource::Other;
    }
}

CachedResource* InspectorResourceAgent::cachedResource(Document* document, const KURL& url)
{
    const String& urlString = url.string();
    CachedResource* cachedResource = document->cachedResourceLoader()->cachedResource(urlString);
    if (!cachedResource)
        cachedResource = cache()->resourceForURL(urlString);
    return cachedResource;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
