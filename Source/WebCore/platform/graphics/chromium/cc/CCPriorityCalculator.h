/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#ifndef CCPriorityCalculator_h
#define CCPriorityCalculator_h

#include "GraphicsContext3D.h"
#include "IntRect.h"
#include "IntSize.h"

namespace WebCore {

class CCPriorityCalculator {
public:
    static int uiPriority(bool drawsToRootSurface);
    static int visiblePriority(bool drawsToRootSurface);
    static int renderSurfacePriority();
    static int lingeringPriority(int previousPriority);
    int priorityFromDistance(const IntRect& visibleRect, const IntRect& textureRect, bool drawsToRootSurface) const;
    int priorityFromDistance(unsigned pixels, bool drawsToRootSurface) const;
    int priorityFromVisibility(bool visible, bool drawsToRootSurface) const;

    static inline int highestPriority() { return std::numeric_limits<int>::min(); }
    static inline int lowestPriority() { return std::numeric_limits<int>::max(); }
    static inline bool priorityIsLower(int a, int b) { return a > b; }
    static inline bool priorityIsHigher(int a, int b) { return a < b; }
    static inline bool maxPriority(int a, int b) { return priorityIsHigher(a, b) ? a : b; }
};

}

#endif
