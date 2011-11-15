/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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
#include "RenderThemeQtMobile.h"

#include "CSSStyleSelector.h"
#include "CSSValueKeywords.h"
#include "Color.h"
#include "Document.h"
#include "Font.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLSelectElement.h"
#include "LocalizedStrings.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PaintInfo.h"
#include "PassRefPtr.h"
#include "QWebPageClient.h"
#include "RenderBox.h"
#if ENABLE(PROGRESS_TAG)
#include "RenderProgress.h"
#endif
#if HAVE(QSTYLE)
#include "RenderThemeQStyle.h"
#endif

#include <QColor>
#include <QFile>
#include <QFontMetrics>
#include <QPainter>
#include <QPixmapCache>

namespace WebCore {

using namespace HTMLNames;

// Constants extracted from QCommonStyle and QWindowsStyle
static const int arrowBoxWidth = 16;
static const int frameWidth = 2;
static const int checkBoxWidth = 13;
static const int radioWidth = 12;
static const int buttonIconSize = 16;
static const int sliderWidth = 11;
static const int sliderHeight = 0;

// Other constants used by the mobile theme
static float buttonPaddingLeft = 18;
static float buttonPaddingRight = 18;
static float buttonPaddingTop = 2;
static float buttonPaddingBottom = 3;
static float menuListPadding = 9;
static float textFieldPadding = 5;


static inline void drawRectangularControlBackground(QPainter* painter, const QPen& pen, const QRect& rect, const QBrush& brush)
{
    QPen oldPen = painter->pen();
    QBrush oldBrush = painter->brush();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(pen);
    painter->setBrush(brush);

    int line = 1;
    painter->drawRoundedRect(rect.adjusted(line, line, -line, -line),
            /* xRadius */ 5.0, /* yRadious */ 5.0);
    painter->setPen(oldPen);
    painter->setBrush(oldBrush);
}

QSharedPointer<StylePainter> RenderThemeQtMobile::getStylePainter(const PaintInfo& pi)
{
    return QSharedPointer<StylePainter>(new StylePainterMobile(this, pi));
}

StylePainterMobile::StylePainterMobile(RenderThemeQtMobile* theme, const PaintInfo& paintInfo)
    : StylePainter(theme, paintInfo)
{
}

void StylePainterMobile::drawChecker(QPainter* painter, int size, QColor color) const
{
    int border = qMin(qMax(1, int(0.2 * size)), 6);
    int checkerSize = qMax(size - 2 * border, 3);
    int width = checkerSize / 3;
    int middle = qMax(3 * checkerSize / 7, 3);
    int x = ((size - checkerSize) >> 1);
    int y = ((size - checkerSize) >> 1) + (checkerSize - width - middle);
    QVector<QLineF> lines(checkerSize + 1);
    painter->setPen(color);
    for (int i = 0; i < middle; ++i) {
        lines[i] = QLineF(x, y, x, y + width);
        ++x;
        ++y;
    }
    for (int i = middle; i <= checkerSize; ++i) {
        lines[i] = QLineF(x, y, x, y + width);
        ++x;
        --y;
    }
    painter->drawLines(lines.constData(), lines.size());
}

QPixmap StylePainterMobile::findChecker(const QRect& rect, bool disabled) const
{
    int size = qMin(rect.width(), rect.height());
    QPixmap result;
    static const QString prefix = QLatin1String("$qt-maemo5-mobile-theme-checker-");
    QString key = prefix + QString::number(size) + QLatin1String("-") + (disabled ? QLatin1String("disabled") : QLatin1String("enabled"));
    if (!QPixmapCache::find(key, result)) {
        result = QPixmap(size, size);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        drawChecker(&painter, size, disabled ? Qt::lightGray : Qt::darkGray);
        QPixmapCache::insert(key, result);
    }
    return result;
}

void StylePainterMobile::drawRadio(QPainter* painter, const QSize& size, bool checked, QColor color) const
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    // get minor size to do not paint a wide elipse
    qreal squareSize = qMin(size.width(), size.height());
    // deflate one pixel
    QRect rect = QRect(QPoint(1, 1), QSize(squareSize - 2, squareSize - 2));
    const QPoint centerGradient(rect.bottomRight() * 0.7);

