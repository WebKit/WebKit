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
#include "FileSystem.h"
#include "FloatRoundedRect.h"
#include "FontDescription.h"
#include "GRefPtrGtk.h"
#include "GUniquePtrGtk.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "GtkVersioning.h"
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

#ifndef GTK_API_VERSION_2

static void themeChangedCallback()
{
    Page::updateStyleForAllPagesAfterGlobalChangeInEnvironment();
}

RenderThemeGtk::RenderThemeGtk()
{
    static bool themeMonitorInitialized = false;
    if (!themeMonitorInitialized) {
        GtkSettings* settings = gtk_settings_get_default();
        g_signal_connect(settings, "notify::gtk-theme-name", G_CALLBACK(themeChangedCallback), nullptr);
        g_signal_connect(settings, "notify::gtk-color-scheme", G_CALLBACK(themeChangedCallback), nullptr);
        themeMonitorInitialized = true;
    }
}

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
};

#if !GTK_CHECK_VERSION(3, 20, 0)
// This is the default value defined by GTK+, where it was defined as MIN_ARROW_SIZE in gtkarrow.c.
static const int minArrowSize = 15;
// This is the default value defined by GTK+, where it was defined as MIN_ARROW_WIDTH in gtkspinbutton.c.
static const int minSpinButtonArrowSize = 6;

static GRefPtr<GtkStyleContext> createStyleContext(RenderThemePart themePart, GtkStyleContext* parent = nullptr)
{
    GRefPtr<GtkWidgetPath> path = adoptGRef(parent ? gtk_widget_path_copy(gtk_style_context_get_path(parent)) : gtk_widget_path_new());

    switch (themePart) {
    case Entry:
    case EntrySelection:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_ENTRY);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_ENTRY);
        break;
    case EntryIconLeft:
    case EntryIconRight:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_ENTRY);
        break;
    case Button:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_BUTTON);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_BUTTON);
        gtk_widget_path_iter_add_class(path.get(), -1, "text-button");
        break;
    case CheckButton:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_CHECK_BUTTON);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_CHECK);
        break;
    case RadioButton:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_RADIO_BUTTON);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_RADIO);
        break;
    case ComboBox:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_COMBO_BOX);
        break;
    case ComboBoxButton:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_BUTTON);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_BUTTON);
        gtk_widget_path_iter_add_class(path.get(), -1, "text-button");
        gtk_widget_path_iter_add_class(path.get(), -1, "combo");
        break;
    case ComboBoxArrow:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_ARROW);
        gtk_widget_path_iter_add_class(path.get(), -1, "arrow");
        break;
    case Scale:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_SCALE);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_SCALE);
        break;
    case ScaleTrough:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_SCALE);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_SCALE);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_TROUGH);
        break;
    case ScaleSlider:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_SCALE);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_SCALE);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_SLIDER);
        break;
    case ProgressBar:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_PROGRESS_BAR);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_PROGRESSBAR);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_HORIZONTAL);
        break;
    case ProgressBarTrough:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_PROGRESS_BAR);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_TROUGH);
        break;
    case ProgressBarProgress:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_PROGRESS_BAR);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_PROGRESSBAR);
        break;
    case ListBox:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_TREE_VIEW);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_VIEW);
        break;
    case SpinButton:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_SPIN_BUTTON);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_SPINBUTTON);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_HORIZONTAL);
        break;
    case SpinButtonUpButton:
    case SpinButtonDownButton:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_SPIN_BUTTON);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_SPINBUTTON);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_BUTTON);
        break;
#if ENABLE(VIDEO)
    case MediaButton:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_IMAGE);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_IMAGE);
        break;
#endif // ENABLE(VIDEO)
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    GRefPtr<GtkStyleContext> context = adoptGRef(gtk_style_context_new());
    gtk_style_context_set_path(context.get(), path.get());
    gtk_style_context_set_parent(context.get(), parent);
    return context;
}

static GRefPtr<GdkPixbuf> loadThemedIcon(GtkStyleContext* context, const char* iconName, GtkIconSize iconSize)
{
    GRefPtr<GIcon> icon = adoptGRef(g_themed_icon_new(iconName));
    unsigned lookupFlags = GTK_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_FORCE_SIZE | GTK_ICON_LOOKUP_FORCE_SVG;
#if GTK_CHECK_VERSION(3, 14, 0)
    GtkTextDirection direction = gtk_style_context_get_direction(context);
    if (direction & GTK_TEXT_DIR_LTR)
        lookupFlags |= GTK_ICON_LOOKUP_DIR_LTR;
    else if (direction & GTK_TEXT_DIR_RTL)
        lookupFlags |= GTK_ICON_LOOKUP_DIR_RTL;
#endif
    int width, height;
    gtk_icon_size_lookup(iconSize, &width, &height);
    GRefPtr<GtkIconInfo> iconInfo = adoptGRef(gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_default(), icon.get(), std::min(width, height), static_cast<GtkIconLookupFlags>(lookupFlags)));
    if (!iconInfo)
        return nullptr;

    return adoptGRef(gtk_icon_info_load_symbolic_for_context(iconInfo.get(), context, nullptr, nullptr));
}
#endif // !GTK_CHECK_VERSION(3, 20, 0)

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

#if GTK_CHECK_VERSION(3, 20, 0)
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
#else
static GtkTextDirection gtkTextDirection(TextDirection direction)
{
    switch (direction) {
    case TextDirection::RTL:
        return GTK_TEXT_DIR_RTL;
    case TextDirection::LTR:
        return GTK_TEXT_DIR_LTR;
    default:
        return GTK_TEXT_DIR_NONE;
    }
}

static GtkStateFlags gtkIconStateFlags(RenderTheme* theme, const RenderObject& renderObject)
{
    if (!theme->isEnabled(renderObject))
        return GTK_STATE_FLAG_INSENSITIVE;
    if (theme->isPressed(renderObject))
        return GTK_STATE_FLAG_ACTIVE;
    if (theme->isHovered(renderObject))
        return GTK_STATE_FLAG_PRELIGHT;

    return GTK_STATE_FLAG_NORMAL;
}

