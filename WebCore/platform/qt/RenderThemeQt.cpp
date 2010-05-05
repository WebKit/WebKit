/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 *               2006 Dirk Mueller <mueller@kde.org>
 *               2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Holger Hans Peter Freyther
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
#include "RenderThemeQt.h"

#include "CSSStyleSelector.h"
#include "CSSStyleSheet.h"
#include "Chrome.h"
#include "ChromeClientQt.h"
#include "Color.h"
#include "Document.h"
#include "Font.h"
#include "FontSelector.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#if USE(QT_MOBILE_THEME)
#include "Maemo5Webstyle.h"
#endif
#include "NotImplemented.h"
#include "Page.h"
#include "QtStyleOptionWebComboBox.h"
#include "QWebPageClient.h"
#include "RenderBox.h"
#if ENABLE(PROGRESS_TAG)
#include "RenderProgress.h"
#endif
#include "RenderSlider.h"
#include "RenderTheme.h"
#include "TimeRanges.h"
#include "ScrollbarThemeQt.h"
#include "UserAgentStyleSheets.h"
#include "qwebpage.h"


#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QFile>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QStyleFactory>
#include <QStyleOptionButton>
#include <QStyleOptionFrameV2>
#if ENABLE(PROGRESS_TAG)
#include <QStyleOptionProgressBarV2>
#endif
#include <QStyleOptionSlider>
#include <QWidget>


namespace WebCore {

using namespace HTMLNames;


StylePainter::StylePainter(RenderThemeQt* theme, const RenderObject::PaintInfo& paintInfo)
{
    init(paintInfo.context ? paintInfo.context : 0, theme->qStyle());
}

StylePainter::StylePainter(ScrollbarThemeQt* theme, GraphicsContext* context)
{
    init(context, theme->style());
}

void StylePainter::init(GraphicsContext* context, QStyle* themeStyle)
{
    painter = static_cast<QPainter*>(context->platformContext());
    widget = 0;
    QPaintDevice* dev = 0;
    if (painter)
        dev = painter->device();
    if (dev && dev->devType() == QInternal::Widget)
        widget = static_cast<QWidget*>(dev);
    style = themeStyle;

    if (painter) {
        // the styles often assume being called with a pristine painter where no brush is set,
        // so reset it manually
        oldBrush = painter->brush();
        painter->setBrush(Qt::NoBrush);

        // painting the widget with anti-aliasing will make it blurry
        // disable it here and restore it later
        oldAntialiasing = painter->testRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::Antialiasing, false);
    }
}

StylePainter::~StylePainter()
{
    if (painter) {
        painter->setBrush(oldBrush);
        painter->setRenderHints(QPainter::Antialiasing, oldAntialiasing);
    }
}

PassRefPtr<RenderTheme> RenderThemeQt::create(Page* page)
{
    return adoptRef(new RenderThemeQt(page));
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    if (page)
        return RenderThemeQt::create(page);

    static RenderTheme* fallback = RenderThemeQt::create(0).releaseRef();
    return fallback;
}

RenderThemeQt::RenderThemeQt(Page* page)
    : RenderTheme()
    , m_page(page)
    , m_lineEdit(0)
{
    QPushButton button;
    button.setAttribute(Qt::WA_MacSmallSize);
    QFont defaultButtonFont = QApplication::font(&button);
    QFontInfo fontInfo(defaultButtonFont);
    m_buttonFontFamily = defaultButtonFont.family();
#ifdef Q_WS_MAC
    m_buttonFontPixelSize = fontInfo.pixelSize();
#endif

#if USE(QT_MOBILE_THEME)
    m_fallbackStyle = new Maemo5WebStyle;
#else
    m_fallbackStyle = QStyleFactory::create(QLatin1String("windows"));
#endif
}

RenderThemeQt::~RenderThemeQt()
{
    delete m_fallbackStyle;
#ifndef QT_NO_LINEEDIT
    delete m_lineEdit;
#endif
}

#if USE(QT_MOBILE_THEME)
bool RenderThemeQt::isControlStyled(const RenderStyle* style, const BorderData& border, const FillLayer& fill, const Color& backgroundColor) const
{
    switch (style->appearance()) {
    case PushButtonPart:
    case ButtonPart:
    case MenulistPart:
    case TextFieldPart:
    case TextAreaPart:
        return true;
    case CheckboxPart:
    case RadioPart:
        return false;
    default:
        return RenderTheme::isControlStyled(style, border, fill, backgroundColor);
    }
}

int RenderThemeQt::popupInternalPaddingBottom(RenderStyle* style) const
{
    return 1;
}
#endif

