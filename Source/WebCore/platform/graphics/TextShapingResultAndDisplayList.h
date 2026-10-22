/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/DisplayList.h>
#include <WebCore/FloatSize.h>
#include <WebCore/FontCascade.h>
#include <wtf/RefPtr.h>

namespace WebCore {

struct TextShapingResultAndDisplayList {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(TextShapingResultAndDisplayList);
public:
    struct PaintDisplayListParameters {
        FloatSize scaleFactor;
        FontSmoothingMode fontSmoothing { FontSmoothingMode::Auto };
        bool shouldSubpixelQuantizeFonts { false };
        FontCascade::CustomFontNotReadyAction customFontNotReadyAction { FontCascade::CustomFontNotReadyAction::DoNotPaintIfFontNotReady };

        friend bool operator==(const PaintDisplayListParameters&, const PaintDisplayListParameters&) = default;
    };

    TextShapingResult textShapingResult;
    RefPtr<const DisplayList::DisplayList> displayList;

    RefPtr<const DisplayList::DisplayList> paintDisplayList;
    PaintDisplayListParameters paintDisplayListParameters;
};

} // namespace WebCore
