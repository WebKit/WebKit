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
#include "RenderThemeQStyle.h"

#include "CSSFontSelector.h"
#include "CSSStyleSelector.h"
#include "CSSValueKeywords.h"
#include "Chrome.h"
#include "ChromeClientQt.h"
#include "Color.h"
#include "Document.h"
#include "Font.h"
#include "FontSelector.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "LocalizedStrings.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PaintInfo.h"
#include "QWebPageClient.h"
#include "QtStyleOptionWebComboBox.h"
#include "RenderBox.h"
#if ENABLE(PROGRESS_TAG)
#include "RenderProgress.h"
#endif
#include "RenderSlider.h"
#include "ScrollbarThemeQt.h"
#include "SliderThumbElement.h"
#include "UserAgentStyleSheets.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QFontMetrics>
#include <QLineEdit>
#include <QMacStyle>
#include <QPainter>
#ifndef QT_NO_STYLE_PLASTIQUE
#include <QPlastiqueStyle>
#endif
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

inline static void initStyleOption(QWidget *widget, QStyleOption& option)
{
    if (widget)
        option.initFrom(widget);
    else {
        /*
          If a widget is not directly available for rendering, we fallback to default
          value for an active widget.
         */
        option.state = QStyle::State_Active | QStyle::State_Enabled;
    }
}


QSharedPointer<StylePainter> RenderThemeQStyle::getStylePainter(const PaintInfo& paintInfo)
{
    return QSharedPointer<StylePainter>(new StylePainterQStyle(this, paintInfo));
}

StylePainterQStyle::StylePainterQStyle(RenderThemeQStyle* theme, const PaintInfo& paintInfo)
    : StylePainter(theme, paintInfo)
{
    init(paintInfo.context ? paintInfo.context : 0, theme->qStyle());
}

StylePainterQStyle::StylePainterQStyle(ScrollbarThemeQt* theme, GraphicsContext* context)
    : StylePainter()
{
    init(context, theme->style());
}

void StylePainterQStyle::init(GraphicsContext* context, QStyle* themeStyle)
{
    painter = static_cast<QPainter*>(context->platformContext());
    widget = 0;
    QPaintDevice* dev = 0;
    if (painter)
        dev = painter->device();
    if (dev && dev->devType() == QInternal::Widget)
        widget = static_cast<QWidget*>(dev);
    style = themeStyle;

    StylePainter::init(context);
}

PassRefPtr<RenderTheme> RenderThemeQStyle::create(Page* page)
{
    return adoptRef(new RenderThemeQStyle(page));
}

RenderThemeQStyle::RenderThemeQStyle(Page* page)
    : RenderThemeQt(page)
#ifndef QT_NO_LINEEDIT
    , m_lineEdit(0)
#endif
{
    QPushButton button;
    QFont defaultButtonFont = QApplication::font(&button);
    m_buttonFontFamily = defaultButtonFont.family();
#ifdef Q_WS_MAC
    button.setAttribute(Qt::WA_MacSmallSize);
    QFontInfo fontInfo(defaultButtonFont);
    m_buttonFontPixelSize = fontInfo.pixelSize();
#endif

    m_fallbackStyle = QStyleFactory::create(QLatin1String("windows"));
}

RenderThemeQStyle::~RenderThemeQStyle()
{
    delete m_fallbackStyle;
#ifndef QT_NO_LINEEDIT
    delete m_lineEdit;
#endif
}


// for some widget painting, we need to fallback to Windows style
QStyle* RenderThemeQStyle::fallbackStyle() const
{
    return (m_fallbackStyle) ? m_fallbackStyle : QApplication::style();
}

QStyle* RenderThemeQStyle::qStyle() const
{
    if (m_page) {
        QWebPageClient* pageClient = m_page->chrome()->client()->platformPageClient();

        if (pageClient)
            return pageClient->style();
    }

    return QApplication::style();
}

int RenderThemeQStyle::findFrameLineWidth(QStyle* style) const
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

QRect RenderThemeQStyle::inflateButtonRect(const QRect& originalRect) const
{
    QStyleOptionButton option;
    option.state |= QStyle::State_Small;
    option.rect = originalRect;

    QRect layoutRect = qStyle()->subElementRect(QStyle::SE_PushButtonLayoutItem, &option, 0);
    if (!layoutRect.isNull()) {
        int paddingLeft = layoutRect.left() - originalRect.left();
        int paddingRight = originalRect.right() - layoutRect.right();
        int paddingTop = layoutRect.top() - originalRect.top();
        int paddingBottom = originalRect.bottom() - layoutRect.bottom();

        return originalRect.adjusted(-paddingLeft, -paddingTop, paddingRight, paddingBottom);
    }
    return originalRect;
}

