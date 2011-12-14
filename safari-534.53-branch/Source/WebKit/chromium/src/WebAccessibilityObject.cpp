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

#include "config.h"
#include "WebAccessibilityObject.h"

#include "AccessibilityObject.h"
#include "CSSPrimitiveValueMappings.h"
#include "Document.h"
#include "EventHandler.h"
#include "FrameView.h"
#include "Node.h"
#include "PlatformKeyboardEvent.h"
#include "RenderStyle.h"
#include "UserGestureIndicator.h"
#include "WebDocument.h"
#include "WebNode.h"
#include "WebPoint.h"
#include "WebRect.h"
#include "WebString.h"
#include "WebURL.h"

using namespace WebCore;

namespace WebKit {

class WebAccessibilityObjectPrivate : public WebCore::AccessibilityObject {
};

void WebAccessibilityObject::reset()
{
    assign(0);
}

void WebAccessibilityObject::assign(const WebKit::WebAccessibilityObject& other)
{
    WebAccessibilityObjectPrivate* p = const_cast<WebAccessibilityObjectPrivate*>(other.m_private);
    if (p)
        p->ref();
    assign(p);
}

bool WebAccessibilityObject::equals(const WebAccessibilityObject& n) const
{
    return (m_private == n.m_private);
}

WebString WebAccessibilityObject::accessibilityDescription() const
{
    if (!m_private)
        return WebString();

    m_private->updateBackingStore();
    return m_private->accessibilityDescription();
}

WebString WebAccessibilityObject::actionVerb() const
{
    if (!m_private)
        return WebString();

    m_private->updateBackingStore();
    return m_private->actionVerb();
}

bool WebAccessibilityObject::canSetFocusAttribute() const
{
    if (!m_private)
        return false;

    m_private->updateBackingStore();
    return m_private->canSetFocusAttribute();
}

bool WebAccessibilityObject::canSetValueAttribute() const
{
    if (!m_private)
        return false;

    m_private->updateBackingStore();
    return m_private->canSetValueAttribute();
}

bool WebAccessibilityObject::isValid() const
{
    if (!m_private)
        return false;

    m_private->updateBackingStore();
    return m_private->axObjectID();
}

unsigned WebAccessibilityObject::childCount() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->children().size();
}

WebAccessibilityObject WebAccessibilityObject::childAt(unsigned index) const
{
    if (!m_private)
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    if (m_private->children().size() <= index)
        return WebAccessibilityObject();

    return WebAccessibilityObject(m_private->children()[index]);
}

WebAccessibilityObject WebAccessibilityObject::firstChild() const
{
    if (!m_private)
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    return WebAccessibilityObject(m_private->firstChild());
}

WebAccessibilityObject WebAccessibilityObject::focusedChild() const
{
    if (!m_private)
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    RefPtr<AccessibilityObject> focused = m_private->focusedUIElement();
    if (m_private == focused.get() || focused->parentObject() == m_private)
        return WebAccessibilityObject(focused);

    return WebAccessibilityObject();
}

WebAccessibilityObject WebAccessibilityObject::lastChild() const
{
    if (!m_private)
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    return WebAccessibilityObject(m_private->lastChild());
}


WebAccessibilityObject WebAccessibilityObject::nextSibling() const
{
    if (!m_private)
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    return WebAccessibilityObject(m_private->nextSibling());
}

WebAccessibilityObject WebAccessibilityObject::parentObject() const
{
    if (!m_private)
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    return WebAccessibilityObject(m_private->parentObject());
}


WebAccessibilityObject WebAccessibilityObject::previousSibling() const
{
    if (!m_private)
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    return WebAccessibilityObject(m_private->previousSibling());
}

bool WebAccessibilityObject::canSetSelectedAttribute() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->canSetSelectedAttribute();
}

bool WebAccessibilityObject::isAnchor() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->isAnchor();
}

bool WebAccessibilityObject::isChecked() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->isChecked();
}

bool WebAccessibilityObject::isCollapsed() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->isCollapsed();
}


bool WebAccessibilityObject::isFocused() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->isFocused();
}

bool WebAccessibilityObject::isEnabled() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->isEnabled();
}

bool WebAccessibilityObject::isHovered() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->isHovered();
}

bool WebAccessibilityObject::isIndeterminate() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->isIndeterminate();
}

bool WebAccessibilityObject::isLinked() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->isLinked();
}

bool WebAccessibilityObject::isMultiSelectable() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->isMultiSelectable();
}

bool WebAccessibilityObject::isOffScreen() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->isOffScreen();
}

bool WebAccessibilityObject::isPasswordField() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->isPasswordField();
}

bool WebAccessibilityObject::isPressed() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->isPressed();
}

bool WebAccessibilityObject::isReadOnly() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->isReadOnly();
}

bool WebAccessibilityObject::isSelected() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->isSelected();
}

bool WebAccessibilityObject::isVisible() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->isVisible();
}

bool WebAccessibilityObject::isVisited() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->isVisited();
}

WebRect WebAccessibilityObject::boundingBoxRect() const
{
    if (!m_private)
        return WebRect();

    m_private->updateBackingStore();
    return m_private->boundingBoxRect();
}

