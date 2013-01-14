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
#include "platform/WebCommon.h"
#include "platform/WebPrivatePtr.h"
#include "platform/WebVector.h"

#if WEBKIT_IMPLEMENTATION
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace WebCore { class AccessibilityObject; }

namespace WebKit {

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

    WebAccessibilityObject() { }
    WebAccessibilityObject(const WebAccessibilityObject& o) { assign(o); }
    WebAccessibilityObject& operator=(const WebAccessibilityObject& o)
    {
        assign(o);
        return *this;
    }

    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT void assign(const WebAccessibilityObject&);
    WEBKIT_EXPORT bool equals(const WebAccessibilityObject&) const;

    bool isNull() const { return m_private.isNull(); }
    // isDetached also checks for null, so it's safe to just call isDetached.
    WEBKIT_EXPORT bool isDetached() const;

    // Static methods for enabling accessibility.
    WEBKIT_EXPORT static void enableAccessibility();
    WEBKIT_EXPORT static bool accessibilityEnabled();

    WEBKIT_EXPORT int axID() const;

    // Update the underlying tree, and return true if this object is
    // still valid (not detached). Note that calling this method
    // can cause other WebAccessibilityObjects to become invalid, too,
    // so always call isDetached if updateBackingStoreAndCheckValidity
    // has been called on any object, or if any other WebCore code has run.
    WEBKIT_EXPORT bool updateBackingStoreAndCheckValidity();

    WEBKIT_EXPORT WebString accessibilityDescription() const;
    WEBKIT_EXPORT unsigned childCount() const;

    WEBKIT_EXPORT WebAccessibilityObject childAt(unsigned) const;
    WEBKIT_EXPORT WebAccessibilityObject firstChild() const;
    WEBKIT_EXPORT WebAccessibilityObject focusedChild() const;
    WEBKIT_EXPORT WebAccessibilityObject lastChild() const;
    WEBKIT_EXPORT WebAccessibilityObject nextSibling() const;
    WEBKIT_EXPORT WebAccessibilityObject parentObject() const;
    WEBKIT_EXPORT WebAccessibilityObject previousSibling() const;

    WEBKIT_EXPORT bool isAnchor() const;
    WEBKIT_EXPORT bool isAriaReadOnly() const;
    WEBKIT_EXPORT bool isButtonStateMixed() const;
    WEBKIT_EXPORT bool isChecked() const;
    WEBKIT_EXPORT bool isCollapsed() const;
    WEBKIT_EXPORT bool isControl() const;
    WEBKIT_EXPORT bool isEnabled() const;
    WEBKIT_EXPORT bool isFocused() const;
    WEBKIT_EXPORT bool isHovered() const;
    WEBKIT_EXPORT bool isIndeterminate() const;
    WEBKIT_EXPORT bool isLinked() const;
    WEBKIT_EXPORT bool isLoaded() const;
    WEBKIT_EXPORT bool isMultiSelectable() const;
    WEBKIT_EXPORT bool isOffScreen() const;
    WEBKIT_EXPORT bool isPasswordField() const;
    WEBKIT_EXPORT bool isPressed() const;
    WEBKIT_EXPORT bool isReadOnly() const;
    WEBKIT_EXPORT bool isRequired() const;
    WEBKIT_EXPORT bool isSelected() const;
    WEBKIT_EXPORT bool isSelectedOptionActive() const;
    WEBKIT_EXPORT bool isVertical() const;
    WEBKIT_EXPORT bool isVisible() const;
    WEBKIT_EXPORT bool isVisited() const;