static void adjustRectForFocus(GtkStyleContext* context, FloatRect& rect)
{
    gint focusWidth, focusPad;
    gtk_style_context_get_style(context, "focus-line-width", &focusWidth, "focus-padding", &focusPad, nullptr);
    rect.inflate(focusWidth + focusPad);
}

void RenderThemeGtk::adjustRepaintRect(const RenderObject& renderObject, FloatRect& rect)
{
    GRefPtr<GtkStyleContext> context;
    bool checkInteriorFocus = false;
    ControlPart part = renderObject.style().appearance();
    switch (part) {
    case CheckboxPart:
    case RadioPart:
        context = createStyleContext(part == CheckboxPart ? CheckButton : RadioButton);

        gint indicatorSpacing;
        gtk_style_context_get_style(context.get(), "indicator-spacing", &indicatorSpacing, nullptr);
        rect.inflate(indicatorSpacing);

        return;
    case SliderVerticalPart:
    case SliderHorizontalPart:
        context = createStyleContext(ScaleSlider);
        break;
    case ButtonPart:
    case MenulistButtonPart:
    case MenulistPart:
        context = createStyleContext(Button);
        checkInteriorFocus = true;
        break;
    case TextFieldPart:
    case TextAreaPart:
        context = createStyleContext(Entry);
        checkInteriorFocus = true;
        break;
    default:
        return;
    }

    ASSERT(context);
    if (checkInteriorFocus) {
        gboolean interiorFocus;
        gtk_style_context_get_style(context.get(), "interior-focus", &interiorFocus, nullptr);
        if (interiorFocus)
            return;
    }
    adjustRectForFocus(context.get(), rect);
}
#endif // GTK_CHECK_VERSION(3, 20, 0)

void RenderThemeGtk::adjustButtonStyle(StyleResolver&, RenderStyle& style, const Element*) const
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

#if GTK_CHECK_VERSION(3, 20, 0)
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
#else
static void setToggleSize(RenderThemePart themePart, RenderStyle& style)
{
    // The width and height are both specified, so we shouldn't change them.
    if (!style.width().isIntrinsicOrAuto() && !style.height().isAuto())
        return;

    GRefPtr<GtkStyleContext> context = createStyleContext(themePart);
    // Other ports hard-code this to 13. GTK+ users tend to demand the native look.
    gint indicatorSize;
    gtk_style_context_get_style(context.get(), "indicator-size", &indicatorSize, nullptr);

    if (style.width().isIntrinsicOrAuto())
        style.setWidth(Length(indicatorSize, Fixed));

    if (style.height().isAuto())
        style.setHeight(Length(indicatorSize, Fixed));
}

