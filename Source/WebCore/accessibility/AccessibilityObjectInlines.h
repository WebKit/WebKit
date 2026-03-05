/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#include "AXObjectCache.h"
#include "AXObjectRareData.h"
#include "AXTextMarker.h"
#include "AXUtilities.h"
#include "AccessibilityObject.h"
#include "Color.h"
#include "Document.h"
#include "Element.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLParserIdioms.h"
#include "LocalFrame.h"
#include "NodeDocument.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "SimpleRange.h"
#include "TextIterator.h"

namespace WebCore {

inline void AccessibilityObject::init()
{
    m_role = determineAccessibilityRole();

    if (needsRareData())
        ensureRareData();
}

inline AXObjectCache* AccessibilityObject::axObjectCache() const
{
    return m_axObjectCache.get();
}

inline bool AccessibilityObject::isDetached() const
{
    return !wrapper();
}

inline bool AccessibilityObject::isNonNativeTextControl() const
{
    return (isARIATextControl() || hasContentEditableAttributeSet()) && !isNativeTextControl();
}

inline bool AccessibilityObject::hasTreeItemRole() const
{
    RefPtr element = this->element();
    return element && hasRole(*element, "treeitem"_s);
}

inline bool AccessibilityObject::hasTreeRole() const
{
    RefPtr element = this->element();
    return element && hasRole(*element, "tree"_s);
}

inline AXTextMarkerRange AccessibilityObject::textMarkerRange() const
{
    return simpleRange();
}

inline LocalFrame* AccessibilityObject::frame() const
{
    Node* node = this->node();
    return node ? node->document().frame() : nullptr;
}

inline bool AccessibilityObject::hasRowGroupTag() const
{
    auto elementName = this->elementName();
    return elementName == ElementName::HTML_thead || elementName == ElementName::HTML_tbody || elementName == ElementName::HTML_tfoot;
}

inline bool AccessibilityObject::hasElementName(ElementName name) const
{
    return elementName() == name;
}

inline SRGBA<uint8_t> AccessibilityObject::colorValue() const
{
    return Color::black;
}

inline bool AccessibilityObject::isInlineText() const
{
    return is<RenderInline>(renderer());
}

inline Element* AccessibilityObject::element() const
{
    return dynamicDowncast<Element>(node());
}

inline CommandType AccessibilityObject::commandType() const
{
    return CommandType::Invalid;
}

inline bool AccessibilityObject::hasDatalist() const
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(element());
    return input && input->hasDataList();
}

inline TextIterator AccessibilityObject::textIteratorIgnoringFullSizeKana(const SimpleRange& range)
{
    return TextIterator(range, { TextIteratorBehavior::IgnoresFullSizeKana });
}

inline bool AccessibilityObject::isIgnoredByDefault() const
{
    return defaultObjectInclusion() == AccessibilityObjectInclusion::IgnoreObject;
}

inline bool AccessibilityObject::isRenderHidden() const
{
    CheckedPtr style = this->style();
    return WebCore::isRenderHidden(style.get());
}

inline ElementName AccessibilityObject::elementName() const
{
    RefPtr element = this->element();
    return element ? element->elementName() : ElementName::Unknown;
}

inline bool AccessibilityObject::isFigureElement() const
{
    return elementName() == ElementName::HTML_figure;
}

inline bool AccessibilityObject::isOutput() const
{
    return elementName() == ElementName::HTML_output;
}

inline bool AccessibilityObject::isHidden() const
{
    return isAXHidden() || isRenderHidden();
}

inline bool AccessibilityObject::isVisible() const
{
    return !isHidden();
}

inline AXObjectRareData& AccessibilityObject::ensureRareData()
{
    if (!hasRareData())
        m_rareDataWithBitfields.setPointer(makeUnique<AXObjectRareData>());
    return *rareData();
}

