/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (c) 2023 Igalia S.L.
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

#pragma once

#if ENABLE(LAYER_BASED_SVG_ENGINE)

#include "RenderSVGHiddenContainer.h"

namespace WebCore {

class RenderSVGResourceContainer : public RenderSVGHiddenContainer {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGResourceContainer);
public:
    virtual ~RenderSVGResourceContainer();

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    void idChanged();
    void repaintAllClients() const;

protected:
    RenderSVGResourceContainer(Type, SVGElement&, RenderStyle&&);

private:
    void willBeDestroyed() final;
    void registerResource();

    AtomString m_id;
    bool m_registered { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGResourceContainer, isRenderSVGResourceContainer())

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
