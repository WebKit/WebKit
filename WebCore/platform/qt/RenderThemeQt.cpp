/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 *               2006 Dirk Mueller <mueller@kde.org>
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "qwebpage.h"
#include "RenderThemeQt.h"
#include "ChromeClientQt.h"
#include "NotImplemented.h"

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
#include "Page.h"
#include "Font.h"
#include "RenderTheme.h"
#include "GraphicsContext.h"

namespace WebCore {

RenderTheme* theme()
{
    static RenderThemeQt rt;
    return &rt;
}

RenderThemeQt::RenderThemeQt()
    : RenderTheme()
{
}

bool RenderThemeQt::supportsHover(const RenderStyle*) const
{
    return true;
}

bool RenderThemeQt::supportsFocusRing(const RenderStyle* style) const
{
    return supportsFocus(style->appearance());
}

short RenderThemeQt::baselinePosition(const RenderObject* o) const
{
    if (o->style()->appearance() == CheckboxAppearance ||
        o->style()->appearance() == RadioAppearance)
        return o->marginTop() + o->height() - 2; // Same as in old khtml
    return RenderTheme::baselinePosition(o);
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

bool RenderThemeQt::supportsControlTints() const
{
    return true;
}

void RenderThemeQt::adjustRepaintRect(const RenderObject* o, IntRect& r)
{
    switch (o->style()->appearance()) {
    case CheckboxAppearance: {
        break;
    }
    case RadioAppearance: {
        break;
    }
    case PushButtonAppearance:
    case ButtonAppearance: {
        break;
    }
    case MenulistAppearance: {
        break;
    }
    default:
        break;
    }
}

bool RenderThemeQt::isControlStyled(const RenderStyle* style, const BorderData& border,
                                     const BackgroundLayer& background, const Color& backgroundColor) const
{
    if (style->appearance() == TextFieldAppearance || style->appearance() == TextAreaAppearance)
        return style->border() != border;

    return RenderTheme::isControlStyled(style, border, background, backgroundColor);
}

void RenderThemeQt::paintResizeControl(GraphicsContext*, const IntRect&)
{
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

void RenderThemeQt::systemFont(int propId, FontDescription& fontDescription) const
{
    // no-op
}

int RenderThemeQt::minimumMenuListSize(RenderStyle*) const
{
    const QFontMetrics &fm = QApplication::fontMetrics();
    return 7 * fm.width(QLatin1Char('x'));
}

void RenderThemeQt::adjustSliderThumbSize(RenderObject* o) const
{
    RenderTheme::adjustSliderThumbSize(o);
}

bool RenderThemeQt::paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintButton(o, i, r);
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
    const int ff = 13;
    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(ff, Fixed));

    if (style->height().isAuto())
        style->setHeight(Length(ff, Fixed));
}

bool RenderThemeQt::paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintButton(o, i, r);
}

void RenderThemeQt::setRadioSize(RenderStyle* style) const
{
    // This is the same as checkboxes.
    setCheckboxSize(style);
}

void RenderThemeQt::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    // Ditch the border.
    style->resetBorder();

    // Height is locked to auto.
    style->setHeight(Length(Auto));

    // White-space is locked to pre
    style->setWhiteSpace(PRE);

    setButtonSize(style);

    setButtonPadding(style);
}

bool RenderThemeQt::paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    QStyle* style = 0;
    QPainter* painter = 0;
    QWidget* widget = 0;

    if (!getStylePainterAndWidgetFromPaintInfo(i, style, painter, widget))
        return true;

    QStyleOptionButton option;
    if (widget)
        option.initFrom(widget);
    option.rect = r;

    // Get the correct theme data for a button
    EAppearance appearance = applyTheme(option, o);

    if(appearance == PushButtonAppearance || appearance == ButtonAppearance)
        style->drawControl(QStyle::CE_PushButton, &option, painter);
    else if(appearance == RadioAppearance)
        style->drawPrimitive(QStyle::PE_IndicatorRadioButton, &option, painter, widget);
    else if(appearance == CheckboxAppearance)
        style->drawPrimitive(QStyle::PE_IndicatorCheckBox, &option, painter, widget);

    return false;
}

void RenderThemeQt::setButtonSize(RenderStyle* style) const
{
    setPrimitiveSize(style);
}

bool RenderThemeQt::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    QStyle* style = 0;
    QPainter* painter = 0;
    QWidget* widget = 0;

    if (!getStylePainterAndWidgetFromPaintInfo(i, style, painter, widget))
        return true;

    QStyleOptionFrameV2 panel;
    if (widget)
        panel.initFrom(widget);
    panel.rect = r;
    panel.state |= QStyle::State_Sunken;
    panel.features = QStyleOptionFrameV2::None;

    // Get the correct theme data for a button
    EAppearance appearance = applyTheme(panel, o);
    Q_ASSERT(appearance == TextFieldAppearance || appearance == SearchFieldAppearance);

    // Now paint the text field.
    style->drawPrimitive(QStyle::PE_PanelLineEdit, &panel, painter, widget);
    style->drawPrimitive(QStyle::PE_FrameLineEdit, &panel, painter, widget);
      
    return false;
}