    QRadialGradient radialGradient(centerGradient, centerGradient.x() - 1);
    radialGradient.setColorAt(0.0, Qt::white);
    radialGradient.setColorAt(0.6, Qt::white);
    radialGradient.setColorAt(1.0, color);

    painter->setPen(color);
    painter->setBrush(color);
    painter->drawEllipse(rect);
    painter->setBrush(radialGradient);
    painter->drawEllipse(rect);

    int border = 0.1 * (rect.width() + rect.height());
    border = qMin(qMax(2, border), 10);
    rect.adjust(border, border, -border, -border);
    if (checked) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawEllipse(rect);
    }
}

QPixmap StylePainterMobile::findRadio(const QSize& size, bool checked, bool disabled) const
{
    QPixmap result;
    static const QString prefix = QLatin1String("$qt-maemo5-mobile-theme-radio-");
    QString key = prefix + QString::number(size.width()) + QLatin1String("-") + QString::number(size.height()) + QLatin1String("-")
        + (disabled ? QLatin1String("disabled") : QLatin1String("enabled")) + (checked ? QLatin1String("-checked") : QLatin1String(""));
    if (!QPixmapCache::find(key, result)) {
        result = QPixmap(size);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        drawRadio(&painter, size, checked, disabled ? Qt::lightGray : Qt::darkGray);
        QPixmapCache::insert(key, result);
    }
    return result;
}

void StylePainterMobile::drawMultipleComboButton(QPainter* painter, const QSize& size, QColor color) const
{
    int rectWidth = size.width() - 1;
    int width = qMax(2, rectWidth >> 3);
    int distance = (rectWidth - 3 * width) >> 1;
    int top = (size.height() - width) >> 1;

    painter->setPen(color);
    painter->setBrush(color);

    painter->drawRect(0, top, width, width);
    painter->drawRect(width + distance, top, width, width);
    painter->drawRect(2 * (width + distance), top, width, width);
}

void StylePainterMobile::drawSimpleComboButton(QPainter* painter, const QSize& size, QColor color) const
{
    int width = size.width();
    int midle = width >> 1;
    QVector<QLine> lines(width + 1);

    for (int x = 0, y = 0;  x < midle; x++, y++) {
        lines[x] = QLine(x, y, x, y + 2);
        lines[x + midle] = QLine(width - x - 1, y, width - x - 1, y + 2);
    }
    // Just to ensure the lines' intersection.
    lines[width] = QLine(midle, midle, midle, midle + 2);

    painter->setPen(color);
    painter->setBrush(color);
    painter->drawLines(lines);
}

QSize StylePainterMobile::getButtonImageSize(const QSize& buttonSize) const
{
    const int border = qMax(3, buttonSize.width() >> 3) << 1;

    int width = buttonSize.width() - border;
    int height = buttonSize.height() - border;

    if (width < 0 || height < 0)
        return QSize();

    if (height >= (width >> 1))
        width = width >> 1 << 1;
    else
        width = height << 1;

    return QSize(width + 1, width);
}

QPixmap StylePainterMobile::findComboButton(const QSize& size, bool multiple, bool disabled) const
{
    QPixmap result;
    QSize imageSize = getButtonImageSize(size);

    if (imageSize.isNull())
        return QPixmap();
    static const QString prefix = QLatin1String("$qt-maemo5-mobile-theme-combo-");
    QString key = prefix + (multiple ? QLatin1String("multiple-") : QLatin1String("simple-"))
        + QString::number(imageSize.width()) + QLatin1String("-") + QString::number(imageSize.height())
        + QLatin1String("-") + (disabled ? QLatin1String("disabled") : QLatin1String("enabled"));
    if (!QPixmapCache::find(key, result)) {
        result = QPixmap(imageSize);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        if (multiple)
            drawMultipleComboButton(&painter, imageSize, disabled ? Qt::lightGray : Qt::darkGray);
        else
            drawSimpleComboButton(&painter, imageSize, disabled ? Qt::lightGray : Qt::darkGray);
        QPixmapCache::insert(key, result);
    }
    return result;
}

