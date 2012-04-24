/*
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2011-2012 Nokia Corporation and/or its subsidiary(-ies).
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
#ifndef RenderThemeQStyle_h
#define RenderThemeQStyle_h

#include "RenderThemeQt.h"

#include <QStyle>

QT_BEGIN_NAMESPACE
#ifndef QT_NO_LINEEDIT
class QLineEdit;
#endif
class QPainter;
class QWidget;
QT_END_NAMESPACE

namespace WebCore {

class ScrollbarThemeQStyle;

class RenderThemeQStyle : public RenderThemeQt {
private:
    RenderThemeQStyle(Page*);
    virtual ~RenderThemeQStyle();

public:
    static PassRefPtr<RenderTheme> create(Page*);

    virtual void adjustSliderThumbSize(RenderStyle*) const;

    QStyle* qStyle() const;

protected:
    virtual void adjustButtonStyle(StyleResolver*, RenderStyle*, Element*) const;
    virtual bool paintButton(RenderObject*, const PaintInfo&, const IntRect&);

    virtual bool paintTextField(RenderObject*, const PaintInfo&, const IntRect&);

    virtual bool paintTextArea(RenderObject*, const PaintInfo&, const IntRect&);
    virtual void adjustTextAreaStyle(StyleResolver*, RenderStyle*, Element*) const;

    virtual bool paintMenuList(RenderObject*, const PaintInfo&, const IntRect&);

    virtual bool paintMenuListButton(RenderObject*, const PaintInfo&, const IntRect&);
    virtual void adjustMenuListButtonStyle(StyleResolver*, RenderStyle*, Element*) const;

#if ENABLE(PROGRESS_TAG)
    // Returns the duration of the animation for the progress bar.
    virtual double animationDurationForProgressBar(RenderProgress*) const;
    virtual bool paintProgressBar(RenderObject*, const PaintInfo&, const IntRect&);
#endif

    virtual bool paintSliderTrack(RenderObject*, const PaintInfo&, const IntRect&);
    virtual void adjustSliderTrackStyle(StyleResolver*, RenderStyle*, Element*) const;

    virtual bool paintSliderThumb(RenderObject*, const PaintInfo&, const IntRect&);
    virtual void adjustSliderThumbStyle(StyleResolver*, RenderStyle*, Element*) const;

    virtual bool paintSearchField(RenderObject*, const PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldDecorationStyle(StyleResolver*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldDecoration(RenderObject*, const PaintInfo&, const IntRect&);

    virtual void adjustSearchFieldResultsDecorationStyle(StyleResolver*, RenderStyle*, Element*) const;
    virtual bool paintSearchFieldResultsDecoration(RenderObject*, const PaintInfo&, const IntRect&);

#ifndef QT_NO_SPINBOX
    virtual bool paintInnerSpinButton(RenderObject*, const PaintInfo&, const IntRect&);
#endif

protected:
    virtual void computeSizeBasedOnStyle(RenderStyle*) const;

    virtual QSharedPointer<StylePainter> getStylePainter(const PaintInfo&);

    virtual QRect inflateButtonRect(const QRect& originalRect) const;

    virtual void setPopupPadding(RenderStyle*) const;

private:
    ControlPart initializeCommonQStyleOptions(QStyleOption&, RenderObject*) const;

    void setButtonPadding(RenderStyle*) const;

    int findFrameLineWidth(QStyle*) const;

    QStyle* fallbackStyle() const;

#ifdef Q_OS_MAC
    int m_buttonFontPixelSize;
#endif

    QStyle* m_fallbackStyle;
#ifndef QT_NO_LINEEDIT
    mutable QLineEdit* m_lineEdit;
#endif
};

class StylePainterQStyle : public StylePainter {
public:
    explicit StylePainterQStyle(RenderThemeQStyle*, const PaintInfo&);
    explicit StylePainterQStyle(ScrollbarThemeQStyle*, GraphicsContext*);

    bool isValid() const { return style && StylePainter::isValid(); }

    QWidget* widget;
    QStyle* style;

    void drawPrimitive(QStyle::PrimitiveElement pe, const QStyleOption& opt)
    { style->drawPrimitive(pe, &opt, painter, widget); }
    void drawControl(QStyle::ControlElement ce, const QStyleOption& opt)
    { style->drawControl(ce, &opt, painter, widget); }
    void drawComplexControl(QStyle::ComplexControl cc, const QStyleOptionComplex& opt)
    { style->drawComplexControl(cc, &opt, painter, widget); }

private:
    void init(GraphicsContext*, QStyle*);

    Q_DISABLE_COPY(StylePainterQStyle)
};

}

#endif // RenderThemeQStyle_h
