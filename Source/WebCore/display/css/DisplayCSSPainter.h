/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "FloatSize.h"
#include <wtf/Vector.h>

namespace WebCore {

class FillLayer;
class GraphicsContext;
class IntRect;

namespace Display {

class FillLayerImageGeometry;
class Box;
class BoxModelBox;
class ContainerBox;
class StackingItem;
class Tree;

struct PaintingContext;

class CSSPainter {
public:
    static void paintTree(const Tree&, PaintingContext&, const IntRect& dirtyRect);

private:
    enum class PaintPhase {
        BlockBackgrounds,
        Floats,
        BlockForegrounds
    };
    static void recursivePaintDescendants(const ContainerBox&, PaintingContext&);
    static void recursivePaintDescendantsForPhase(const ContainerBox&, PaintingContext&, PaintPhase);

    enum class IncludeStackingContextDescendants { Yes, No };
    static void paintAtomicallyPaintedBox(const StackingItem&, PaintingContext&, const IntRect& dirtyRect, IncludeStackingContextDescendants = IncludeStackingContextDescendants::No);

    static void paintStackingContext(const StackingItem&, PaintingContext&, const IntRect& dirtyRect);
};

} // namespace Display
} // namespace WebCore