static void paintToggle(const RenderThemeGtk* theme, RenderThemePart themePart, const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& fullRect)
{
    GRefPtr<GtkStyleContext> context = createStyleContext(themePart);
    gtk_style_context_set_direction(context.get(), static_cast<GtkTextDirection>(gtkTextDirection(renderObject.style().direction())));

    unsigned flags = 0;
    if (!theme->isEnabled(renderObject))
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    else if (theme->isHovered(renderObject))
        flags |= GTK_STATE_FLAG_PRELIGHT;
    if (theme->isIndeterminate(renderObject))
        flags |= GTK_STATE_FLAG_INCONSISTENT;
    else if (theme->isChecked(renderObject))
#if GTK_CHECK_VERSION(3, 13, 7)
        flags |= GTK_STATE_FLAG_CHECKED;
#else
        flags |= GTK_STATE_FLAG_ACTIVE;
#endif
    if (theme->isPressed(renderObject))
        flags |= GTK_STATE_FLAG_SELECTED;
    gtk_style_context_set_state(context.get(), static_cast<GtkStateFlags>(flags));

    // Some themes do not render large toggle buttons properly, so we simply
    // shrink the rectangle back down to the default size and then center it
    // in the full toggle button region. The reason for not simply forcing toggle
    // buttons to be a smaller size is that we don't want to break site layouts.
    FloatRect rect(fullRect);
    gint indicatorSize;
    gtk_style_context_get_style(context.get(), "indicator-size", &indicatorSize, nullptr);
    IntSize minSize(indicatorSize, indicatorSize);
    shrinkToMinimumSizeAndCenterRectangle(rect, minSize);

    gtk_render_background(context.get(), paintInfo.context().platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(context.get(), paintInfo.context().platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());

    if (themePart == CheckButton)
        gtk_render_check(context.get(), paintInfo.context().platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());
    else
        gtk_render_option(context.get(), paintInfo.context().platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());

    if (theme->isFocused(renderObject)) {
        IntRect indicatorRect(rect);
        gint indicatorSpacing;
        gtk_style_context_get_style(context.get(), "indicator-spacing", &indicatorSpacing, nullptr);
        indicatorRect.inflate(indicatorSpacing);
        gtk_render_focus(context.get(), paintInfo.context().platformContext()->cr(), indicatorRect.x(), indicatorRect.y(),
            indicatorRect.width(), indicatorRect.height());
    }
}
#endif // GTK_CHECK_VERSION(3, 20, 0)

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

#if GTK_CHECK_VERSION(3, 20, 0)
bool RenderThemeGtk::paintButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    auto& buttonWidget = static_cast<RenderThemeButton&>(RenderThemeWidget::getOrCreate(isDefault(renderObject) ? RenderThemeWidget::Type::ButtonDefault : RenderThemeWidget::Type::Button));
    buttonWidget.button().setState(themePartStateFlags(*this, Button, renderObject));
    buttonWidget.button().render(paintInfo.context().platformContext()->cr(), rect);
    if (isFocused(renderObject))
        buttonWidget.button().renderFocus(paintInfo.context().platformContext()->cr(), rect);
    return false;
}
#else
static void renderButton(RenderTheme* theme, GtkStyleContext* context, const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    IntRect buttonRect(rect);

    guint flags = 0;
    if (!theme->isEnabled(renderObject))
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    else if (theme->isHovered(renderObject))
        flags |= GTK_STATE_FLAG_PRELIGHT;
    if (theme->isPressed(renderObject))
        flags |= GTK_STATE_FLAG_ACTIVE;
    gtk_style_context_set_state(context, static_cast<GtkStateFlags>(flags));

    if (theme->isDefault(renderObject)) {
        GtkBorder* borderPtr = 0;
        GtkBorder border = { 1, 1, 1, 1 };

        gtk_style_context_get_style(context, "default-border", &borderPtr, nullptr);
        if (borderPtr) {
            border = *borderPtr;
            gtk_border_free(borderPtr);
        }

        buttonRect.move(border.left, border.top);
        buttonRect.setWidth(buttonRect.width() - (border.left + border.right));
        buttonRect.setHeight(buttonRect.height() - (border.top + border.bottom));

        gtk_style_context_add_class(context, GTK_STYLE_CLASS_DEFAULT);
    }

    gtk_render_background(context, paintInfo.context().platformContext()->cr(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());
    gtk_render_frame(context, paintInfo.context().platformContext()->cr(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());

    if (theme->isFocused(renderObject)) {
        gint focusWidth, focusPad;
        gboolean displaceFocus, interiorFocus;
        gtk_style_context_get_style(
            context,
            "focus-line-width", &focusWidth,
            "focus-padding", &focusPad,
            "interior-focus", &interiorFocus,
            "displace-focus", &displaceFocus,
            nullptr);

        if (interiorFocus) {
            GtkBorder borderWidth;
            gtk_style_context_get_border(context, gtk_style_context_get_state(context), &borderWidth);

            buttonRect = IntRect(
                buttonRect.x() + borderWidth.left + focusPad,
                buttonRect.y() + borderWidth.top + focusPad,
                buttonRect.width() - (2 * focusPad + borderWidth.left + borderWidth.right),
                buttonRect.height() - (2 * focusPad + borderWidth.top + borderWidth.bottom));
        } else
            buttonRect.inflate(focusWidth + focusPad);

        if (displaceFocus && theme->isPressed(renderObject)) {
            gint childDisplacementX;
            gint childDisplacementY;
            gtk_style_context_get_style(context, "child-displacement-x", &childDisplacementX, "child-displacement-y", &childDisplacementY, nullptr);
            buttonRect.move(childDisplacementX, childDisplacementY);
        }

        gtk_render_focus(context, paintInfo.context().platformContext()->cr(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());
    }
}
bool RenderThemeGtk::paintButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    GRefPtr<GtkStyleContext> context = createStyleContext(Button);
    gtk_style_context_set_direction(context.get(), static_cast<GtkTextDirection>(gtkTextDirection(renderObject.style().direction())));
    renderButton(this, context.get(), renderObject, paintInfo, rect);
    return false;
}
#endif // GTK_CHECK_VERSION(3, 20, 0)

static Color menuListColor(const Element* element)
{
#if GTK_CHECK_VERSION(3, 20, 0)
    auto& comboWidget = static_cast<RenderThemeComboBox&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::ComboBox));
    GtkStateFlags state = element->isDisabledFormControl() ? GTK_STATE_FLAG_INSENSITIVE : GTK_STATE_FLAG_NORMAL;
    comboWidget.comboBox().setState(state);
    comboWidget.button().setState(state);
    return comboWidget.button().color();
#else
    GRefPtr<GtkStyleContext> parentStyleContext = createStyleContext(ComboBox);
    GRefPtr<GtkStyleContext> buttonStyleContext = createStyleContext(ComboBoxButton, parentStyleContext.get());
    gtk_style_context_set_state(buttonStyleContext.get(), element->isDisabledFormControl() ? GTK_STATE_FLAG_INSENSITIVE : GTK_STATE_FLAG_NORMAL);

    GdkRGBA gdkRGBAColor;
    gtk_style_context_get_color(buttonStyleContext.get(), gtk_style_context_get_state(buttonStyleContext.get()), &gdkRGBAColor);
    return gdkRGBAColor;
#endif // GTK_CHECK_VERSION(3, 20, 0)
}

void RenderThemeGtk::adjustMenuListStyle(StyleResolver&, RenderStyle& style, const Element* element) const
{
    // The tests check explicitly that select menu buttons ignore line height.
    style.setLineHeight(RenderStyle::initialLineHeight());

    // We cannot give a proper rendering when border radius is active, unfortunately.
    style.resetBorderRadius();

    if (element)
        style.setColor(menuListColor(element));
}

void RenderThemeGtk::adjustMenuListButtonStyle(StyleResolver& styleResolver, RenderStyle& style, const Element* e) const
{
    adjustMenuListStyle(styleResolver, style, e);
}

#if GTK_CHECK_VERSION(3, 20, 0)
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
#else
LengthBox RenderThemeGtk::popupInternalPaddingBox(const RenderStyle& style) const
{
    if (style.appearance() == NoControlPart)
        return { 0, 0, 0, 0 };

    GRefPtr<GtkStyleContext> parentContext = createStyleContext(ComboBox);
    GRefPtr<GtkStyleContext> context = createStyleContext(ComboBoxButton, parentContext.get());
    gtk_style_context_set_direction(context.get(), static_cast<GtkTextDirection>(gtkTextDirection(style.direction())));
    gtk_style_context_set_state(context.get(), static_cast<GtkStateFlags>(0));
    GtkBorder borderWidth = { 0, 0, 0, 0 };
    gtk_style_context_get_border(context.get(), gtk_style_context_get_state(context.get()), &borderWidth);

    gboolean interiorFocus;
    gint focusWidth, focusPad;
    gtk_style_context_get_style(context.get(), "interior-focus", &interiorFocus, "focus-line-width", &focusWidth, "focus-padding", &focusPad, nullptr);
    focusWidth = interiorFocus ? focusWidth + focusPad : 0;

    return { borderWidth.top + focusWidth, borderWidth.right + focusWidth + (style.direction() == TextDirection::LTR ? minArrowSize : 0),
        borderWidth.bottom + focusWidth, borderWidth.left + focusWidth + (style.direction() == TextDirection::RTL ? minArrowSize : 0) };
}