void RenderThemeQt::adjustTextFieldStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
}

void RenderThemeQt::adjustMenuListStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->resetBorder();

    // Height is locked to auto.
    style->setHeight(Length(Auto));

    // White-space is locked to pre
    style->setWhiteSpace(PRE);

    setPrimitiveSize(style);

    // Add in the padding that we'd like to use.
    setPopupPadding(style);

    // Our font is locked to the appropriate system font size for the control.  To clarify, we first use the CSS-specified font to figure out
    // a reasonable control size, but once that control size is determined, we throw that font away and use the appropriate
    // system font for the control size instead.
    //setFontFromControlSize(selector, style);
}

bool RenderThemeQt::paintMenuList(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    QStyle* style = 0;
    QPainter* painter = 0;
    QWidget* widget = 0;

    if (!getStylePainterAndWidgetFromPaintInfo(i, style, painter, widget))
        return true;

    QStyleOptionComboBox opt;
    if (widget)
        opt.initFrom(widget);
    EAppearance appearance = applyTheme(opt, o);
    const QPoint topLeft = r.topLeft();
    painter->translate(topLeft);
    opt.rect.moveTo(QPoint(0,0));
    opt.rect.setSize(r.size());

    opt.frame = false;

    style->drawComplexControl(QStyle::CC_ComboBox, &opt, painter, widget);
    painter->translate(-topLeft);
    return false;
}


bool RenderThemeQt::paintMenuListButton(RenderObject* o, const RenderObject::PaintInfo& pi,
                                        const IntRect& r)
{
    notImplemented();
    return RenderTheme::paintMenuListButton(o, pi, r);
}

void RenderThemeQt::adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style,
                                              Element* e) const
{
    notImplemented();
    RenderTheme::adjustMenuListButtonStyle(selector, style, e);
}

bool RenderThemeQt::paintSliderTrack(RenderObject* o, const RenderObject::PaintInfo& pi,
                                     const IntRect& r)
{
    notImplemented();
    return RenderTheme::paintSliderTrack(o, pi, r);
}

bool RenderThemeQt::paintSliderThumb(RenderObject* o, const RenderObject::PaintInfo& pi,
                                     const IntRect& r)
{
    notImplemented();
    return RenderTheme::paintSliderThumb(o, pi, r);
}

bool RenderThemeQt::paintSearchField(RenderObject* o, const RenderObject::PaintInfo& pi,
                                     const IntRect& r)
{
    paintTextField(o, pi, r);
    return false;
}

void RenderThemeQt::adjustSearchFieldStyle(CSSStyleSelector* selector, RenderStyle* style,
                                           Element* e) const
{
    notImplemented();
    RenderTheme::adjustSearchFieldStyle(selector, style, e);
}

void RenderThemeQt::adjustSearchFieldCancelButtonStyle(CSSStyleSelector* selector, RenderStyle* style,
                                                       Element* e) const
{
    notImplemented();
    RenderTheme::adjustSearchFieldCancelButtonStyle(selector, style, e);
}

bool RenderThemeQt::paintSearchFieldCancelButton(RenderObject* o, const RenderObject::PaintInfo& pi,
                                                 const IntRect& r)
{
    notImplemented();
    return RenderTheme::paintSearchFieldCancelButton(o, pi, r);
}

void RenderThemeQt::adjustSearchFieldDecorationStyle(CSSStyleSelector* selector, RenderStyle* style,
                                                     Element* e) const
{
    notImplemented();
    RenderTheme::adjustSearchFieldDecorationStyle(selector, style, e);
}

bool RenderThemeQt::paintSearchFieldDecoration(RenderObject* o, const RenderObject::PaintInfo& pi,
                                               const IntRect& r)
{
    notImplemented();
    return RenderTheme::paintSearchFieldDecoration(o, pi, r);
}

void RenderThemeQt::adjustSearchFieldResultsDecorationStyle(CSSStyleSelector* selector, RenderStyle* style,
                                                            Element* e) const
{
    notImplemented();
    RenderTheme::adjustSearchFieldResultsDecorationStyle(selector, style, e);
}

bool RenderThemeQt::paintSearchFieldResultsDecoration(RenderObject* o, const RenderObject::PaintInfo& pi,
                                                      const IntRect& r)
{
    notImplemented();
    return RenderTheme::paintSearchFieldResultsDecoration(o, pi, r);
}

bool RenderThemeQt::supportsFocus(EAppearance appearance) const
{
    switch (appearance) {
        case PushButtonAppearance:
        case ButtonAppearance:
        case TextFieldAppearance:
        case MenulistAppearance:
        case RadioAppearance:
        case CheckboxAppearance:
            return true;
        default: // No for all others...
            return false;
    }
}

