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
#include "WebDocument.h"
#include "WebNode.h"
#include <public/WebPoint.h>
#include <public/WebRect.h>
#include <public/WebString.h>
#include <public/WebURL.h>
#include <wtf/text/StringBuilder.h>

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

bool WebAccessibilityObject::isDetached() const
{
    if (m_private.isNull())
        return true;

    return m_private->isDetached();
}

int WebAccessibilityObject::axID() const
{
    if (isDetached())
        return -1;

    return m_private->axObjectID();
}

bool WebAccessibilityObject::updateBackingStoreAndCheckValidity()
{
    if (!isDetached())
        m_private->updateBackingStore();
    return !isDetached();
}

WebString WebAccessibilityObject::accessibilityDescription() const
{
    if (isDetached())
        return WebString();

    return m_private->accessibilityDescription();
}

WebString WebAccessibilityObject::actionVerb() const
{
    if (isDetached())
        return WebString();

    return m_private->actionVerb();
}

bool WebAccessibilityObject::canDecrement() const
{
    if (isDetached())
        return false;

    return m_private->isSlider();
}

bool WebAccessibilityObject::canIncrement() const
{
    if (isDetached())
        return false;

    return m_private->isSlider();
}

bool WebAccessibilityObject::canPress() const
{
    if (isDetached())
        return false;

    return m_private->actionElement() || m_private->isButton() || m_private->isMenuRelated();
}

bool WebAccessibilityObject::canSetFocusAttribute() const
{
    if (isDetached())
        return false;

    return m_private->canSetFocusAttribute();
}

bool WebAccessibilityObject::canSetValueAttribute() const
{
    if (isDetached())
        return false;

    return m_private->canSetValueAttribute();
}

unsigned WebAccessibilityObject::childCount() const
{
    if (isDetached())
        return 0;

    return m_private->children().size();
}

WebAccessibilityObject WebAccessibilityObject::childAt(unsigned index) const
{
    if (isDetached())
        return WebAccessibilityObject();

    if (m_private->children().size() <= index)
        return WebAccessibilityObject();

    return WebAccessibilityObject(m_private->children()[index]);
}

WebAccessibilityObject WebAccessibilityObject::firstChild() const
{
    if (isDetached())
        return WebAccessibilityObject();

    return WebAccessibilityObject(m_private->firstChild());
}

WebAccessibilityObject WebAccessibilityObject::focusedChild() const
{
    if (isDetached())
        return WebAccessibilityObject();

    RefPtr<AccessibilityObject> focused = m_private->focusedUIElement();
    if (m_private.get() == focused.get() || m_private.get() == focused->parentObject())
        return WebAccessibilityObject(focused);

    return WebAccessibilityObject();
}

WebAccessibilityObject WebAccessibilityObject::lastChild() const
{
    if (isDetached())
        return WebAccessibilityObject();

    return WebAccessibilityObject(m_private->lastChild());
}


WebAccessibilityObject WebAccessibilityObject::nextSibling() const
{
    if (isDetached())
        return WebAccessibilityObject();

    return WebAccessibilityObject(m_private->nextSibling());
}

WebAccessibilityObject WebAccessibilityObject::parentObject() const
{
    if (isDetached())
        return WebAccessibilityObject();

    return WebAccessibilityObject(m_private->parentObject());
}


WebAccessibilityObject WebAccessibilityObject::previousSibling() const
{
    if (isDetached())
        return WebAccessibilityObject();

    return WebAccessibilityObject(m_private->previousSibling());
}

bool WebAccessibilityObject::canSetSelectedAttribute() const
{
    if (isDetached())
        return 0;

    return m_private->canSetSelectedAttribute();
}

bool WebAccessibilityObject::isAnchor() const
{
    if (isDetached())
        return 0;

    return m_private->isAnchor();
}

bool WebAccessibilityObject::isAriaReadOnly() const
{
    if (isDetached())
        return 0;

    return equalIgnoringCase(m_private->getAttribute(HTMLNames::aria_readonlyAttr), "true");
}

bool WebAccessibilityObject::isButtonStateMixed() const
{
    if (isDetached())
        return 0;

    return m_private->checkboxOrRadioValue() == ButtonStateMixed;
}

bool WebAccessibilityObject::isChecked() const
{
    if (isDetached())
        return 0;

    return m_private->isChecked();
}

bool WebAccessibilityObject::isCollapsed() const
{
    if (isDetached())
        return 0;

    return m_private->isCollapsed();
}

