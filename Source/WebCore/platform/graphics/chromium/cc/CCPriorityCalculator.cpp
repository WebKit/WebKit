/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "CCPriorityCalculator.h"

#include "LayerRendererChromium.h"

using namespace std;

namespace WebCore {

// static
int CCPriorityCalculator::uiPriority(bool drawsToRootSurface)
{
    return drawsToRootSurface ? -1 : 2;
}

// static
int CCPriorityCalculator::visiblePriority(bool drawsToRootSurface)
{
    return drawsToRootSurface ? 0 : 3;
}

// static
int CCPriorityCalculator::renderSurfacePriority()
{
    return 1;
}

// static
int CCPriorityCalculator::lingeringPriority(int previousPriority)
{
    // FIXME: We should remove this once we have priorities for all
    //        textures (we can't currently calculate distances for
    //        off-screen textures).
    int lingeringPriority = 1000000;
    return min(numeric_limits<int>::max() - 1,
               max(lingeringPriority, previousPriority)) + 1;
}

namespace {
unsigned manhattanDistance(const IntRect& a, const IntRect& b)
{
    IntRect c = unionRect(a, b);
    int x = max(0, c.width() - a.width() - b.width() + 1);
    int y = max(0, c.height() - a.height() - b.height() + 1);
    return (x + y);
}
}

int CCPriorityCalculator::priorityFromDistance(const IntRect& visibleRect, const IntRect& textureRect, bool drawsToRootSurface) const
{
    unsigned distance = manhattanDistance(visibleRect, textureRect);
    if (!distance)
        return visiblePriority(drawsToRootSurface);
    return visiblePriority(false) + distance;
}

int CCPriorityCalculator::priorityFromDistance(unsigned pixels, bool drawsToRootSurface) const
{
    if (!pixels)
        return visiblePriority(drawsToRootSurface);
    return visiblePriority(false) + pixels;
}

int CCPriorityCalculator::priorityFromVisibility(bool visible, bool drawsToRootSurface) const
{
    return visible ? visiblePriority(drawsToRootSurface) : lowestPriority();
}

} // WebCore

