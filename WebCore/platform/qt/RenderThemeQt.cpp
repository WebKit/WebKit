/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 *               2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "config.h"

#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QStyle>
#include <QWidget>
#include <QPainter>
#include <QStyleOptionButton>
#include <QStyleOptionFrameV2>

#include "Color.h"
#include "Document.h"
#include "RenderTheme.h"
#include "GraphicsContext.h"

namespace WebCore {

class RenderThemeQt : public RenderTheme
{
public:
    RenderThemeQt() : RenderTheme() { }

    // A method asking if the theme's controls actually care about redrawing when hovered.
    virtual bool supportsHover(const RenderStyle*) const { return true; }
    virtual bool supportsFocusRing(const RenderStyle* style) const { return false; }

    virtual bool paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
    {
        return paintButton(o, i, r);
    }
 
    virtual void setCheckboxSize(RenderStyle*) const;

    virtual bool paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
    {
        return paintButton(o, i, r);
    }

    virtual void setRadioSize(RenderStyle*) const;

    virtual void adjustButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual void adjustTextFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual bool paintTextField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual bool isControlStyled(const RenderStyle*, const BorderData&,
                                 const BackgroundLayer&, const Color&) const;

    virtual bool controlSupportsTints(const RenderObject*) const;

    virtual void systemFont(int propId, FontDescription&) const;

    // The platform selection color.
    virtual Color platformActiveSelectionBackgroundColor() const;
    virtual Color platformInactiveSelectionBackgroundColor() const;
    virtual Color platformActiveSelectionForegroundColor() const;
    virtual Color platformInactiveSelectionForegroundColor() const;
private:
    void addIntrinsicMargins(RenderStyle*) const;
    void close();

    bool supportsFocus(EAppearance) const;

    bool getStylePainterAndWidgetFromPaintInfo(const RenderObject::PaintInfo&, QStyle*&, QPainter*&, QWidget*&) const;
    EAppearance applyTheme(QStyleOption&, RenderObject*) const;
};

RenderTheme* theme()
{
    static RenderThemeQt rt;
    return &rt;
}

bool RenderThemeQt::isControlStyled(const RenderStyle* style, const BorderData& border,
                                     const BackgroundLayer& background, const Color& backgroundColor) const
{
    if (style->appearance() == TextFieldAppearance || style->appearance() == TextAreaAppearance)
        return style->border() != border;

    return RenderTheme::isControlStyled(style, border, background, backgroundColor);
}

bool RenderThemeQt::controlSupportsTints(const RenderObject* o) const
{
    if (!isEnabled(o))
        return false;

    // Checkboxes only have tint when checked.
    if (o->style()->appearance() == CheckboxAppearance)
        return isChecked(o);

    // For now assume other controls have tint if enabled.
    return true;
}

void RenderThemeQt::systemFont(int propId, FontDescription& fontDescription) const
{
    // no-op
}

void RenderThemeQt::addIntrinsicMargins(RenderStyle* style) const
{
    // Cut out the intrinsic margins completely if we end up using a small font size
    if (style->fontSize() < 11)
        return;

    // Intrinsic margin value.
    const int m = 2;

    // FIXME: Using width/height alone and not also dealing with min-width/max-width is flawed.
    if (style->width().isIntrinsicOrAuto()) {
        if (style->marginLeft().quirk())
            style->setMarginLeft(Length(m, Fixed));

        if (style->marginRight().quirk())
            style->setMarginRight(Length(m, Fixed));
    }

    if (style->height().isAuto()) {
        if (style->marginTop().quirk())
            style->setMarginTop(Length(m, Fixed));

        if (style->marginBottom().quirk())
            style->setMarginBottom(Length(m, Fixed));
    }
}

bool RenderThemeQt::getStylePainterAndWidgetFromPaintInfo(const RenderObject::PaintInfo& i, QStyle*& style, QPainter*& painter, QWidget*& widget) const
{
    painter = (i.context ? static_cast<QPainter*>(i.context->platformContext()) : 0);
    widget = (painter ? static_cast<QWidget*>(painter->device()) : 0);
    style = (widget ? widget->style() : 0);

    return (painter && widget && style);
}

void RenderThemeQt::setCheckboxSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // FIXME:  A hard-coded size of 13 is used.  This is wrong but necessary for now.  It matches Firefox.
    // At different DPI settings on Windows, querying the theme gives you a larger size that accounts for
    // the higher DPI.  Until our entire engine honors a DPI setting other than 96, we can't rely on the theme's
    // metrics.
    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(13, Fixed));

    if (style->height().isAuto())
        style->setHeight(Length(13, Fixed));
}