bool RenderThemeQt::getStylePainterAndWidgetFromPaintInfo(const RenderObject::PaintInfo& i, QStyle*& style,
                                                          QPainter*& painter, QWidget*& widget) const
{
    painter = (i.context ? static_cast<QPainter*>(i.context->platformContext()) : 0);
    widget = 0;
    QPaintDevice* dev = 0;
    if (painter)
        dev = painter->device();
    if (dev && dev->devType() == QInternal::Widget)
        widget = static_cast<QWidget*>(dev);
    style = (widget ? widget->style() : QApplication::style());

    return (painter && style);
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

    EAppearance result = o->style()->appearance();

    switch (result) {
        case PushButtonAppearance:
        case SquareButtonAppearance:
        case ButtonAppearance:
        case ButtonBevelAppearance:
        case ListItemAppearance:
        case MenulistButtonAppearance:
        case ScrollbarButtonLeftAppearance:
        case ScrollbarButtonRightAppearance:
        case ScrollbarTrackHorizontalAppearance:
        case ScrollbarTrackVerticalAppearance:
        case ScrollbarThumbHorizontalAppearance:
        case ScrollbarThumbVerticalAppearance:
        case SearchFieldResultsButtonAppearance:
        case SearchFieldCancelButtonAppearance: {
            if (isPressed(o))
                option.state |= QStyle::State_Sunken;
            else if (result == PushButtonAppearance)
                option.state |= QStyle::State_Raised;
            break;
        }
    }

    if(result == RadioAppearance || result == CheckboxAppearance)
        option.state |= (isChecked(o) ? QStyle::State_On : QStyle::State_Off);

    // If the webview has a custom palette, use it
    Page* page = o->document()->page();
    if (page) {
        QWidget* view = static_cast<ChromeClientQt*>(page->chrome()->client())->m_webPage->view();
        if (view)
            option.palette = view->palette();
    }

    return result;
}

void RenderThemeQt::setSizeFromFont(RenderStyle* style) const
{
    // FIXME: Check is flawed, since it doesn't take min-width/max-width into account.
     IntSize size = sizeForFont(style);
    if (style->width().isIntrinsicOrAuto() && size.width() > 0)
        style->setWidth(Length(size.width(), Fixed));
    if (style->height().isAuto() && size.height() > 0)
        style->setHeight(Length(size.height(), Fixed));
}

IntSize RenderThemeQt::sizeForFont(RenderStyle* style) const
{
    const QFontMetrics fm(style->font().font());
    QSize size(0, 0);
    switch (style->appearance()) {
    case CheckboxAppearance: {
        break;
    }
    case RadioAppearance: {
        break;
    }
    case PushButtonAppearance:
    case ButtonAppearance: {
        QSize sz = fm.size(Qt::TextShowMnemonic, QString::fromLatin1("X"));
        QStyleOptionButton opt;
        sz = QApplication::style()->sizeFromContents(QStyle::CT_PushButton,
                                                     &opt, sz, 0);
        size.setHeight(sz.height());
        break;
    }
    case MenulistAppearance: {
        QSize sz;
        sz.setHeight(qMax(fm.lineSpacing(), 14) + 2);
        QStyleOptionComboBox opt;
        sz = QApplication::style()->sizeFromContents(QStyle::CT_ComboBox,
                                                     &opt, sz, 0);
        size.setHeight(sz.height());
        break;
    }
    case TextFieldAppearance: {
        const int verticalMargin   = 1;
        const int horizontalMargin = 2;
        int h = qMax(fm.lineSpacing(), 14) + 2*verticalMargin;
        int w = fm.width(QLatin1Char('x')) * 17 + 2*horizontalMargin;
        QStyleOptionFrameV2 opt;
        opt.lineWidth = QApplication::style()->pixelMetric(QStyle::PM_DefaultFrameWidth,
                                                           &opt, 0);
        QSize sz = QApplication::style()->sizeFromContents(QStyle::CT_LineEdit,
                                                           &opt,
                                                           QSize(w, h).expandedTo(QApplication::globalStrut()),
                                                           0);
        size.setHeight(sz.height());
        break;
    }
    default:
        break;
    }
    return size;
}

void RenderThemeQt::setButtonPadding(RenderStyle* style) const
{
    const int padding = 8;
    style->setPaddingLeft(Length(padding, Fixed));
    style->setPaddingRight(Length(padding, Fixed));
    style->setPaddingTop(Length(0, Fixed));
    style->setPaddingBottom(Length(0, Fixed));
}

void RenderThemeQt::setPopupPadding(RenderStyle* style) const
{
    const int padding = 8;
    style->setPaddingLeft(Length(padding, Fixed));
    QStyleOptionComboBox opt;
    int w = QApplication::style()->pixelMetric(QStyle::PM_ButtonIconSize, &opt, 0);
    style->setPaddingRight(Length(padding + w, Fixed));

    style->setPaddingTop(Length(1, Fixed));
    style->setPaddingBottom(Length(0, Fixed));
}

void RenderThemeQt::setPrimitiveSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // Use the font size to determine the intrinsic width of the control.
    setSizeFromFont(style);
}

}

// vim: ts=4 sw=4 et