bool RenderThemeGtk::paintMenuList(const RenderObject& renderObject, const PaintInfo& paintInfo, const FloatRect& r)
{
    // FIXME: adopt subpixel themes.
    IntRect rect = IntRect(r);

    cairo_t* cairoContext = paintInfo.context().platformContext()->cr();
    GtkTextDirection direction = static_cast<GtkTextDirection>(gtkTextDirection(renderObject.style().direction()));

    GRefPtr<GtkStyleContext> parentStyleContext = createStyleContext(ComboBox);

    // Paint the button.
    GRefPtr<GtkStyleContext> buttonStyleContext = createStyleContext(ComboBoxButton, parentStyleContext.get());
    gtk_style_context_set_direction(buttonStyleContext.get(), direction);
    renderButton(this, buttonStyleContext.get(), renderObject, paintInfo, rect);

    // Get the inner rectangle.
    gint focusWidth, focusPad;
    GtkBorder* innerBorderPtr = 0;
    GtkBorder innerBorder = { 1, 1, 1, 1 };
    gtk_style_context_get_style(buttonStyleContext.get(), "inner-border", &innerBorderPtr, "focus-line-width", &focusWidth, "focus-padding", &focusPad, nullptr);
    if (innerBorderPtr) {
        innerBorder = *innerBorderPtr;
        gtk_border_free(innerBorderPtr);
    }

    GtkBorder borderWidth;
    GtkStateFlags state = gtk_style_context_get_state(buttonStyleContext.get());
    gtk_style_context_get_border(buttonStyleContext.get(), state, &borderWidth);

    focusWidth += focusPad;
    IntRect innerRect(
        rect.x() + innerBorder.left + borderWidth.left + focusWidth,
        rect.y() + innerBorder.top + borderWidth.top + focusWidth,
        rect.width() - borderWidth.left - borderWidth.right - innerBorder.left - innerBorder.right - (2 * focusWidth),
        rect.height() - borderWidth.top - borderWidth.bottom - innerBorder.top - innerBorder.bottom - (2 * focusWidth));

    if (isPressed(renderObject)) {
        gint childDisplacementX;
        gint childDisplacementY;
        gtk_style_context_get_style(buttonStyleContext.get(), "child-displacement-x", &childDisplacementX, "child-displacement-y", &childDisplacementY, nullptr);
        innerRect.move(childDisplacementX, childDisplacementY);
    }
    innerRect.setWidth(std::max(1, innerRect.width()));
    innerRect.setHeight(std::max(1, innerRect.height()));

    // Paint the arrow.
    GRefPtr<GtkStyleContext> arrowStyleContext = createStyleContext(ComboBoxArrow, buttonStyleContext.get());
    gtk_style_context_set_direction(arrowStyleContext.get(), direction);

    gfloat arrowScaling;
    gtk_style_context_get_style(parentStyleContext.get(), "arrow-scaling", &arrowScaling, nullptr);

    IntSize arrowSize(minArrowSize, innerRect.height());
    FloatPoint arrowPosition(innerRect.location());
    if (direction == GTK_TEXT_DIR_LTR)
        arrowPosition.move(innerRect.width() - arrowSize.width(), 0);

    // GTK+ actually fetches the xalign and valign values from the widget, but since we
    // don't have a widget here, we are just using the default xalign and valign values of 0.5.
    gint extent = std::min(arrowSize.width(), arrowSize.height()) * arrowScaling;
    arrowPosition.move((arrowSize.width() - extent) / 2, (arrowSize.height() - extent) / 2);

    gtk_style_context_set_state(arrowStyleContext.get(), state);
    gtk_render_arrow(arrowStyleContext.get(), cairoContext, G_PI, arrowPosition.x(), arrowPosition.y(), extent);

    return false;
}
#endif // GTK_CHECK_VERSION(3, 20, 0)

bool RenderThemeGtk::paintMenuListButtonDecorations(const RenderBox& object, const PaintInfo& info, const FloatRect& rect)
{
    return paintMenuList(object, info, rect);
}

bool RenderThemeGtk::isControlStyled(const RenderStyle& style, const BorderData& border, const FillLayer& background, const Color& backgroundColor) const
{
    // To avoid rendering issues with dark themes, if text input elements have color styling, we don't style them with GTK.
    if ((style.appearance() == TextFieldPart || style.appearance() == TextAreaPart || style.appearance() == SearchFieldPart) && style.color() != RenderStyle::initialColor())
        return true;

    return RenderTheme::isControlStyled(style, border, background, backgroundColor);
}

#if GTK_CHECK_VERSION(3, 20, 0)

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


void RenderThemeGtk::adjustTextFieldStyle(StyleResolver&, RenderStyle& style, const Element* element) const
{
    if (!is<HTMLInputElement>(element) || !shouldHaveSpinButton(downcast<HTMLInputElement>(*element)))
        return;

    style.setMinHeight(Length(spinButtonSize().height(), Fixed));

    // The default theme for the GTK+ port uses very wide spin buttons (66px) compared to what other
    // browsers use (~13 px). And unfortunately, most of the web developers won't test how their site
    // renders on WebKitGTK+. To ensure that spin buttons don't end up covering the values of the input
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
    }
    return false;
}
#else
void RenderThemeGtk::adjustTextFieldStyle(StyleResolver&, RenderStyle&, const Element*) const
{
}

