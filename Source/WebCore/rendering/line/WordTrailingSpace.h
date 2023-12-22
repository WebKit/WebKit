/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003-2021 Apple Inc. All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 ChangSeok Oh <shivamidow@gmail.com>
 * Copyright (C) 2013 Adobe Systems Inc. All right reserved.
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

#include "FontCascade.h"
#include "RenderBlock.h"
#include "RenderStyle.h"
#include <optional>
#include <wtf/HashSet.h>

namespace WebCore {

struct WordTrailingSpace {
    WordTrailingSpace(const RenderStyle& style, bool measuringWithTrailingWhitespaceEnabled = true)
        : m_style(style)
    {
        if (!measuringWithTrailingWhitespaceEnabled || !m_style.fontCascade().enableKerning())
            m_state = WordTrailingSpaceState::Initialized;
    }

    std::optional<float> width(SingleThreadWeakHashSet<const Font>& fallbackFonts)
    {
        if (m_state == WordTrailingSpaceState::Initialized)
            return m_width;

        auto& font = m_style.fontCascade();
        m_width = font.width(RenderBlock::constructTextRun(&space, 1, m_style), &fallbackFonts) + font.wordSpacing();
        m_state = WordTrailingSpaceState::Initialized;
        return m_width;
    }

private:
    enum class WordTrailingSpaceState { Uninitialized, Initialized };
    const RenderStyle& m_style;
    WordTrailingSpaceState m_state { WordTrailingSpaceState::Uninitialized };
    std::optional<float> m_width;
};

} // namespace WebCore