// for some widget painting, we need to fallback to Windows style
QStyle* RenderThemeQt::fallbackStyle() const
{
    return (m_fallbackStyle) ? m_fallbackStyle : QApplication::style();
}

QStyle* RenderThemeQt::qStyle() const
{
#if USE(QT_MOBILE_THEME)
    return fallbackStyle();
#endif

    if (m_page) {
        QWebPageClient* pageClient = m_page->chrome()->client()->platformPageClient();

        if (pageClient)
            return pageClient->style();
    }

    return QApplication::style();
}

String RenderThemeQt::extraDefaultStyleSheet()
{
    String result = RenderTheme::extraDefaultStyleSheet();
#if ENABLE(NO_LISTBOX_RENDERING)
    result += String(themeQtNoListboxesUserAgentStyleSheet, sizeof(themeQtNoListboxesUserAgentStyleSheet));
#endif
#if USE(QT_MOBILE_THEME)
    result += String(themeQtMaemo5UserAgentStyleSheet, sizeof(themeQtMaemo5UserAgentStyleSheet));
#endif
    return result;
}

bool RenderThemeQt::supportsHover(const RenderStyle*) const
{
    return true;
}

bool RenderThemeQt::supportsFocusRing(const RenderStyle*) const
{
    return true; // Qt provides this through the style
}

int RenderThemeQt::baselinePosition(const RenderObject* o) const
{
    if (!o->isBox())
        return 0;

    if (o->style()->appearance() == CheckboxPart || o->style()->appearance() == RadioPart)
        return toRenderBox(o)->marginTop() + toRenderBox(o)->height() - 2; // Same as in old khtml
    return RenderTheme::baselinePosition(o);
}

bool RenderThemeQt::controlSupportsTints(const RenderObject* o) const
{
    if (!isEnabled(o))
        return false;

    // Checkboxes only have tint when checked.
    if (o->style()->appearance() == CheckboxPart)
        return isChecked(o);

    // For now assume other controls have tint if enabled.
    return true;
}

bool RenderThemeQt::supportsControlTints() const
{
    return true;
}

int RenderThemeQt::findFrameLineWidth(QStyle* style) const
{
#ifndef QT_NO_LINEEDIT
    if (!m_lineEdit)
        m_lineEdit = new QLineEdit();
#endif

    QStyleOptionFrameV2 opt;
    QWidget* widget = 0;
#ifndef QT_NO_LINEEDIT
    widget = m_lineEdit;
#endif
    return style->pixelMetric(QStyle::PM_DefaultFrameWidth, &opt, widget);
}

static QRect inflateButtonRect(const QRect& originalRect, QStyle* style)
{
    QStyleOptionButton option;
    option.state |= QStyle::State_Small;
    option.rect = originalRect;

    QRect layoutRect = style->subElementRect(QStyle::SE_PushButtonLayoutItem, &option, 0);
    if (!layoutRect.isNull()) {
        int paddingLeft = layoutRect.left() - originalRect.left();
        int paddingRight = originalRect.right() - layoutRect.right();
        int paddingTop = layoutRect.top() - originalRect.top();
        int paddingBottom = originalRect.bottom() - layoutRect.bottom();

        return originalRect.adjusted(-paddingLeft, -paddingTop, paddingRight, paddingBottom);
    }
    return originalRect;
}