bool WebAccessibilityObject::isControl() const
{
    if (isDetached())
        return 0;

    return m_private->isControl();
}

bool WebAccessibilityObject::isEnabled() const
{
    if (isDetached())
        return 0;

    return m_private->isEnabled();
}

bool WebAccessibilityObject::isFocused() const
{
    if (isDetached())
        return 0;

    return m_private->isFocused();
}

bool WebAccessibilityObject::isHovered() const
{
    if (isDetached())
        return 0;

    return m_private->isHovered();
}

bool WebAccessibilityObject::isIndeterminate() const
{
    if (isDetached())
        return 0;

    return m_private->isIndeterminate();
}

bool WebAccessibilityObject::isLinked() const
{
    if (isDetached())
        return 0;

    return m_private->isLinked();
}

bool WebAccessibilityObject::isLoaded() const
{
    if (isDetached())
        return 0;

    return m_private->isLoaded();
}

bool WebAccessibilityObject::isMultiSelectable() const
{
    if (isDetached())
        return 0;

    return m_private->isMultiSelectable();
}

bool WebAccessibilityObject::isOffScreen() const
{
    if (isDetached())
        return 0;

    return m_private->isOffScreen();
}

bool WebAccessibilityObject::isPasswordField() const
{
    if (isDetached())
        return 0;

    return m_private->isPasswordField();
}

bool WebAccessibilityObject::isPressed() const
{
    if (isDetached())
        return 0;

    return m_private->isPressed();
}

bool WebAccessibilityObject::isReadOnly() const
{
    if (isDetached())
        return 0;

    return m_private->isReadOnly();
}

bool WebAccessibilityObject::isRequired() const
{
    if (isDetached())
        return 0;

    return m_private->isRequired();
}

bool WebAccessibilityObject::isSelected() const
{
    if (isDetached())
        return 0;

    return m_private->isSelected();
}

bool WebAccessibilityObject::isSelectedOptionActive() const
{
    if (isDetached())
        return false;

    return m_private->isSelectedOptionActive();
}

bool WebAccessibilityObject::isVertical() const
{
    if (isDetached())
        return 0;

    return m_private->orientation() == AccessibilityOrientationVertical;
}

bool WebAccessibilityObject::isVisible() const
{
    if (isDetached())
        return 0;

    return m_private->isVisible();
}

bool WebAccessibilityObject::isVisited() const
{
    if (isDetached())
        return 0;

    return m_private->isVisited();
}

WebString WebAccessibilityObject::accessKey() const
{
    if (isDetached())
        return WebString();

    return WebString(m_private->accessKey());
}

bool WebAccessibilityObject::ariaHasPopup() const
{
    if (isDetached())
        return 0;

    return m_private->ariaHasPopup();
}

bool WebAccessibilityObject::ariaLiveRegionAtomic() const
{
    if (isDetached())
        return 0;

    return m_private->ariaLiveRegionAtomic();
}

bool WebAccessibilityObject::ariaLiveRegionBusy() const
{
    if (isDetached())
        return 0;

    return m_private->ariaLiveRegionBusy();
}

WebString WebAccessibilityObject::ariaLiveRegionRelevant() const
{
    if (isDetached())
        return WebString();

    return m_private->ariaLiveRegionRelevant();
}

WebString WebAccessibilityObject::ariaLiveRegionStatus() const
{
    if (isDetached())
        return WebString();

    return m_private->ariaLiveRegionStatus();
}

WebRect WebAccessibilityObject::boundingBoxRect() const
{
    if (isDetached())
        return WebRect();

    return m_private->pixelSnappedBoundingBoxRect();
}

bool WebAccessibilityObject::canvasHasFallbackContent() const
{
    if (isDetached())
        return false;

    return m_private->canvasHasFallbackContent();
}

double WebAccessibilityObject::estimatedLoadingProgress() const
{
    if (isDetached())
        return 0.0;

    return m_private->estimatedLoadingProgress();
}

WebString WebAccessibilityObject::helpText() const
{
    if (isDetached())
        return WebString();

    return m_private->helpText();
}

int WebAccessibilityObject::headingLevel() const
{
    if (isDetached())
        return 0;

    return m_private->headingLevel();
}

int WebAccessibilityObject::hierarchicalLevel() const
{
    if (isDetached())
        return 0;

    return m_private->hierarchicalLevel();
}

