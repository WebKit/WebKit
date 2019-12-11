/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#include "DetailsMarkerControl.h"
#include "RenderBlockFlow.h"

namespace WebCore {

class RenderDetailsMarker final : public RenderBlockFlow {
    WTF_MAKE_ISO_ALLOCATED(RenderDetailsMarker);
public:
    RenderDetailsMarker(DetailsMarkerControl&, RenderStyle&&);
    DetailsMarkerControl& element() const { return static_cast<DetailsMarkerControl&>(nodeForNonAnonymous()); }

    enum Orientation { Up, Down, Left, Right };
    Orientation orientation() const;

private:
    const char* renderName() const override { return "RenderDetailsMarker"; }
    bool isDetailsMarker() const override { return true; }
    void paint(PaintInfo&, const LayoutPoint&) override;

    bool isOpen() const;
    Path getCanonicalPath() const;
    Path getPath(const LayoutPoint& origin) const;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderDetailsMarker, isDetailsMarker())
