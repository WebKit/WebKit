/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef WebRenderObject_h
#define WebRenderObject_h

#include "APIObject.h"
#include <WebCore/IntRect.h>
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

namespace API {
class Array;
}

namespace WebCore {
    class RenderObject;
}

namespace WebKit {

class WebPage;

class WebRenderObject : public API::ObjectImpl<API::Object::Type::RenderObject> {
public:
    static PassRefPtr<WebRenderObject> create(WebPage*);
    static PassRefPtr<WebRenderObject> create(WebCore::RenderObject* renderer)
    {
        return adoptRef(new WebRenderObject(renderer, false));
    }

    static PassRefPtr<WebRenderObject> create(const String& name, const String& elementTagName, const String& elementID, PassRefPtr<API::Array> elementClassNames, WebCore::IntPoint absolutePosition, WebCore::IntRect frameRect, PassRefPtr<API::Array> children);

    virtual ~WebRenderObject();

    API::Array* children() const { return m_children.get(); }

    const String& name() const { return m_name; }
    const String& elementTagName() const { return m_elementTagName; }
    const String& elementID() const { return m_elementID; }
    API::Array* elementClassNames() const { return m_elementClassNames.get(); }
    WebCore::IntPoint absolutePosition() const { return m_absolutePosition; }
    WebCore::IntRect frameRect() const { return m_frameRect; }

private:
    WebRenderObject(WebCore::RenderObject*, bool shouldIncludeDescendants);
    WebRenderObject(const String& name, const String& elementTagName, const String& elementID, PassRefPtr<API::Array> elementClassNames, WebCore::IntPoint absolutePosition, WebCore::IntRect frameRect, PassRefPtr<API::Array> children);

    RefPtr<API::Array> m_children;

    String m_name;
    String m_elementTagName;
    String m_elementID;
    RefPtr<API::Array> m_elementClassNames;
    WebCore::IntPoint m_absolutePosition;
    WebCore::IntRect m_frameRect;
};

} // namespace WebKit

#endif // WebRenderObject_h
