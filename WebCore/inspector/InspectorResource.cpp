/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorResource.h"

#include "CachedResource.h"
#include "DocLoader.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "TextEncoding.h"

#include <runtime/JSLock.h>

namespace WebCore {

// XMLHttpRequestResource Class

struct XMLHttpRequestResource {
    XMLHttpRequestResource(const JSC::UString& sourceString)
    {
        JSC::JSLock lock(false);
        this->sourceString = sourceString.rep();
    }

    ~XMLHttpRequestResource()
    {
        JSC::JSLock lock(false);
        sourceString.clear();
    }

    RefPtr<JSC::UString::Rep> sourceString;
};

    InspectorResource::InspectorResource(long long identifier, DocumentLoader* documentLoader, Frame* frame)
    : identifier(identifier)
    , loader(documentLoader)
    , frame(frame)
    , scriptContext(0)
    , scriptObject(0)
    , expectedContentLength(0)
    , cached(false)
    , finished(false)
    , failed(false)
    , length(0)
    , responseStatusCode(0)
    , startTime(-1.0)
    , responseReceivedTime(-1.0)
    , endTime(-1.0)
    , xmlHttpRequestResource(0)
    {
    }

InspectorResource::~InspectorResource()
{
    setScriptObject(0, 0);
}

InspectorResource::Type InspectorResource::type() const
{
    if (xmlHttpRequestResource)
        return XHR;

    if (requestURL == loader->requestURL())
        return Doc;

    if (loader->frameLoader() && requestURL == loader->frameLoader()->iconURL())
        return Image;

    CachedResource* cachedResource = frame->document()->docLoader()->cachedResource(requestURL.string());
    if (!cachedResource)
        return Other;

    switch (cachedResource->type()) {
        case CachedResource::ImageResource:
            return Image;
        case CachedResource::FontResource:
            return Font;
        case CachedResource::CSSStyleSheet:
#if ENABLE(XSLT)
        case CachedResource::XSLStyleSheet:
#endif
            return Stylesheet;
        case CachedResource::Script:
            return Script;
        default:
            return Other;
    }
}

void InspectorResource::setScriptObject(JSContextRef context, JSObjectRef newScriptObject)
{
    if (scriptContext && scriptObject)
        JSValueUnprotect(scriptContext, scriptObject);

    scriptObject = newScriptObject;
    scriptContext = context;

    ASSERT((context && newScriptObject) || (!context && !newScriptObject));
    if (context && newScriptObject)
        JSValueProtect(context, newScriptObject);
}

void InspectorResource::setXMLHttpRequestProperties(const JSC::UString& data)
{
    xmlHttpRequestResource.set(new XMLHttpRequestResource(data));
}

void InspectorResource::setScriptProperties(const JSC::UString& data)
{
    xmlHttpRequestResource.set(new XMLHttpRequestResource(data));
}

String InspectorResource::sourceString() const
{
    if (xmlHttpRequestResource)
        return JSC::UString(xmlHttpRequestResource->sourceString);

    RefPtr<SharedBuffer> buffer;
    String textEncodingName;

    if (requestURL == loader->requestURL()) {
        buffer = loader->mainResourceData();
        textEncodingName = frame->document()->inputEncoding();
    } else {
        CachedResource* cachedResource = frame->document()->docLoader()->cachedResource(requestURL.string());
        if (!cachedResource)
            return String();

        if (cachedResource->isPurgeable()) {
            // If the resource is purgeable then make it unpurgeable to get
            // get its data. This might fail, in which case we return an
            // empty String.
            // FIXME: should we do something else in the case of a purged
            // resource that informs the user why there is no data in the
            // inspector?
            if (!cachedResource->makePurgeable(false))
                return String();
        }

        buffer = cachedResource->data();
        textEncodingName = cachedResource->encoding();
    }

    if (!buffer)
        return String();

    TextEncoding encoding(textEncodingName);
    if (!encoding.isValid())
        encoding = WindowsLatin1Encoding();
    return encoding.decode(buffer->data(), buffer->size());
}

} // namespace WebCore