void RenderThemeQt::adjustRepaintRect(const RenderObject* o, IntRect& rect)
{
    switch (o->style()->appearance()) {
    case CheckboxPart:
        break;
    case RadioPart:
        break;
    case PushButtonPart:
    case ButtonPart: {
        QRect inflatedRect = inflateButtonRect(rect, qStyle());
        rect = IntRect(inflatedRect.x(), inflatedRect.y(), inflatedRect.width(), inflatedRect.height());
        break;
    }
    case MenulistPart:
        break;
    default:
        break;
    }
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

void RenderThemeQt::systemFont(int, FontDescription&) const
{
    // no-op
}

int RenderThemeQt::minimumMenuListSize(RenderStyle*) const
{
    const QFontMetrics &fm = QApplication::fontMetrics();
    return 7 * fm.width(QLatin1Char('x'));
}

void RenderThemeQt::computeSizeBasedOnStyle(RenderStyle* renderStyle) const
{
    QSize size(0, 0);
    const QFontMetrics fm(renderStyle->font().font());
    QStyle* style = qStyle();

    switch (renderStyle->appearance()) {
    case TextAreaPart:
    case TextFieldPart: {
        int padding = findFrameLineWidth(style);

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
        QStyleOption styleOption;
        styleOption.state |= QStyle::State_Small;
        int checkBoxWidth = style->pixelMetric(QStyle::PM_IndicatorWidth, &styleOption);
        size = QSize(checkBoxWidth, checkBoxWidth);
        break;
    }
    case RadioPart: {
        QStyleOption styleOption;
        styleOption.state |= QStyle::State_Small;
        int radioWidth = style->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth, &styleOption);
        size = QSize(radioWidth, radioWidth);
        break;
    }
    case PushButtonPart:
    case ButtonPart: {
        QStyleOptionButton styleOption;
        styleOption.state |= QStyle::State_Small;
        QSize contentSize = fm.size(Qt::TextShowMnemonic, QString::fromLatin1("X"));
        QSize pushButtonSize = style->sizeFromContents(QStyle::CT_PushButton,
                                                       &styleOption, contentSize, 0);
        styleOption.rect = QRect(0, 0, pushButtonSize.width(), pushButtonSize.height());
        QRect layoutRect = style->subElementRect(QStyle::SE_PushButtonLayoutItem,
                                                 &styleOption, 0);

        // If the style supports layout rects we use that, and  compensate accordingly
        // in paintButton() below.
        if (!layoutRect.isNull())
            size.setHeight(layoutRect.height());
        else
            size.setHeight(pushButtonSize.height());

        break;
    }
    case MenulistPart: {
        QStyleOptionComboBox styleOption;
        styleOption.state |= QStyle::State_Small;
        int contentHeight = qMax(fm.lineSpacing(), 14) + 2;
        QSize menuListSize = style->sizeFromContents(QStyle::CT_ComboBox,
                                                     &styleOption, QSize(0, contentHeight), 0);
        size.setHeight(menuListSize.height());
        break;
    }
    default:
        break;
    }

    // FIXME: Check is flawed, since it doesn't take min-width/max-width into account.
    if (renderStyle->width().isIntrinsicOrAuto() && size.width() > 0)
        renderStyle->setWidth(Length(size.width(), Fixed));
    if (renderStyle->height().isAuto() && size.height() > 0)
        renderStyle->setHeight(Length(size.height(), Fixed));
}

void RenderThemeQt::setCheckboxSize(RenderStyle* style) const
{
    computeSizeBasedOnStyle(style);
}

bool RenderThemeQt::paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintButton(o, i, r);
}

void RenderThemeQt::setRadioSize(RenderStyle* style) const
{
    computeSizeBasedOnStyle(style);
}

bool RenderThemeQt::paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintButton(o, i, r);
}

void RenderThemeQt::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element*) const
{
    // Ditch the border.
    style->resetBorder();

#ifdef Q_WS_MAC
    if (style->appearance() == PushButtonPart) {
        // The Mac ports ignore the specified height for <input type="button"> elements
        // unless a border and/or background CSS property is also specified.
        style->setHeight(Length(Auto));
    }
#endif

    // White-space is locked to pre
    style->setWhiteSpace(PRE);

    FontDescription fontDescription = style->fontDescription();
    fontDescription.setIsAbsoluteSize(true);

#ifdef Q_WS_MAC // Use fixed font size and family on Mac (like Safari does)
    fontDescription.setSpecifiedSize(m_buttonFontPixelSize);
    fontDescription.setComputedSize(m_buttonFontPixelSize);
#else
    fontDescription.setSpecifiedSize(style->fontSize());
    fontDescription.setComputedSize(style->fontSize());
#endif

    FontFamily fontFamily;
    fontFamily.setFamily(m_buttonFontFamily);
    fontDescription.setFamily(fontFamily);
    style->setFontDescription(fontDescription);
    style->font().update(selector->fontSelector());
    style->setLineHeight(RenderStyle::initialLineHeight());

    setButtonSize(style);
    setButtonPadding(style);
}

void RenderThemeQt::setButtonSize(RenderStyle* style) const
{
    computeSizeBasedOnStyle(style);
}

void RenderThemeQt::setButtonPadding(RenderStyle* style) const
{
    QStyleOptionButton styleOption;
    styleOption.state |= QStyle::State_Small;

    // Fake a button rect here, since we're just computing deltas
    QRect originalRect = QRect(0, 0, 100, 30);
    styleOption.rect = originalRect;

    // Default padding is based on the button margin pixel metric
    int buttonMargin = qStyle()->pixelMetric(QStyle::PM_ButtonMargin, &styleOption, 0);
    int paddingLeft = buttonMargin;
    int paddingRight = buttonMargin;
    int paddingTop = 1;
    int paddingBottom = 0;

    // Then check if the style uses layout margins
    QRect layoutRect = qStyle()->subElementRect(QStyle::SE_PushButtonLayoutItem,
                                                &styleOption, 0);
    if (!layoutRect.isNull()) {
        QRect contentsRect = qStyle()->subElementRect(QStyle::SE_PushButtonContents,
                                                      &styleOption, 0);
        paddingLeft = contentsRect.left() - layoutRect.left();
        paddingRight = layoutRect.right() - contentsRect.right();
        paddingTop = contentsRect.top() - layoutRect.top();

        // Can't use this right now because we don't have the baseline to compensate
        // paddingBottom = layoutRect.bottom() - contentsRect.bottom();
    }

    style->setPaddingLeft(Length(paddingLeft, Fixed));
    style->setPaddingRight(Length(paddingRight, Fixed));
    style->setPaddingTop(Length(paddingTop, Fixed));
    style->setPaddingBottom(Length(paddingBottom, Fixed));
}