WebAccessibilityObject WebAccessibilityObject::hitTest(const WebPoint& point) const
{
    if (isDetached())
        return WebAccessibilityObject();

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
    if (isDetached())
        return WebString();

    String accessKey = m_private->accessKey();
    if (accessKey.isNull())
        return WebString();

    DEFINE_STATIC_LOCAL(String, modifierString, ());
    if (modifierString.isNull()) {
        unsigned modifiers = EventHandler::accessKeyModifiers();
        // Follow the same order as Mozilla MSAA implementation:
        // Ctrl+Alt+Shift+Meta+key. MSDN states that keyboard shortcut strings
        // should not be localized and defines the separator as "+".
        StringBuilder modifierStringBuilder;
        if (modifiers & PlatformEvent::CtrlKey)
            modifierStringBuilder.appendLiteral("Ctrl+");
        if (modifiers & PlatformEvent::AltKey)
            modifierStringBuilder.appendLiteral("Alt+");
        if (modifiers & PlatformEvent::ShiftKey)
            modifierStringBuilder.appendLiteral("Shift+");
        if (modifiers & PlatformEvent::MetaKey)
            modifierStringBuilder.appendLiteral("Win+");
        modifierString = modifierStringBuilder.toString();
    }

    return String(modifierString + accessKey);
}

bool WebAccessibilityObject::performDefaultAction() const
{
    if (isDetached())
        return false;

    return m_private->performDefaultAction();
}

bool WebAccessibilityObject::increment() const
{
    if (isDetached())
        return false;

    if (canIncrement()) {
        m_private->increment();
        return true;
    }
    return false;
}

bool WebAccessibilityObject::decrement() const
{
    if (isDetached())
        return false;

    if (canDecrement()) {
        m_private->decrement();
        return true;
    }
    return false;
}

bool WebAccessibilityObject::press() const
{
    if (isDetached())
        return false;

    return m_private->press();
}

WebAccessibilityRole WebAccessibilityObject::roleValue() const
{
    if (isDetached())
        return WebKit::WebAccessibilityRoleUnknown;

    return static_cast<WebAccessibilityRole>(m_private->roleValue());
}

unsigned WebAccessibilityObject::selectionEnd() const
{
    if (isDetached())
        return 0;

    return m_private->selectedTextRange().start + m_private->selectedTextRange().length;
}

unsigned WebAccessibilityObject::selectionStart() const
{
    if (isDetached())
        return 0;

    return m_private->selectedTextRange().start;
}

unsigned WebAccessibilityObject::selectionEndLineNumber() const
{
    if (isDetached())
        return 0;

    VisiblePosition position = m_private->visiblePositionForIndex(selectionEnd());
    int lineNumber = m_private->lineForPosition(position);
    if (lineNumber < 0)
        return 0;
    return lineNumber;
}

unsigned WebAccessibilityObject::selectionStartLineNumber() const
{
    if (isDetached())
        return 0;

    VisiblePosition position = m_private->visiblePositionForIndex(selectionStart());
    int lineNumber = m_private->lineForPosition(position);
    if (lineNumber < 0)
        return 0;
    return lineNumber;
}

void WebAccessibilityObject::setFocused(bool on) const
{
    if (!isDetached())
        m_private->setFocused(on);
}

void WebAccessibilityObject::setSelectedTextRange(int selectionStart, int selectionEnd) const
{
    if (isDetached())
        return;

    m_private->setSelectedTextRange(PlainTextRange(selectionStart, selectionEnd - selectionStart));
}

WebString WebAccessibilityObject::stringValue() const
{
    if (isDetached())
        return WebString();

    return m_private->stringValue();
}

WebString WebAccessibilityObject::title() const
{
    if (isDetached())
        return WebString();

    return m_private->title();
}

WebAccessibilityObject WebAccessibilityObject::titleUIElement() const
{
    if (isDetached())
        return WebAccessibilityObject();

    return WebAccessibilityObject(m_private->titleUIElement());
}

WebURL WebAccessibilityObject::url() const
{
    if (isDetached())
        return WebURL();
    
    return m_private->url();
}

bool WebAccessibilityObject::supportsRangeValue() const
{
    if (isDetached())
        return false;

    return m_private->supportsRangeValue();
}

WebString WebAccessibilityObject::valueDescription() const
{
    if (isDetached())
        return WebString();

    return m_private->valueDescription();
}

