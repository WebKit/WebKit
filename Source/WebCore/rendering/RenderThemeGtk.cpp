/*
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2009 Kenneth Rohde Christiansen
 * Copyright (C) 2010 Igalia S.L.
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
#include "RenderThemeGtk.h"

#include "CSSValueKeywords.h"
#include "FileList.h"
#include "FloatRoundedRect.h"
#include "FontDescription.h"
#include "GRefPtrGtk.h"
#include "GUniquePtrGtk.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "LocalizedStrings.h"
#include "MediaControlElements.h"
#include "Page.h"
#include "PaintInfo.h"
#include "PlatformContextCairo.h"
#include "RenderBox.h"
#include "RenderObject.h"
#include "RenderProgress.h"
#include "RenderThemeWidget.h"
#include "ScrollbarThemeGtk.h"
#include "StringTruncator.h"
#include "TimeRanges.h"
#include "UserAgentScripts.h"
#include "UserAgentStyleSheets.h"
#include <cmath>
#include <gdk/gdk.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <wtf/FileSystem.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

RenderTheme& RenderTheme::singleton()
{
    static NeverDestroyed<RenderThemeGtk> theme;
    return theme;
}

static double getScreenDPI()
{
    // FIXME: Really this should be the widget's screen.
    GdkScreen* screen = gdk_screen_get_default();
    if (!screen)
        return 96; // Default to 96 DPI.

    float dpi = gdk_screen_get_resolution(screen);
    if (dpi <= 0)
        return 96;
    return dpi;
}

void RenderThemeGtk::updateCachedSystemFontDescription(CSSValueID, FontCascadeDescription& fontDescription) const
{
    GtkSettings* settings = gtk_settings_get_default();
    if (!settings)
        return;

    // This will be a font selection string like "Sans 10" so we cannot use it as the family name.
    GUniqueOutPtr<gchar> fontName;
    g_object_get(settings, "gtk-font-name", &fontName.outPtr(), nullptr);
    if (!fontName || !fontName.get()[0])
        return;

    PangoFontDescription* pangoDescription = pango_font_description_from_string(fontName.get());
    if (!pangoDescription)
        return;

    fontDescription.setOneFamily(pango_font_description_get_family(pangoDescription));

    int size = pango_font_description_get_size(pangoDescription) / PANGO_SCALE;
    // If the size of the font is in points, we need to convert it to pixels.
    if (!pango_font_description_get_size_is_absolute(pangoDescription))
        size = size * (getScreenDPI() / 72.0);

    fontDescription.setSpecifiedSize(size);
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setWeight(normalWeightValue());
    fontDescription.setItalic(FontSelectionValue());
    pango_font_description_free(pangoDescription);
}

#if ENABLE(DATALIST_ELEMENT)
IntSize RenderThemeGtk::sliderTickSize() const
{
    // FIXME: We need to set this to the size of one tick mark.
    return IntSize(0, 0);
}

int RenderThemeGtk::sliderTickOffsetFromTrackCenter() const
{
    // FIXME: We need to set this to the position of the tick marks.
    return 0;
}
#endif

enum RenderThemePart {
    Entry,
    EntrySelection,
    EntryIconLeft,
    EntryIconRight,
    Button,
    CheckButton,
    RadioButton,
    ComboBox,
    ComboBoxButton,
    ComboBoxArrow,
    Scale,
    ScaleTrough,
    ScaleSlider,
    ProgressBar,
    ProgressBarTrough,
    ProgressBarProgress,
    ListBox,
    SpinButton,
    SpinButtonUpButton,
    SpinButtonDownButton,
#if ENABLE(VIDEO)
    MediaButton,
#endif
    Window,
};

#if ENABLE(VIDEO)
static bool nodeHasPseudo(Node& node, const char* pseudo)
{
    return is<Element>(node) && downcast<Element>(node).pseudo() == pseudo;
}

static bool nodeHasClass(Node* node, const char* className)
{
    if (!is<Element>(*node))
        return false;

    Element& element = downcast<Element>(*node);

    if (!element.hasClass())
        return false;

    return element.classNames().contains(className);
}
#endif // ENABLE(VIDEO)

RenderThemeGtk::~RenderThemeGtk() = default;

static bool supportsFocus(ControlPart appearance)
{
    switch (appearance) {
    case PushButtonPart:
    case ButtonPart:
    case TextFieldPart:
    case TextAreaPart:
    case SearchFieldPart:
    case MenulistPart:
    case RadioPart:
    case CheckboxPart:
    case SliderHorizontalPart:
    case SliderVerticalPart:
        return true;
    default:
        return false;
    }
}

bool RenderThemeGtk::supportsFocusRing(const RenderStyle& style) const
{
    return supportsFocus(style.appearance());
}

bool RenderThemeGtk::controlSupportsTints(const RenderObject& o) const
{
    return isEnabled(o);
}

int RenderThemeGtk::baselinePosition(const RenderBox& box) const
{
    // FIXME: This strategy is possibly incorrect for the GTK+ port.
    if (box.style().appearance() == CheckboxPart || box.style().appearance() == RadioPart)
        return box.marginTop() + box.height() - 2;
    return RenderTheme::baselinePosition(box);
}

void RenderThemeGtk::adjustRepaintRect(const RenderObject&, FloatRect&)
{
}
static GtkStateFlags themePartStateFlags(const RenderThemeGtk& theme, RenderThemePart themePart, const RenderObject& renderObject)
{
    unsigned stateFlags = 0;
    switch (renderObject.style().direction()) {
    case TextDirection::RTL:
        stateFlags |= GTK_STATE_FLAG_DIR_RTL;
        break;
    case TextDirection::LTR:
        stateFlags |= GTK_STATE_FLAG_DIR_LTR;
        break;
    }

    if (!theme.isEnabled(renderObject) || (themePart == Entry && theme.isReadOnlyControl(renderObject)))
        stateFlags |= GTK_STATE_FLAG_INSENSITIVE;
    else {
        if (theme.isHovered(renderObject))
            stateFlags |= GTK_STATE_FLAG_PRELIGHT;
        if (theme.isFocused(renderObject))
            stateFlags |= GTK_STATE_FLAG_FOCUSED;
    }

    switch (themePart) {
    case CheckButton:
    case RadioButton:
        if (theme.isChecked(renderObject))
            stateFlags |= GTK_STATE_FLAG_CHECKED;
        if (theme.isIndeterminate(renderObject))
            stateFlags |= GTK_STATE_FLAG_INCONSISTENT;
        if (theme.isPressed(renderObject))
            stateFlags |= GTK_STATE_FLAG_SELECTED;
        break;
    case Button:
    case ComboBoxButton:
    case ScaleSlider:
    case EntryIconLeft:
    case EntryIconRight:
#if ENABLE(VIDEO)
    case MediaButton:
#endif
        if (theme.isPressed(renderObject))
            stateFlags |= GTK_STATE_FLAG_ACTIVE;
        break;
    case SpinButtonUpButton:
        if (theme.isPressed(renderObject) && theme.isSpinUpButtonPartPressed(renderObject))
            stateFlags |= GTK_STATE_FLAG_ACTIVE;
        if (theme.isHovered(renderObject) && !theme.isSpinUpButtonPartHovered(renderObject))
            stateFlags &= ~GTK_STATE_FLAG_PRELIGHT;
        break;
    case SpinButtonDownButton:
        if (theme.isPressed(renderObject) && !theme.isSpinUpButtonPartPressed(renderObject))
            stateFlags |= GTK_STATE_FLAG_ACTIVE;
        if (theme.isHovered(renderObject) && theme.isSpinUpButtonPartHovered(renderObject))
            stateFlags &= ~GTK_STATE_FLAG_PRELIGHT;
        break;
    default:
        break;
    }

    return static_cast<GtkStateFlags>(stateFlags);
}

void RenderThemeGtk::adjustButtonStyle(RenderStyle& style, const Element*) const
{
    // Some layout tests check explicitly that buttons ignore line-height.
    if (style.appearance() == PushButtonPart)
        style.setLineHeight(RenderStyle::initialLineHeight());
}

static void shrinkToMinimumSizeAndCenterRectangle(FloatRect& rect, const IntSize& minSize)
{
    if (rect.width() > minSize.width()) {
        rect.inflateX(-(rect.width() - minSize.width()) / 2);
        rect.setWidth(minSize.width()); // In case rect.width() was equal to minSize.width() + 1.
    }

    if (rect.height() > minSize.height()) {
        rect.inflateY(-(rect.height() - minSize.height()) / 2);
        rect.setHeight(minSize.height()); // In case rect.height() was equal to minSize.height() + 1.
    }
}

static void setToggleSize(RenderThemePart themePart, RenderStyle& style)
{
    ASSERT(themePart == CheckButton || themePart == RadioButton);

    // The width and height are both specified, so we shouldn't change them.
    if (!style.width().isIntrinsicOrAuto() && !style.height().isAuto())
        return;

    auto& toggleWidget = static_cast<RenderThemeToggleButton&>(RenderThemeWidget::getOrCreate(themePart == CheckButton ? RenderThemeWidget::Type::CheckButton : RenderThemeWidget::Type::RadioButton));
    toggleWidget.button().setState(GTK_STATE_FLAG_NORMAL);
    toggleWidget.toggle().setState(GTK_STATE_FLAG_NORMAL);
    IntSize preferredSize = toggleWidget.button().preferredSize();
    preferredSize = preferredSize.expandedTo(toggleWidget.toggle().preferredSize());

    if (style.width().isIntrinsicOrAuto())
        style.setWidth(Length(preferredSize.width(), Fixed));

    if (style.height().isAuto())
        style.setHeight(Length(preferredSize.height(), Fixed));
}

static void paintToggle(const RenderThemeGtk* theme, RenderThemePart themePart, const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& fullRect)
{
    ASSERT(themePart == CheckButton || themePart == RadioButton);

    auto& toggleWidget = static_cast<RenderThemeToggleButton&>(RenderThemeWidget::getOrCreate(themePart == CheckButton ? RenderThemeWidget::Type::CheckButton : RenderThemeWidget::Type::RadioButton));
    auto toggleState = themePartStateFlags(*theme, themePart, renderObject);
    toggleWidget.button().setState(toggleState);
    toggleWidget.toggle().setState(toggleState);

    FloatRect rect = fullRect;
    // Some themes do not render large toggle buttons properly, so we simply
    // shrink the rectangle back down to the default size and then center it
    // in the full toggle button region. The reason for not simply forcing toggle
    // buttons to be a smaller size is that we don't want to break site layouts.
    IntSize preferredSize = toggleWidget.button().preferredSize();
    preferredSize = preferredSize.expandedTo(toggleWidget.toggle().preferredSize());
    shrinkToMinimumSizeAndCenterRectangle(rect, preferredSize);
    toggleWidget.button().render(paintInfo.context().platformContext()->cr(), rect);
    toggleWidget.toggle().render(paintInfo.context().platformContext()->cr(), rect);

    if (theme->isFocused(renderObject))
        toggleWidget.button().renderFocus(paintInfo.context().platformContext()->cr(), rect);
}

void RenderThemeGtk::setCheckboxSize(RenderStyle& style) const
{
    setToggleSize(CheckButton, style);
}

bool RenderThemeGtk::paintCheckbox(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    paintToggle(this, CheckButton, renderObject, paintInfo, rect);
    return false;
}

void RenderThemeGtk::setRadioSize(RenderStyle& style) const
{
    setToggleSize(RadioButton, style);
}

bool RenderThemeGtk::paintRadio(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    paintToggle(this, RadioButton, renderObject, paintInfo, rect);
    return false;
}

bool RenderThemeGtk::paintButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    auto& buttonWidget = static_cast<RenderThemeButton&>(RenderThemeWidget::getOrCreate(isDefault(renderObject) ? RenderThemeWidget::Type::ButtonDefault : RenderThemeWidget::Type::Button));
    buttonWidget.button().setState(themePartStateFlags(*this, Button, renderObject));
    buttonWidget.button().render(paintInfo.context().platformContext()->cr(), rect);
    if (isFocused(renderObject))
        buttonWidget.button().renderFocus(paintInfo.context().platformContext()->cr(), rect);
    return false;
}

static Color menuListColor(const Element* element)
{
    auto& comboWidget = static_cast<RenderThemeComboBox&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::ComboBox));
    GtkStateFlags state = element->isDisabledFormControl() ? GTK_STATE_FLAG_INSENSITIVE : GTK_STATE_FLAG_NORMAL;
    comboWidget.comboBox().setState(state);
    comboWidget.button().setState(state);
    return comboWidget.button().color();
}

void RenderThemeGtk::adjustMenuListStyle(RenderStyle& style, const Element* element) const
{
    // The tests check explicitly that select menu buttons ignore line height.
    style.setLineHeight(RenderStyle::initialLineHeight());

    // We cannot give a proper rendering when border radius is active, unfortunately.
    style.resetBorderRadius();

    if (element)
        style.setColor(menuListColor(element));
}

void RenderThemeGtk::adjustMenuListButtonStyle(RenderStyle& style, const Element* e) const
{
    adjustMenuListStyle(style, e);
}

/*
 * GtkComboBox gadgets tree
 *
 * combobox
 * ├── box.linked
 * │   ╰── button.combo
 * │       ╰── box
 * │           ├── cellview
 * │           ╰── arrow
 * ╰── window.popup
 */
