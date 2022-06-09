/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
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

#pragma once

#include "HTMLFrameElement.h"
#include "RenderFrameBase.h"

namespace WebCore {

struct FrameEdgeInfo;

class RenderFrame final : public RenderFrameBase {
    WTF_MAKE_ISO_ALLOCATED(RenderFrame);
public:
    RenderFrame(HTMLFrameElement&, RenderStyle&&);

    HTMLFrameElement& frameElement() const;
    FrameEdgeInfo edgeInfo() const;

    void updateFromElement() final;

private:
    void frameOwnerElement() const = delete;

    const char* renderName() const final { return "RenderFrame"; }
    bool isFrame() const final { return true; }
};

inline RenderFrame* HTMLFrameElement::renderer() const
{
    return downcast<RenderFrame>(HTMLFrameElementBase::renderer());
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderFrame, isFrame())
