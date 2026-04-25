/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/Element.h>
#include <WebCore/RenderStyle+GettersInlines.h>
#include <WebCore/StyleContainmentChecker.h>

namespace WebCore {
namespace Style {

inline ContainmentChecker::ContainmentChecker(const RenderStyle& style, const Element& element)
    : m_style { style }
    , m_element { element }
{
}

inline ContainmentChecker::~ContainmentChecker() = default;

inline bool ContainmentChecker::shouldApplyLayoutContainment() const
{
    // https://drafts.csswg.org/css-contain/#containment-layout

    // content-visibility hidden and auto turns on layout containment.
    auto hasContainment = m_style->usedContain().contains(ContainValue::Layout)
        || m_style->contentVisibility() == ContentVisibility::Hidden
        || m_style->contentVisibility() == ContentVisibility::Auto;
    if (!hasContainment)
        return false;

    // Giving an element layout containment has no effect if any of the following are true:
    //   if the element does not generate a principal box (as is the case with display: contents or display: none)
    //   if its principal box is an internal table box other than table-cell
    //   if its principal box is an internal ruby box or a non-atomic inline-level box

    //   if the element does not generate a principal box (as is the case with display: contents or display: none)
    if (!m_style->display().doesGenerateBox())
        return false;
    if (m_style->display().isInternalTableBox() && m_style->display() != DisplayType::TableCell)
        return false;
    if (m_style->display().isRubyContainerOrInternalRubyBox() || (m_style->display() == DisplayType::InlineFlow && !m_element->isReplaced(m_style.ptr())))
        return false;
    return true;
}

inline bool ContainmentChecker::shouldApplySizeContainment() const
{
    // https://drafts.csswg.org/css-contain/#containment-size

    auto hasContainment = m_style->usedContain().contains(ContainValue::Size)
        || m_style->contentVisibility() == ContentVisibility::Hidden
        || (m_style->contentVisibility() == ContentVisibility::Auto && !m_element->isRelevantToUser());
    if (!hasContainment)
        return false;

    // Giving an element size containment has no effect if any of the following are true:
    //   if the element does not generate a principal box (as is the case with display: contents or display: none)
    //   if its inner display type is table
    //   if its principal box is an internal table box
    //   if its principal box is an internal ruby box or a non-atomic inline-level box

    if (!m_style->display().doesGenerateBox())
        return false;
    if (m_style->display().isTableBox())
        return false;
    if (m_style->display().isInternalTableBox())
        return false;
    if (m_style->display().isRubyContainerOrInternalRubyBox() || (m_style->display() == DisplayType::InlineFlow && !m_element->isReplaced(m_style.ptr())))
        return false;
    return true;
}

inline bool ContainmentChecker::shouldApplyInlineSizeContainment() const
{
    // https://drafts.csswg.org/css-contain/#containment-inline-size

    if (!m_style->usedContain().contains(ContainValue::InlineSize))
        return false;

    // Giving an element inline-size containment has no effect if any of the following are true:
    //   if the element does not generate a principal box (as is the case with display: contents or display: none)
    //   if its inner display type is table
    //   if its principal box is an internal table box
    //   if its principal box is an internal ruby box or a non-atomic inline-level box

    if (!m_style->display().doesGenerateBox())
        return false;
    if (m_style->display().isTableBox())
        return false;
    if (m_style->display().isInternalTableBox())
        return false;
    if (m_style->display().isRubyContainerOrInternalRubyBox() || (m_style->display() == DisplayType::InlineFlow && !m_element->isReplaced(m_style.ptr())))
        return false;
    return true;
}

inline bool ContainmentChecker::shouldApplyStyleContainment() const
{
    // https://drafts.csswg.org/css-contain/#containment-style

    // content-visibility hidden and auto turns on style containment.
    return m_style->usedContain().contains(ContainValue::Style)
        || m_style->contentVisibility() == ContentVisibility::Hidden
        || m_style->contentVisibility() == ContentVisibility::Auto;
}

inline bool ContainmentChecker::shouldApplyPaintContainment() const
{
    // https://drafts.csswg.org/css-contain/#containment-paint

    // content-visibility hidden and auto turns on paint containment.
    auto hasContainment = m_style->usedContain().contains(ContainValue::Paint)
        || m_style->contentVisibility() == ContentVisibility::Hidden
        || m_style->contentVisibility() == ContentVisibility::Auto;
    if (!hasContainment)
        return false;

    // Giving an element paint containment has no effect if any of the following are true:
    //   if the element does not generate a principal box (as is the case with display: contents or display: none)
    //   if its principal box is an internal table box other than table-cell
    //   if its principal box is an internal ruby box or a non-atomic inline-level box

    if (!m_style->display().doesGenerateBox())
        return false;
    if (m_style->display().isInternalTableBox() && m_style->display() != DisplayType::TableCell)
        return false;
    if (m_style->display().isRubyContainerOrInternalRubyBox() || (m_style->display() == DisplayType::InlineFlow && !m_element->isReplaced(m_style.ptr())))
        return false;
    return true;
}

inline bool ContainmentChecker::isSkippedContentRoot() const
{
    if (!shouldApplySizeContainment())
        return false;

    switch (m_style->contentVisibility()) {
    case ContentVisibility::Visible:
        return false;
    case ContentVisibility::Hidden:
        return true;
    case ContentVisibility::Auto:
        return !m_element->isRelevantToUser();
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

} // namespace Style
} // namespace WebCore
