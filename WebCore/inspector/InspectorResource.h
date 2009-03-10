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

#ifndef InspectorResource_h
#define InspectorResource_h

#include <JavaScriptCore/JSContextRef.h>

#include "HTTPHeaderMap.h"
#include "KURL.h"

#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace JSC {
    class UString;
}

namespace WebCore {
    class DocumentLoader;
    class Frame;
    struct XMLHttpRequestResource;

    class InspectorResource : public RefCounted<InspectorResource> {
    public:

        // Keep these in sync with WebInspector.Resource.Type
        enum Type {
            Doc,
            Stylesheet,
            Image,
            Font,
            Script,
            XHR,
            Media,
            Other
        };

        static PassRefPtr<InspectorResource> create(long long identifier, DocumentLoader* documentLoader, Frame* frame)
        {
            return adoptRef(new InspectorResource(identifier, documentLoader, frame));
        }

        ~InspectorResource();
        Type type() const;
        void setScriptObject(JSContextRef, JSObjectRef);
        void setXMLHttpRequestProperties(const JSC::UString& data);
        void setScriptProperties(const JSC::UString& data);

        String sourceString() const;

        long long identifier;
        RefPtr<DocumentLoader> loader;
        RefPtr<Frame> frame;
        KURL requestURL;
        HTTPHeaderMap requestHeaderFields;
        HTTPHeaderMap responseHeaderFields;
        String mimeType;
        String suggestedFilename;
        JSContextRef scriptContext;
        JSObjectRef scriptObject;
        long long expectedContentLength;
        bool cached;
        bool finished;
        bool failed;
        int length;
        int responseStatusCode;
        double startTime;
        double responseReceivedTime;
        double endTime;

    private:
        InspectorResource(long long identifier, DocumentLoader*, Frame*);

        OwnPtr<XMLHttpRequestResource> xmlHttpRequestResource;
    };

} // namespace WebCore

#endif // InspectorResource_h