LengthBox RenderThemeGtk::popupInternalPaddingBox(const RenderStyle& style) const
{
    if (style.appearance() == NoControlPart)
        return LengthBox(0);

    auto& comboWidget = static_cast<RenderThemeComboBox&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::ComboBox));
    comboWidget.comboBox().setState(GTK_STATE_FLAG_NORMAL);
    comboWidget.button().setState(GTK_STATE_FLAG_NORMAL);
    comboWidget.arrow().setState(GTK_STATE_FLAG_NORMAL);
    GtkBorder comboContentsBox = comboWidget.comboBox().contentsBox();
    GtkBorder boxContentsBox = comboWidget.box().contentsBox();
    GtkBorder buttonContentsBox = comboWidget.button().contentsBox();
    GtkBorder buttonBoxContentsBox = comboWidget.buttonBox().contentsBox();
    GtkBorder padding;
    padding.left = comboContentsBox.left + boxContentsBox.left + buttonContentsBox.left + buttonBoxContentsBox.left;
    padding.right = comboContentsBox.right + boxContentsBox.right + buttonContentsBox.right + buttonBoxContentsBox.right;
    padding.top = comboContentsBox.top + boxContentsBox.top + buttonContentsBox.top + buttonBoxContentsBox.top;
    padding.bottom = comboContentsBox.bottom + boxContentsBox.bottom + buttonContentsBox.bottom + buttonBoxContentsBox.bottom;

    auto arrowSize = comboWidget.arrow().preferredSize();
    return LengthBox(padding.top, padding.right + (style.direction() == TextDirection::LTR ? arrowSize.width() : 0),
        padding.bottom, padding.left + (style.direction() == TextDirection::RTL ? arrowSize.width() : 0));
}

