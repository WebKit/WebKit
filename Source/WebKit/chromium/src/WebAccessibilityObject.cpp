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

#include "AXObjectCache.h"
#include "AccessibilityObject.h"
#include "AccessibilityTable.h"
#include "AccessibilityTableCell.h"
#include "CSSPrimitiveValueMappings.h"
#include "Document.h"
#include "EventHandler.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "Node.h"
#include "PlatformKeyboardEvent.h"
#include "RenderStyle.h"
#include "UserGestureIndicator.h"
#include "WebDocument.h"
#include "WebNode.h"
#include "platform/WebPoint.h"
#include "platform/WebRect.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"

using namespace WebCore;

namespace WebKit {

void WebAccessibilityObject::reset()
{
    m_private.reset();
}

void WebAccessibilityObject::assign(const WebKit::WebAccessibilityObject& other)
{
    m_private = other.m_private;
}

bool WebAccessibilityObject::equals(const WebAccessibilityObject& n) const
{
    return (m_private.get() == n.m_private.get());
}

// static
void WebAccessibilityObject::enableAccessibility()
{
    AXObjectCache::enableAccessibility();
}

// static
bool WebAccessibilityObject::accessibilityEnabled()
{
    return AXObjectCache::accessibilityEnabled();
}

int WebAccessibilityObject::axID() const
{
    if (m_private.isNull())
        return -1;

    m_private->updateBackingStore();
    return m_private->axObjectID();
}

WebString WebAccessibilityObject::accessibilityDescription() const
{
    if (m_private.isNull())
        return WebString();

    m_private->updateBackingStore();
    return m_private->accessibilityDescription();
}

WebString WebAccessibilityObject::actionVerb() const
{
    if (m_private.isNull())
        return WebString();

    m_private->updateBackingStore();
    return m_private->actionVerb();
}

bool WebAccessibilityObject::canSetFocusAttribute() const
{
    if (m_private.isNull())
        return false;

    m_private->updateBackingStore();
    return m_private->canSetFocusAttribute();
}

bool WebAccessibilityObject::canSetValueAttribute() const
{
    if (m_private.isNull())
        return false;

    m_private->updateBackingStore();
    return m_private->canSetValueAttribute();
}

bool WebAccessibilityObject::isValid() const
{
    if (m_private.isNull())
        return false;

    m_private->updateBackingStore();
    return m_private->axObjectID();
}

unsigned WebAccessibilityObject::childCount() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->children().size();
}

WebAccessibilityObject WebAccessibilityObject::childAt(unsigned index) const
{
    if (m_private.isNull())
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    if (m_private->children().size() <= index)
        return WebAccessibilityObject();

    return WebAccessibilityObject(m_private->children()[index]);
}

WebAccessibilityObject WebAccessibilityObject::firstChild() const
{
    if (m_private.isNull())
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    return WebAccessibilityObject(m_private->firstChild());
}

WebAccessibilityObject WebAccessibilityObject::focusedChild() const
{
    if (m_private.isNull())
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    RefPtr<AccessibilityObject> focused = m_private->focusedUIElement();
    if (m_private.get() == focused.get() || m_private.get() == focused->parentObject())
        return WebAccessibilityObject(focused);

    return WebAccessibilityObject();
}

WebAccessibilityObject WebAccessibilityObject::lastChild() const
{
    if (m_private.isNull())
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    return WebAccessibilityObject(m_private->lastChild());
}


WebAccessibilityObject WebAccessibilityObject::nextSibling() const
{
    if (m_private.isNull())
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    return WebAccessibilityObject(m_private->nextSibling());
}

WebAccessibilityObject WebAccessibilityObject::parentObject() const
{
    if (m_private.isNull())
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    return WebAccessibilityObject(m_private->parentObject());
}


WebAccessibilityObject WebAccessibilityObject::previousSibling() const
{
    if (m_private.isNull())
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    return WebAccessibilityObject(m_private->previousSibling());
}

bool WebAccessibilityObject::canSetSelectedAttribute() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->canSetSelectedAttribute();
}

