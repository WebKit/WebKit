/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGTextPath.h"

#include "AffineTransform.h"
#include "FloatRect.h"
#include "InlineFlowBox.h"

namespace WebCore {

RenderSVGTextPath::RenderSVGTextPath(Node* n)
    : RenderSVGInline(n)
    , m_startOffset(0.0)
    , m_exactAlignment(true)
    , m_stretchMethod(false)
{
}

Path RenderSVGTextPath::layoutPath() const
{
    return m_layoutPath;
}

void RenderSVGTextPath::setLayoutPath(const Path& path)
{
    m_layoutPath = path;
}

float RenderSVGTextPath::startOffset() const
{
    return m_startOffset;
}

void RenderSVGTextPath::setStartOffset(float offset)
{
    m_startOffset = offset;
}

bool RenderSVGTextPath::exactAlignment() const
{
    return m_exactAlignment;
}

void RenderSVGTextPath::setExactAlignment(bool value)
{
    m_exactAlignment = value;
}

bool RenderSVGTextPath::stretchMethod() const
{
    return m_stretchMethod;
}

void RenderSVGTextPath::setStretchMethod(bool value)
{
    m_stretchMethod = value;
}

void RenderSVGTextPath::absoluteRects(Vector<IntRect>& rects, int tx, int ty)
{
    for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
        FloatRect rect(tx + curr->xPos(), ty + curr->yPos(), curr->width(), curr->height());
        rects.append(enclosingIntRect(absoluteTransform().mapRect(rect)));
    }
}

}

#endif // ENABLE(SVG)
