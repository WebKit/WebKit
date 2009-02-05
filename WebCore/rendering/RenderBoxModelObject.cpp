/*
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
 *
 */

#include "config.h"
#include "RenderBoxModelObject.h"

#include "RenderBlock.h"

namespace WebCore {

RenderBoxModelObject::RenderBoxModelObject(Node* node)
    : RenderObject(node)
{
}

RenderBoxModelObject::~RenderBoxModelObject()
{
}

int RenderBoxModelObject::relativePositionOffsetX() const
{
    if (!style()->left().isAuto()) {
        if (!style()->right().isAuto() && containingBlock()->style()->direction() == RTL)
            return -style()->right().calcValue(containingBlockWidth());
        return style()->left().calcValue(containingBlockWidth());
    }
    if (!style()->right().isAuto())
        return -style()->right().calcValue(containingBlockWidth());
    return 0;
}

int RenderBoxModelObject::relativePositionOffsetY() const
{
    if (!style()->top().isAuto()) {
        if (!style()->top().isPercent() || containingBlock()->style()->height().isFixed())
            return style()->top().calcValue(containingBlockHeight());
    } else if (!style()->bottom().isAuto()) {
        if (!style()->bottom().isPercent() || containingBlock()->style()->height().isFixed())
            return -style()->bottom().calcValue(containingBlockHeight());
    }
    return 0;
}

} // namespace WebCore