void RenderThemeQStyle::computeSizeBasedOnStyle(RenderStyle* renderStyle) const
{
    QSize size(0, 0);
    const QFontMetrics fm(renderStyle->font().font());
    QStyle* style = qStyle();

    switch (renderStyle->appearance()) {
    case TextAreaPart:
    case SearchFieldPart:
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
        checkBoxWidth *= renderStyle->effectiveZoom();
        size = QSize(checkBoxWidth, checkBoxWidth);
        break;
    }
    case RadioPart: {
        QStyleOption styleOption;
        styleOption.state |= QStyle::State_Small;
        int radioWidth = style->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth, &styleOption);
        radioWidth *= renderStyle->effectiveZoom();
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
        renderStyle->setMinWidth(Length(size.width(), Fixed));
    if (renderStyle->height().isAuto() && size.height() > 0)
        renderStyle->setMinHeight(Length(size.height(), Fixed));
}



void RenderThemeQStyle::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element*) const
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

void RenderThemeQStyle::setButtonPadding(RenderStyle* style) const
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
    int paddingTop = buttonMargin;
    int paddingBottom = buttonMargin;

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

bool RenderThemeQStyle::paintButton(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    StylePainterQStyle p(this, i);
    if (!p.isValid())
       return true;

    QStyleOptionButton option;
    initStyleOption(p.widget, option);
    option.rect = r;
    option.state |= QStyle::State_Small;

    ControlPart appearance = initializeCommonQStyleOptions(option, o);
    if (appearance == PushButtonPart || appearance == ButtonPart) {
        option.rect = inflateButtonRect(option.rect);
        p.drawControl(QStyle::CE_PushButton, option);
    } else if (appearance == RadioPart)
       p.drawControl(QStyle::CE_RadioButton, option);
    else if (appearance == CheckboxPart)
       p.drawControl(QStyle::CE_CheckBox, option);

    return false;
}

