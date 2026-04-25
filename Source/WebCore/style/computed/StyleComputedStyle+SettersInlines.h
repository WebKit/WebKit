/**
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2014-2021 Google Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

#define COMPUTED_STYLE_PROPERTIES_SETTERS_INLINES_INCLUDE_TRAP 1
#include "StyleComputedStyleProperties+SettersInlines.h"
#undef COMPUTED_STYLE_PROPERTIES_SETTERS_INLINES_INCLUDE_TRAP

namespace WebCore {
namespace Style {

// MARK: - Non-property setters

inline void ComputedStyle::inheritColumnPropertiesFrom(const ComputedStyle& parent)
{
    m_nonInheritedData.access().miscData.access().multiCol = parent.m_nonInheritedData->miscData->multiCol;
}

// MARK: - Style adjustment utilities

inline void ComputedStyle::resetBorderBottom()
{
    setBorderBottom(BorderValue { });
}

inline void ComputedStyle::resetBorderLeft()
{
    setBorderLeft(BorderValue { });
}

inline void ComputedStyle::resetBorderRight()
{
    setBorderRight(BorderValue { });
}

inline void ComputedStyle::resetBorderTop()
{
    setBorderTop(BorderValue { });
}

inline void ComputedStyle::resetMargin()
{
    setMarginBox(MarginBox { 0_css_px });
}

inline void ComputedStyle::resetPadding()
{
    setPaddingBox(PaddingBox { 0_css_px });
}

inline void ComputedStyle::resetBorder()
{
    resetBorderExceptRadius();
    resetBorderRadius();
}

inline void ComputedStyle::resetBorderExceptRadius()
{
    setBorderImage(BorderImage { });
    resetBorderTop();
    resetBorderRight();
    resetBorderBottom();
    resetBorderLeft();
}

inline void ComputedStyle::resetBorderRadius()
{
    setBorderTopLeftRadius(initialBorderTopLeftRadius());
    setBorderTopRightRadius(initialBorderTopRightRadius());
    setBorderBottomLeftRadius(initialBorderBottomLeftRadius());
    setBorderBottomRightRadius(initialBorderBottomRightRadius());
}

} // namespace Style
} // namespace WebCore