bool RenderThemeGtk::paintMenuList(const RenderObject& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
{
    auto& comboWidget = static_cast<RenderThemeComboBox&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::ComboBox));
    auto comboState = themePartStateFlags(*this, ComboBoxButton, renderObject);
    comboWidget.comboBox().setState(comboState);
    comboWidget.button().setState(comboState);
    comboWidget.arrow().setState(comboState);

    cairo_t* cr = paintInfo.context().platformContext()->cr();
    comboWidget.comboBox().render(cr, rect);
    comboWidget.box().render(cr, rect);
    FloatRect contentsRect;
    comboWidget.button().render(cr, rect, &contentsRect);
    comboWidget.buttonBox().render(cr, contentsRect);
    comboWidget.arrow().render(cr, contentsRect);
    if (isFocused(renderObject))
        comboWidget.button().renderFocus(cr, rect);

    return false;
}

bool RenderThemeGtk::paintMenuListButtonDecorations(const RenderBox& object, const PaintInfo& info, const FloatRect& rect)
{
    return paintMenuList(object, info, rect);
}

static IntSize spinButtonSize()
{
    auto& spinButtonWidget = static_cast<RenderThemeSpinButton&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::SpinButton));
    spinButtonWidget.spinButton().setState(GTK_STATE_FLAG_NORMAL);
    spinButtonWidget.entry().setState(GTK_STATE_FLAG_NORMAL);
    spinButtonWidget.up().setState(GTK_STATE_FLAG_NORMAL);
    spinButtonWidget.down().setState(GTK_STATE_FLAG_NORMAL);

    IntSize preferredSize = spinButtonWidget.spinButton().preferredSize();
    preferredSize = preferredSize.expandedTo(spinButtonWidget.entry().preferredSize());
    IntSize upPreferredSize = preferredSize.expandedTo(spinButtonWidget.up().preferredSize());
    IntSize downPreferredSize = preferredSize.expandedTo(spinButtonWidget.down().preferredSize());

    return IntSize(upPreferredSize.width() + downPreferredSize.width(), std::max(upPreferredSize.height(), downPreferredSize.height()));
}


void RenderThemeGtk::adjustTextFieldStyle(RenderStyle& style, const Element* element) const
{
    if (!is<HTMLInputElement>(element) || !shouldHaveSpinButton(downcast<HTMLInputElement>(*element)))
        return;

    style.setMinHeight(Length(spinButtonSize().height(), Fixed));

    // The default theme for the GTK+ port uses very wide spin buttons (66px) compared to what other
    // browsers use (~13 px). And unfortunately, most of the web developers won't test how their site
    // renders on WebKitGTK. To ensure that spin buttons don't end up covering the values of the input
    // field, we override the width of the input element and always increment it with the width needed
    // for the spinbutton (when drawing the spinbutton).
    int minimumWidth = style.width().intValue() + spinButtonSize().width();
    style.setMinWidth(Length(minimumWidth, Fixed));
}

