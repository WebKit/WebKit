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
#include "ChromeClientQt.h"
#include "Color.h"
#include "Document.h"
#include "Font.h"
#include "FontSelector.h"
#include "GraphicsContext.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "NotImplemented.h"
#include "Page.h"
#include "RenderBox.h"
#include "RenderTheme.h"
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
#include <QWidget>


namespace WebCore {

using namespace HTMLNames;


StylePainter::StylePainter(const RenderObject::PaintInfo& paintInfo)
{
    init(paintInfo.context ? paintInfo.context : 0);
}

StylePainter::StylePainter(GraphicsContext* context)
{
    init(context);
}

void StylePainter::init(GraphicsContext* context)
{
    painter = static_cast<QPainter*>(context->platformContext());
    widget = 0;
    QPaintDevice* dev = 0;
    if (painter)
        dev = painter->device();
    if (dev && dev->devType() == QInternal::Widget)
        widget = static_cast<QWidget*>(dev);
    style = (widget ? widget->style() : QApplication::style());

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
    , m_fallbackStyle(0)
{
    QPushButton button;
    button.setAttribute(Qt::WA_MacSmallSize);
    QFont defaultButtonFont = QApplication::font(&button);
    QFontInfo fontInfo(defaultButtonFont);
    m_buttonFontFamily = defaultButtonFont.family();
#ifdef Q_WS_MAC
    m_buttonFontPixelSize = fontInfo.pixelSize();
#endif
}

RenderThemeQt::~RenderThemeQt()
{
    delete m_fallbackStyle;
}

// for some widget painting, we need to fallback to Windows style
QStyle* RenderThemeQt::fallbackStyle()
{
    if (!m_fallbackStyle)
        m_fallbackStyle = QStyleFactory::create(QLatin1String("windows"));

    if (!m_fallbackStyle)
        m_fallbackStyle = QApplication::style();

    return m_fallbackStyle;
}

QStyle* RenderThemeQt::qStyle() const
{
    if (m_page) {
        ChromeClientQt* client = static_cast<ChromeClientQt*>(m_page->chrome()->client());

        if (!client->m_webPage)
            return QApplication::style();

        if (QWidget* view = client->m_webPage->view())
            return view->style();
    }

    return QApplication::style();
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

static int findFrameLineWidth(QStyle* style)
{
    QLineEdit lineEdit;
    QStyleOptionFrameV2 opt;
    return style->pixelMetric(QStyle::PM_DefaultFrameWidth, &opt, &lineEdit);
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
    // If the width and height are both specified, then we have nothing to do.
    if (!renderStyle->width().isIntrinsicOrAuto() && !renderStyle->height().isAuto())
        return;

    QSize size(0, 0);
    const QFontMetrics fm(renderStyle->font().font());
    QStyle* style = qStyle();

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
    case TextFieldPart: {
        const int verticalMargin = 1;
        const int horizontalMargin = 2;
        int h = qMax(fm.lineSpacing(), 14) + 2*verticalMargin;
        int w = fm.width(QLatin1Char('x')) * 17 + 2*horizontalMargin;
        QStyleOptionFrameV2 opt;
        opt.lineWidth = findFrameLineWidth(style);
        QSize sz = style->sizeFromContents(QStyle::CT_LineEdit,
                                           &opt,
                                           QSize(w, h).expandedTo(QApplication::globalStrut()),
                                           0);
        size.setHeight(sz.height());

        renderStyle->setPaddingLeft(Length(opt.lineWidth, Fixed));
        renderStyle->setPaddingRight(Length(opt.lineWidth, Fixed));
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

    // Height is locked to auto.
    style->setHeight(Length(Auto));

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
    StylePainter p(i);
    if (!p.isValid())
       return true;

    QStyleOptionButton option;
    if (p.widget)
       option.initFrom(p.widget);

    option.rect = r;
    option.state |= QStyle::State_Small;

    ControlPart appearance = applyTheme(option, o);
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
    StylePainter p(i);
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
    ControlPart appearance = applyTheme(panel, o);
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
    StylePainter p(i);
    if (!p.isValid())
        return true;

    QStyleOptionComboBox opt;
    if (p.widget)
        opt.initFrom(p.widget);
    applyTheme(opt, o);

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
    // WORKAROUND because html.css specifies -webkit-border-radius for <select> so we override it here
    // see also http://bugs.webkit.org/show_bug.cgi?id=18399
    style->resetBorderRadius();

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
    StylePainter p(i);
    if (!p.isValid())
        return true;

    QStyleOptionComboBox option;
    if (p.widget)
        option.initFrom(p.widget);
    applyTheme(option, o);
    option.rect = r;

    // for drawing the combo box arrow, rely only on the fallback style
    p.style = fallbackStyle();
    option.subControls = QStyle::SC_ComboBoxArrow;
    p.drawComplexControl(QStyle::CC_ComboBox, option);

    return false;
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
        return true;
    default: // No for all others...
        return false;
    }
}

ControlPart RenderThemeQt::applyTheme(QStyleOption& option, RenderObject* o) const
{
    // Default bits: no focus, no mouse over
    option.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver);

    if (!isEnabled(o))
        option.state &= ~QStyle::State_Enabled;

    if (isReadOnlyControl(o))
        // Readonly is supported on textfields.
        option.state |= QStyle::State_ReadOnly;

    if (supportsFocus(o->style()->appearance()) && isFocused(o)) {
        option.state |= QStyle::State_HasFocus;
        option.state |= QStyle::State_KeyboardFocusChange;
    }

    if (isHovered(o))
        option.state |= QStyle::State_MouseOver;

    option.direction = Qt::LeftToRight;
    if (o->style() && o->style()->direction() == WebCore::RTL)
        option.direction = Qt::RightToLeft;

    ControlPart result = o->style()->appearance();

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
        else if (result == PushButtonPart)
            option.state |= QStyle::State_Raised;
        break;
    }
    }