bool RenderThemeQt::paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    StylePainter p(this, i);
    if (!p.isValid())
       return true;

    QStyleOptionButton option;
    if (p.widget)
       option.initFrom(p.widget);

    option.rect = r;
    option.state |= QStyle::State_Small;

    ControlPart appearance = initializeCommonQStyleOptions(option, o);
    if (appearance == PushButtonPart || appearance == ButtonPart) {
        option.rect = inflateButtonRect(option.rect, qStyle());
        p.drawControl(QStyle::CE_PushButton, option);
    } else if (appearance == RadioPart)
       p.drawControl(QStyle::CE_RadioButton, option);
    else if (appearance == CheckboxPart)
       p.drawControl(QStyle::CE_CheckBox, option);

    return false;
}

void RenderThemeQt::adjustTextFieldStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setBackgroundColor(Color::transparent);
    style->resetBorder();
    style->resetPadding();
    computeSizeBasedOnStyle(style);
}

bool RenderThemeQt::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    StylePainter p(this, i);
    if (!p.isValid())
        return true;

    QStyleOptionFrameV2 panel;
    if (p.widget)
        panel.initFrom(p.widget);

    panel.rect = r;
    panel.lineWidth = findFrameLineWidth(qStyle());
    panel.state |= QStyle::State_Sunken;
    panel.features = QStyleOptionFrameV2::None;

    // Get the correct theme data for a text field
    ControlPart appearance = initializeCommonQStyleOptions(panel, o);
    if (appearance != TextFieldPart
        && appearance != SearchFieldPart
        && appearance != TextAreaPart
        && appearance != ListboxPart)
        return true;

    // Now paint the text field.
    p.drawPrimitive(QStyle::PE_PanelLineEdit, panel);

    return false;
}

void RenderThemeQt::adjustTextAreaStyle(CSSStyleSelector* selector, RenderStyle* style, Element* element) const
{
    adjustTextFieldStyle(selector, style, element);
}

bool RenderThemeQt::paintTextArea(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeQt::adjustMenuListStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->resetBorder();

    // Height is locked to auto.
    style->setHeight(Length(Auto));

    // White-space is locked to pre
    style->setWhiteSpace(PRE);

    computeSizeBasedOnStyle(style);

    // Add in the padding that we'd like to use.
    setPopupPadding(style);
}

void RenderThemeQt::setPopupPadding(RenderStyle* style) const
{
    const int padding = 8;
    style->setPaddingLeft(Length(padding, Fixed));

    QStyleOptionComboBox opt;
    int w = qStyle()->pixelMetric(QStyle::PM_ButtonIconSize, &opt, 0);
    style->setPaddingRight(Length(padding + w, Fixed));

    style->setPaddingTop(Length(2, Fixed));
    style->setPaddingBottom(Length(0, Fixed));
}


bool RenderThemeQt::paintMenuList(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    StylePainter p(this, i);
    if (!p.isValid())
        return true;

    QtStyleOptionWebComboBox opt(o);
    if (p.widget)
        opt.initFrom(p.widget);
    initializeCommonQStyleOptions(opt, o);

    const QPoint topLeft = r.topLeft();
    p.painter->translate(topLeft);
    opt.rect.moveTo(QPoint(0, 0));
    opt.rect.setSize(r.size());

    p.drawComplexControl(QStyle::CC_ComboBox, opt);
    p.painter->translate(-topLeft);
    return false;
}

void RenderThemeQt::adjustMenuListButtonStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
#if USE(QT_MOBILE_THEME)
    // Mobile theme uses border radius.
#else
    // WORKAROUND because html.css specifies -webkit-border-radius for <select> so we override it here
    // see also http://bugs.webkit.org/show_bug.cgi?id=18399
    style->resetBorderRadius();