bool RenderThemeGtk::paintTextField(const RenderObject& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (is<HTMLInputElement>(renderObject.node()) && shouldHaveSpinButton(downcast<HTMLInputElement>(*renderObject.node()))) {
        auto& spinButtonWidget = static_cast<RenderThemeSpinButton&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::SpinButton));
        auto spinButtonState = themePartStateFlags(*this, Entry, renderObject);
        spinButtonWidget.spinButton().setState(spinButtonState);
        spinButtonWidget.entry().setState(spinButtonState);
        spinButtonWidget.spinButton().render(paintInfo.context().platformContext()->cr(), rect);
        spinButtonWidget.entry().render(paintInfo.context().platformContext()->cr(), rect);
    } else {
        auto& entryWidget = static_cast<RenderThemeEntry&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::Entry));
        entryWidget.entry().setState(themePartStateFlags(*this, Entry, renderObject));
        entryWidget.entry().render(paintInfo.context().platformContext()->cr(), rect);

#if ENABLE(DATALIST_ELEMENT)
        if (is<HTMLInputElement>(renderObject.generatingNode())) {
            const auto& input = downcast<HTMLInputElement>(*(renderObject.generatingNode()));
            if (input.list())
                paintListButtonForInput(renderObject, paintInfo, rect);
        }
#endif
    }
    return false;
}

static void adjustSearchFieldIconStyle(RenderThemePart themePart, RenderStyle& style)
{
    ASSERT(themePart == EntryIconLeft || themePart == EntryIconRight);
    auto& searchEntryWidget = static_cast<RenderThemeSearchEntry&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::SearchEntry));
    searchEntryWidget.entry().setState(GTK_STATE_FLAG_NORMAL);
    searchEntryWidget.leftIcon().setState(GTK_STATE_FLAG_NORMAL);
    searchEntryWidget.rightIcon().setState(GTK_STATE_FLAG_NORMAL);

    // Get the icon size based on the font size.
    auto& icon = static_cast<RenderThemeIconGadget&>(themePart == EntryIconLeft ? searchEntryWidget.leftIcon() : searchEntryWidget.rightIcon());
    icon.setIconSize(style.computedFontPixelSize());
    IntSize preferredSize = icon.preferredSize();
    GtkBorder contentsBox = searchEntryWidget.entry().contentsBox();
    if (themePart == EntryIconLeft)
        preferredSize.expand(contentsBox.left, contentsBox.top + contentsBox.bottom);
    else
        preferredSize.expand(contentsBox.right, contentsBox.top + contentsBox.bottom);
    style.setWidth(Length(preferredSize.width(), Fixed));
    style.setHeight(Length(preferredSize.height(), Fixed));
}

bool RenderThemeGtk::paintTextArea(const RenderObject& o, const PaintInfo& i, const FloatRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeGtk::adjustSearchFieldResultsButtonStyle(RenderStyle& style, const Element* e) const
{
    adjustSearchFieldCancelButtonStyle(style, e);
}

bool RenderThemeGtk::paintSearchFieldResultsButton(const RenderBox& o, const PaintInfo& i, const IntRect& rect)
{
    return paintSearchFieldResultsDecorationPart(o, i, rect);
}

void RenderThemeGtk::adjustSearchFieldResultsDecorationPartStyle(RenderStyle& style, const Element*) const
{
    adjustSearchFieldIconStyle(EntryIconLeft, style);
}

void RenderThemeGtk::adjustSearchFieldCancelButtonStyle(RenderStyle& style, const Element*) const
{
    adjustSearchFieldIconStyle(EntryIconRight, style);
}

static bool paintSearchFieldIcon(RenderThemeGtk* theme, RenderThemePart themePart, const RenderBox& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    ASSERT(themePart == EntryIconLeft || themePart == EntryIconRight);
    auto& searchEntryWidget = static_cast<RenderThemeSearchEntry&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::SearchEntry));
    searchEntryWidget.entry().setState(themePartStateFlags(*theme, Entry, renderObject));
    auto& icon = static_cast<RenderThemeIconGadget&>(themePart == EntryIconLeft ? searchEntryWidget.leftIcon() : searchEntryWidget.rightIcon());
    icon.setState(themePartStateFlags(*theme, themePart, renderObject));
    icon.setIconSize(renderObject.style().computedFontPixelSize());
    GtkBorder contentsBox = searchEntryWidget.entry().contentsBox();
    IntRect iconRect = rect;
    if (themePart == EntryIconLeft) {
        iconRect.move(contentsBox.left, contentsBox.top);
        iconRect.contract(contentsBox.left, contentsBox.top + contentsBox.bottom);
    } else
        iconRect.contract(contentsBox.right, contentsBox.top + contentsBox.bottom);
    return !icon.render(paintInfo.context().platformContext()->cr(), iconRect);
}
bool RenderThemeGtk::paintSearchFieldResultsDecorationPart(const RenderBox& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintSearchFieldIcon(this, EntryIconLeft, renderObject, paintInfo, rect);
}

bool RenderThemeGtk::paintSearchFieldCancelButton(const RenderBox& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintSearchFieldIcon(this, EntryIconRight, renderObject, paintInfo, rect);
}

#if ENABLE(DATALIST_ELEMENT)
void RenderThemeGtk::adjustListButtonStyle(RenderStyle& style, const Element*) const
{
    // Add a margin to place the button at end of the input field.
    if (style.isLeftToRightDirection())
        style.setMarginRight(Length(-4, Fixed));
    else
        style.setMarginLeft(Length(-4, Fixed));
}

void RenderThemeGtk::paintListButtonForInput(const RenderObject& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
{
    // Use a combo box widget to render its arrow.
    auto& comboWidget = static_cast<RenderThemeComboBox&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::ComboBox));
    comboWidget.arrow().setState(themePartStateFlags(*this, ComboBoxButton, renderObject));

    // But a search entry widget to get the contents rect, since this is a text input field.
    auto& searchEntryWidget = static_cast<RenderThemeSearchEntry&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::SearchEntry));
    auto& icon = static_cast<RenderThemeIconGadget&>(searchEntryWidget.rightIcon());
    icon.setIconSize(comboWidget.arrow().preferredSize().width());
    GtkBorder contentsBox = searchEntryWidget.entry().contentsBox();
    FloatRect adjustedRect(rect);
    adjustedRect.move(contentsBox.left, contentsBox.top);
    adjustedRect.contract(contentsBox.right + 1, contentsBox.top + contentsBox.bottom);
    comboWidget.arrow().render(paintInfo.context().platformContext()->cr(), adjustedRect);
}
#endif

