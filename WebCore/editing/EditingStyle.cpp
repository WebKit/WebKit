/*
 * Copyright (C) 2007, 2008, 2009 Apple Computer, Inc.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "EditingStyle.h"

#include "ApplyStyleCommand.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSValueKeywords.h"
#include "Frame.h"
#include "RenderStyle.h"
#include "SelectionController.h"

namespace WebCore {

// Editing style properties must be preserved during editing operation.
// e.g. when a user inserts a new paragraph, all properties listed here must be copied to the new paragraph.
// FIXME: The current editingStyleProperties contains all inheritableProperties but we may not need to preserve all inheritable properties
static const int editingStyleProperties[] = {
    // CSS inheritable properties
    CSSPropertyBorderCollapse,
    CSSPropertyColor,
    CSSPropertyFontFamily,
    CSSPropertyFontSize,
    CSSPropertyFontStyle,
    CSSPropertyFontVariant,
    CSSPropertyFontWeight,
    CSSPropertyLetterSpacing,
    CSSPropertyLineHeight,
    CSSPropertyOrphans,
    CSSPropertyTextAlign,
    CSSPropertyTextIndent,
    CSSPropertyTextTransform,
    CSSPropertyWhiteSpace,
    CSSPropertyWidows,
    CSSPropertyWordSpacing,
    CSSPropertyWebkitBorderHorizontalSpacing,
    CSSPropertyWebkitBorderVerticalSpacing,
    CSSPropertyWebkitTextDecorationsInEffect,
    CSSPropertyWebkitTextFillColor,
    CSSPropertyWebkitTextSizeAdjust,
    CSSPropertyWebkitTextStrokeColor,
    CSSPropertyWebkitTextStrokeWidth,
};
size_t numEditingStyleProperties = WTF_ARRAY_LENGTH(editingStyleProperties);

static PassRefPtr<CSSMutableStyleDeclaration> copyEditingProperties(CSSStyleDeclaration* style)
{
    return style->copyPropertiesInSet(editingStyleProperties, numEditingStyleProperties);
}

static PassRefPtr<CSSMutableStyleDeclaration> editingStyleFromComputedStyle(PassRefPtr<CSSComputedStyleDeclaration> style)
{
    if (!style)
        return CSSMutableStyleDeclaration::create();
    return copyEditingProperties(style.get());
}

EditingStyle::EditingStyle()
    : m_shouldUseFixedDefaultFontSize(false)
{
}

EditingStyle::EditingStyle(Node* node)
    : m_shouldUseFixedDefaultFontSize(false)
{
    init(node);
}

EditingStyle::EditingStyle(const Position& position)
    : m_shouldUseFixedDefaultFontSize(false)
{
    init(position.node());
}

EditingStyle::EditingStyle(const CSSStyleDeclaration* style)
    : m_mutableStyle(style->copy())
    , m_shouldUseFixedDefaultFontSize(false)
{
}

void EditingStyle::init(Node* node)
{
    RefPtr<CSSComputedStyleDeclaration> computedStyleAtPosition = computedStyle(node);
    m_mutableStyle = editingStyleFromComputedStyle(computedStyleAtPosition);

    if (node && node->computedStyle()) {
        RenderStyle* renderStyle = node->computedStyle();
        removeTextFillAndStrokeColorsIfNeeded(renderStyle);
        replaceFontSizeByKeywordIfPossible(renderStyle, computedStyleAtPosition.get());
    }

    m_shouldUseFixedDefaultFontSize = computedStyleAtPosition->useFixedFontDefaultSize();
}

void EditingStyle::removeTextFillAndStrokeColorsIfNeeded(RenderStyle* renderStyle)
{
    // If a node's text fill color is invalid, then its children use 
    // their font-color as their text fill color (they don't
    // inherit it).  Likewise for stroke color.
    ExceptionCode ec = 0;
    if (!renderStyle->textFillColor().isValid())
        m_mutableStyle->removeProperty(CSSPropertyWebkitTextFillColor, ec);
    if (!renderStyle->textStrokeColor().isValid())
        m_mutableStyle->removeProperty(CSSPropertyWebkitTextStrokeColor, ec);
    ASSERT(!ec);
}

void EditingStyle::replaceFontSizeByKeywordIfPossible(RenderStyle* renderStyle, CSSComputedStyleDeclaration* computedStyle)
{
    ASSERT(renderStyle);
    if (renderStyle->fontDescription().keywordSize())
        m_mutableStyle->setProperty(CSSPropertyFontSize, computedStyle->getFontSizeCSSValuePreferringKeyword()->cssText());
}

bool EditingStyle::isEmpty() const
{
    return !m_mutableStyle || m_mutableStyle->isEmpty();
}

bool EditingStyle::textDirection(WritingDirection& writingDirection) const
{
    RefPtr<CSSValue> unicodeBidi = m_mutableStyle->getPropertyCSSValue(CSSPropertyUnicodeBidi);
    if (!unicodeBidi)
        return false;

    ASSERT(unicodeBidi->isPrimitiveValue());
    int unicodeBidiValue = static_cast<CSSPrimitiveValue*>(unicodeBidi.get())->getIdent();
    if (unicodeBidiValue == CSSValueEmbed) {
        RefPtr<CSSValue> direction = m_mutableStyle->getPropertyCSSValue(CSSPropertyDirection);
        ASSERT(!direction || direction->isPrimitiveValue());
        if (!direction)
            return false;

        writingDirection = static_cast<CSSPrimitiveValue*>(direction.get())->getIdent() == CSSValueLtr ? LeftToRightWritingDirection : RightToLeftWritingDirection;

        return true;
    }

    if (unicodeBidiValue == CSSValueNormal) {
        writingDirection = NaturalWritingDirection;
        return true;
    }

    return false;
}

void EditingStyle::setStyle(PassRefPtr<CSSMutableStyleDeclaration> style)
{
    m_mutableStyle = style;
    // FIXME: We should be able to figure out whether or not font is fixed width for mutable style.
    // We need to check font-family is monospace as in FontDescription but we don't want to duplicate code here.
    m_shouldUseFixedDefaultFontSize = false;
}

void EditingStyle::clear()
{
    m_mutableStyle.clear();
    m_shouldUseFixedDefaultFontSize = false;
}

void EditingStyle::removeBlockProperties()
{
    if (!m_mutableStyle)
        return;

    m_mutableStyle->removeBlockProperties();
}

void EditingStyle::removeStyleAddedByNode(Node* node)
{
    if (!node || !node->parentNode())
        return;
    RefPtr<CSSMutableStyleDeclaration> parentStyle = editingStyleFromComputedStyle(computedStyle(node->parentNode()));
    RefPtr<CSSMutableStyleDeclaration> nodeStyle = editingStyleFromComputedStyle(computedStyle(node));
    parentStyle->diff(nodeStyle.get());
    nodeStyle->diff(m_mutableStyle.get());
}

void EditingStyle::removeStyleConflictingWithStyleOfNode(Node* node)
{
    if (!node || !node->parentNode() || !m_mutableStyle)
        return;
    RefPtr<CSSMutableStyleDeclaration> parentStyle = editingStyleFromComputedStyle(computedStyle(node->parentNode()));
    RefPtr<CSSMutableStyleDeclaration> nodeStyle = editingStyleFromComputedStyle(computedStyle(node));
    parentStyle->diff(nodeStyle.get());

    CSSMutableStyleDeclaration::const_iterator end = nodeStyle->end();
    for (CSSMutableStyleDeclaration::const_iterator it = nodeStyle->begin(); it != end; ++it)
        m_mutableStyle->removeProperty(it->id());
}

void EditingStyle::removeNonEditingProperties()
{
    if (m_mutableStyle)
        m_mutableStyle = copyEditingProperties(m_mutableStyle.get());
}

void EditingStyle::prepareToApplyAt(const Position& position, ShouldPreserveWritingDirection shouldPreserveWritingDirection)
{
    if (!m_mutableStyle)
        return;

    // ReplaceSelectionCommand::handleStyleSpans() requires that this function only removes the editing style.
    // If this function was modified in the future to delete all redundant properties, then add a boolean value to indicate
    // which one of editingStyleAtPosition or computedStyle is called.
    RefPtr<EditingStyle> style = EditingStyle::create(position);

    RefPtr<CSSValue> unicodeBidi;
    RefPtr<CSSValue> direction;
    if (shouldPreserveWritingDirection == PreserveWritingDirection) {
        unicodeBidi = m_mutableStyle->getPropertyCSSValue(CSSPropertyUnicodeBidi);
        direction = m_mutableStyle->getPropertyCSSValue(CSSPropertyDirection);
    }

    style->m_mutableStyle->diff(m_mutableStyle.get());

    // if alpha value is zero, we don't add the background color.
    RefPtr<CSSValue> backgroundColor = m_mutableStyle->getPropertyCSSValue(CSSPropertyBackgroundColor);
    if (backgroundColor && backgroundColor->isPrimitiveValue()
        && !alphaChannel(static_cast<CSSPrimitiveValue*>(backgroundColor.get())->getRGBA32Value())) {
        ExceptionCode ec;
        m_mutableStyle->removeProperty(CSSPropertyBackgroundColor, ec);
    }

    if (unicodeBidi) {
        ASSERT(unicodeBidi->isPrimitiveValue());
        m_mutableStyle->setProperty(CSSPropertyUnicodeBidi, static_cast<CSSPrimitiveValue*>(unicodeBidi.get())->getIdent());
        if (direction) {
            ASSERT(direction->isPrimitiveValue());
            m_mutableStyle->setProperty(CSSPropertyDirection, static_cast<CSSPrimitiveValue*>(direction.get())->getIdent());
        }
    }
}

PassRefPtr<EditingStyle> editingStyleIncludingTypingStyle(const Position& position)
{
    RefPtr<EditingStyle> editingStyle = EditingStyle::create(position);
    RefPtr<EditingStyle> typingStyle = position.node()->document()->frame()->selection()->typingStyle();
    if (typingStyle && typingStyle->style())
        editingStyle->style()->merge(copyEditingProperties(typingStyle->style()).get());
    return editingStyle;
}
    
}