#endif

    // Height is locked to auto.
    style->setHeight(Length(Auto));

    // White-space is locked to pre
    style->setWhiteSpace(PRE);

    computeSizeBasedOnStyle(style);

    // Add in the padding that we'd like to use.
    setPopupPadding(style);
}

bool RenderThemeQt::paintMenuListButton(RenderObject* o, const RenderObject::PaintInfo& i,
                                        const IntRect& r)
{
    StylePainter p(this, i);
    if (!p.isValid())
        return true;

    QtStyleOptionWebComboBox option(o);
    if (p.widget)
        option.initFrom(p.widget);
    initializeCommonQStyleOptions(option, o);
    option.rect = r;

    // for drawing the combo box arrow, rely only on the fallback style
    p.style = fallbackStyle();
    option.subControls = QStyle::SC_ComboBoxArrow;
    p.drawComplexControl(QStyle::CC_ComboBox, option);

    return false;
}

#if ENABLE(PROGRESS_TAG)
double RenderThemeQt::animationRepeatIntervalForProgressBar(RenderProgress* renderProgress) const
{
    if (renderProgress->position() >= 0)
        return 0;

    // FIXME: Use hard-coded value until http://bugreports.qt.nokia.com/browse/QTBUG-9171 is fixed.
    // Use the value from windows style which is 10 fps.
    return 0.1;
}

double RenderThemeQt::animationDurationForProgressBar(RenderProgress* renderProgress) const
{
    if (renderProgress->position() >= 0)
        return 0;

    QStyleOptionProgressBarV2 option;
    option.rect.setSize(renderProgress->size());
    // FIXME: Until http://bugreports.qt.nokia.com/browse/QTBUG-9171 is fixed,
    // we simulate one square animating across the progress bar.
    return (option.rect.width() / qStyle()->pixelMetric(QStyle::PM_ProgressBarChunkWidth, &option)) * animationRepeatIntervalForProgressBar(renderProgress);
}

void RenderThemeQt::adjustProgressBarStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(0);
}

bool RenderThemeQt::paintProgressBar(RenderObject* o, const RenderObject::PaintInfo& pi, const IntRect& r)
{
    StylePainter p(this, pi);
    if (!p.isValid())
       return true;

    QStyleOptionProgressBarV2 option;
    if (p.widget)
       option.initFrom(p.widget);
    initializeCommonQStyleOptions(option, o);

    RenderProgress* renderProgress = toRenderProgress(o);
    option.rect = r;
    option.maximum = std::numeric_limits<int>::max();
    option.minimum = 0;
    option.progress = (renderProgress->position() * std::numeric_limits<int>::max());

    const QPoint topLeft = r.topLeft();
    p.painter->translate(topLeft);
    option.rect.moveTo(QPoint(0, 0));
    option.rect.setSize(r.size());

    if (option.progress < 0) {
        // FIXME: Until http://bugreports.qt.nokia.com/browse/QTBUG-9171 is fixed,
        // we simulate one square animating across the progress bar.
        p.drawControl(QStyle::CE_ProgressBarGroove, option);
        int chunkWidth = qStyle()->pixelMetric(QStyle::PM_ProgressBarChunkWidth, &option);
        QColor color = (option.palette.highlight() == option.palette.background()) ? option.palette.color(QPalette::Active, QPalette::Highlight) : option.palette.color(QPalette::Highlight);
        if (renderProgress->style()->direction() == RTL)
            p.painter->fillRect(option.rect.right() - chunkWidth  - renderProgress->animationProgress() * option.rect.width(), 0, chunkWidth, option.rect.height(), color);
        else
            p.painter->fillRect(renderProgress->animationProgress() * option.rect.width(), 0, chunkWidth, option.rect.height(), color);
    } else
        p.drawControl(QStyle::CE_ProgressBar, option);

    p.painter->translate(-topLeft);

    return false;
}
#endif

bool RenderThemeQt::paintSliderTrack(RenderObject* o, const RenderObject::PaintInfo& pi,
                                     const IntRect& r)
{
    StylePainter p(this, pi);
    if (!p.isValid())
       return true;

    QStyleOptionSlider option;
    if (p.widget)
       option.initFrom(p.widget);
    ControlPart appearance = initializeCommonQStyleOptions(option, o);

    RenderSlider* renderSlider = toRenderSlider(o);
    IntRect thumbRect = renderSlider->thumbRect();

    option.rect = r;

    int value;
    if (appearance == SliderVerticalPart) {
        option.maximum = r.height() - thumbRect.height();
        value = thumbRect.y();
    } else {
        option.maximum = r.width() - thumbRect.width();
        value = thumbRect.x();
    }

    value = QStyle::sliderValueFromPosition(0, option.maximum, value, option.maximum);

    option.sliderValue = value;
    option.sliderPosition = value;
    if (appearance == SliderVerticalPart)
        option.orientation = Qt::Vertical;

    if (renderSlider->inDragMode()) {
        option.activeSubControls = QStyle::SC_SliderHandle;
        option.state |= QStyle::State_Sunken;
    }

    const QPoint topLeft = r.topLeft();
    p.painter->translate(topLeft);
    option.rect.moveTo(QPoint(0, 0));
    option.rect.setSize(r.size());

    p.drawComplexControl(QStyle::CC_Slider, option);
    p.painter->translate(-topLeft);

    return false;
}