bool WebAccessibilityObject::isAnchor() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isAnchor();
}

bool WebAccessibilityObject::isAriaReadOnly() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return equalIgnoringCase(m_private->getAttribute(HTMLNames::aria_readonlyAttr), "true");
}

bool WebAccessibilityObject::isButtonStateMixed() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->checkboxOrRadioValue() == ButtonStateMixed;
}

bool WebAccessibilityObject::isChecked() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isChecked();
}

bool WebAccessibilityObject::isCollapsed() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isCollapsed();
}

bool WebAccessibilityObject::isControl() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isControl();
}

bool WebAccessibilityObject::isEnabled() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isEnabled();
}

bool WebAccessibilityObject::isFocused() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isFocused();
}

bool WebAccessibilityObject::isHovered() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isHovered();
}

bool WebAccessibilityObject::isIndeterminate() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isIndeterminate();
}

bool WebAccessibilityObject::isLinked() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isLinked();
}

bool WebAccessibilityObject::isLoaded() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isLoaded();
}

bool WebAccessibilityObject::isMultiSelectable() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isMultiSelectable();
}

bool WebAccessibilityObject::isOffScreen() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isOffScreen();
}

bool WebAccessibilityObject::isPasswordField() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isPasswordField();
}

bool WebAccessibilityObject::isPressed() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isPressed();
}

bool WebAccessibilityObject::isReadOnly() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isReadOnly();
}

bool WebAccessibilityObject::isRequired() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isRequired();
}

bool WebAccessibilityObject::isSelected() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isSelected();
}

bool WebAccessibilityObject::isSelectedOptionActive() const
{
    if (m_private.isNull())
        return false;

    m_private->updateBackingStore();
    return m_private->isSelectedOptionActive();
}

bool WebAccessibilityObject::isVertical() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->orientation() == AccessibilityOrientationVertical;
}

bool WebAccessibilityObject::isVisible() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isVisible();
}

bool WebAccessibilityObject::isVisited() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->isVisited();
}

WebString WebAccessibilityObject::accessKey() const
{
    if (m_private.isNull())
        return WebString();

    m_private->updateBackingStore();
    return WebString(m_private->accessKey());
}

bool WebAccessibilityObject::ariaHasPopup() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->ariaHasPopup();
}

bool WebAccessibilityObject::ariaLiveRegionAtomic() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->ariaLiveRegionAtomic();
}

bool WebAccessibilityObject::ariaLiveRegionBusy() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->ariaLiveRegionBusy();
}

WebString WebAccessibilityObject::ariaLiveRegionRelevant() const
{
    if (m_private.isNull())
        return WebString();

    m_private->updateBackingStore();
    return m_private->ariaLiveRegionRelevant();
}

WebString WebAccessibilityObject::ariaLiveRegionStatus() const
{
    if (m_private.isNull())
        return WebString();

    m_private->updateBackingStore();
    return m_private->ariaLiveRegionStatus();
}

WebRect WebAccessibilityObject::boundingBoxRect() const
{
    if (m_private.isNull())
        return WebRect();

    m_private->updateBackingStore();
    return m_private->boundingBoxRect();
}

double WebAccessibilityObject::estimatedLoadingProgress() const
{
    if (m_private.isNull())
        return 0.0;

    m_private->updateBackingStore();
    return m_private->estimatedLoadingProgress();
}

WebString WebAccessibilityObject::helpText() const
{
    if (m_private.isNull())
        return WebString();

    m_private->updateBackingStore();
    return m_private->helpText();
}

int WebAccessibilityObject::headingLevel() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->headingLevel();
}

int WebAccessibilityObject::hierarchicalLevel() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->hierarchicalLevel();
}

WebAccessibilityObject WebAccessibilityObject::hitTest(const WebPoint& point) const
{
    if (m_private.isNull())
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    IntPoint contentsPoint = m_private->documentFrameView()->windowToContents(point);
    RefPtr<AccessibilityObject> hit = m_private->accessibilityHitTest(contentsPoint);

    if (hit)
        return WebAccessibilityObject(hit);

    if (m_private->boundingBoxRect().contains(contentsPoint))
        return *this;

    return WebAccessibilityObject();
}

