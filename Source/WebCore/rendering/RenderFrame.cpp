/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
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
#include "RenderFrame.h"

#include "HTMLFrameElement.h"

namespace WebCore {

RenderFrame::RenderFrame(HTMLFrameElement& frame, PassRef<RenderStyle> style)
    : RenderFrameBase(frame, std::move(style))
{
}

HTMLFrameElement& RenderFrame::frameElement() const
{
    return toHTMLFrameElement(RenderFrameBase::frameOwnerElement());
}

FrameEdgeInfo RenderFrame::edgeInfo() const
{
    return FrameEdgeInfo(frameElement().noResize(), frameElement().hasFrameBorder());
}

void RenderFrame::updateFromElement()
{
    if (parent() && parent()->isFrameSet())
        toRenderFrameSet(parent())->notifyFrameEdgeInfoChanged();
}

} // namespace WebCore
