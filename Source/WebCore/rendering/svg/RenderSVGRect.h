/*
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2020, 2021, 2022 Igalia S.L.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "RenderSVGShape.h"

namespace WebCore {

class SVGRectElement;

class RenderSVGRect final : public RenderSVGShape {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGRect);
public:
    RenderSVGRect(SVGRectElement&, RenderStyle&&);
    virtual ~RenderSVGRect();

    SVGRectElement& rectElement() const;

private:
    void graphicsElement() const = delete;

    ASCIILiteral renderName() const override { return "RenderSVGRect"_s; }

    void updateShapeFromElement() override;
    bool isEmpty() const override { return hasPath() ? RenderSVGShape::isEmpty() : m_fillBoundingBox.isEmpty(); }
    bool isRenderingDisabled() const override;
    void fillShape(GraphicsContext&) const override;
    void strokeShape(GraphicsContext&) const override;
    bool shapeDependentStrokeContains(const FloatPoint&, PointCoordinateSpace = GlobalCoordinateSpace) override;
    bool shapeDependentFillContains(const FloatPoint&, const WindRule) const override;

private:
    bool definitelyHasSimpleStroke() const;
    bool canUseStrokeHitTestFastPath() const;
};

} // namespace WebCore

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