WebString WebAccessibilityObject::keyboardShortcut() const
{
    if (m_private.isNull())
        return WebString();

    m_private->updateBackingStore();
    String accessKey = m_private->accessKey();
    if (accessKey.isNull())
        return WebString();

    DEFINE_STATIC_LOCAL(String, modifierString, ());
    if (modifierString.isNull()) {
        unsigned modifiers = EventHandler::accessKeyModifiers();
        // Follow the same order as Mozilla MSAA implementation:
        // Ctrl+Alt+Shift+Meta+key. MSDN states that keyboard shortcut strings
        // should not be localized and defines the separator as "+".
        if (modifiers & PlatformEvent::CtrlKey)
            modifierString += "Ctrl+";
        if (modifiers & PlatformEvent::AltKey)
            modifierString += "Alt+";
        if (modifiers & PlatformEvent::ShiftKey)
            modifierString += "Shift+";
        if (modifiers & PlatformEvent::MetaKey)
            modifierString += "Win+";
    }

    return String(modifierString + accessKey);
}

bool WebAccessibilityObject::performDefaultAction() const
{
    if (m_private.isNull())
        return false;

    UserGestureIndicator gestureIndicator(DefinitelyProcessingUserGesture);

    m_private->updateBackingStore();
    return m_private->performDefaultAction();
}

WebAccessibilityRole WebAccessibilityObject::roleValue() const
{
    if (m_private.isNull())
        return WebKit::WebAccessibilityRoleUnknown;

    m_private->updateBackingStore();
    return static_cast<WebAccessibilityRole>(m_private->roleValue());
}

unsigned WebAccessibilityObject::selectionEnd() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->selectedTextRange().start + m_private->selectedTextRange().length;
}

unsigned WebAccessibilityObject::selectionStart() const
{
    if (m_private.isNull())
        return 0;

    m_private->updateBackingStore();
    return m_private->selectedTextRange().start;
}

void WebAccessibilityObject::setFocused(bool on) const
{
    if (!m_private.isNull())
        m_private->setFocused(on);
}

WebString WebAccessibilityObject::stringValue() const
{
    if (m_private.isNull())
        return WebString();

    m_private->updateBackingStore();
    return m_private->stringValue();
}

WebString WebAccessibilityObject::title() const
{
    if (m_private.isNull())
        return WebString();

    m_private->updateBackingStore();
    return m_private->title();
}

WebAccessibilityObject WebAccessibilityObject::titleUIElement() const
{
    if (m_private.isNull())
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    return WebAccessibilityObject(m_private->titleUIElement());
}

WebURL WebAccessibilityObject::url() const
{
    if (m_private.isNull())
        return WebURL();
    
    m_private->updateBackingStore();
    return m_private->url();
}

WebString WebAccessibilityObject::valueDescription() const
{
    if (m_private.isNull())
        return WebString();

    m_private->updateBackingStore();
    return m_private->valueDescription();
}

float WebAccessibilityObject::valueForRange() const
{
    if (m_private.isNull())
        return 0.0;

    m_private->updateBackingStore();
    return m_private->valueForRange();
}

float WebAccessibilityObject::maxValueForRange() const
{
    if (m_private.isNull())
        return 0.0;

    m_private->updateBackingStore();
    return m_private->maxValueForRange();
}

float WebAccessibilityObject::minValueForRange() const
{
    if (m_private.isNull())
        return 0.0;

    m_private->updateBackingStore();
    return m_private->minValueForRange();
}

WebNode WebAccessibilityObject::node() const
{
    if (m_private.isNull())
        return WebNode();

    m_private->updateBackingStore();

    Node* node = m_private->node();
    if (!node)
        return WebNode();

    return WebNode(node);
}

WebDocument WebAccessibilityObject::document() const
{
    if (m_private.isNull())
        return WebDocument();

    m_private->updateBackingStore();

    Document* document = m_private->document();
    if (!document)
        return WebDocument();

    return WebDocument(document);
}