bool RenderThemeGtk::paintTextField(const RenderObject& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
{
    GRefPtr<GtkStyleContext> context = createStyleContext(Entry);
    gtk_style_context_set_direction(context.get(), static_cast<GtkTextDirection>(gtkTextDirection(renderObject.style().direction())));

    guint flags = 0;
    if (!isEnabled(renderObject) || isReadOnlyControl(renderObject))
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    else if (isFocused(renderObject))
        flags |= GTK_STATE_FLAG_FOCUSED;
    gtk_style_context_set_state(context.get(), static_cast<GtkStateFlags>(flags));

    gtk_render_background(context.get(), paintInfo.context().platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(context.get(), paintInfo.context().platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());

    if (isFocused(renderObject) && isEnabled(renderObject)) {
        gboolean interiorFocus;
        gint focusWidth, focusPad;
        gtk_style_context_get_style(context.get(), "interior-focus", &interiorFocus, "focus-line-width", &focusWidth, "focus-padding", &focusPad, nullptr);
        if (!interiorFocus) {
            IntRect focusRect(rect);
            focusRect.inflate(focusWidth + focusPad);
            gtk_render_focus(context.get(), paintInfo.context().platformContext()->cr(), focusRect.x(), focusRect.y(), focusRect.width(), focusRect.height());
        }
    }

    return false;
}
#endif

#if GTK_CHECK_VERSION(3, 20, 0)
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
#else
// Defined in GTK+ (gtk/gtkiconfactory.c)
static const gint gtkIconSizeMenu = 16;
static const gint gtkIconSizeSmallToolbar = 18;
static const gint gtkIconSizeButton = 20;
static const gint gtkIconSizeLargeToolbar = 24;
static const gint gtkIconSizeDnd = 32;
static const gint gtkIconSizeDialog = 48;

static GtkIconSize getIconSizeForPixelSize(gint pixelSize)
{
    if (pixelSize < gtkIconSizeSmallToolbar)
        return GTK_ICON_SIZE_MENU;
    if (pixelSize >= gtkIconSizeSmallToolbar && pixelSize < gtkIconSizeButton)
        return GTK_ICON_SIZE_SMALL_TOOLBAR;
    if (pixelSize >= gtkIconSizeButton && pixelSize < gtkIconSizeLargeToolbar)
        return GTK_ICON_SIZE_BUTTON;
    if (pixelSize >= gtkIconSizeLargeToolbar && pixelSize < gtkIconSizeDnd)
        return GTK_ICON_SIZE_LARGE_TOOLBAR;
    if (pixelSize >= gtkIconSizeDnd && pixelSize < gtkIconSizeDialog)
        return GTK_ICON_SIZE_DND;

    return GTK_ICON_SIZE_DIALOG;
}

static void adjustSearchFieldIconStyle(RenderThemePart themePart, RenderStyle& style)
{
    style.resetBorder();
    style.resetPadding();

    GRefPtr<GtkStyleContext> parentContext = createStyleContext(Entry);
    GRefPtr<GtkStyleContext> context = createStyleContext(themePart, parentContext.get());

    GtkBorder padding;
    gtk_style_context_get_padding(context.get(), gtk_style_context_get_state(context.get()), &padding);

    // Get the icon size based on the font size.
    int fontSize = style.computedFontPixelSize();
    if (fontSize < gtkIconSizeMenu) {
        style.setWidth(Length(fontSize + (padding.left + padding.right), Fixed));
        style.setHeight(Length(fontSize + (padding.top + padding.bottom), Fixed));
        return;
    }
    gint width = 0, height = 0;
    gtk_icon_size_lookup(getIconSizeForPixelSize(fontSize), &width, &height);
    style.setWidth(Length(width + (padding.left + padding.right), Fixed));
    style.setHeight(Length(height + (padding.top + padding.bottom), Fixed));
}
#endif

bool RenderThemeGtk::paintTextArea(const RenderObject& o, const PaintInfo& i, const FloatRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeGtk::adjustSearchFieldResultsButtonStyle(StyleResolver& styleResolver, RenderStyle& style, const Element* e) const
{
    adjustSearchFieldCancelButtonStyle(styleResolver, style, e);
}

bool RenderThemeGtk::paintSearchFieldResultsButton(const RenderBox& o, const PaintInfo& i, const IntRect& rect)
{
    return paintSearchFieldResultsDecorationPart(o, i, rect);
}

void RenderThemeGtk::adjustSearchFieldResultsDecorationPartStyle(StyleResolver&, RenderStyle& style, const Element*) const
{
    adjustSearchFieldIconStyle(EntryIconLeft, style);
}

void RenderThemeGtk::adjustSearchFieldCancelButtonStyle(StyleResolver&, RenderStyle& style, const Element*) const
{
    adjustSearchFieldIconStyle(EntryIconRight, style);
}

#if GTK_CHECK_VERSION(3, 20, 0)
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
#else
static bool paintIcon(GtkStyleContext* context, GraphicsContext& graphicsContext, const IntRect& rect, const char* iconName)
{
    GRefPtr<GdkPixbuf> icon = loadThemedIcon(context, iconName, getIconSizeForPixelSize(rect.height()));
    if (!icon)
        return false;

    if (gdk_pixbuf_get_width(icon.get()) > rect.width() || gdk_pixbuf_get_height(icon.get()) > rect.height())
        icon = adoptGRef(gdk_pixbuf_scale_simple(icon.get(), rect.width(), rect.height(), GDK_INTERP_BILINEAR));

    gtk_render_icon(context, graphicsContext.platformContext()->cr(), icon.get(), rect.x(), rect.y());
    return true;
}

static bool paintEntryIcon(RenderThemePart themePart, const char* iconName, GraphicsContext& graphicsContext, const IntRect& rect, GtkTextDirection direction, GtkStateFlags state)
{
    GRefPtr<GtkStyleContext> parentContext = createStyleContext(Entry);
    GRefPtr<GtkStyleContext> context = createStyleContext(themePart, parentContext.get());
    gtk_style_context_set_direction(context.get(), direction);
    gtk_style_context_set_state(context.get(), state);
    return paintIcon(context.get(), graphicsContext, rect, iconName);
}

static IntRect centerRectVerticallyInParentInputElement(const RenderObject& renderObject, const IntRect& rect)
{
    if (!renderObject.node())
        return IntRect();

    // Get the renderer of <input> element.
    Node* input = renderObject.node()->shadowHost();
    if (!input)
        input = renderObject.node();
    if (!is<RenderBox>(*input->renderer()))
        return IntRect();

    // If possible center the y-coordinate of the rect vertically in the parent input element.
    // We also add one pixel here to ensure that the y coordinate is rounded up for box heights
    // that are even, which looks in relation to the box text.
    IntRect inputContentBox = downcast<RenderBox>(*input->renderer()).absoluteContentBox();

    // Make sure the scaled decoration stays square and will fit in its parent's box.
    int iconSize = std::min(inputContentBox.width(), std::min(inputContentBox.height(), rect.height()));
    IntRect scaledRect(rect.x(), inputContentBox.y() + (inputContentBox.height() - iconSize + 1) / 2, iconSize, iconSize);
    return scaledRect;
}

bool RenderThemeGtk::paintSearchFieldResultsDecorationPart(const RenderBox& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    IntRect iconRect = centerRectVerticallyInParentInputElement(renderObject, rect);
    if (iconRect.isEmpty())
        return true;

    return !paintEntryIcon(EntryIconLeft, "edit-find-symbolic", paintInfo.context(), iconRect, gtkTextDirection(renderObject.style().direction()),
        gtkIconStateFlags(this, renderObject));
}

bool RenderThemeGtk::paintSearchFieldCancelButton(const RenderBox& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    IntRect iconRect = centerRectVerticallyInParentInputElement(renderObject, rect);
    if (iconRect.isEmpty())
        return true;

    return !paintEntryIcon(EntryIconRight, "edit-clear-symbolic", paintInfo.context(), iconRect, gtkTextDirection(renderObject.style().direction()),
        gtkIconStateFlags(this, renderObject));
}
#endif // GTK_CHECK_VERSION(3, 20, 0)

void RenderThemeGtk::adjustSearchFieldStyle(StyleResolver&, RenderStyle& style, const Element*) const
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

void RenderThemeGtk::adjustSliderTrackStyle(StyleResolver&, RenderStyle& style, const Element*) const
{
    style.setBoxShadow(nullptr);
}

void RenderThemeGtk::adjustSliderThumbStyle(StyleResolver& styleResolver, RenderStyle& style, const Element* element) const
{
    RenderTheme::adjustSliderThumbStyle(styleResolver, style, element);
    style.setBoxShadow(nullptr);
}

#if GTK_CHECK_VERSION(3, 20, 0)
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
#else
bool RenderThemeGtk::paintSliderTrack(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    ControlPart part = renderObject.style().appearance();
    ASSERT(part == SliderHorizontalPart || part == SliderVerticalPart);

    GRefPtr<GtkStyleContext> parentContext = createStyleContext(Scale);
    gtk_style_context_add_class(parentContext.get(), part == SliderHorizontalPart ? GTK_STYLE_CLASS_HORIZONTAL : GTK_STYLE_CLASS_VERTICAL);
    GRefPtr<GtkStyleContext> context = createStyleContext(ScaleTrough, parentContext.get());
    gtk_style_context_set_direction(context.get(), gtkTextDirection(renderObject.style().direction()));

    if (!isEnabled(renderObject))
        gtk_style_context_set_state(context.get(), GTK_STATE_FLAG_INSENSITIVE);

    IntRect sliderRect = rect;
    // GTK+ uses the slider thumb size and margins to calculate the trough size, but in WebKit we render the thumb and
    // the slider track separately and the track rectangle we receive here can't be used to apply the GTK+ CSS sizes
    // and margins. So we use a maximum fixed size for the trough to match at least Adwaita, but that should look
    // good in other themes as well.
    static const int sliderSize = 4;

    if (part == SliderHorizontalPart) {
        sliderRect.setHeight(std::min(rect.height(), sliderSize));
        sliderRect.move(0, (rect.height() - sliderRect.height()) / 2);
    } else {
        sliderRect.setWidth(std::min(rect.width(), sliderSize));
        sliderRect.move((rect.width() - sliderRect.width()) / 2, 0);
    }

    gtk_render_background(context.get(), paintInfo.context().platformContext()->cr(), sliderRect.x(), sliderRect.y(), sliderRect.width(), sliderRect.height());
    gtk_render_frame(context.get(), paintInfo.context().platformContext()->cr(), sliderRect.x(), sliderRect.y(), sliderRect.width(), sliderRect.height());

    if (isFocused(renderObject)) {
        gint focusWidth, focusPad;
        gtk_style_context_get_style(context.get(), "focus-line-width", &focusWidth, "focus-padding", &focusPad, nullptr);
        IntRect focusRect(sliderRect);
        focusRect.inflate(focusWidth + focusPad);
        gtk_render_focus(context.get(), paintInfo.context().platformContext()->cr(), focusRect.x(), focusRect.y(), focusRect.width(), focusRect.height());
    }

    return false;
}

void RenderThemeGtk::adjustSliderThumbSize(RenderStyle& style, const Element*) const
{
    ControlPart part = style.appearance();
    if (part != SliderThumbHorizontalPart && part != SliderThumbVerticalPart)
        return;

    GRefPtr<GtkStyleContext> context = createStyleContext(Scale);
    gint sliderWidth, sliderLength;
    gtk_style_context_get_style(context.get(), "slider-width", &sliderWidth, "slider-length", &sliderLength, nullptr);

    if (part == SliderThumbHorizontalPart) {
        style.setWidth(Length(sliderLength, Fixed));
        style.setHeight(Length(sliderWidth, Fixed));
        return;
    }
    ASSERT(part == SliderThumbVerticalPart);
    style.setWidth(Length(sliderWidth, Fixed));
    style.setHeight(Length(sliderLength, Fixed));
}

bool RenderThemeGtk::paintSliderThumb(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    ControlPart part = renderObject.style().appearance();
    ASSERT(part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart);

    // FIXME: The entire slider is too wide, stretching the thumb into an oval rather than a circle.
    GRefPtr<GtkStyleContext> parentContext = createStyleContext(Scale);
    gtk_style_context_add_class(parentContext.get(), part == SliderThumbHorizontalPart ? GTK_STYLE_CLASS_HORIZONTAL : GTK_STYLE_CLASS_VERTICAL);
    GRefPtr<GtkStyleContext> troughContext = createStyleContext(ScaleTrough, parentContext.get());
    GRefPtr<GtkStyleContext> context = createStyleContext(ScaleSlider, troughContext.get());
    gtk_style_context_set_direction(context.get(), gtkTextDirection(renderObject.style().direction()));

    guint flags = 0;
    if (!isEnabled(renderObject))
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    else if (isHovered(renderObject))
        flags |= GTK_STATE_FLAG_PRELIGHT;
    if (isPressed(renderObject))
        flags |= GTK_STATE_FLAG_ACTIVE;
    gtk_style_context_set_state(context.get(), static_cast<GtkStateFlags>(flags));

    gtk_render_slider(context.get(), paintInfo.context().platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height(),
        part == SliderThumbHorizontalPart ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);

    return false;
}
#endif

#if GTK_CHECK_VERSION(3, 20, 0)
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
#else
IntRect RenderThemeGtk::progressBarRectForBounds(const RenderObject&, const IntRect& bounds) const
{
    return bounds;
}

bool RenderThemeGtk::paintProgressBar(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!renderObject.isProgress())
        return true;

    GRefPtr<GtkStyleContext> parentContext = createStyleContext(ProgressBar);
    GRefPtr<GtkStyleContext> troughContext = createStyleContext(ProgressBarTrough, parentContext.get());
    GRefPtr<GtkStyleContext> context = createStyleContext(ProgressBarProgress, troughContext.get());

    gtk_render_background(troughContext.get(), paintInfo.context().platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(troughContext.get(), paintInfo.context().platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());

    gtk_style_context_set_state(context.get(), static_cast<GtkStateFlags>(0));

    GtkBorder padding;
    gtk_style_context_get_padding(context.get(), gtk_style_context_get_state(context.get()), &padding);
    IntRect progressRect(
        rect.x() + padding.left,
        rect.y() + padding.top,
        rect.width() - (padding.left + padding.right),
        rect.height() - (padding.top + padding.bottom));
    progressRect = RenderThemeGtk::calculateProgressRect(renderObject, progressRect);

    if (!progressRect.isEmpty()) {
#if GTK_CHECK_VERSION(3, 13, 7)
        gtk_render_background(context.get(), paintInfo.context().platformContext()->cr(), progressRect.x(), progressRect.y(), progressRect.width(), progressRect.height());
        gtk_render_frame(context.get(), paintInfo.context().platformContext()->cr(), progressRect.x(), progressRect.y(), progressRect.width(), progressRect.height());
#else
        gtk_render_activity(context.get(), paintInfo.context().platformContext()->cr(), progressRect.x(), progressRect.y(), progressRect.width(), progressRect.height());
#endif
    }

    return false;
}
#endif // GTK_CHECK_VERSION(3, 20, 0)

#if GTK_CHECK_VERSION(3, 20, 0)
RenderTheme::InnerSpinButtonLayout RenderThemeGtk::innerSpinButtonLayout(const RenderObject& renderObject) const
{
    return renderObject.style().direction() == TextDirection::RTL ? InnerSpinButtonLayout::HorizontalUpLeft : InnerSpinButtonLayout::HorizontalUpRight;
}

void RenderThemeGtk::adjustInnerSpinButtonStyle(StyleResolver&, RenderStyle& style, const Element*) const
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
#else
RenderTheme::InnerSpinButtonLayout RenderThemeGtk::innerSpinButtonLayout(const RenderObject&) const
{
    return InnerSpinButtonLayout::Vertical;
}
static gint spinButtonArrowSize(GtkStyleContext* context)
{
    PangoFontDescription* fontDescription;
    gtk_style_context_get(context, gtk_style_context_get_state(context), "font", &fontDescription, nullptr);
    gint fontSize = pango_font_description_get_size(fontDescription);
    gint arrowSize = std::max(PANGO_PIXELS(fontSize), minSpinButtonArrowSize);
    pango_font_description_free(fontDescription);

    return arrowSize - arrowSize % 2; // Force even.
}

void RenderThemeGtk::adjustInnerSpinButtonStyle(StyleResolver&, RenderStyle& style, const Element*) const
{
    GRefPtr<GtkStyleContext> context = createStyleContext(SpinButton);

    GtkBorder padding;
    gtk_style_context_get_padding(context.get(), gtk_style_context_get_state(context.get()), &padding);

    int width = spinButtonArrowSize(context.get()) + padding.left + padding.right;
    style.setWidth(Length(width, Fixed));
    style.setMinWidth(Length(width, Fixed));
}

static void paintSpinArrowButton(RenderTheme* theme, GtkStyleContext* parentContext, const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect, GtkArrowType arrowType)
{
    ASSERT(arrowType == GTK_ARROW_UP || arrowType == GTK_ARROW_DOWN);

    GRefPtr<GtkStyleContext> context = createStyleContext(arrowType == GTK_ARROW_UP ? SpinButtonUpButton : SpinButtonDownButton, parentContext);
    GtkTextDirection direction = gtk_style_context_get_direction(context.get());
    guint state = static_cast<guint>(gtk_style_context_get_state(context.get()));
    if (!(state & GTK_STATE_FLAG_INSENSITIVE)) {
        if (theme->isPressed(renderObject)) {
            if ((arrowType == GTK_ARROW_UP && theme->isSpinUpButtonPartPressed(renderObject))
                || (arrowType == GTK_ARROW_DOWN && !theme->isSpinUpButtonPartPressed(renderObject)))
                state |= GTK_STATE_FLAG_ACTIVE;
        } else if (theme->isHovered(renderObject)) {
            if ((arrowType == GTK_ARROW_UP && theme->isSpinUpButtonPartHovered(renderObject))
                || (arrowType == GTK_ARROW_DOWN && !theme->isSpinUpButtonPartHovered(renderObject)))
                state |= GTK_STATE_FLAG_PRELIGHT;
        }
    }
    gtk_style_context_set_state(context.get(), static_cast<GtkStateFlags>(state));

    // Paint button.
    IntRect buttonRect(rect);
    guint junction = gtk_style_context_get_junction_sides(context.get());
    if (arrowType == GTK_ARROW_UP)
        junction |= GTK_JUNCTION_BOTTOM;
    else {
        junction |= GTK_JUNCTION_TOP;
        buttonRect.move(0, rect.height() / 2);
    }
    buttonRect.setHeight(rect.height() / 2);
    gtk_style_context_set_junction_sides(context.get(), static_cast<GtkJunctionSides>(junction));

    gtk_render_background(context.get(), paintInfo.context().platformContext()->cr(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());
    gtk_render_frame(context.get(), paintInfo.context().platformContext()->cr(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());

    // Paint arrow centered inside button.
    // This code is based on gtkspinbutton.c code.
    IntRect arrowRect;
    gdouble angle;
    if (arrowType == GTK_ARROW_UP) {
        angle = 0;
        arrowRect.setY(rect.y());
        arrowRect.setHeight(rect.height() / 2 - 2);
    } else {
        angle = G_PI;
        arrowRect.setY(rect.y() + buttonRect.y());
        arrowRect.setHeight(rect.height() - arrowRect.y() - 2);
    }
    arrowRect.setWidth(rect.width() - 3);
    if (direction == GTK_TEXT_DIR_LTR)
        arrowRect.setX(rect.x() + 1);
    else
        arrowRect.setX(rect.x() + 2);

    gint width = arrowRect.width() / 2;
    width -= width % 2 - 1; // Force odd.
    gint height = (width + 1) / 2;

    arrowRect.move((arrowRect.width() - width) / 2, (arrowRect.height() - height) / 2);
    gtk_render_arrow(context.get(), paintInfo.context().platformContext()->cr(), angle, arrowRect.x(), arrowRect.y(), width);
}

bool RenderThemeGtk::paintInnerSpinButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    GRefPtr<GtkStyleContext> context = createStyleContext(SpinButton);
    gtk_style_context_set_direction(context.get(), gtkTextDirection(renderObject.style().direction()));

    guint flags = 0;
    if (!isEnabled(renderObject) || isReadOnlyControl(renderObject))
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    else if (isFocused(renderObject))
        flags |= GTK_STATE_FLAG_FOCUSED;
    gtk_style_context_set_state(context.get(), static_cast<GtkStateFlags>(flags));

    paintSpinArrowButton(this, context.get(), renderObject, paintInfo, rect, GTK_ARROW_UP);
    paintSpinArrowButton(this, context.get(), renderObject, paintInfo, rect, GTK_ARROW_DOWN);

    return false;
}
#endif // GTK_CHECK_VERSION(3, 20, 0)

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

#if GTK_CHECK_VERSION(3, 20, 0)
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
    }

    ASSERT(gadget);
    gadget->setState(state);
    return colorType == StyleColorBackground ? gadget->backgroundColor() : gadget->color();
}
#else
static Color styleColor(RenderThemePart themePart, GtkStateFlags state, StyleColorType colorType)
{
    GRefPtr<GtkStyleContext> context = createStyleContext(themePart);
    gtk_style_context_set_state(context.get(), state);

    GdkRGBA gdkRGBAColor;
    if (colorType == StyleColorBackground)
        gtk_style_context_get_background_color(context.get(), state, &gdkRGBAColor);
    else
        gtk_style_context_get_color(context.get(), state, &gdkRGBAColor);
    return gdkRGBAColor;
}
#endif // GTK_CHECK_VERSION(3, 20, 0)

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

Color RenderThemeGtk::systemColor(CSSValueID cssValueId, OptionSet<StyleColor::Options> options) const
{
    switch (cssValueId) {
    case CSSValueButtontext:
        return styleColor(Button, GTK_STATE_FLAG_ACTIVE, StyleColorForeground);
    case CSSValueCaptiontext:
        return styleColor(Entry, GTK_STATE_FLAG_ACTIVE, StyleColorForeground);
    default:
        return RenderTheme::systemColor(cssValueId, options);
    }
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

#if GTK_CHECK_VERSION(3, 20, 0)
bool RenderThemeGtk::paintMediaButton(const RenderObject& renderObject, GraphicsContext& graphicsContext, const IntRect& rect, const char* iconName)
{
    auto& iconWidget = static_cast<RenderThemeIcon&>(RenderThemeWidget::getOrCreate(RenderThemeWidget::Type::Icon));
    auto& icon = static_cast<RenderThemeIconGadget&>(iconWidget.icon());
    icon.setState(themePartStateFlags(*this, MediaButton, renderObject));
    icon.setIconSize(RenderThemeIconGadget::IconSizeGtk::Menu);
    icon.setIconName(iconName);
    return !icon.render(graphicsContext.platformContext()->cr(), rect);
}
#else
bool RenderThemeGtk::paintMediaButton(const RenderObject& renderObject, GraphicsContext& graphicsContext, const IntRect& rect, const char* iconName)
{
    GRefPtr<GtkStyleContext> context = createStyleContext(MediaButton);
    gtk_style_context_set_direction(context.get(), gtkTextDirection(renderObject.style().direction()));
    gtk_style_context_set_state(context.get(), gtkIconStateFlags(this, renderObject));
    static const unsigned mediaIconSize = 16;
    IntRect iconRect(rect.x() + (rect.width() - mediaIconSize) / 2, rect.y() + (rect.height() - mediaIconSize) / 2, mediaIconSize, mediaIconSize);
    return !paintIcon(context.get(), graphicsContext, iconRect, iconName);
}
#endif // GTK_CHECK_VERSION(3, 20, 0)

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

void RenderThemeGtk::adjustProgressBarStyle(StyleResolver&, RenderStyle& style, const Element*) const
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
    scriptBuilder.append(mediaControlsLocalizedStringsJavaScript, sizeof(mediaControlsLocalizedStringsJavaScript));
    scriptBuilder.append(mediaControlsBaseJavaScript, sizeof(mediaControlsBaseJavaScript));
    scriptBuilder.append(mediaControlsGtkJavaScript, sizeof(mediaControlsGtkJavaScript));
    return scriptBuilder.toString();
}
#endif // ENABLE(VIDEO)

#endif // GTK_API_VERSION_2
}