bool RenderThemeQStyle::paintTextField(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    StylePainterQStyle p(this, i);
    if (!p.isValid())
        return true;

    QStyleOptionFrameV2 panel;
    initStyleOption(p.widget, panel);
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

void RenderThemeQStyle::adjustTextAreaStyle(CSSStyleSelector* selector, RenderStyle* style, Element* element) const
{
    adjustTextFieldStyle(selector, style, element);
}

bool RenderThemeQStyle::paintTextArea(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeQStyle::setPopupPadding(RenderStyle* style) const
{
    const int paddingLeft = 4;
    const int paddingRight = style->width().isFixed() || style->width().isPercent() ? 5 : 8;

    style->setPaddingLeft(Length(paddingLeft, Fixed));

    QStyleOptionComboBox opt;
    int w = qStyle()->pixelMetric(QStyle::PM_ButtonIconSize, &opt, 0);
    style->setPaddingRight(Length(paddingRight + w, Fixed));

    style->setPaddingTop(Length(2, Fixed));
    style->setPaddingBottom(Length(2, Fixed));
}


bool RenderThemeQStyle::paintMenuList(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    StylePainterQStyle p(this, i);
    if (!p.isValid())
        return true;

    QtStyleOptionWebComboBox opt(o);
    initStyleOption(p.widget, opt);
    initializeCommonQStyleOptions(opt, o);

    IntRect rect = r;

#if defined(Q_WS_MAC) && !defined(QT_NO_STYLE_MAC)
    // QMacStyle makes the combo boxes a little bit smaller to leave space for the focus rect.
    // Because of it, the combo button is drawn at a point to the left of where it was expect to be and may end up
    // overlapped with the text. This will force QMacStyle to draw the combo box with the expected width.
    if (qobject_cast<QMacStyle*>(p.style))
        rect.inflateX(3);
#endif

    const QPoint topLeft = rect.location();
    p.painter->translate(topLeft);
    opt.rect.moveTo(QPoint(0, 0));
    opt.rect.setSize(rect.size());

    p.drawComplexControl(QStyle::CC_ComboBox, opt);
    p.painter->translate(-topLeft);
    return false;
}

void RenderThemeQStyle::adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    // WORKAROUND because html.css specifies -webkit-border-radius for <select> so we override it here
    // see also http://bugs.webkit.org/show_bug.cgi?id=18399
    style->resetBorderRadius();

    RenderThemeQt::adjustMenuListButtonStyle(selector, style, e);
}

bool RenderThemeQStyle::paintMenuListButton(RenderObject* o, const PaintInfo& i,
                                        const IntRect& r)
{
    StylePainterQStyle p(this, i);
    if (!p.isValid())
        return true;

    QtStyleOptionWebComboBox option(o);
    initStyleOption(p.widget, option);
    initializeCommonQStyleOptions(option, o);
    option.rect = r;

    // for drawing the combo box arrow, rely only on the fallback style
    p.style = fallbackStyle();
    option.subControls = QStyle::SC_ComboBoxArrow;
    p.drawComplexControl(QStyle::CC_ComboBox, option);

    return false;
}

#if ENABLE(PROGRESS_TAG)
double RenderThemeQStyle::animationDurationForProgressBar(RenderProgress* renderProgress) const
{
    if (renderProgress->position() >= 0)
        return 0;

    QStyleOptionProgressBarV2 option;
    option.rect.setSize(renderProgress->size());
    // FIXME: Until http://bugreports.qt.nokia.com/browse/QTBUG-9171 is fixed,
    // we simulate one square animating across the progress bar.
    return (option.rect.width() / qStyle()->pixelMetric(QStyle::PM_ProgressBarChunkWidth, &option)) * animationRepeatIntervalForProgressBar(renderProgress);
}

bool RenderThemeQStyle::paintProgressBar(RenderObject* o, const PaintInfo& pi, const IntRect& r)
{
    if (!o->isProgress())
        return true;

    StylePainterQStyle p(this, pi);
    if (!p.isValid())
       return true;

    QStyleOptionProgressBarV2 option;
    initStyleOption(p.widget, option);
    initializeCommonQStyleOptions(option, o);

    RenderProgress* renderProgress = toRenderProgress(o);
    option.rect = r;
    option.maximum = std::numeric_limits<int>::max();
    option.minimum = 0;
    option.progress = (renderProgress->position() * std::numeric_limits<int>::max());

    const QPoint topLeft = r.location();
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

bool RenderThemeQStyle::paintSliderTrack(RenderObject* o, const PaintInfo& pi,
                                     const IntRect& r)
{
    StylePainterQStyle p(this, pi);
    if (!p.isValid())
        return true;

    const QPoint topLeft = r.location();
    p.painter->translate(topLeft);

    QStyleOptionSlider option;
    initStyleOption(p.widget, option);
    option.subControls = QStyle::SC_SliderGroove;
    ControlPart appearance = initializeCommonQStyleOptions(option, o);
    option.rect = r;
    option.rect.moveTo(QPoint(0, 0));
    if (appearance == SliderVerticalPart)
        option.orientation = Qt::Vertical;
    if (isPressed(o))
        option.state |= QStyle::State_Sunken;

    // some styles need this to show a highlight on one side of the groove
    HTMLInputElement* slider = o->node()->toInputElement();
    if (slider) {
        option.upsideDown = (appearance == SliderHorizontalPart) && !o->style()->isLeftToRightDirection();
        // Use the width as a multiplier in case the slider values are <= 1
        const int width = r.width() > 0 ? r.width() : 100;
        option.maximum = slider->maximum() * width;
        option.minimum = slider->minimum() * width;
        if (!option.upsideDown)
            option.sliderPosition = slider->valueAsNumber() * width;
        else
            option.sliderPosition = option.minimum + option.maximum - slider->valueAsNumber() * width;
    }

    p.drawComplexControl(QStyle::CC_Slider, option);

    if (option.state & QStyle::State_HasFocus) {
        QStyleOptionFocusRect focusOption;
        focusOption.rect = r;
        p.drawPrimitive(QStyle::PE_FrameFocusRect, focusOption);
    }
    p.painter->translate(-topLeft);
    return false;
}

void RenderThemeQStyle::adjustSliderTrackStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(nullptr);
}

bool RenderThemeQStyle::paintSliderThumb(RenderObject* o, const PaintInfo& pi,
                                     const IntRect& r)
{
    StylePainterQStyle p(this, pi);
    if (!p.isValid())
        return true;

    const QPoint topLeft = r.location();
    p.painter->translate(topLeft);

    QStyleOptionSlider option;
    initStyleOption(p.widget, option);
    option.subControls = QStyle::SC_SliderHandle;
    ControlPart appearance = initializeCommonQStyleOptions(option, o);
    option.rect = r;
    option.rect.moveTo(QPoint(0, 0));
    if (appearance == SliderThumbVerticalPart)
        option.orientation = Qt::Vertical;
    if (isPressed(o)) {
        option.activeSubControls = QStyle::SC_SliderHandle;
        option.state |= QStyle::State_Sunken;
    }

    p.drawComplexControl(QStyle::CC_Slider, option);
    p.painter->translate(-topLeft);
    return false;
}

void RenderThemeQStyle::adjustSliderThumbStyle(CSSStyleSelector* selector, RenderStyle* style, Element* element) const
{
    RenderTheme::adjustSliderThumbStyle(selector, style, element);
    style->setBoxShadow(nullptr);
}

bool RenderThemeQStyle::paintSearchField(RenderObject* o, const PaintInfo& pi,
                                     const IntRect& r)
{
    return paintTextField(o, pi, r);
}

void RenderThemeQStyle::adjustSearchFieldDecorationStyle(CSSStyleSelector* selector, RenderStyle* style,
                                                     Element* e) const
{
    notImplemented();
    RenderTheme::adjustSearchFieldDecorationStyle(selector, style, e);
}

bool RenderThemeQStyle::paintSearchFieldDecoration(RenderObject* o, const PaintInfo& pi,
                                               const IntRect& r)
{
    notImplemented();
    return RenderTheme::paintSearchFieldDecoration(o, pi, r);
}

void RenderThemeQStyle::adjustSearchFieldResultsDecorationStyle(CSSStyleSelector* selector, RenderStyle* style,
                                                            Element* e) const
{
    notImplemented();
    RenderTheme::adjustSearchFieldResultsDecorationStyle(selector, style, e);
}

bool RenderThemeQStyle::paintSearchFieldResultsDecoration(RenderObject* o, const PaintInfo& pi,
                                                      const IntRect& r)
{
    notImplemented();
    return RenderTheme::paintSearchFieldResultsDecoration(o, pi, r);
}

#ifndef QT_NO_SPINBOX

bool RenderThemeQStyle::paintInnerSpinButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& rect)
{
    StylePainterQStyle p(this, paintInfo);
    if (!p.isValid())
       return true;

    QStyleOptionSpinBox option;
    initStyleOption(p.widget, option);
    option.subControls = QStyle::SC_SpinBoxUp | QStyle::SC_SpinBoxDown;
    if (!isReadOnlyControl(o)) {
        if (isEnabled(o))
            option.stepEnabled = QAbstractSpinBox::StepUpEnabled | QAbstractSpinBox::StepDownEnabled;
        if (isPressed(o)) {
            option.state |= QStyle::State_Sunken;
            if (isSpinUpButtonPartPressed(o))
                option.activeSubControls = QStyle::SC_SpinBoxUp;
            else
                option.activeSubControls = QStyle::SC_SpinBoxDown;
        }
    }
    // Render the spin buttons for LTR or RTL accordingly.
    option.direction = o->style()->isLeftToRightDirection() ? Qt::LeftToRight : Qt::RightToLeft;

    IntRect buttonRect = rect;
    // Default to moving the buttons a little bit within the editor frame.
    int inflateX = -2;
    int inflateY = -2;
#if defined(Q_WS_MAC) && !defined(QT_NO_STYLE_MAC)
    // QMacStyle will position the aqua buttons flush to the right.
    // This will move them more within the control for better style, a la
    // Chromium look & feel.
    if (qobject_cast<QMacStyle*>(p.style)) {
        inflateX = -4;
        // Render mini aqua spin buttons for QMacStyle to fit nicely into
        // the editor area, like Chromium.
        option.state |= QStyle::State_Mini;
    }
#endif
#if !defined(QT_NO_STYLE_PLASTIQUE)
    // QPlastiqueStyle looks best when the spin buttons are flush with the frame's edge.
    if (qobject_cast<QPlastiqueStyle*>(p.style)) {
        inflateX = 0;
        inflateY = 0;
    }
#endif

    buttonRect.inflateX(inflateX);
    buttonRect.inflateY(inflateY);
    option.rect = buttonRect;

    p.drawComplexControl(QStyle::CC_SpinBox, option);
    return false;
}
#endif