void StylePainterMobile::drawLineEdit(const QRect& rect, bool sunken, bool enabled)
{
    QPen pen(Qt::darkGray, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(pen);

    if (sunken) {
        drawRectangularControlBackground(painter, pen, rect, QBrush(Qt::darkGray));
        return;
    }

    QLinearGradient linearGradient;
    if (!enabled) {
        linearGradient.setStart(rect.topLeft());
        linearGradient.setFinalStop(rect.bottomLeft());
        linearGradient.setColorAt(0.0, Qt::lightGray);
        linearGradient.setColorAt(1.0, Qt::white);
    } else {
        linearGradient.setStart(rect.topLeft());
        linearGradient.setFinalStop(QPoint(rect.topLeft().x(),
                    rect.topLeft().y() + /* offset limit for gradient */ 20));
        linearGradient.setColorAt(0.0, Qt::darkGray);
        linearGradient.setColorAt(0.35, Qt::white);
    }

    drawRectangularControlBackground(painter, pen, rect, linearGradient);
}

void StylePainterMobile::drawCheckBox(const QRect& rect, bool checked, bool enabled)
{
    QLinearGradient linearGradient(rect.topLeft(), rect.bottomLeft());
    if (!enabled) {
        linearGradient.setColorAt(0.0, Qt::lightGray);
        linearGradient.setColorAt(0.5, Qt::white);
    } else {
        linearGradient.setColorAt(0.0, Qt::darkGray);
        linearGradient.setColorAt(0.5, Qt::white);
    }

    painter->setPen(QPen(enabled ? Qt::darkGray : Qt::lightGray));
    painter->setBrush(linearGradient);
    painter->drawRect(rect);
    const QRect r = rect.adjusted(1, 1, -1, -1);

    if (!checked)
        return;

    QPixmap checker = findChecker(r, !enabled);
    if (checker.isNull())
        return;

    int x = (r.width() - checker.width()) >> 1;
    int y = (r.height() - checker.height()) >> 1;
    painter->drawPixmap(r.x() + x, r.y() + y, checker);
}

void StylePainterMobile::drawRadioButton(const QRect& rect, bool checked, bool enabled)
{
    QPixmap radio = findRadio(rect.size(), checked, !enabled);
    if (radio.isNull())
        return;
    painter->drawPixmap(rect.x(), rect.y(), radio);
}

void StylePainterMobile::drawPushButton(const QRect& rect, bool sunken, bool enabled)
{
    QPen pen(Qt::darkGray, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(pen);

    if (sunken) {
        drawRectangularControlBackground(painter, pen, rect, QBrush(Qt::darkGray));
        return;
    }

    QLinearGradient linearGradient;
    if (!enabled) {
        linearGradient.setStart(rect.bottomLeft());
        linearGradient.setFinalStop(rect.topLeft());
        linearGradient.setColorAt(0.0, Qt::gray);
        linearGradient.setColorAt(1.0, Qt::white);
    } else {
        linearGradient.setStart(rect.bottomLeft());
        linearGradient.setFinalStop(QPoint(rect.bottomLeft().x(),
                    rect.bottomLeft().y() - /* offset limit for gradient */ 20));
        linearGradient.setColorAt(0.0, Qt::gray);
        linearGradient.setColorAt(0.4, Qt::white);
    }

    drawRectangularControlBackground(painter, pen, rect, linearGradient);
}

void StylePainterMobile::drawComboBox(const QRect& rect, bool multiple, bool enabled)
{
    QPen pen(Qt::darkGray, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QLinearGradient linearGradient;
    if (!enabled) {
        linearGradient.setStart(rect.bottomLeft());
        linearGradient.setFinalStop(rect.topLeft());
        linearGradient.setColorAt(0.0, Qt::gray);
        linearGradient.setColorAt(1.0, Qt::white);
    } else {
        linearGradient.setStart(rect.bottomLeft());
        linearGradient.setFinalStop(QPoint(rect.bottomLeft().x(),
                    rect.bottomLeft().y() - /* offset limit for gradient */ 20));
        linearGradient.setColorAt(0.0, Qt::gray);
        linearGradient.setColorAt(0.4, Qt::white);
    }

    drawRectangularControlBackground(painter, pen, rect, linearGradient);

    const QRect r(rect.x() + rect.width() - frameWidth - arrowBoxWidth, rect.y() + frameWidth
                   , arrowBoxWidth, rect.height() - 2 * frameWidth);
    QPixmap pic = findComboButton(r.size(), multiple, !enabled);

    if (pic.isNull())
        return;

    int x = (r.width() - pic.width()) >> 1;
    int y = (r.height() - pic.height()) >> 1;
    painter->drawPixmap(r.x() + x, r.y() + y, pic);

    painter->setPen(enabled ? Qt::darkGray : Qt::gray);
    painter->drawLine(r.left() - 2, r.top() + 2, r.left() - 2, r.bottom() - 2);
}


PassRefPtr<RenderTheme> RenderThemeQtMobile::create(Page* page)
{
    return adoptRef(new RenderThemeQtMobile(page));
}

static PassRefPtr<RenderTheme> createTheme(Page* page)
{
#if HAVE(QSTYLE)
    if (!RenderThemeQt::useMobileTheme())
        return RenderThemeQStyle::create(page);
#endif
    return RenderThemeQtMobile::create(page);
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    if (page)
        return createTheme(page);
    static RenderTheme* fallback = createTheme(0).leakRef();
    return fallback;
}

RenderThemeQtMobile::RenderThemeQtMobile(Page* page)
    : RenderThemeQt(page)
{
}

RenderThemeQtMobile::~RenderThemeQtMobile()
{
}

bool RenderThemeQtMobile::isControlStyled(const RenderStyle* style, const BorderData& border, const FillLayer& fill, const Color& backgroundColor) const
{
    switch (style->appearance()) {
    case CheckboxPart:
    case RadioPart:
        return false;
    default:
        return RenderThemeQt::isControlStyled(style, border, fill, backgroundColor);
    }
}

int RenderThemeQtMobile::popupInternalPaddingBottom(RenderStyle* style) const
{
    return 1;
}

void RenderThemeQtMobile::computeSizeBasedOnStyle(RenderStyle* renderStyle) const
{
    QSize size(0, 0);
    const QFontMetrics fm(renderStyle->font().font());

    switch (renderStyle->appearance()) {
    case TextAreaPart:
    case SearchFieldPart:
    case TextFieldPart: {
        int padding = frameWidth;
        renderStyle->setPaddingLeft(Length(padding, Fixed));
        renderStyle->setPaddingRight(Length(padding, Fixed));
        renderStyle->setPaddingTop(Length(padding, Fixed));
        renderStyle->setPaddingBottom(Length(padding, Fixed));
        break;
    }
    default:
        break;
    }
    // If the width and height are both specified, then we have nothing to do.
    if (!renderStyle->width().isIntrinsicOrAuto() && !renderStyle->height().isAuto())
        return;

    switch (renderStyle->appearance()) {
    case CheckboxPart: {
        const int w = checkBoxWidth * renderStyle->effectiveZoom();
        size = QSize(w, w);
        break;
    }
    case RadioPart: {
        const int w = radioWidth * renderStyle->effectiveZoom();
        size = QSize(w, w);
        break;
    }
    default:
        break;
    }

    // FIXME: Check is flawed, since it doesn't take min-width/max-width into account.
    if (renderStyle->width().isIntrinsicOrAuto() && size.width() > 0)
        renderStyle->setMinWidth(Length(size.width(), Fixed));
    if (renderStyle->height().isAuto() && size.height() > 0)
        renderStyle->setMinHeight(Length(size.height(), Fixed));
}

void RenderThemeQtMobile::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element*) const
{
    // Ditch the border.
    style->resetBorder();

    FontDescription fontDescription = style->fontDescription();
    fontDescription.setIsAbsoluteSize(true);

    fontDescription.setSpecifiedSize(style->fontSize());
    fontDescription.setComputedSize(style->fontSize());

    style->setLineHeight(RenderStyle::initialLineHeight());
    setButtonSize(style);
    setButtonPadding(style);
}


void RenderThemeQtMobile::setButtonPadding(RenderStyle* style) const
{
    if (!style)
        return;
    style->setPaddingLeft(Length(buttonPaddingLeft, Fixed));
    style->setPaddingRight(Length(buttonPaddingRight, Fixed));
    style->setPaddingTop(Length(buttonPaddingTop, Fixed));
    style->setPaddingBottom(Length(buttonPaddingBottom, Fixed));
}

bool RenderThemeQtMobile::paintButton(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    StylePainterMobile p(this, i);
    if (!p.isValid())
       return true;

    ControlPart appearance = o->style()->appearance();
    if (appearance == PushButtonPart || appearance == ButtonPart) {
        p.drawPushButton(r, isPressed(o));
    } else if (appearance == RadioPart)
       p.drawRadioButton(r, isChecked(o));
    else if (appearance == CheckboxPart)
       p.drawCheckBox(r, isChecked(o));

    return false;
}

void RenderThemeQtMobile::adjustTextFieldStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    // Resetting the style like this leads to differences like:
    // - RenderTextControl {INPUT} at (2,2) size 168x25 [bgcolor=#FFFFFF] border: (2px inset #000000)]
    // + RenderTextControl {INPUT} at (2,2) size 166x26
    // in layout tests when a CSS style is applied that doesn't affect background color, border or
    // padding. Just worth keeping in mind!
    style->setBackgroundColor(Color::transparent);
    style->resetBorder();
    style->resetPadding();
    computeSizeBasedOnStyle(style);
    style->setPaddingLeft(Length(textFieldPadding, Fixed));
    style->setPaddingRight(Length(textFieldPadding, Fixed));
}

bool RenderThemeQtMobile::paintTextField(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    StylePainterMobile p(this, i);
    if (!p.isValid())
        return true;

    ControlPart appearance = o->style()->appearance();
    if (appearance != TextFieldPart
        && appearance != SearchFieldPart
        && appearance != TextAreaPart
        && appearance != ListboxPart)
        return true;

    // Now paint the text field.
    p.drawLineEdit(r, /*sunken = */isPressed(o));
    return false;
}

void RenderThemeQtMobile::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    RenderThemeQt::adjustMenuListStyle(selector, style, e);
    style->setPaddingLeft(Length(menuListPadding, Fixed));
}