void RenderThemeQt::adjustSliderTrackStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(0);
}

bool RenderThemeQt::paintSliderThumb(RenderObject* o, const RenderObject::PaintInfo& pi,
                                     const IntRect& r)
{
    // We've already painted it in paintSliderTrack(), no need to do anything here.
    return false;
}

void RenderThemeQt::adjustSliderThumbStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(0);
}

bool RenderThemeQt::paintSearchField(RenderObject* o, const RenderObject::PaintInfo& pi,
                                     const IntRect& r)
{
    return true;
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

bool RenderThemeQt::supportsFocus(ControlPart appearance) const
{
    switch (appearance) {
    case PushButtonPart:
    case ButtonPart:
    case TextFieldPart:
    case TextAreaPart:
    case ListboxPart:
    case MenulistPart:
    case RadioPart:
    case CheckboxPart:
    case SliderHorizontalPart:
    case SliderVerticalPart:
        return true;
    default: // No for all others...
        return false;
    }
}

void RenderThemeQt::setPaletteFromPageClientIfExists(QPalette& palette) const
{
#if USE(QT_MOBILE_THEME)
    static QPalette lightGrayPalette(Qt::lightGray);
    palette = lightGrayPalette;
    return;
#endif
    // If the webview has a custom palette, use it
    if (!m_page)
        return;
    Chrome* chrome = m_page->chrome();
    if (!chrome)
        return;
    ChromeClient* chromeClient = chrome->client();
    if (!chromeClient)
        return;
    QWebPageClient* pageClient = chromeClient->platformPageClient();
    if (!pageClient)
        return;
    palette = pageClient->palette();
}

ControlPart RenderThemeQt::initializeCommonQStyleOptions(QStyleOption& option, RenderObject* o) const
{
    // Default bits: no focus, no mouse over
    option.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver);

    if (!isEnabled(o))
        option.state &= ~QStyle::State_Enabled;

    if (isReadOnlyControl(o))
        // Readonly is supported on textfields.
        option.state |= QStyle::State_ReadOnly;

    option.direction = Qt::LeftToRight;

    if (isHovered(o))
        option.state |= QStyle::State_MouseOver;

    setPaletteFromPageClientIfExists(option.palette);
    RenderStyle* style = o->style();
    if (!style)
        return NoControlPart;

    ControlPart result = style->appearance();
    if (supportsFocus(result) && isFocused(o)) {
        option.state |= QStyle::State_HasFocus;
        option.state |= QStyle::State_KeyboardFocusChange;
    }

    if (style->direction() == WebCore::RTL)
        option.direction = Qt::RightToLeft;

    switch (result) {
    case PushButtonPart:
    case SquareButtonPart:
    case ButtonPart:
    case ButtonBevelPart:
    case ListItemPart:
    case MenulistButtonPart:
    case SearchFieldResultsButtonPart:
    case SearchFieldCancelButtonPart: {
        if (isPressed(o))
            option.state |= QStyle::State_Sunken;
        else if (result == PushButtonPart || result == ButtonPart)
            option.state |= QStyle::State_Raised;
        break;
    }
    case RadioPart:
    case CheckboxPart:
        option.state |= (isChecked(o) ? QStyle::State_On : QStyle::State_Off);
    }

    return result;
}

#if ENABLE(VIDEO)

String RenderThemeQt::extraMediaControlsStyleSheet()
{
    return String(mediaControlsQtUserAgentStyleSheet, sizeof(mediaControlsQtUserAgentStyleSheet));
}

// Helper class to transform the painter's world matrix to the object's content area, scaled to 0,0,100,100
class WorldMatrixTransformer {
public:
    WorldMatrixTransformer(QPainter* painter, RenderObject* renderObject, const IntRect& r) : m_painter(painter)
    {
        RenderStyle* style = renderObject->style();
        m_originalTransform = m_painter->transform();
        m_painter->translate(r.x() + style->paddingLeft().value(), r.y() + style->paddingTop().value());
        m_painter->scale((r.width() - style->paddingLeft().value() - style->paddingRight().value()) / 100.0,
             (r.height() - style->paddingTop().value() - style->paddingBottom().value()) / 100.0);
    }

