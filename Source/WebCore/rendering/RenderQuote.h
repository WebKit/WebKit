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

#ifndef RenderQuote_h
#define RenderQuote_h

#include "RenderInline.h"

namespace WebCore {

class RenderQuote final : public RenderInline {
public:
    RenderQuote(Document&, RenderStyle&&, QuoteType);
    virtual ~RenderQuote();

    void attachQuote();

private:
    void detachQuote();

    const char* renderName() const override { return "RenderQuote"; }
    bool isQuote() const override { return true; }
    void styleDidChange(StyleDifference, const RenderStyle*) override;
    void insertedIntoTree() override;
    void willBeRemovedFromTree() override;

    String computeText() const;
    void updateText();
    void updateDepth();

    const QuoteType m_type;
    int m_depth { -1 };
    RenderQuote* m_next { nullptr };
    RenderQuote* m_previous { nullptr };
    bool m_isAttached { false };
    String m_text;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderQuote, isQuote())

#endif // RenderQuote_h
