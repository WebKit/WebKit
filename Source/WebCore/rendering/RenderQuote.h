/*
 * Copyright (C) 2011 Nokia Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "RenderInline.h"

namespace WebCore {

class RenderQuote final : public RenderInline {
    WTF_MAKE_ISO_ALLOCATED(RenderQuote);
public:
    RenderQuote(Document&, RenderStyle&&, QuoteType);
    virtual ~RenderQuote();

    void updateRenderer(RenderTreeBuilder&, RenderQuote* previousQuote);

private:
    ASCIILiteral renderName() const override { return "RenderQuote"_s; }
    bool isOpen() const;
    void styleDidChange(StyleDifference, const RenderStyle*) override;
    void insertedIntoTree(IsInternalMove) override;
    void willBeRemovedFromTree(IsInternalMove) override;

    String computeText() const;
    void updateTextRenderer(RenderTreeBuilder&);

    const QuoteType m_type;
    int m_depth { -1 };
    String m_text;

    bool m_needsTextUpdate { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderQuote, isRenderQuote())