    ~WorldMatrixTransformer() { m_painter->setTransform(m_originalTransform); }

private:
    QPainter* m_painter;
    QTransform m_originalTransform;
};

HTMLMediaElement* RenderThemeQt::getMediaElementFromRenderObject(RenderObject* o) const
{
    Node* node = o->node();
    Node* mediaNode = node ? node->shadowAncestorNode() : 0;
    if (!mediaNode || (!mediaNode->hasTagName(videoTag) && !mediaNode->hasTagName(audioTag)))
        return 0;

    return static_cast<HTMLMediaElement*>(mediaNode);
}

double RenderThemeQt::mediaControlsBaselineOpacity() const
{
    return 0.4;
}

void RenderThemeQt::paintMediaBackground(QPainter* painter, const IntRect& r) const
{
    painter->setPen(Qt::NoPen);
    static QColor transparentBlack(0, 0, 0, mediaControlsBaselineOpacity() * 255);
    painter->setBrush(transparentBlack);
    painter->drawRoundedRect(r.x(), r.y(), r.width(), r.height(), 5.0, 5.0);
}

QColor RenderThemeQt::getMediaControlForegroundColor(RenderObject* o) const
{
    QColor fgColor = platformActiveSelectionBackgroundColor();
    if (o && o->node()->active())
        fgColor = fgColor.lighter();
    return fgColor;
}

bool RenderThemeQt::paintMediaFullscreenButton(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    return RenderTheme::paintMediaFullscreenButton(o, paintInfo, r);
}

bool RenderThemeQt::paintMediaMuteButton(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    HTMLMediaElement* mediaElement = getMediaElementFromRenderObject(o);
    if (!mediaElement)
        return false;

    StylePainter p(this, paintInfo);
    if (!p.isValid())
        return true;

    p.painter->setRenderHint(QPainter::Antialiasing, true);

    paintMediaBackground(p.painter, r);

    WorldMatrixTransformer transformer(p.painter, o, r);
    const QPointF speakerPolygon[6] = { QPointF(20, 30), QPointF(50, 30), QPointF(80, 0),
            QPointF(80, 100), QPointF(50, 70), QPointF(20, 70)};

    p.painter->setBrush(mediaElement->muted() ? Qt::darkRed : getMediaControlForegroundColor(o));
    p.painter->drawPolygon(speakerPolygon, 6);

    return false;
}

bool RenderThemeQt::paintMediaPlayButton(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    HTMLMediaElement* mediaElement = getMediaElementFromRenderObject(o);
    if (!mediaElement)
        return false;

    StylePainter p(this, paintInfo);
    if (!p.isValid())
        return true;

    p.painter->setRenderHint(QPainter::Antialiasing, true);

    paintMediaBackground(p.painter, r);

    WorldMatrixTransformer transformer(p.painter, o, r);
    p.painter->setBrush(getMediaControlForegroundColor(o));
    if (mediaElement->canPlay()) {
        const QPointF playPolygon[3] = { QPointF(0, 0), QPointF(100, 50), QPointF(0, 100)};
        p.painter->drawPolygon(playPolygon, 3);
    } else {
        p.painter->drawRect(0, 0, 30, 100);
        p.painter->drawRect(70, 0, 30, 100);
    }

    return false;
}

bool RenderThemeQt::paintMediaSeekBackButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&)
{
    // We don't want to paint this at the moment.
    return false;
}

bool RenderThemeQt::paintMediaSeekForwardButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&)
{
    // We don't want to paint this at the moment.
    return false;
}

bool RenderThemeQt::paintMediaCurrentTime(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    StylePainter p(this, paintInfo);
    if (!p.isValid())
        return true;

    p.painter->setRenderHint(QPainter::Antialiasing, true);
    paintMediaBackground(p.painter, r);

    return false;
}

String RenderThemeQt::formatMediaControlsCurrentTime(float currentTime, float duration) const
{
    return formatMediaControlsTime(currentTime) + " / " + formatMediaControlsTime(duration);
}

String RenderThemeQt::formatMediaControlsRemainingTime(float currentTime, float duration) const
{
    return String();
}