inline void AccessibilityObject::setLastKnownIsIgnoredValue(bool isIgnored)
{
    m_lastKnownIsIgnoredValue = isIgnored ? AccessibilityObjectInclusion::IgnoreObject : AccessibilityObjectInclusion::IncludeObject;
}

inline bool AccessibilityObject::ignoredFromPresentationalRole() const
{
    return role() == AccessibilityRole::Presentational || inheritsPresentationalRole();
}

inline void AccessibilityObject::scrollToMakeVisible() const
{
    scrollToMakeVisible({ SelectionRevealMode::Reveal, ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded, ShouldAllowCrossOriginScrolling::Yes });
}

inline bool AccessibilityObject::supportsChecked() const
{
    switch (role()) {
    case AccessibilityRole::Checkbox:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::Switch:
        return true;
    default:
        return false;
    }
}

inline bool AccessibilityObject::supportsRowCountChange() const
{
    switch (role()) {
    case AccessibilityRole::Tree:
    case AccessibilityRole::TreeGrid:
    case AccessibilityRole::Grid:
    case AccessibilityRole::Table:
        return true;
    default:
        return false;
    }
}

inline String AccessibilityObject::datetimeAttributeValue() const
{
    return getAttribute(HTMLNames::datetimeAttr);
}

inline String AccessibilityObject::linkRelValue() const
{
    return getAttribute(HTMLNames::relAttr);
}

inline bool AccessibilityObject::supportsKeyShortcuts() const
{
    return hasAttribute(HTMLNames::aria_keyshortcutsAttr);
}

inline String AccessibilityObject::keyShortcuts() const
{
    return getAttribute(HTMLNames::aria_keyshortcutsAttr);
}

inline int AccessibilityObject::integralAttribute(const QualifiedName& attributeName) const
{
    return parseHTMLInteger(getAttribute(attributeName)).value_or(0);
}

inline bool AccessibilityObject::supportsCurrent() const
{
    return hasAttribute(HTMLNames::aria_currentAttr);
}

inline bool AccessibilityObject::ariaIsMultiline() const
{
    return equalLettersIgnoringASCIICase(getAttribute(HTMLNames::aria_multilineAttr), "true"_s);
}

inline const AccessibilityObject::AccessibilityChildrenVector& AccessibilityObject::children(bool updateChildrenIfNeeded)
{
    if (updateChildrenIfNeeded)
        updateChildrenIfNecessary();

    return m_children;
}

inline bool AccessibilityObject::supportsAutoComplete() const
{
    return (isComboBox() || isARIATextControl()) && hasAttribute(HTMLNames::aria_autocompleteAttr);
}

inline bool AccessibilityObject::isARIATextControl() const
{
    auto role = ariaRoleAttribute();
    return role == AccessibilityRole::TextArea || role == AccessibilityRole::TextField || role == AccessibilityRole::SearchField;
}

// https://github.com/w3c/aria/pull/1860
// If accname cannot be derived from content or author, accname can be derived on permitted roles
// from the first descendant element node with a heading role.
inline bool AccessibilityObject::accessibleNameDerivesFromHeading() const
{
    switch (role()) {
    case AccessibilityRole::ApplicationAlertDialog:
    case AccessibilityRole::ApplicationDialog:
    case AccessibilityRole::DocumentArticle:
        return true;
    default:
        return false;
    }
}

inline void AccessibilityObject::initializeAncestorFlags(const OptionSet<AXAncestorFlag>& flags)
{
    m_ancestorFlags.set(AXAncestorFlag::FlagsInitialized, true);
    m_ancestorFlags.add(flags);
}

inline std::optional<AXTreeID> AccessibilityObject::treeID() const
{
    auto* cache = axObjectCache();
    return cache ? std::optional { cache->treeID() } : std::nullopt;
}

inline void AccessibilityObject::recomputeIsIgnored()
{
    // isIgnoredWithoutCache will update m_lastKnownIsIgnoredValue and perform any necessary actions if it has changed.
    isIgnoredWithoutCache(axObjectCache());
}

} // namespace WebCore
