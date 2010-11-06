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
#include "Frame.h"
#include "RenderStyle.h"
#include "SelectionController.h"

namespace WebCore {

static PassRefPtr<CSSMutableStyleDeclaration> editingStyleFromComputedStyle(CSSComputedStyleDeclaration* style)
{
    if (!style)
        return CSSMutableStyleDeclaration::create();
    return ApplyStyleCommand::removeNonEditingProperties(style);
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
    m_mutableStyle = editingStyleFromComputedStyle(computedStyleAtPosition.get());

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

void EditingStyle::prepareToApplyAt(const Position& position)
{
    // ReplaceSelectionCommand::handleStyleSpans() requires that this function only removes the editing style.
    // If this function was modified in the future to delete all redundant properties, then add a boolean value to indicate
    // which one of editingStyleAtPosition or computedStyle is called.
    RefPtr<EditingStyle> style = EditingStyle::create(position);
    style->m_mutableStyle->diff(m_mutableStyle.get());

    // if alpha value is zero, we don't add the background color.
    RefPtr<CSSValue> backgroundColor = m_mutableStyle->getPropertyCSSValue(CSSPropertyBackgroundColor);
    if (backgroundColor && backgroundColor->isPrimitiveValue()
        && !alphaChannel(static_cast<CSSPrimitiveValue*>(backgroundColor.get())->getRGBA32Value())) {
        ExceptionCode ec;
        m_mutableStyle->removeProperty(CSSPropertyBackgroundColor, ec);
    }
}

PassRefPtr<EditingStyle> editingStyleIncludingTypingStyle(const Position& position)
{
    RefPtr<EditingStyle> editingStyle = EditingStyle::create(position);
    RefPtr<CSSMutableStyleDeclaration> typingStyle = position.node()->document()->frame()->selection()->typingStyle();
    if (typingStyle) {
        RefPtr<CSSMutableStyleDeclaration> inheritableTypingStyle = typingStyle->copy();
        ApplyStyleCommand::removeNonEditingProperties(inheritableTypingStyle.get());
        editingStyle->style()->merge(inheritableTypingStyle.get(), true);
    }
    return editingStyle;
}
    
}