void RenderThemeGtk::adjustSearchFieldStyle(RenderStyle& style, const Element*) const
{
    // We cannot give a proper rendering when border radius is active, unfortunately.
    style.resetBorderRadius();
    style.setLineHeight(RenderStyle::initialLineHeight());
}

bool RenderThemeGtk::paintSearchField(const RenderObject& o, const PaintInfo& i, const IntRect& rect)
{
    return paintTextField(o, i, rect);
}

bool RenderThemeGtk::shouldHaveCapsLockIndicator(const HTMLInputElement& element) const
{
    return element.isPasswordField();
}

void RenderThemeGtk::adjustSliderTrackStyle(RenderStyle& style, const Element*) const
{
    style.setBoxShadow(nullptr);
}

void RenderThemeGtk::adjustSliderThumbStyle(RenderStyle& style, const Element* element) const
{
    RenderTheme::adjustSliderThumbStyle(style, element);
    style.setBoxShadow(nullptr);
}

/*
 * GtkScale
 *
 * scale
 * ╰── contents
 *     ╰── trough
 *         ├── slider
 *         ╰── [highlight]
 */
bool RenderThemeGtk::paintSliderTrack(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    ControlPart part = renderObject.style().appearance();
    ASSERT(part == SliderHorizontalPart || part == SliderVerticalPart);

    auto& sliderWidget = static_cast<RenderThemeSlider&>(RenderThemeWidget::getOrCreate(part == SliderHorizontalPart ? RenderThemeWidget::Type::HorizontalSlider : RenderThemeWidget::Type::VerticalSlider));
    auto scaleState = themePartStateFlags(*this, Scale, renderObject);
    auto& scale = sliderWidget.scale();
    scale.setState(scaleState);
    auto& contents = sliderWidget.contents();
    auto& trough = sliderWidget.trough();
    trough.setState(scaleState);
    auto& slider = sliderWidget.slider();
    auto& highlight = sliderWidget.highlight();

    // The given rectangle is not calculated based on the scale size, but all the margins and paddings are based on it.
    IntSize preferredSize = scale.preferredSize();
    preferredSize = preferredSize.expandedTo(contents.preferredSize());
    preferredSize = preferredSize.expandedTo(trough.preferredSize());
    FloatRect trackRect = rect;
    if (part == SliderHorizontalPart) {
        trackRect.move(0, rect.height() / 2 - (preferredSize.height() / 2));
        trackRect.setHeight(preferredSize.height());
    } else {
        trackRect.move(rect.width() / 2 - (preferredSize.width() / 2), 0);
        trackRect.setWidth(preferredSize.width());
    }

    FloatRect contentsRect;
    scale.render(paintInfo.context().platformContext()->cr(), trackRect, &contentsRect);
    contents.render(paintInfo.context().platformContext()->cr(), contentsRect, &contentsRect);
    // Scale trough defines its size querying slider and highlight.
    if (part == SliderHorizontalPart)
        contentsRect.setHeight(trough.preferredSize().height() + std::max(slider.preferredSize().height(), highlight.preferredSize().height()));
    else
        contentsRect.setWidth(trough.preferredSize().width() + std::max(slider.preferredSize().width(), highlight.preferredSize().width()));
    FloatRect troughRect = contentsRect;
    trough.render(paintInfo.context().platformContext()->cr(), troughRect, &contentsRect);
    if (isFocused(renderObject))
        trough.renderFocus(paintInfo.context().platformContext()->cr(), troughRect);

    LayoutPoint thumbLocation;
    if (is<HTMLInputElement>(renderObject.node())) {
        auto& input = downcast<HTMLInputElement>(*renderObject.node());
        if (auto* element = input.sliderThumbElement())
            thumbLocation = element->renderBox()->location();
    }

    if (part == SliderHorizontalPart) {
        if (renderObject.style().direction() == TextDirection::RTL) {
            contentsRect.move(thumbLocation.x(), 0);
            contentsRect.setWidth(contentsRect.width() - thumbLocation.x());
        } else
            contentsRect.setWidth(thumbLocation.x());
    } else
        contentsRect.setHeight(thumbLocation.y());
    highlight.render(paintInfo.context().platformContext()->cr(), contentsRect);

    return false;
}

void RenderThemeGtk::adjustSliderThumbSize(RenderStyle& style, const Element*) const
{
    ControlPart part = style.appearance();
    if (part != SliderThumbHorizontalPart && part != SliderThumbVerticalPart)
        return;

    auto& sliderWidget = static_cast<RenderThemeSlider&>(RenderThemeWidget::getOrCreate(part == SliderThumbHorizontalPart ? RenderThemeWidget::Type::HorizontalSlider : RenderThemeWidget::Type::VerticalSlider));
    sliderWidget.scale().setState(GTK_STATE_FLAG_NORMAL);
    sliderWidget.trough().setState(GTK_STATE_FLAG_NORMAL);

    IntSize preferredSize = sliderWidget.scale().preferredSize();
    preferredSize = preferredSize.expandedTo(sliderWidget.contents().preferredSize());
    preferredSize = preferredSize.expandedTo(sliderWidget.trough().preferredSize());
    preferredSize = preferredSize.expandedTo(sliderWidget.slider().preferredSize());
    if (part == SliderThumbHorizontalPart) {
        style.setWidth(Length(preferredSize.width(), Fixed));
        style.setHeight(Length(preferredSize.height(), Fixed));
        return;
    }
    ASSERT(part == SliderThumbVerticalPart);
    style.setWidth(Length(preferredSize.height(), Fixed));
    style.setHeight(Length(preferredSize.width(), Fixed));
}

