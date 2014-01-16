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

#ifndef RenderTableCaption_h
#define RenderTableCaption_h

#include "RenderBlockFlow.h"

namespace WebCore {

class RenderTable;

class RenderTableCaption FINAL : public RenderBlockFlow {
public:
    RenderTableCaption(Element&, PassRef<RenderStyle>);
    virtual ~RenderTableCaption();

    Element& element() const { return toElement(nodeForNonAnonymous()); }

    virtual LayoutUnit containingBlockLogicalWidthForContent() const override;
    
private:
    virtual bool isTableCaption() const override { return true; }

    virtual void insertedIntoTree() override;
    virtual void willBeRemovedFromTree() override;

    RenderTable* table() const;
};

RENDER_OBJECT_TYPE_CASTS(RenderTableCaption, isTableCaption())

} // namespace WebCore

#endif // RenderTableCaption_h
