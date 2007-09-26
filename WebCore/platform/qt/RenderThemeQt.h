/*
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2007 Trolltech
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
#ifndef RenderThemeQt_H
#define RenderThemeQt_H

#include "RenderTheme.h"

class QStyle;
class QPainter;
class QWidget;
class QStyleOption;

namespace WebCore {

class RenderStyle;

class RenderThemeQt : public RenderTheme
{
public:
    RenderThemeQt();

    virtual bool supportsHover(const RenderStyle*) const;
    virtual bool supportsFocusRing(const RenderStyle* style) const;

    virtual short baselinePosition(const RenderObject* o) const;

    // A method asking if the control changes its tint when the window has focus or not.
    virtual bool controlSupportsTints(const RenderObject*) const;

    // A general method asking if any control tinting is supported at all.
    virtual bool supportsControlTints() const;

    virtual void adjustRepaintRect(const RenderObject* o, IntRect& r);

    virtual bool isControlStyled(const RenderStyle*, const BorderData&,
                                 const BackgroundLayer&, const Color&) const;

    virtual void paintResizeControl(GraphicsContext*, const IntRect&);

    // The platform selection color.
    virtual Color platformActiveSelectionBackgroundColor() const;
    virtual Color platformInactiveSelectionBackgroundColor() const;
    virtual Color platformActiveSelectionForegroundColor() const;
    virtual Color platformInactiveSelectionForegroundColor() const;

    virtual void systemFont(int propId, FontDescription&) const;

    virtual int minimumMenuListSize(RenderStyle*) const;

    virtual void adjustSliderThumbSize(RenderObject*) const;

protected:
    virtual bool paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);
    virtual void setCheckboxSize(RenderStyle*) const;

    virtual bool paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);
    virtual void setRadioSize(RenderStyle*) const;

    virtual void adjustButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual void setButtonSize(RenderStyle*) const;

    virtual bool paintTextField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual void adjustTextFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

    virtual bool paintMenuList(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r);
    virtual void adjustMenuListStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

    virtual bool paintMenuListButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual void adjustMenuListButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

    virtual bool paintSliderTrack(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual bool paintSliderThumb(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual bool paintSearchField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual void adjustSearchFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

    virtual void adjustSearchFieldCancelButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldCancelButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldDecorationStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldDecoration(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldResultsDecorationStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldResultsDecoration(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
private:
    bool supportsFocus(EAppearance) const;

    bool getStylePainterAndWidgetFromPaintInfo(const RenderObject::PaintInfo&, QStyle*&, QPainter*&, QWidget*&) const;
    EAppearance applyTheme(QStyleOption&, RenderObject*) const;

    void setSizeFromFont(RenderStyle*) const;
    IntSize sizeForFont(RenderStyle*) const;
    void setButtonPadding(RenderStyle*) const;
    void setPopupPadding(RenderStyle*) const;
    void setPrimitiveSize(RenderStyle*) const;
};

}

#endif
