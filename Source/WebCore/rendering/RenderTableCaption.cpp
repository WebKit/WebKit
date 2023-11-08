/*
 * Copyright (C) 2011 Robert Hogan <robert@roberthogan.net>
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
#include "RenderTableCaption.h"

#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderTable.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderTableCaption);

RenderTableCaption::RenderTableCaption(Element& element, RenderStyle&& style)
    : RenderBlockFlow(Type::TableCaption, element, WTFMove(style))
{
    ASSERT(isRenderTableCaption());
}

RenderTableCaption::~RenderTableCaption() = default;

void RenderTableCaption::insertedIntoTree(IsInternalMove isInternalMove)
{
    RenderBlockFlow::insertedIntoTree(isInternalMove);
    table()->addCaption(*this);
}

void RenderTableCaption::willBeRemovedFromTree(IsInternalMove isInternalMove)
{
    RenderBlockFlow::willBeRemovedFromTree(isInternalMove);
    table()->removeCaption(*this);
}

RenderTable* RenderTableCaption::table() const
{
    return downcast<RenderTable>(parent());
}

LayoutUnit RenderTableCaption::containingBlockLogicalWidthForContent() const
{
    if (auto* containingBlock = this->containingBlock())
        return containingBlock->logicalWidth();
    return { };
}

}