WebString WebAccessibilityObject::helpText() const
{
    if (!m_private)
        return WebString();

    m_private->updateBackingStore();
    return m_private->helpText();
}

int WebAccessibilityObject::headingLevel() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->headingLevel();
}

WebAccessibilityObject WebAccessibilityObject::hitTest(const WebPoint& point) const
{
    if (!m_private)
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    IntPoint contentsPoint = m_private->documentFrameView()->windowToContents(point);
    RefPtr<AccessibilityObject> hit = m_private->accessibilityHitTest(contentsPoint);

    if (hit.get())
        return WebAccessibilityObject(hit);

    if (m_private->boundingBoxRect().contains(contentsPoint))
        return *this;

    return WebAccessibilityObject();
}

WebString WebAccessibilityObject::keyboardShortcut() const
{
    if (!m_private)
        return WebString();

    m_private->updateBackingStore();
    String accessKey = m_private->accessKey();
    if (accessKey.isNull())
        return WebString();

    static String modifierString;
    if (modifierString.isNull()) {
        unsigned modifiers = EventHandler::accessKeyModifiers();
        // Follow the same order as Mozilla MSAA implementation:
        // Ctrl+Alt+Shift+Meta+key. MSDN states that keyboard shortcut strings
        // should not be localized and defines the separator as "+".
        if (modifiers & PlatformKeyboardEvent::CtrlKey)
            modifierString += "Ctrl+";
        if (modifiers & PlatformKeyboardEvent::AltKey)
            modifierString += "Alt+";
        if (modifiers & PlatformKeyboardEvent::ShiftKey)
            modifierString += "Shift+";
        if (modifiers & PlatformKeyboardEvent::MetaKey)
            modifierString += "Win+";
    }

    return String(modifierString + accessKey);
}

bool WebAccessibilityObject::performDefaultAction() const
{
    if (!m_private)
        return false;

    UserGestureIndicator gestureIndicator(DefinitelyProcessingUserGesture);

    m_private->updateBackingStore();
    return m_private->performDefaultAction();
}

WebAccessibilityRole WebAccessibilityObject::roleValue() const
{
    if (!m_private)
        return WebKit::WebAccessibilityRoleUnknown;

    m_private->updateBackingStore();
    return static_cast<WebAccessibilityRole>(m_private->roleValue());
}

void WebAccessibilityObject::setFocused(bool on) const
{
    if (m_private)
        m_private->setFocused(on);
}

WebString WebAccessibilityObject::stringValue() const
{
    if (!m_private)
        return WebString();

    m_private->updateBackingStore();
    return m_private->stringValue();
}

WebString WebAccessibilityObject::title() const
{
    if (!m_private)
        return WebString();

    m_private->updateBackingStore();
    return m_private->title();
}

WebURL WebAccessibilityObject::url() const
{
    if (!m_private)
        return WebURL();
    
    m_private->updateBackingStore();
    return m_private->url();
}

WebNode WebAccessibilityObject::node() const
{
    if (!m_private)
        return WebNode();

    m_private->updateBackingStore();

    Node* node = m_private->node();
    if (!node)
        return WebNode();

    return WebNode(node);
}

WebDocument WebAccessibilityObject::document() const
{
    if (!m_private)
        return WebDocument();

    m_private->updateBackingStore();

    Document* document = m_private->document();
    if (!document)
        return WebDocument();

    return WebDocument(document);
}

bool WebAccessibilityObject::hasComputedStyle() const
{
    Document* document = m_private->document();
    if (document)
        document->updateStyleIfNeeded();

    Node* node = m_private->node();
    if (!node)
        return false;

    return node->computedStyle();
}

WebString WebAccessibilityObject::computedStyleDisplay() const
{
    Document* document = m_private->document();
    if (document)
        document->updateStyleIfNeeded();

    Node* node = m_private->node();
    if (!node)
        return WebString();

    RenderStyle* renderStyle = node->computedStyle();
    if (!renderStyle)
        return WebString();

    return WebString(CSSPrimitiveValue::create(renderStyle->display())->getStringValue());
}

bool WebAccessibilityObject::accessibilityIsIgnored() const
{
    m_private->updateBackingStore();
    return m_private->accessibilityIsIgnored();
}

WebAccessibilityObject::WebAccessibilityObject(const WTF::PassRefPtr<WebCore::AccessibilityObject>& object)
    : m_private(static_cast<WebAccessibilityObjectPrivate*>(object.releaseRef()))
{
}

WebAccessibilityObject& WebAccessibilityObject::operator=(const WTF::PassRefPtr<WebCore::AccessibilityObject>& object)
{
    assign(static_cast<WebAccessibilityObjectPrivate*>(object.releaseRef()));
    return *this;
}

WebAccessibilityObject::operator WTF::PassRefPtr<WebCore::AccessibilityObject>() const
{
    return PassRefPtr<WebCore::AccessibilityObject>(const_cast<WebAccessibilityObjectPrivate*>(m_private));
}

void WebAccessibilityObject::assign(WebAccessibilityObjectPrivate* p)
{
    // p is already ref'd for us by the caller
    if (m_private)
        m_private->deref();
    m_private = p;
}

} // namespace WebKit
