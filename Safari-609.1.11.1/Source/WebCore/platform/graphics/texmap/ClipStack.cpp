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
