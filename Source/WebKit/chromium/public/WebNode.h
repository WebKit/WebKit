/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebNode_h
#define WebNode_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"
#include "WebString.h"

namespace WebCore { class Node; }

namespace WebKit {
class WebDOMEventListener;
class WebDOMEventListenerPrivate;
class WebDocument;
class WebFrame;
class WebNodeList;

// Provides access to some properties of a DOM node.
class WebNode {
public:
    virtual ~WebNode() { reset(); }

    WebNode() { }
    WebNode(const WebNode& n) { assign(n); }
    WebNode& operator=(const WebNode& n)
    {
        assign(n);
        return *this;
    }

    WEBKIT_API void reset();
    WEBKIT_API void assign(const WebNode&);

    WEBKIT_API bool equals(const WebNode&) const;
    // Required for using WebNodes in std maps.  Note the order used is
    // arbitrary and should not be expected to have any specific meaning.
    WEBKIT_API bool lessThan(const WebNode&) const;
    
    bool isNull() const { return m_private.isNull(); }

    enum NodeType {
        ElementNode = 1,
        AttributeNode,
        TextNode,
        CDataSectionNode,
        EntityReferenceNode,
        EntityNode,
        ProcessingInstructionsNode,
        CommentNode,
        DocumentNode,
        DocumentTypeNode,
        DocumentFragmentNode,
        NotationNode,
        XPathNamespaceNode,
        ShadowRootNode
    };
    WEBKIT_API NodeType nodeType() const;
    WEBKIT_API WebNode parentNode() const;
    WEBKIT_API WebString nodeName() const;
    WEBKIT_API WebString nodeValue() const;
    WEBKIT_API bool setNodeValue(const WebString&);
    WEBKIT_API WebDocument document() const;
    WEBKIT_API WebNode firstChild() const;
    WEBKIT_API WebNode lastChild() const;
    WEBKIT_API WebNode previousSibling() const;
    WEBKIT_API WebNode nextSibling() const;
    WEBKIT_API bool hasChildNodes() const;
    WEBKIT_API WebNodeList childNodes();
    WEBKIT_API WebString createMarkup() const;
    WEBKIT_API bool isTextNode() const;
    WEBKIT_API bool isFocusable() const;
    WEBKIT_API bool isContentEditable() const;
    WEBKIT_API bool isElementNode() const;
    WEBKIT_API void addEventListener(const WebString& eventType, WebDOMEventListener* listener, bool useCapture);
    WEBKIT_API void removeEventListener(const WebString& eventType, WebDOMEventListener* listener, bool useCapture);
    WEBKIT_API void simulateClick();
    WEBKIT_API WebNodeList getElementsByTagName(const WebString&) const;

    // Returns true if the node has a non-empty bounding box in layout.
    // This does not 100% guarantee the user can see it, but is pretty close.
    // Note: This method only works properly after layout has occurred.
    WEBKIT_API bool hasNonEmptyBoundingBox() const;

    template<typename T> T to()
    {
        T res;
        res.WebNode::assign(*this);
        return res;
    }

    template<typename T> const T toConst() const
    {
        T res;
        res.WebNode::assign(*this);
        return res;
    }

#if WEBKIT_IMPLEMENTATION
    WebNode(const WTF::PassRefPtr<WebCore::Node>&);
    WebNode& operator=(const WTF::PassRefPtr<WebCore::Node>&);
    operator WTF::PassRefPtr<WebCore::Node>() const;
#endif

#if WEBKIT_IMPLEMENTATION
    template<typename T> T* unwrap()
    {
        return static_cast<T*>(m_private.get());
    }

    template<typename T> const T* constUnwrap() const
    {
        return static_cast<const T*>(m_private.get());
    }
#endif

protected:
    WebPrivatePtr<WebCore::Node> m_private;
};

inline bool operator==(const WebNode& a, const WebNode& b)
{
    return a.equals(b);
}

inline bool operator!=(const WebNode& a, const WebNode& b)
{
    return !(a == b);
}

inline bool operator<(const WebNode& a, const WebNode& b)
{
    return a.lessThan(b);
}

} // namespace WebKit

#endif
