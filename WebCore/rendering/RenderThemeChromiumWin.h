/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2008, 2009 Google, Inc.
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

#ifndef RenderThemeChromiumWin_h
#define RenderThemeChromiumWin_h

#include "RenderTheme.h"

#if WIN32
typedef void* HANDLE;
typedef struct HINSTANCE__* HINSTANCE;
typedef HINSTANCE HMODULE;
#endif

namespace WebCore {

    struct ThemeData {
        ThemeData() : m_part(0), m_state(0), m_classicState(0) {}

        unsigned m_part;
        unsigned m_state;
        unsigned m_classicState;
    };

    class RenderThemeChromiumWin : public RenderTheme {
    public:
        RenderThemeChromiumWin() { }
        ~RenderThemeChromiumWin() { }

        virtual String extraDefaultStyleSheet();
        virtual String extraQuirksStyleSheet();
#if ENABLE(VIDEO)
        virtual String extraMediaControlsStyleSheet();
#endif

        // A method asking if the theme's controls actually care about redrawing when hovered.
        virtual bool supportsHover(const RenderStyle*) const { return true; }

        // A method asking if the theme is able to draw the focus ring.
        virtual bool supportsFocusRing(const RenderStyle*) const;

        // The platform selection color.
        virtual Color platformActiveSelectionBackgroundColor() const;
        virtual Color platformInactiveSelectionBackgroundColor() const;
        virtual Color platformActiveSelectionForegroundColor() const;
        virtual Color platformInactiveSelectionForegroundColor() const;
        virtual Color platformActiveTextSearchHighlightColor() const;
        virtual Color platformInactiveTextSearchHighlightColor() const;

        virtual double caretBlinkInterval() const;

        // System fonts.
        virtual void systemFont(int propId, FontDescription&) const;

        virtual int minimumMenuListSize(RenderStyle*) const;

        virtual void adjustSliderThumbSize(RenderObject*) const;

        virtual bool paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r) { return paintButton(o, i, r); }
        virtual void setCheckboxSize(RenderStyle*) const;

        virtual bool paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r) { return paintButton(o, i, r); }
        virtual void setRadioSize(RenderStyle*) const;

        virtual bool paintButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

        virtual bool paintTextField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

        virtual bool paintTextArea(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r) { return paintTextField(o, i, r); }

        virtual bool paintSliderTrack(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

        virtual bool paintSliderThumb(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r) { return paintSliderTrack(o, i, r); }

        virtual bool paintSearchField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r) { return paintTextField(o, i, r); }

        virtual void adjustSearchFieldCancelButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual bool paintSearchFieldCancelButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

        virtual void adjustSearchFieldDecorationStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

        virtual void adjustSearchFieldResultsDecorationStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual bool paintSearchFieldResultsDecoration(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

        virtual void adjustSearchFieldResultsButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual bool paintSearchFieldResultsButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

        // MenuList refers to an unstyled menulist (meaning a menulist without
        // background-color or border set) and MenuListButton refers to a styled
        // menulist (a menulist with background-color or border set). They have
        // this distinction to support showing aqua style themes whenever they
        // possibly can, which is something we don't want to replicate.
        //
        // In short, we either go down the MenuList code path or the MenuListButton
        // codepath. We never go down both. And in both cases, they render the
        // entire menulist.
        virtual void adjustMenuListStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual bool paintMenuList(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
        virtual void adjustMenuListButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual bool paintMenuListButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

        // These methods define the padding for the MenuList's inner block.
        virtual int popupInternalPaddingLeft(RenderStyle*) const;
        virtual int popupInternalPaddingRight(RenderStyle*) const;
        virtual int popupInternalPaddingTop(RenderStyle*) const;
        virtual int popupInternalPaddingBottom(RenderStyle*) const;

        virtual int buttonInternalPaddingLeft() const;
        virtual int buttonInternalPaddingRight() const;
        virtual int buttonInternalPaddingTop() const;
        virtual int buttonInternalPaddingBottom() const;

        // Provide a way to pass the default font size from the Settings object
        // to the render theme.  FIXME: http://b/1129186 A cleaner way would be
        // to remove the default font size from this object and have callers
        // that need the value to get it directly from the appropriate Settings
        // object.
        static void setDefaultFontSize(int);

    private:
        unsigned determineState(RenderObject*);
        unsigned determineSliderThumbState(RenderObject*);
        unsigned determineClassicState(RenderObject*);

        ThemeData getThemeData(RenderObject*);

        bool paintTextFieldInternal(RenderObject*, const RenderObject::PaintInfo&, const IntRect&, bool);

        int menuListInternalPadding(RenderStyle*, int paddingType) const;
    };

} // namespace WebCore

#endif