bool RenderThemeGtk::paintSliderThumb(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    ControlPart part = renderObject.style().appearance();
    ASSERT(part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart);

    auto& sliderWidget = static_cast<RenderThemeSlider&>(RenderThemeWidget::getOrCreate(part == SliderThumbHorizontalPart ? RenderThemeWidget::Type::HorizontalSlider : RenderThemeWidget::Type::VerticalSlider));
    auto scaleState = themePartStateFlags(*this, Scale, renderObject);
    auto& scale = sliderWidget.scale();
    scale.setState(scaleState);
    auto& contents = sliderWidget.contents();
    auto& trough = sliderWidget.trough();
    trough.setState(scaleState);
    auto& slider = sliderWidget.slider();
    slider.setState(themePartStateFlags(*this, ScaleSlider, renderObject));
    auto& highlight = sliderWidget.highlight();

    GtkBorder scaleContentsBox = scale.contentsBox();
    GtkBorder contentsContentsBox = contents.contentsBox();
    GtkBorder troughContentsBox = trough.contentsBox();
    GtkBorder padding;
    padding.left = scaleContentsBox.left + contentsContentsBox.left + troughContentsBox.left;
    padding.right = scaleContentsBox.right + contentsContentsBox.right + troughContentsBox.right;
    padding.top = scaleContentsBox.top + contentsContentsBox.top + troughContentsBox.top;
    padding.bottom = scaleContentsBox.bottom + contentsContentsBox.bottom + troughContentsBox.bottom;

    // Scale trough defines its size querying slider and highlight.
    int troughHeight = trough.preferredSize().height() + std::max(slider.preferredSize().height(), highlight.preferredSize().height());
    IntRect sliderRect(rect.location(), IntSize(troughHeight, troughHeight));
    sliderRect.move(padding.left, padding.top);
    sliderRect.contract(padding.left + padding.right, padding.top + padding.bottom);
    slider.render(paintInfo.context().platformContext()->cr(), sliderRect);
    return false;
}

IntRect RenderThemeGtk::progressBarRectForBounds(const RenderObject& renderObject, const IntRect& bounds) const
{
    const auto& renderProgress = downcast<RenderProgress>(renderObject);
    auto& progressBarWidget = static_cast<RenderThemeProgressBar&>(RenderThemeWidget::getOrCreate(renderProgress.isDeterminate() ? RenderThemeProgressBar::Type::ProgressBar : RenderThemeProgressBar::Type::IndeterminateProgressBar));
    IntSize preferredSize = progressBarWidget.progressBar().preferredSize();
    preferredSize = preferredSize.expandedTo(progressBarWidget.trough().preferredSize());
    preferredSize = preferredSize.expandedTo(progressBarWidget.progress().preferredSize());
    return IntRect(bounds.x(), bounds.y(), bounds.width(), preferredSize.height());
}

bool RenderThemeGtk::paintProgressBar(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!renderObject.isProgress())
        return true;

    const auto& renderProgress = downcast<RenderProgress>(renderObject);
    auto& progressBarWidget = static_cast<RenderThemeProgressBar&>(RenderThemeWidget::getOrCreate(renderProgress.isDeterminate() ? RenderThemeProgressBar::Type::ProgressBar : RenderThemeProgressBar::Type::IndeterminateProgressBar));
    progressBarWidget.progressBar().render(paintInfo.context().platformContext()->cr(), rect);
    progressBarWidget.trough().render(paintInfo.context().platformContext()->cr(), rect);
    progressBarWidget.progress().render(paintInfo.context().platformContext()->cr(), calculateProgressRect(renderObject, rect));
    return false;
}

RenderTheme::InnerSpinButtonLayout RenderThemeGtk::innerSpinButtonLayout(const RenderObject& renderObject) const
{
    return renderObject.style().direction() == TextDirection::RTL ? InnerSpinButtonLayout::HorizontalUpLeft : InnerSpinButtonLayout::HorizontalUpRight;
}

void RenderThemeGtk::adjustInnerSpinButtonStyle(RenderStyle& style, const Element*) const
{
    style.setWidth(Length(spinButtonSize().width(), Fixed));
    style.setHeight(Length(spinButtonSize().height(), Fixed));
}

bool RenderThemeGtk::paintInnerSpinButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    auto& spinButtonWidget = static_cast<RenderThemeSpinButton&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::SpinButton));
    auto spinButtonState = themePartStateFlags(*this, SpinButton, renderObject);
    spinButtonWidget.spinButton().setState(spinButtonState);
    spinButtonWidget.entry().setState(spinButtonState);
    auto& up = spinButtonWidget.up();
    up.setState(themePartStateFlags(*this, SpinButtonUpButton, renderObject));
    auto& down = spinButtonWidget.down();
    down.setState(themePartStateFlags(*this, SpinButtonDownButton, renderObject));

    IntRect iconRect = rect;
    iconRect.setWidth(iconRect.width() / 2);
    if (renderObject.style().direction() == TextDirection::RTL)
        up.render(paintInfo.context().platformContext()->cr(), iconRect);
    else
        down.render(paintInfo.context().platformContext()->cr(), iconRect);
    iconRect.move(iconRect.width(), 0);
    if (renderObject.style().direction() == TextDirection::RTL)
        down.render(paintInfo.context().platformContext()->cr(), iconRect);
    else
        up.render(paintInfo.context().platformContext()->cr(), iconRect);

    return false;
}

Seconds RenderThemeGtk::caretBlinkInterval() const
{
    GtkSettings* settings = gtk_settings_get_default();

    gboolean shouldBlink;
    gint time;

    g_object_get(settings, "gtk-cursor-blink", &shouldBlink, "gtk-cursor-blink-time", &time, nullptr);

    if (!shouldBlink)
        return 0_s;

    return 500_us * time;
}

enum StyleColorType { StyleColorBackground, StyleColorForeground };

static Color styleColor(RenderThemePart themePart, GtkStateFlags state, StyleColorType colorType)
{
    RenderThemeGadget* gadget = nullptr;
    switch (themePart) {
    default:
        ASSERT_NOT_REACHED();
        FALLTHROUGH;
    case Entry:
        gadget = &static_cast<RenderThemeEntry&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::Entry)).entry();
        break;
    case EntrySelection:
        gadget = static_cast<RenderThemeEntry&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::SelectedEntry)).selection();
        break;
    case ListBox:
        gadget = &static_cast<RenderThemeListView&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::ListView)).treeview();
        break;
    case Button:
        gadget = &static_cast<RenderThemeButton&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::Button)).button();
        break;
    case Window:
        gadget = &static_cast<RenderThemeWindow&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::Window)).window();
        break;
    }

    ASSERT(gadget);
    gadget->setState(state);
    return colorType == StyleColorBackground ? gadget->backgroundColor() : gadget->color();
}

