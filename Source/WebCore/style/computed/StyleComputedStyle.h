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

#include <WebCore/StyleComputedStyleProperties.h>

namespace WebCore {
namespace Style {

class ComputedStyle final : public ComputedStyleProperties {
public:
    void inheritFrom(const ComputedStyle&);
    void inheritIgnoringCustomPropertiesFrom(const ComputedStyle&);
    void NODELETE inheritUnicodeBidiFrom(const ComputedStyle&);
    inline void inheritColumnPropertiesFrom(const ComputedStyle&);
    void fastPathInheritFrom(const ComputedStyle&);
    void copyNonInheritedFrom(const ComputedStyle&);
    void copyContentFrom(const ComputedStyle&);
    void copyPseudoElementsFrom(const ComputedStyle&);
    void NODELETE copyPseudoElementBitsFrom(const ComputedStyle&);

    // MARK: - Comparisons

    bool operator==(const ComputedStyle&) const;

    bool inheritedEqual(const ComputedStyle&) const;
    bool nonInheritedEqual(const ComputedStyle&) const;
    bool fastPathInheritedEqual(const ComputedStyle&) const;
    bool nonFastPathInheritedEqual(const ComputedStyle&) const;
    bool descendantAffectingNonInheritedPropertiesEqual(const ComputedStyle&) const;
    bool borderAndBackgroundEqual(const ComputedStyle&) const;
    inline bool containerTypeAndNamesEqual(const ComputedStyle&) const;
    inline bool columnSpanEqual(const ComputedStyle&) const;
    inline bool scrollPaddingEqual(const ComputedStyle&) const;
    inline bool fontCascadeEqual(const ComputedStyle&) const;
    bool NODELETE scrollSnapDataEquivalent(const ComputedStyle&) const;

    // MARK: - Style reset utilities

    inline void resetBorder();
    inline void resetBorderExceptRadius();
    inline void resetBorderTop();
    inline void resetBorderRight();
    inline void resetBorderBottom();
    inline void resetBorderLeft();
    inline void resetBorderRadius();
    inline void resetMargin();
    inline void resetPadding();

    // MARK: - Derived Values

    WEBCORE_EXPORT float computedLineHeight() const;
    float computeLineHeight(const LineHeight&) const;

    // MARK: - Non-property initial values.

    static inline PageSize initialPageSize();
    static constexpr ZIndex initialUsedZIndex();
#if ENABLE(TEXT_AUTOSIZING)
    static inline LineHeight initialSpecifiedLineHeight();
#endif

private:
    friend class WebCore::RenderStyleProperties;

    ComputedStyle(ComputedStyle&&);
    ComputedStyle& operator=(ComputedStyle&&);
    ComputedStyle(CreateDefaultStyleTag);
    ComputedStyle(const ComputedStyle&, CloneTag);
    ComputedStyle(ComputedStyle&, ComputedStyle&&);
};

} // namespace Style
} // namespace WebCore