bool WebAccessibilityObject::hasComputedStyle() const
{
    if (m_private.isNull())
        return false;

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
    if (m_private.isNull())
        return WebString();

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
    if (m_private.isNull())
        return false;

    m_private->updateBackingStore();
    return m_private->accessibilityIsIgnored();
}

bool WebAccessibilityObject::lineBreaks(WebVector<int>& result) const
{
    if (m_private.isNull())
        return false;

    m_private->updateBackingStore();
    int textLength = m_private->textLength();
    if (!textLength)
        return false;

    VisiblePosition pos = m_private->visiblePositionForIndex(textLength);
    int lineBreakCount = m_private->lineForPosition(pos);
    if (lineBreakCount <= 0)
        return false;

    WebVector<int> lineBreaks(static_cast<size_t>(lineBreakCount));
    for (int i = 0; i < lineBreakCount; i++) {
        PlainTextRange range = m_private->doAXRangeForLine(i);
        lineBreaks[i] = range.start + range.length;
    }
    result.swap(lineBreaks);
    return true;
}

unsigned WebAccessibilityObject::columnCount() const
{
    if (m_private.isNull())
        return false;

    m_private->updateBackingStore();
    if (!m_private->isAccessibilityTable())
        return 0;

    return static_cast<WebCore::AccessibilityTable*>(m_private.get())->columnCount();
}

unsigned WebAccessibilityObject::rowCount() const
{
    if (m_private.isNull())
        return false;

    m_private->updateBackingStore();
    if (!m_private->isAccessibilityTable())
        return 0;

    return static_cast<WebCore::AccessibilityTable*>(m_private.get())->rowCount();
}

WebAccessibilityObject WebAccessibilityObject::cellForColumnAndRow(unsigned column, unsigned row) const
{
    m_private->updateBackingStore();
    if (!m_private->isAccessibilityTable())
        return WebAccessibilityObject();

    WebCore::AccessibilityTableCell* cell = static_cast<WebCore::AccessibilityTable*>(m_private.get())->cellForColumnAndRow(column, row);
    return WebAccessibilityObject(static_cast<WebCore::AccessibilityObject*>(cell));
}

unsigned WebAccessibilityObject::cellColumnIndex() const
{
    m_private->updateBackingStore();
    if (!m_private->isTableCell())
       return 0;

    pair<int, int> columnRange;
    static_cast<WebCore::AccessibilityTableCell*>(m_private.get())->columnIndexRange(columnRange);
    return columnRange.first;
}

unsigned WebAccessibilityObject::cellColumnSpan() const
{
    m_private->updateBackingStore();
    if (!m_private->isTableCell())
       return 0;

    pair<int, int> columnRange;
    static_cast<WebCore::AccessibilityTableCell*>(m_private.get())->columnIndexRange(columnRange);
    return columnRange.second;
}

unsigned WebAccessibilityObject::cellRowIndex() const
{
    m_private->updateBackingStore();
    if (!m_private->isTableCell())
       return 0;

    pair<int, int> rowRange;
    static_cast<WebCore::AccessibilityTableCell*>(m_private.get())->rowIndexRange(rowRange);
    return rowRange.first;
}

unsigned WebAccessibilityObject::cellRowSpan() const
{
    m_private->updateBackingStore();
    if (!m_private->isTableCell())
       return 0;

    pair<int, int> rowRange;
    static_cast<WebCore::AccessibilityTableCell*>(m_private.get())->rowIndexRange(rowRange);
    return rowRange.second;
}

WebAccessibilityObject::WebAccessibilityObject(const WTF::PassRefPtr<WebCore::AccessibilityObject>& object)
    : m_private(object)
{
}

WebAccessibilityObject& WebAccessibilityObject::operator=(const WTF::PassRefPtr<WebCore::AccessibilityObject>& object)
{
    m_private = object;
    return *this;
}

WebAccessibilityObject::operator WTF::PassRefPtr<WebCore::AccessibilityObject>() const
{
    return m_private.get();
}

} // namespace WebKit
