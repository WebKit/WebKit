/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include <WebCore/LayoutShape.h>
#include <WebCore/LayoutSize.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>

namespace WebCore {

class RenderBox;

Ref<const LayoutShape> makeShapeForShapeOutside(const RenderBox&);

class ShapeOutsideInfo final {
    WTF_MAKE_TZONE_ALLOCATED(ShapeOutsideInfo);
public:
    explicit ShapeOutsideInfo(const RenderBox&);
    ~ShapeOutsideInfo();

    static bool isEnabledFor(const RenderBox&);

    void invalidateForSizeChangeIfNeeded();

    void markShapeAsDirty() { m_shape = nullptr; }

    LayoutRect computedShapePhysicalBoundingBox() const;
    NODELETE FloatPoint shapeToRendererPoint(const FloatPoint&) const;

    const LayoutShape& computedShape() const;

private:
    LayoutUnit logicalTopOffset() const;
    LayoutUnit logicalLeftOffset() const;

    // The info is kept in a weak map and can outlive the renderer.
    const SingleThreadWeakRef<const RenderBox> m_renderer;

    mutable RefPtr<const LayoutShape> m_shape;
    LayoutSize m_cachedShapeLogicalSize;
};

}
