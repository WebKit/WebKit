/**
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#include <WebCore/StyleComputedStyle.h>

#define COMPUTED_STYLE_PROPERTIES_INITIAL_INLINES_INCLUDE_TRAP 1
#include <WebCore/StyleComputedStyleProperties+InitialInlines.h>
#undef COMPUTED_STYLE_PROPERTIES_INITIAL_INLINES_INCLUDE_TRAP

namespace WebCore {
namespace Style {

// MARK: - Non-property initial values

constexpr ZIndex ComputedStyle::initialUsedZIndex()
{
    return CSS::Keyword::Auto { };
}

inline PageSize ComputedStyle::initialPageSize()
{
    return CSS::Keyword::Auto { };
}

#if ENABLE(TEXT_AUTOSIZING)

inline LineHeight ComputedStyle::initialSpecifiedLineHeight()
{
    return CSS::Keyword::Normal { };
}

#endif

} // namespace Style
} // namespace WebCore