void RenderThemeQtMobile::setPopupPadding(RenderStyle* style) const
{
    const int paddingLeft = 4;
    const int paddingRight = style->width().isFixed() || style->width().isPercent() ? 5 : 8;

    style->setPaddingLeft(Length(paddingLeft, Fixed));
    style->setPaddingRight(Length(paddingRight + buttonIconSize, Fixed));

    style->setPaddingTop(Length(2, Fixed));
    style->setPaddingBottom(Length(2, Fixed));
}


bool RenderThemeQtMobile::paintMenuList(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    StylePainterMobile p(this, i);
    if (!p.isValid())
        return true;

    const QPoint topLeft = r.location();
    p.painter->translate(topLeft);
    const QRect rect(QPoint(0, 0), r.size());

    p.drawComboBox(rect, checkMultiple(o));
    p.painter->translate(-topLeft);
    return false;
}

bool RenderThemeQtMobile::paintMenuListButton(RenderObject* o, const PaintInfo& i,
                                        const IntRect& r)
{
    StylePainterMobile p(this, i);
    if (!p.isValid())
        return true;

    p.drawComboBox(r, checkMultiple(o));

    return false;
}

#if ENABLE(PROGRESS_TAG)
double RenderThemeQtMobile::animationDurationForProgressBar(RenderProgress* renderProgress) const
{
    notImplemented();
    return 0.;
}

