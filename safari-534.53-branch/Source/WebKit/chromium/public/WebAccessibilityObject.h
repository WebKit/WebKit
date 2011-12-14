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

#ifndef WebAccessibilityObject_h
#define WebAccessibilityObject_h

#include "WebAccessibilityRole.h"
#include "WebCommon.h"

#if WEBKIT_IMPLEMENTATION
namespace WebCore { class AccessibilityObject; }
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace WebKit {

class WebAccessibilityObjectPrivate;
class WebNode;
class WebDocument;
class WebString;
class WebURL;
struct WebPoint;
struct WebRect;

// A container for passing around a reference to AccessibilityObject.
class WebAccessibilityObject {
public:
    ~WebAccessibilityObject() { reset(); }

    WebAccessibilityObject() : m_private(0) { }
    WebAccessibilityObject(const WebAccessibilityObject& o) : m_private(0) { assign(o); }
    WebAccessibilityObject& operator=(const WebAccessibilityObject& o)
    {
        assign(o);
        return *this;
    }

    WEBKIT_API void reset();
    WEBKIT_API void assign(const WebAccessibilityObject&);
    WEBKIT_API bool equals(const WebAccessibilityObject&) const;

    bool isNull() const { return !m_private; }

    WEBKIT_API WebString accessibilityDescription() const;
    WEBKIT_API WebString actionVerb() const;
    WEBKIT_API bool canSetFocusAttribute() const;
    WEBKIT_API bool canSetValueAttribute() const;
    WEBKIT_API bool isValid() const;

    WEBKIT_API unsigned childCount() const;

    WEBKIT_API WebAccessibilityObject childAt(unsigned) const;
    WEBKIT_API WebAccessibilityObject firstChild() const;
    WEBKIT_API WebAccessibilityObject focusedChild() const;
    WEBKIT_API WebAccessibilityObject lastChild() const;
    WEBKIT_API WebAccessibilityObject nextSibling() const;
    WEBKIT_API WebAccessibilityObject parentObject() const;
    WEBKIT_API WebAccessibilityObject previousSibling() const;

    WEBKIT_API bool canSetSelectedAttribute() const;
    WEBKIT_API bool isAnchor() const;
    WEBKIT_API bool isChecked() const;
    WEBKIT_API bool isCollapsed() const;
    WEBKIT_API bool isFocused() const;
    WEBKIT_API bool isEnabled() const;
    WEBKIT_API bool isHovered() const;
    WEBKIT_API bool isIndeterminate() const;
    WEBKIT_API bool isLinked() const;
    WEBKIT_API bool isMultiSelectable() const;
    WEBKIT_API bool isOffScreen() const;
    WEBKIT_API bool isPasswordField() const;
    WEBKIT_API bool isPressed() const;
    WEBKIT_API bool isReadOnly() const;
    WEBKIT_API bool isSelected() const;
    WEBKIT_API bool isVisible() const;
    WEBKIT_API bool isVisited() const;

    WEBKIT_API WebRect boundingBoxRect() const;
    WEBKIT_API WebString helpText() const;
    WEBKIT_API int headingLevel() const;
    WEBKIT_API WebAccessibilityObject hitTest(const WebPoint&) const;
    WEBKIT_API WebString keyboardShortcut() const;
    WEBKIT_API bool performDefaultAction() const;
    WEBKIT_API WebAccessibilityRole roleValue() const;
    WEBKIT_API void setFocused(bool) const;
    WEBKIT_API WebString stringValue() const;
    WEBKIT_API WebString title() const;
    WEBKIT_API WebURL url() const;

    WEBKIT_API WebNode node() const;
    WEBKIT_API WebDocument document() const;
    WEBKIT_API bool hasComputedStyle() const;
    WEBKIT_API WebString computedStyleDisplay() const;
    WEBKIT_API bool accessibilityIsIgnored() const;

#if WEBKIT_IMPLEMENTATION
    WebAccessibilityObject(const WTF::PassRefPtr<WebCore::AccessibilityObject>&);
    WebAccessibilityObject& operator=(const WTF::PassRefPtr<WebCore::AccessibilityObject>&);
    operator WTF::PassRefPtr<WebCore::AccessibilityObject>() const;
#endif

private:
    void assign(WebAccessibilityObjectPrivate*);
    WebAccessibilityObjectPrivate* m_private;
};

} // namespace WebKit

#endif
