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

#include "GraphicsContext3D.h"

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

void ClipStack::apply(GraphicsContext3D& context)
{
    if (clipState.scissorBox.isEmpty())
        return;

    context.scissor(clipState.scissorBox.x(),
        (yAxisMode == YAxisMode::Inverted) ? size.height() - clipState.scissorBox.maxY() : clipState.scissorBox.y(),
        clipState.scissorBox.width(), clipState.scissorBox.height());
    context.stencilOp(GraphicsContext3D::KEEP, GraphicsContext3D::KEEP, GraphicsContext3D::KEEP);
    context.stencilFunc(GraphicsContext3D::EQUAL, clipState.stencilIndex - 1, clipState.stencilIndex - 1);
    if (clipState.stencilIndex == 1)
        context.disable(GraphicsContext3D::STENCIL_TEST);
    else
        context.enable(GraphicsContext3D::STENCIL_TEST);
}

void ClipStack::applyIfNeeded(GraphicsContext3D& context)
{
    if (!clipStateDirty)
        return;

    clipStateDirty = false;
    apply(context);
}

} // namespace WebCore