float WebAccessibilityObject::valueForRange() const
{
    if (isDetached())
        return 0.0;

    return m_private->valueForRange();
}

float WebAccessibilityObject::maxValueForRange() const
{
    if (isDetached())
        return 0.0;

    return m_private->maxValueForRange();
}

float WebAccessibilityObject::minValueForRange() const
{
    if (isDetached())
        return 0.0;

    return m_private->minValueForRange();
}

WebNode WebAccessibilityObject::node() const
{
    if (isDetached())
        return WebNode();

    Node* node = m_private->node();
    if (!node)
        return WebNode();

    return WebNode(node);
}

WebDocument WebAccessibilityObject::document() const
{
    if (isDetached())
        return WebDocument();

    Document* document = m_private->document();
    if (!document)
        return WebDocument();

    return WebDocument(document);
}

bool WebAccessibilityObject::hasComputedStyle() const
{
    if (isDetached())
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
    if (isDetached())
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
    if (isDetached())
        return false;

    return m_private->accessibilityIsIgnored();
}

bool WebAccessibilityObject::lineBreaks(WebVector<int>& result) const
{
    if (isDetached())
        return false;

    Vector<int> lineBreaksVector;
    m_private->lineBreaks(lineBreaksVector);

    size_t vectorSize = lineBreaksVector.size();
    WebVector<int> lineBreaksWebVector(vectorSize);
    for (size_t i = 0; i< vectorSize; i++)
        lineBreaksWebVector[i] = lineBreaksVector[i];
    result.swap(lineBreaksWebVector);

    return true;
}

unsigned WebAccessibilityObject::columnCount() const
{
    if (isDetached())
        return false;

    if (!m_private->isAccessibilityTable())
        return 0;

    return static_cast<WebCore::AccessibilityTable*>(m_private.get())->columnCount();
}

unsigned WebAccessibilityObject::rowCount() const
{
    if (isDetached())
        return false;

    if (!m_private->isAccessibilityTable())
        return 0;

    return static_cast<WebCore::AccessibilityTable*>(m_private.get())->rowCount();
}

WebAccessibilityObject WebAccessibilityObject::cellForColumnAndRow(unsigned column, unsigned row) const
{
    if (isDetached())
        return WebAccessibilityObject();

    if (!m_private->isAccessibilityTable())
        return WebAccessibilityObject();

    WebCore::AccessibilityTableCell* cell = static_cast<WebCore::AccessibilityTable*>(m_private.get())->cellForColumnAndRow(column, row);
    return WebAccessibilityObject(static_cast<WebCore::AccessibilityObject*>(cell));
}

unsigned WebAccessibilityObject::cellColumnIndex() const
{
    if (isDetached())
        return 0;

    if (!m_private->isTableCell())
       return 0;

    pair<int, int> columnRange;
    static_cast<WebCore::AccessibilityTableCell*>(m_private.get())->columnIndexRange(columnRange);
    return columnRange.first;
}

unsigned WebAccessibilityObject::cellColumnSpan() const
{
    if (isDetached())
        return 0;

    if (!m_private->isTableCell())
       return 0;

    pair<int, int> columnRange;
    static_cast<WebCore::AccessibilityTableCell*>(m_private.get())->columnIndexRange(columnRange);
    return columnRange.second;
}

unsigned WebAccessibilityObject::cellRowIndex() const
{
    if (isDetached())
        return 0;

    if (!m_private->isTableCell())
       return 0;

    pair<int, int> rowRange;
    static_cast<WebCore::AccessibilityTableCell*>(m_private.get())->rowIndexRange(rowRange);
    return rowRange.first;
}

unsigned WebAccessibilityObject::cellRowSpan() const
{
    if (isDetached())
        return 0;

    if (!m_private->isTableCell())
       return 0;

    pair<int, int> rowRange;
    static_cast<WebCore::AccessibilityTableCell*>(m_private.get())->rowIndexRange(rowRange);
    return rowRange.second;
}

void WebAccessibilityObject::scrollToMakeVisible() const
{
    if (!isDetached())
        m_private->scrollToMakeVisible();
}

void WebAccessibilityObject::scrollToMakeVisibleWithSubFocus(const WebRect& subfocus) const
{
    if (!isDetached())
        m_private->scrollToMakeVisibleWithSubFocus(subfocus);
}

void WebAccessibilityObject::scrollToGlobalPoint(const WebPoint& point) const
{
    if (!isDetached())
        m_private->scrollToGlobalPoint(point);
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
