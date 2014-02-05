/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef RenderSVGTextPath_h
#define RenderSVGTextPath_h

#include "RenderSVGInline.h"

namespace WebCore {

class RenderSVGTextPath final : public RenderSVGInline {
public:
    RenderSVGTextPath(SVGTextPathElement&, PassRef<RenderStyle>);

    SVGTextPathElement& textPathElement() const;

    Path layoutPath() const;
    float startOffset() const;
    bool exactAlignment() const;
    bool stretchMethod() const;

private:
    void graphicsElement() const = delete;

    virtual bool isSVGTextPath() const override { return true; }
    virtual const char* renderName() const override { return "RenderSVGTextPath"; }

    Path m_layoutPath;
};

RENDER_OBJECT_TYPE_CASTS(RenderSVGTextPath, isSVGTextPath())

}

#endif // RenderSVGTextPath_h