Color RenderThemeGtk::platformActiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const
{
    return styleColor(EntrySelection, static_cast<GtkStateFlags>(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_FOCUSED), StyleColorBackground);
}

Color RenderThemeGtk::platformInactiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const
{
    return styleColor(EntrySelection, GTK_STATE_FLAG_SELECTED, StyleColorBackground);
}

Color RenderThemeGtk::platformActiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const
{
    return styleColor(EntrySelection, static_cast<GtkStateFlags>(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_FOCUSED), StyleColorForeground);
}

Color RenderThemeGtk::platformInactiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const
{
    return styleColor(EntrySelection, GTK_STATE_FLAG_SELECTED, StyleColorForeground);
}

Color RenderThemeGtk::platformActiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options>) const
{
    return styleColor(ListBox, static_cast<GtkStateFlags>(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_FOCUSED), StyleColorBackground);
}

Color RenderThemeGtk::platformInactiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options>) const
{
    return styleColor(ListBox, GTK_STATE_FLAG_SELECTED, StyleColorBackground);
}

Color RenderThemeGtk::platformActiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options>) const
{
    return styleColor(ListBox, static_cast<GtkStateFlags>(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_FOCUSED), StyleColorForeground);
}

Color RenderThemeGtk::platformInactiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options>) const
{
    return styleColor(ListBox, GTK_STATE_FLAG_SELECTED, StyleColorForeground);
}

Color RenderThemeGtk::disabledTextColor(const Color&, const Color&) const
{
    return styleColor(Entry, GTK_STATE_FLAG_INSENSITIVE, StyleColorForeground);
}

Color RenderThemeGtk::systemColor(CSSValueID cssValueId, OptionSet<StyleColor::Options> options) const
{
    switch (cssValueId) {
    case CSSValueButtontext:
        return styleColor(Button, GTK_STATE_FLAG_ACTIVE, StyleColorForeground);
    case CSSValueCaptiontext:
        return styleColor(Entry, GTK_STATE_FLAG_ACTIVE, StyleColorForeground);
    case CSSValueText:
        return styleColor(Entry, GTK_STATE_FLAG_ACTIVE, StyleColorForeground);
    case CSSValueGraytext:
        return styleColor(Entry, GTK_STATE_FLAG_INSENSITIVE, StyleColorForeground);
    default:
        break;
    }

    return RenderTheme::systemColor(cssValueId, options);
}

void RenderThemeGtk::platformColorsDidChange()
{
    RenderTheme::platformColorsDidChange();
}

#if ENABLE(VIDEO)
String RenderThemeGtk::extraMediaControlsStyleSheet()
{
    return String(mediaControlsGtkUserAgentStyleSheet, sizeof(mediaControlsGtkUserAgentStyleSheet));
}

#if ENABLE(FULLSCREEN_API)
String RenderThemeGtk::extraFullScreenStyleSheet()
{
    return String();
}
#endif

bool RenderThemeGtk::paintMediaButton(const RenderObject& renderObject, GraphicsContext& graphicsContext, const IntRect& rect, const char* iconName)
{
    auto& iconWidget = static_cast<RenderThemeIcon&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::Icon));
    auto& icon = static_cast<RenderThemeIconGadget&>(iconWidget.icon());
    icon.setState(themePartStateFlags(*this, MediaButton, renderObject));
    icon.setIconSize(RenderThemeIconGadget::IconSizeGtk::Menu);
    icon.setIconName(iconName);
    return !icon.render(graphicsContext.platformContext()->cr(), rect);
}

bool RenderThemeGtk::hasOwnDisabledStateHandlingFor(ControlPart part) const
{
    return (part != MediaMuteButtonPart);
}

bool RenderThemeGtk::paintMediaFullscreenButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintMediaButton(renderObject, paintInfo.context(), rect, "view-fullscreen-symbolic");
}

bool RenderThemeGtk::paintMediaMuteButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    Node* node = renderObject.node();
    if (!node)
        return true;
    Node* mediaNode = node->shadowHost();
    if (!is<HTMLMediaElement>(mediaNode))
        return true;

    HTMLMediaElement* mediaElement = downcast<HTMLMediaElement>(mediaNode);
    return paintMediaButton(renderObject, paintInfo.context(), rect, mediaElement->muted() ? "audio-volume-muted-symbolic" : "audio-volume-high-symbolic");
}

bool RenderThemeGtk::paintMediaPlayButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    Node* node = renderObject.node();
    if (!node)
        return true;
    if (!nodeHasPseudo(*node, "-webkit-media-controls-play-button"))
        return true;

    return paintMediaButton(renderObject, paintInfo.context(), rect, nodeHasClass(node, "paused") ? "media-playback-start-symbolic" : "media-playback-pause-symbolic");
}

bool RenderThemeGtk::paintMediaSeekBackButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintMediaButton(renderObject, paintInfo.context(), rect, "media-seek-backward-symbolic");
}

bool RenderThemeGtk::paintMediaSeekForwardButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintMediaButton(renderObject, paintInfo.context(), rect, "media-seek-forward-symbolic");
}

#if ENABLE(VIDEO_TRACK)
bool RenderThemeGtk::paintMediaToggleClosedCaptionsButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintMediaButton(renderObject, paintInfo.context(), rect, "media-view-subtitles-symbolic");
}
#endif

static FloatRoundedRect::Radii borderRadiiFromStyle(const RenderStyle& style)
{
    return FloatRoundedRect::Radii(
        IntSize(style.borderTopLeftRadius().width.intValue(), style.borderTopLeftRadius().height.intValue()),
        IntSize(style.borderTopRightRadius().width.intValue(), style.borderTopRightRadius().height.intValue()),
        IntSize(style.borderBottomLeftRadius().width.intValue(), style.borderBottomLeftRadius().height.intValue()),
        IntSize(style.borderBottomRightRadius().width.intValue(), style.borderBottomRightRadius().height.intValue()));
}