    if (result == RadioPart || result == CheckboxPart)
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

void RenderThemeQt::paintMediaBackground(QPainter* painter, const IntRect& r) const
{
    painter->setPen(Qt::NoPen);
    static QColor transparentBlack(0, 0, 0, 100);
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

    StylePainter p(paintInfo);
    if (!p.isValid())
        return true;

    p.painter->setRenderHint(QPainter::Antialiasing, true);

    paintMediaBackground(p.painter, r);

    WorldMatrixTransformer transformer(p.painter, o, r);
    const QPointF speakerPolygon[6] = { QPointF(20, 30), QPointF(50, 30), QPointF(80, 0),
            QPointF(80, 100), QPointF(50, 70), QPointF(20, 70)};

    p.painter->setBrush(getMediaControlForegroundColor(o));
    p.painter->drawPolygon(speakerPolygon, 6);

    if (mediaElement->muted()) {
        p.painter->setPen(Qt::red);
        p.painter->drawLine(0, 100, 100, 0);
    }

    return false;
}

bool RenderThemeQt::paintMediaPlayButton(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    HTMLMediaElement* mediaElement = getMediaElementFromRenderObject(o);
    if (!mediaElement)
        return false;

    StylePainter p(paintInfo);
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

bool RenderThemeQt::paintMediaSliderTrack(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    HTMLMediaElement* mediaElement = getMediaElementFromRenderObject(o);
    if (!mediaElement)
        return false;

    StylePainter p(paintInfo);
    if (!p.isValid())
        return true;

    p.painter->setRenderHint(QPainter::Antialiasing, true);

    paintMediaBackground(p.painter, r);

    if (MediaPlayer* player = mediaElement->player()) {
        if (player->totalBytesKnown()) {
            float percentLoaded = static_cast<float>(player->bytesLoaded()) / player->totalBytes();

            WorldMatrixTransformer transformer(p.painter, o, r);
            p.painter->setBrush(getMediaControlForegroundColor());
            p.painter->drawRect(0, 37, 100 * percentLoaded, 26);
        }
    }

    return false;
}

bool RenderThemeQt::paintMediaSliderThumb(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    HTMLMediaElement* mediaElement = getMediaElementFromRenderObject(o->parent());
    if (!mediaElement)
        return false;

    StylePainter p(paintInfo);
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
    if (o->style()->appearance() == MediaSliderThumbPart) {
        RenderStyle* parentStyle = o->parent()->style();
        Q_ASSERT(parentStyle);

        int parentHeight = parentStyle->height().value();
        o->style()->setWidth(Length(parentHeight / 3, Fixed));
        o->style()->setHeight(Length(parentHeight, Fixed));
    }
}

double RenderThemeQt::caretBlinkInterval() const
{
    return  QApplication::cursorFlashTime() / 1000.0 / 2.0;
}

}

// vim: ts=4 sw=4 et