    WEBKIT_EXPORT WebString accessKey() const;
    WEBKIT_EXPORT bool ariaHasPopup() const;
    WEBKIT_EXPORT bool ariaLiveRegionAtomic() const;
    WEBKIT_EXPORT bool ariaLiveRegionBusy() const;
    WEBKIT_EXPORT WebString ariaLiveRegionRelevant() const;
    WEBKIT_EXPORT WebString ariaLiveRegionStatus() const;
    WEBKIT_EXPORT WebRect boundingBoxRect() const;
    WEBKIT_EXPORT bool canvasHasFallbackContent() const;
    WEBKIT_EXPORT WebPoint clickPoint() const;
    WEBKIT_EXPORT void colorValue(int& r, int& g, int& b) const;
    WEBKIT_EXPORT double estimatedLoadingProgress() const;
    WEBKIT_EXPORT WebString helpText() const;
    WEBKIT_EXPORT int headingLevel() const;
    WEBKIT_EXPORT int hierarchicalLevel() const;
    WEBKIT_EXPORT WebAccessibilityObject hitTest(const WebPoint&) const;
    WEBKIT_EXPORT WebString keyboardShortcut() const;
    WEBKIT_EXPORT WebAccessibilityRole roleValue() const;
    WEBKIT_EXPORT unsigned selectionEnd() const;
    WEBKIT_EXPORT unsigned selectionEndLineNumber() const;
    WEBKIT_EXPORT unsigned selectionStart() const;
    WEBKIT_EXPORT unsigned selectionStartLineNumber() const;
    WEBKIT_EXPORT WebString stringValue() const;
    WEBKIT_EXPORT WebString title() const;
    WEBKIT_EXPORT WebAccessibilityObject titleUIElement() const;
    WEBKIT_EXPORT WebURL url() const;

    WEBKIT_EXPORT bool supportsRangeValue() const;
    WEBKIT_EXPORT WebString valueDescription() const;
    WEBKIT_EXPORT float valueForRange() const;
    WEBKIT_EXPORT float maxValueForRange() const;
    WEBKIT_EXPORT float minValueForRange() const;

    WEBKIT_EXPORT WebNode node() const;
    WEBKIT_EXPORT WebDocument document() const;
    WEBKIT_EXPORT bool hasComputedStyle() const;
    WEBKIT_EXPORT WebString computedStyleDisplay() const;
    WEBKIT_EXPORT bool accessibilityIsIgnored() const;
    WEBKIT_EXPORT bool lineBreaks(WebVector<int>&) const;

    // Actions
    WEBKIT_EXPORT WebString actionVerb() const; // The verb corresponding to performDefaultAction.
    WEBKIT_EXPORT bool canDecrement() const;
    WEBKIT_EXPORT bool canIncrement() const;
    WEBKIT_EXPORT bool canPress() const;
    WEBKIT_EXPORT bool canSetFocusAttribute() const;
    WEBKIT_EXPORT bool canSetSelectedAttribute() const;
    WEBKIT_EXPORT bool canSetValueAttribute() const;
    WEBKIT_EXPORT bool performDefaultAction() const;
    WEBKIT_EXPORT bool press() const;
    WEBKIT_EXPORT bool increment() const;
    WEBKIT_EXPORT bool decrement() const;
    WEBKIT_EXPORT void setFocused(bool) const;
    WEBKIT_EXPORT void setSelectedTextRange(int selectionStart, int selectionEnd) const;

    // For a table
    WEBKIT_EXPORT unsigned columnCount() const;
    WEBKIT_EXPORT unsigned rowCount() const;
    WEBKIT_EXPORT WebAccessibilityObject cellForColumnAndRow(unsigned column, unsigned row) const;

    // For a table cell
    WEBKIT_EXPORT unsigned cellColumnIndex() const;
    WEBKIT_EXPORT unsigned cellColumnSpan() const;
    WEBKIT_EXPORT unsigned cellRowIndex() const;
    WEBKIT_EXPORT unsigned cellRowSpan() const;

    // Make this object visible by scrolling as many nested scrollable views as needed.
    WEBKIT_EXPORT void scrollToMakeVisible() const;
    // Same, but if the whole object can't be made visible, try for this subrect, in local coordinates.
    WEBKIT_EXPORT void scrollToMakeVisibleWithSubFocus(const WebRect&) const;
    // Scroll this object to a given point in global coordinates of the top-level window.
    WEBKIT_EXPORT void scrollToGlobalPoint(const WebPoint&) const;

#if WEBKIT_IMPLEMENTATION
    WebAccessibilityObject(const WTF::PassRefPtr<WebCore::AccessibilityObject>&);
    WebAccessibilityObject& operator=(const WTF::PassRefPtr<WebCore::AccessibilityObject>&);
    operator WTF::PassRefPtr<WebCore::AccessibilityObject>() const;
#endif

private:
    WebPrivatePtr<WebCore::AccessibilityObject> m_private;
};

} // namespace WebKit

#endif
