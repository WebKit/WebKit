/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2012 Adobe Systems Incorporated
 * Copyright (C) 2012, 2016 Igalia S.L.
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

#include "config.h"
#include "ClipStack.h"

#include "TextureMapperGLHeaders.h"

namespace WebCore {

void ClipStack::push()
{
    clipStack.append(clipState);
    clipStateDirty = true;
}

void ClipStack::pop()
{
    if (clipStack.isEmpty())
        return;
    clipState = clipStack.last();
    clipStack.removeLast();
    clipStateDirty = true;
}

void ClipStack::reset(const IntRect& rect, ClipStack::YAxisMode mode)
{
    clipStack.clear();
    size = rect.size();
    yAxisMode = mode;
    clipState = State(rect);
    clipStateDirty = true;
}

void ClipStack::intersect(const IntRect& rect)
{
    clipState.scissorBox.intersect(rect);
    clipStateDirty = true;
}

void ClipStack::setStencilIndex(int stencilIndex)
{
    clipState.stencilIndex = stencilIndex;
    clipStateDirty = true;
}

void ClipStack::addRoundedRect(const FloatRoundedRect& roundedRect, const TransformationMatrix& matrix)
{
    if (clipState.roundedRectCount >= s_roundedRectMaxClips)
        return;

    // Ensure that the vectors holding the components have the required size.
    m_roundedRectComponents.grow(s_roundedRectComponentsArraySize);
    m_roundedRectInverseTransformComponents.grow(s_roundedRectInverseTransformComponentsArraySize);

    // Copy the RoundedRect components to the appropriate position in the array.
    int basePosition = clipState.roundedRectCount * s_roundedRectComponentsPerRect;
    m_roundedRectComponents[basePosition] = roundedRect.rect().x();
    m_roundedRectComponents[basePosition + 1] = roundedRect.rect().y();
    m_roundedRectComponents[basePosition + 2] = roundedRect.rect().width();
    m_roundedRectComponents[basePosition + 3] = roundedRect.rect().height();
    m_roundedRectComponents[basePosition + 4] = roundedRect.radii().topLeft().width();
    m_roundedRectComponents[basePosition + 5] = roundedRect.radii().topLeft().height();
    m_roundedRectComponents[basePosition + 6] = roundedRect.radii().topRight().width();
    m_roundedRectComponents[basePosition + 7] = roundedRect.radii().topRight().height();
    m_roundedRectComponents[basePosition + 8] = roundedRect.radii().bottomLeft().width();
    m_roundedRectComponents[basePosition + 9] = roundedRect.radii().bottomLeft().height();
    m_roundedRectComponents[basePosition + 10] = roundedRect.radii().bottomRight().width();
    m_roundedRectComponents[basePosition + 11] = roundedRect.radii().bottomRight().height();

    // Copy the TransformationMatrix components to the appropriate position in the array.
    basePosition = clipState.roundedRectCount * s_roundedRectInverseTransformComponentsPerRect;
    memcpy(m_roundedRectInverseTransformComponents.data() + basePosition, matrix.toColumnMajorFloatArray().data(), s_roundedRectInverseTransformComponentsPerRect * sizeof(float));

    clipState.roundedRectCount++;
}

void ClipStack::apply()
{
    if (clipState.scissorBox.isEmpty())
        return;

    glScissor(clipState.scissorBox.x(),
        (yAxisMode == YAxisMode::Inverted) ? size.height() - clipState.scissorBox.maxY() : clipState.scissorBox.y(),
        clipState.scissorBox.width(), clipState.scissorBox.height());
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilFunc(GL_EQUAL, clipState.stencilIndex - 1, clipState.stencilIndex - 1);
    if (clipState.stencilIndex == 1)
        glDisable(GL_STENCIL_TEST);
    else
        glEnable(GL_STENCIL_TEST);
}

void ClipStack::applyIfNeeded()
{
    if (!clipStateDirty)
        return;

    clipStateDirty = false;
    apply();
}

} // namespace WebCore
