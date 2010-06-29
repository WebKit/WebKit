/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 *
 * All rights reserved.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef RenderThemeHaiku_h
#define RenderThemeHaiku_h

#include "RenderTheme.h"

namespace WebCore {

    class RenderThemeHaiku : public RenderTheme {
    private:
        RenderThemeHaiku();
        virtual ~RenderThemeHaiku();

    public:
        static PassRefPtr<RenderTheme> create();

        // A method asking if the theme's controls actually care about redrawing when hovered.
        virtual bool supportsHover(const RenderStyle* style) const { return false; }

        // A method asking if the theme is able to draw the focus ring.
        virtual bool supportsFocusRing(const RenderStyle*) const;

        // The platform selection color.
        virtual Color platformActiveSelectionBackgroundColor() const;
        virtual Color platformInactiveSelectionBackgroundColor() const;
        virtual Color platformActiveSelectionForegroundColor() const;
        virtual Color platformInactiveSelectionForegroundColor() const;

        virtual Color platformTextSearchHighlightColor() const;

        // System fonts.
        virtual void systemFont(int propId, FontDescription&) const;

    protected:
        virtual bool paintCheckbox(RenderObject*, const PaintInfo&, const IntRect&);
        virtual void setCheckboxSize(RenderStyle*) const;

        virtual bool paintRadio(RenderObject*, const PaintInfo&, const IntRect&);
        virtual void setRadioSize(RenderStyle*) const;

        virtual void adjustMenuListStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual bool paintMenuList(RenderObject*, const PaintInfo&, const IntRect&);
    };

} // namespace WebCore

#endif // RenderThemeHaiku_h