void RenderThemeQt::setRadioSize(RenderStyle* style) const
{
    // This is the same as checkboxes.
    setCheckboxSize(style);
}

bool RenderThemeQt::supportsFocus(EAppearance appearance) const
{
    switch (appearance) {
        case PushButtonAppearance:
        case ButtonAppearance:
        case TextFieldAppearance:
            return true;
        default: // No for all others...
            return false;
    }
}

EAppearance RenderThemeQt::applyTheme(QStyleOption& option, RenderObject* o) const
{
    // Default bits: no focus, no mouse over
    option.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver);

    if (!isEnabled(o))
        option.state &= ~QStyle::State_Enabled;

    if (isReadOnlyControl(o))
        // Readonly is supported on textfields.
        option.state |= QStyle::State_ReadOnly;

    if (supportsFocus(o->style()->appearance()) && isFocused(o))
        option.state |= QStyle::State_HasFocus;

    if (isHovered(o))
        option.state |= QStyle::State_MouseOver;

    if (isPressed(o))
        option.state |= QStyle::State_Sunken;

    EAppearance result = o->style()->appearance();
    if(result == RadioAppearance || result == CheckboxAppearance)
        option.state |= (isChecked(o) ? QStyle::State_On : QStyle::State_Off);

    return result;
}

void RenderThemeQt::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeQt::paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    QStyle* style = 0;
    QPainter* painter = 0;
    QWidget* widget = 0;
    
    if (!getStylePainterAndWidgetFromPaintInfo(i, style, painter, widget))
        return true;
    
    QStyleOptionButton option;
    option.initFrom(widget);
    option.rect = r;

    // Get the correct theme data for a button
    EAppearance appearance = applyTheme(option, o);
  
    if(appearance == PushButtonAppearance || appearance == ButtonAppearance)
        style->drawControl(QStyle::CE_PushButton, &option, painter);
    else if(appearance == RadioAppearance)
        style->drawControl(QStyle::CE_RadioButton, &option, painter);
    else if(appearance == CheckboxAppearance)
        style->drawControl(QStyle::CE_CheckBox, &option, painter);

    return false;
}

void RenderThemeQt::adjustTextFieldStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeQt::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    QStyle* style = 0;
    QPainter* painter = 0;
    QWidget* widget = 0;
    
    if (!getStylePainterAndWidgetFromPaintInfo(i, style, painter, widget))
        return true;
    
    QStyleOptionFrameV2 panel;
    panel.initFrom(widget);
    panel.rect = r;
    // Get the correct theme data for a button
    EAppearance appearance = applyTheme(panel, o);
    Q_ASSERT(appearance == TextFieldAppearance);

    // Now paint the text field.
    style->drawPrimitive(QStyle::PE_PanelLineEdit, &panel, painter, widget);
    style->drawPrimitive(QStyle::PE_FrameLineEdit, &panel, painter, widget);

    return false;
}

Color RenderThemeQt::platformActiveSelectionBackgroundColor() const
{
    QPalette pal = QApplication::palette();
    return pal.brush(QPalette::Active, QPalette::Highlight).color();
}

Color RenderThemeQt::platformInactiveSelectionBackgroundColor() const
{
    QPalette pal = QApplication::palette();
    return pal.brush(QPalette::Inactive, QPalette::Highlight).color();
}

Color RenderThemeQt::platformActiveSelectionForegroundColor() const
{
    QPalette pal = QApplication::palette();
    return pal.brush(QPalette::Active, QPalette::HighlightedText).color();
}

Color RenderThemeQt::platformInactiveSelectionForegroundColor() const
{
    QPalette pal = QApplication::palette();
    return pal.brush(QPalette::Inactive, QPalette::HighlightedText).color();
}

}

// vim: ts=4 sw=4 et