ControlPart RenderThemeQStyle::initializeCommonQStyleOptions(QStyleOption& option, RenderObject* o) const
{
    // Default bits: no focus, no mouse over
    option.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver);

    if (isReadOnlyControl(o))
        // Readonly is supported on textfields.
        option.state |= QStyle::State_ReadOnly;

    option.direction = Qt::LeftToRight;

    if (isHovered(o))
        option.state |= QStyle::State_MouseOver;

    setPaletteFromPageClientIfExists(option.palette);

    if (!isEnabled(o)) {
        option.palette.setCurrentColorGroup(QPalette::Disabled);
        option.state &= ~QStyle::State_Enabled;
    }

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

void RenderThemeQStyle::adjustSliderThumbSize(RenderStyle* style) const
{
    const ControlPart part = style->appearance();
    if (part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart) {
        QStyleOptionSlider option;
        if (part == SliderThumbVerticalPart)
            option.orientation = Qt::Vertical;

        QStyle* qstyle = qStyle();

        int length = qstyle->pixelMetric(QStyle::PM_SliderLength, &option);
        int thickness = qstyle->pixelMetric(QStyle::PM_SliderThickness, &option);
        if (option.orientation == Qt::Vertical) {
            style->setWidth(Length(thickness, Fixed));
            style->setHeight(Length(length, Fixed));
        } else {
            style->setWidth(Length(length, Fixed));
            style->setHeight(Length(thickness, Fixed));
        }
    } else
        RenderThemeQt::adjustSliderThumbSize(style);
}

}

// vim: ts=4 sw=4 et
