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

#include "WebExceptionCode.h"
#include "platform/WebCommon.h"
#include "platform/WebPrivatePtr.h"
#include "platform/WebString.h"

namespace WebCore { class Node; }

namespace WebKit {
class WebDOMEvent;
class WebDOMEventListener;
class WebDOMEventListenerPrivate;
class WebDocument;
class WebElement;
class WebFrame;
class WebNodeList;
class WebPluginContainer;

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

    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT void assign(const WebNode&);

    WEBKIT_EXPORT bool equals(const WebNode&) const;
    // Required for using WebNodes in std maps.  Note the order used is
    // arbitrary and should not be expected to have any specific meaning.
    WEBKIT_EXPORT bool lessThan(const WebNode&) const;

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

    WEBKIT_EXPORT NodeType nodeType() const;
    WEBKIT_EXPORT WebNode parentNode() const;
    WEBKIT_EXPORT WebString nodeName() const;
    WEBKIT_EXPORT WebString nodeValue() const;
    WEBKIT_EXPORT bool setNodeValue(const WebString&);
    WEBKIT_EXPORT WebDocument document() const;
    WEBKIT_EXPORT WebNode firstChild() const;
    WEBKIT_EXPORT WebNode lastChild() const;
    WEBKIT_EXPORT WebNode previousSibling() const;
    WEBKIT_EXPORT WebNode nextSibling() const;
    WEBKIT_EXPORT bool hasChildNodes() const;
    WEBKIT_EXPORT WebNodeList childNodes();
    WEBKIT_EXPORT bool appendChild(const WebNode& child);
    WEBKIT_EXPORT WebString createMarkup() const;
    WEBKIT_EXPORT bool isLink() const;
    WEBKIT_EXPORT bool isTextNode() const;
    WEBKIT_EXPORT bool isFocusable() const;
    WEBKIT_EXPORT bool isContentEditable() const;
    WEBKIT_EXPORT bool isElementNode() const;
    WEBKIT_EXPORT bool hasEventListeners(const WebString& eventType) const;
    WEBKIT_EXPORT void addEventListener(const WebString& eventType, WebDOMEventListener* listener, bool useCapture);
    WEBKIT_EXPORT void removeEventListener(const WebString& eventType, WebDOMEventListener* listener, bool useCapture);
    WEBKIT_EXPORT bool dispatchEvent(const WebDOMEvent&);
    WEBKIT_EXPORT void simulateClick();
    WEBKIT_EXPORT WebNodeList getElementsByTagName(const WebString&) const;
    WEBKIT_EXPORT WebElement querySelector(const WebString&, WebExceptionCode&) const;
    WEBKIT_EXPORT WebElement rootEditableElement() const;
    WEBKIT_EXPORT bool focused() const;
    WEBKIT_EXPORT bool remove();

    // Returns true if the node has a non-empty bounding box in layout.
    // This does not 100% guarantee the user can see it, but is pretty close.
    // Note: This method only works properly after layout has occurred.
    WEBKIT_EXPORT bool hasNonEmptyBoundingBox() const;
    WEBKIT_EXPORT WebPluginContainer* pluginContainer() const;

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