bool RenderThemeGtk::paintMediaSliderTrack(const RenderObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    auto mediaElement = parentMediaElement(o);
    if (!mediaElement)
        return true;

    GraphicsContext& context = paintInfo.context();
    context.save();
    context.setStrokeStyle(NoStroke);

    float mediaDuration = mediaElement->duration();
    float totalTrackWidth = r.width();
    auto& style = o.style();
    RefPtr<TimeRanges> timeRanges = mediaElement->buffered();
    for (unsigned index = 0; index < timeRanges->length(); ++index) {
        float start = timeRanges->start(index).releaseReturnValue();
        float end = timeRanges->end(index).releaseReturnValue();
        float startRatio = start / mediaDuration;
        float lengthRatio = (end - start) / mediaDuration;
        if (!lengthRatio)
            continue;

        IntRect rangeRect(r);
        rangeRect.setWidth(lengthRatio * totalTrackWidth);
        if (index)
            rangeRect.move(startRatio * totalTrackWidth, 0);
        context.fillRoundedRect(FloatRoundedRect(rangeRect, borderRadiiFromStyle(style)), style.visitedDependentColor(CSSPropertyColor));
    }

    context.restore();
    return false;
}

bool RenderThemeGtk::paintMediaSliderThumb(const RenderObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    auto& style = o.style();
    paintInfo.context().fillRoundedRect(FloatRoundedRect(r, borderRadiiFromStyle(style)), style.visitedDependentColor(CSSPropertyColor));
    return false;
}

bool RenderThemeGtk::paintMediaVolumeSliderTrack(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    auto mediaElement = parentMediaElement(renderObject);
    if (!mediaElement)
        return true;

    float volume = mediaElement->muted() ? 0.0f : mediaElement->volume();
    if (!volume)
        return true;

    GraphicsContext& context = paintInfo.context();
    context.save();
    context.setStrokeStyle(NoStroke);

    int rectHeight = rect.height();
    float trackHeight = rectHeight * volume;
    auto& style = renderObject.style();
    IntRect volumeRect(rect);
    volumeRect.move(0, rectHeight - trackHeight);
    volumeRect.setHeight(ceil(trackHeight));

    context.fillRoundedRect(FloatRoundedRect(volumeRect, borderRadiiFromStyle(style)), style.visitedDependentColor(CSSPropertyColor));
    context.restore();

    return false;
}

bool RenderThemeGtk::paintMediaVolumeSliderThumb(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintMediaSliderThumb(renderObject, paintInfo, rect);
}

String RenderThemeGtk::formatMediaControlsCurrentTime(float currentTime, float duration) const
{
    return formatMediaControlsTime(currentTime) + " / " + formatMediaControlsTime(duration);
}

bool RenderThemeGtk::paintMediaCurrentTime(const RenderObject&, const PaintInfo&, const IntRect&)
{
    return false;
}
#endif

void RenderThemeGtk::adjustProgressBarStyle(RenderStyle& style, const Element*) const
{
    style.setBoxShadow(nullptr);
}

// These values have been copied from RenderThemeChromiumSkia.cpp
static const int progressActivityBlocks = 5;
static const int progressAnimationFrames = 10;
static const Seconds progressAnimationInterval { 125_ms };
Seconds RenderThemeGtk::animationRepeatIntervalForProgressBar(RenderProgress&) const
{
    return progressAnimationInterval;
}

Seconds RenderThemeGtk::animationDurationForProgressBar(RenderProgress&) const
{
    return progressAnimationInterval * progressAnimationFrames * 2; // "2" for back and forth;
}

IntRect RenderThemeGtk::calculateProgressRect(const RenderObject& renderObject, const IntRect& fullBarRect)
{
    IntRect progressRect(fullBarRect);
    const auto& renderProgress = downcast<RenderProgress>(renderObject);
    if (renderProgress.isDeterminate()) {
        int progressWidth = progressRect.width() * renderProgress.position();
        if (renderObject.style().direction() == TextDirection::RTL)
            progressRect.setX(progressRect.x() + progressRect.width() - progressWidth);
        progressRect.setWidth(progressWidth);
        return progressRect;
    }

    double animationProgress = renderProgress.animationProgress();

    // Never let the progress rect shrink smaller than 2 pixels.
    int newWidth = std::max(2, progressRect.width() / progressActivityBlocks);
    int movableWidth = progressRect.width() - newWidth;
    progressRect.setWidth(newWidth);

    // We want the first 0.5 units of the animation progress to represent the
    // forward motion and the second 0.5 units to represent the backward motion,
    // thus we multiply by two here to get the full sweep of the progress bar with
    // each direction.
    if (animationProgress < 0.5)
        progressRect.setX(progressRect.x() + (animationProgress * 2 * movableWidth));
    else
        progressRect.setX(progressRect.x() + ((1.0 - animationProgress) * 2 * movableWidth));
    return progressRect;
}

String RenderThemeGtk::fileListNameForWidth(const FileList* fileList, const FontCascade& font, int width, bool multipleFilesAllowed) const
{
    if (width <= 0)
        return String();

    if (fileList->length() > 1)
        return StringTruncator::rightTruncate(multipleFileUploadText(fileList->length()), width, font);

    String string;
    if (fileList->length())
        string = FileSystem::pathGetFileName(fileList->item(0)->path());
    else if (multipleFilesAllowed)
        string = fileButtonNoFilesSelectedLabel();
    else
        string = fileButtonNoFileSelectedLabel();

    return StringTruncator::centerTruncate(string, width, font);
}

#if ENABLE(VIDEO)
String RenderThemeGtk::mediaControlsScript()
{
    StringBuilder scriptBuilder;
    scriptBuilder.appendCharacters(mediaControlsLocalizedStringsJavaScript, sizeof(mediaControlsLocalizedStringsJavaScript));
    scriptBuilder.appendCharacters(mediaControlsBaseJavaScript, sizeof(mediaControlsBaseJavaScript));
    scriptBuilder.appendCharacters(mediaControlsGtkJavaScript, sizeof(mediaControlsGtkJavaScript));
    return scriptBuilder.toString();
}
#endif // ENABLE(VIDEO)
}