bool RenderThemeQtMobile::paintProgressBar(RenderObject* o, const PaintInfo& pi, const IntRect& r)
{
    notImplemented();
    return true;
}
#endif

bool RenderThemeQtMobile::paintSliderTrack(RenderObject* o, const PaintInfo& pi,
                                     const IntRect& r)
{
    notImplemented();
    return true;
}

bool RenderThemeQtMobile::paintSliderThumb(RenderObject* o, const PaintInfo& pi,
                                     const IntRect& r)
{
    // We've already painted it in paintSliderTrack(), no need to do anything here.
    notImplemented();
    return true;
}

bool RenderThemeQtMobile::checkMultiple(RenderObject* o) const
{
    HTMLSelectElement* select = o ? static_cast<HTMLSelectElement*>(o->node()) : 0;
    return select ? select->multiple() : false;
}

void RenderThemeQtMobile::setPaletteFromPageClientIfExists(QPalette& palette) const
{
    static QPalette lightGrayPalette(Qt::lightGray);
    palette = lightGrayPalette;
    return;
}

void RenderThemeQtMobile::adjustSliderThumbSize(RenderStyle* style) const
{
    const ControlPart part = style->appearance();
    if (part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart) {
        style->setWidth(Length(sliderWidth, Fixed));
        style->setHeight(Length(sliderHeight, Fixed));
    } else
        RenderThemeQt::adjustSliderThumbSize(style);
}

}

// vim: ts=4 sw=4 et
