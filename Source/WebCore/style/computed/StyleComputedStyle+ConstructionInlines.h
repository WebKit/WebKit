/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2021 Google Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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
 *
 */

#pragma once

#include "StyleComputedStyle.h"
#include "StyleComputedStyleBase+ConstructionInlines.h"

namespace WebCore {
namespace Style {

inline ComputedStyle::ComputedStyle(ComputedStyle&&) = default;
inline ComputedStyle& ComputedStyle::operator=(ComputedStyle&&) = default;

inline ComputedStyle::ComputedStyle(CreateDefaultStyleTag tag)
    : ComputedStyleProperties { tag }
{
}

inline ComputedStyle::ComputedStyle(const ComputedStyle& other, CloneTag tag)
    : ComputedStyleProperties { other, tag }
{
}

inline ComputedStyle::ComputedStyle(ComputedStyle& a, ComputedStyle&& b)
    : ComputedStyleProperties { a, WTF::move(b) }
{
}

} // namespace Style
} // namespace WebCore