bool RenderThemeQt::paintMediaVolumeSliderTrack(RenderObject *o, const RenderObject::PaintInfo &paintInfo, const IntRect &r)
{
    StylePainter p(this, paintInfo);
    if (!p.isValid())
        return true;

    p.painter->setRenderHint(QPainter::Antialiasing, true);

    paintMediaBackground(p.painter, r);

    if (!o->isSlider())
        return false;

    IntRect b = toRenderBox(o)->contentBoxRect();

    // Position the outer rectangle
    int top = r.y() + b.y();
    int left = r.x() + b.x();
    int width = b.width();
    int height = b.height();

    // Get the scale color from the page client
    QPalette pal = QApplication::palette();
    setPaletteFromPageClientIfExists(pal);
    const QColor highlightText = pal.brush(QPalette::Active, QPalette::HighlightedText).color();
    const QColor scaleColor(highlightText.red(), highlightText.green(), highlightText.blue(), mediaControlsBaselineOpacity() * 255);

    // Draw the outer rectangle
    p.painter->setBrush(scaleColor);
    p.painter->drawRect(left, top, width, height);

    if (!o->node() || !o->node()->hasTagName(inputTag))
        return false;

    HTMLInputElement* slider = static_cast<HTMLInputElement*>(o->node());

    // Position the inner rectangle
    height = height * slider->valueAsNumber();
    top += b.height() - height;

    // Draw the inner rectangle
    p.painter->setPen(Qt::NoPen);
    p.painter->setBrush(getMediaControlForegroundColor(o));
    p.painter->drawRect(left, top, width, height);

    return false;
}

bool RenderThemeQt::paintMediaVolumeSliderThumb(RenderObject *o, const RenderObject::PaintInfo &paintInfo, const IntRect &r)
{
    StylePainter p(this, paintInfo);
    if (!p.isValid())
        return true;

    // Nothing to draw here, this is all done in the track
    return false;
}

bool RenderThemeQt::paintMediaSliderTrack(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    HTMLMediaElement* mediaElement = getMediaElementFromRenderObject(o);
    if (!mediaElement)
        return false;

    StylePainter p(this, paintInfo);
    if (!p.isValid())
        return true;

    p.painter->setRenderHint(QPainter::Antialiasing, true);

    paintMediaBackground(p.painter, r);

#if QT_VERSION >= 0x040700
    if (MediaPlayer* player = mediaElement->player()) {
        // Get the buffered parts of the media
        PassRefPtr<TimeRanges> buffered = player->buffered();
        if (buffered->length() > 0 && player->duration() < std::numeric_limits<float>::infinity()) {
            // Set the transform and brush
            WorldMatrixTransformer transformer(p.painter, o, r);
            p.painter->setBrush(getMediaControlForegroundColor());

            // Paint each buffered section
            ExceptionCode ex;
            for (int i = 0; i < buffered->length(); i++) {
                float startX = (buffered->start(i, ex) / player->duration()) * 100;
                float width = ((buffered->end(i, ex) / player->duration()) * 100) - startX;
                p.painter->drawRect(startX, 37, width, 26);
            }
        }
    }
#endif

    return false;
}

bool RenderThemeQt::paintMediaSliderThumb(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    StylePainter p(this, paintInfo);
    if (!p.isValid())
        return true;

    p.painter->setRenderHint(QPainter::Antialiasing, true);

    p.painter->setPen(Qt::NoPen);
    p.painter->setBrush(getMediaControlForegroundColor(o));
    p.painter->drawRect(r.x(), r.y(), r.width(), r.height());

    return false;
}
#endif

void RenderThemeQt::adjustSliderThumbSize(RenderObject* o) const
{
    ControlPart part = o->style()->appearance();

    if (part == MediaSliderThumbPart) {
        RenderStyle* parentStyle = o->parent()->style();
        Q_ASSERT(parentStyle);

        int parentHeight = parentStyle->height().value();
        o->style()->setWidth(Length(parentHeight / 3, Fixed));
        o->style()->setHeight(Length(parentHeight, Fixed));
    } else if (part == MediaVolumeSliderThumbPart) {
        RenderStyle* parentStyle = o->parent()->style();
        Q_ASSERT(parentStyle);

        int parentWidth = parentStyle->width().value();
        o->style()->setHeight(Length(parentWidth / 3, Fixed));
        o->style()->setWidth(Length(parentWidth, Fixed));
    } else if (part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart) {
        QStyleOptionSlider option;
        if (part == SliderThumbVerticalPart)
            option.orientation = Qt::Vertical;

        QStyle* style = qStyle();

        int width = style->pixelMetric(QStyle::PM_SliderLength, &option);
        int height = style->pixelMetric(QStyle::PM_SliderThickness, &option);
        o->style()->setWidth(Length(width, Fixed));
        o->style()->setHeight(Length(height, Fixed));
    }
}

double RenderThemeQt::caretBlinkInterval() const
{
    return  QApplication::cursorFlashTime() / 1000.0 / 2.0;
}

}

// vim: ts=4 sw=4 et
