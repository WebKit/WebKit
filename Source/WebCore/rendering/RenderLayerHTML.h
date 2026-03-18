/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "RenderLayer.h"

namespace WebCore {

class RenderLayerHTML final : public RenderLayer {
    WTF_MAKE_PREFERABLY_COMPACT_TZONE_ALLOCATED_EXPORT(RenderLayerHTML, WEBCORE_EXPORT);
public:
    static UniquelyOwnedPtr<RenderLayer> create(CheckedRef<RenderLayerModelObject> renderer)
    {
        return adoptUniquelyOwned(static_cast<RenderLayer*>(new RenderLayerHTML(renderer.get())));
    }

    WEBCORE_EXPORT ~RenderLayerHTML() final;

private:
    explicit RenderLayerHTML(RenderLayerModelObject&);
};

} // namespace WebCore
