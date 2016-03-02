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

#ifndef GTK_API_VERSION_2

#include "CSSValueKeywords.h"
#include "GraphicsContext.h"
#include "GtkVersioning.h"
#include "GRefPtrGtk.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "MediaControlElements.h"
#include "Page.h"
#include "PaintInfo.h"
#include "PlatformContextCairo.h"
#include "RenderBox.h"
#include "RenderElement.h"
#include "ScrollbarThemeGtk.h"
#include "TextDirection.h"
#include "UserAgentStyleSheets.h"
#include <cmath>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

namespace WebCore {

// This is the default value defined by GTK+, where it was defined as MIN_ARROW_SIZE in gtkarrow.c.
static const int minArrowSize = 15;
// This is the default value defined by GTK+, where it was defined as MIN_ARROW_WIDTH in gtkspinbutton.c.
static const int minSpinButtonArrowSize = 6;

enum RenderThemePart {
    Entry,
    EntrySelection,
    EntryIconLeft,
    EntryIconRight,
    Button,
    CheckButton,
    CheckButtonCheck,
    RadioButton,
    RadioButtonRadio,
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

static void gtkStyleChangedCallback(GObject*, GParamSpec*)
{
    static_cast<ScrollbarThemeGtk*>(ScrollbarTheme::theme())->themeChanged();
    Page::updateStyleForAllPagesAfterGlobalChangeInEnvironment();
}

static GRefPtr<GtkStyleContext> createStyleContext(RenderThemePart themePart, GtkStyleContext* parent = nullptr)
{
    static bool initialized = false;
    if (!initialized) {
        GtkSettings* settings = gtk_settings_get_default();
        g_signal_connect(settings, "notify::gtk-theme-name", G_CALLBACK(gtkStyleChangedCallback), 0);
        g_signal_connect(settings, "notify::gtk-color-scheme", G_CALLBACK(gtkStyleChangedCallback), 0);
        initialized = true;
    }

    GRefPtr<GtkWidgetPath> path = adoptGRef(parent ? gtk_widget_path_copy(gtk_style_context_get_path(parent)) : gtk_widget_path_new());

    switch (themePart) {
    case Entry:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_ENTRY);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "entry");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_ENTRY);
#endif
        break;
    case EntrySelection:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_ENTRY);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "selection");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_ENTRY);
#endif
        break;
    case EntryIconLeft:
    case EntryIconRight:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_ENTRY);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "image");
        gtk_widget_path_iter_add_class(path.get(), -1, themePart == EntryIconLeft ? "left" : "right");
#endif
        break;
    case Button:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_BUTTON);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "button");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_BUTTON);
#endif
        gtk_widget_path_iter_add_class(path.get(), -1, "text-button");
        break;
    case CheckButton:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_CHECK_BUTTON);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "checkbutton");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_CHECK);
#endif
        break;
    case CheckButtonCheck:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_CHECK_BUTTON);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "check");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_CHECK);
#endif
        break;
    case RadioButton:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_RADIO_BUTTON);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "radiobutton");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_RADIO);
#endif
        break;
    case RadioButtonRadio:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_RADIO_BUTTON);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "radio");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_RADIO);
#endif
        break;
    case ComboBox:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_COMBO_BOX);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "combobox");
#endif
        break;
    case ComboBoxButton:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_BUTTON);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "button");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_BUTTON);
#endif
        gtk_widget_path_iter_add_class(path.get(), -1, "text-button");
        gtk_widget_path_iter_add_class(path.get(), -1, "combo");
        break;
    case ComboBoxArrow:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_ARROW);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "arrow");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, "arrow");
#endif
        break;
    case Scale:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_SCALE);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "scale");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_SCALE);
#endif
        break;
    case ScaleTrough:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_SCALE);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "trough");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_SCALE);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_TROUGH);
#endif
        break;
    case ScaleSlider:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_SCALE);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "slider");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_SCALE);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_SLIDER);
#endif
        break;
    case ProgressBar:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_PROGRESS_BAR);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "progressbar");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_PROGRESSBAR);
#endif
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_HORIZONTAL);
        break;
    case ProgressBarTrough:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_PROGRESS_BAR);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "trough");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_PROGRESSBAR);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_TROUGH);
#endif
        break;
    case ProgressBarProgress:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_PROGRESS_BAR);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "progress");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_PROGRESSBAR);
#endif
        break;
    case ListBox:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_TREE_VIEW);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "treeview");
#endif
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_VIEW);
        break;
    case SpinButton:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_SPIN_BUTTON);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "spinbutton");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_SPINBUTTON);
#endif
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_HORIZONTAL);
        break;
    case SpinButtonUpButton:
    case SpinButtonDownButton:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_SPIN_BUTTON);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "button");
        gtk_widget_path_iter_add_class(path.get(), -1, themePart == SpinButtonUpButton ? "up" : "down");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_SPINBUTTON);
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_BUTTON);
#endif
        break;
#if ENABLE(VIDEO)
    case MediaButton:
        gtk_widget_path_append_type(path.get(), GTK_TYPE_IMAGE);
#if GTK_CHECK_VERSION(3, 19, 2)
        gtk_widget_path_iter_set_object_name(path.get(), -1, "image");
#else
        gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_IMAGE);
#endif
        break;
#endif // ENABLE(VIDEO)
    }

    GRefPtr<GtkStyleContext> context = adoptGRef(gtk_style_context_new());
    gtk_style_context_set_path(context.get(), path.get());
    gtk_style_context_set_parent(context.get(), parent);
    return context;
}

// This is not a static method, because we want to avoid having GTK+ headers in RenderThemeGtk.h.
extern GtkTextDirection gtkTextDirection(TextDirection);

void RenderThemeGtk::platformInit()
{
}

RenderThemeGtk::~RenderThemeGtk()
{
}

static GtkStateFlags gtkIconStateFlags(RenderTheme* theme, RenderObject* renderObject)
{
    if (!theme->isEnabled(renderObject))
        return GTK_STATE_FLAG_INSENSITIVE;
    if (theme->isPressed(renderObject))
        return GTK_STATE_FLAG_ACTIVE;
    if (theme->isHovered(renderObject))
        return GTK_STATE_FLAG_PRELIGHT;

    return GTK_STATE_FLAG_NORMAL;
}

static void adjustRectForFocus(GtkStyleContext* context, IntRect& rect)
{
    gint focusWidth, focusPad;
    gtk_style_context_get_style(context,
                                "focus-line-width", &focusWidth,
                                "focus-padding", &focusPad, NULL);
    rect.inflate(focusWidth + focusPad);
}

void RenderThemeGtk::adjustRepaintRect(const RenderObject* renderObject, IntRect& rect)
{
    GRefPtr<GtkStyleContext> context;
    bool checkInteriorFocus = false;
    ControlPart part = renderObject->style().appearance();
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

static void setToggleSize(RenderThemePart themePart, RenderStyle* style)
{
    // The width and height are both specified, so we shouldn't change them.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    GRefPtr<GtkStyleContext> context = createStyleContext(themePart);
    // Other ports hard-code this to 13. GTK+ users tend to demand the native look.
    gint indicatorSize;
    gtk_style_context_get_style(context.get(), "indicator-size", &indicatorSize, nullptr);
    IntSize minSize(indicatorSize, indicatorSize);

#if GTK_CHECK_VERSION(3, 19, 7)
    GRefPtr<GtkStyleContext> childContext = createStyleContext(themePart == CheckButton ? CheckButtonCheck : RadioButtonRadio, context.get());
    gint minWidth, minHeight;
    gtk_style_context_get(childContext.get(), gtk_style_context_get_state(childContext.get()), "min-width", &minWidth, "min-height", &minHeight, nullptr);
    if (minWidth)
        minSize.setWidth(minWidth);
    if (minHeight)
        minSize.setHeight(minHeight);
#endif

    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(minSize.width(), Fixed));

    if (style->height().isAuto())
        style->setHeight(Length(minSize.height(), Fixed));
}

static void paintToggle(const RenderThemeGtk* theme, RenderThemePart themePart, RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& fullRect)
{
    GRefPtr<GtkStyleContext> parentContext = createStyleContext(themePart);
    GRefPtr<GtkStyleContext> context = createStyleContext(themePart == CheckButton ? CheckButtonCheck : RadioButtonRadio, parentContext.get());
    gtk_style_context_set_direction(context.get(), static_cast<GtkTextDirection>(gtkTextDirection(renderObject->style().direction())));

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
    gint indicatorSize;
    gtk_style_context_get_style(context.get(), "indicator-size", &indicatorSize, nullptr);
    IntSize minSize(indicatorSize, indicatorSize);

#if GTK_CHECK_VERSION(3, 19, 7)
    gint minWidth, minHeight;
    gtk_style_context_get(context.get(), gtk_style_context_get_state(context.get()), "min-width", &minWidth, "min-height", &minHeight, nullptr);
    if (minWidth)
        minSize.setWidth(minWidth);
    if (minHeight)
        minSize.setHeight(minHeight);
#endif

    IntRect rect(fullRect);
    if (rect.width() > minSize.width()) {
        rect.inflateX(-(rect.width() - minSize.width()) / 2);
        rect.setWidth(minSize.width()); // In case rect.width() was equal to minSize.width() + 1.
    }

    if (rect.height() > minSize.height()) {
        rect.inflateY(-(rect.height() - minSize.height()) / 2);
        rect.setHeight(minSize.height()); // In case rect.height() was equal to minSize.height() + 1.
    }

    gtk_render_background(context.get(), paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(context.get(), paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());

    if (themePart == CheckButton)
        gtk_render_check(context.get(), paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());
    else
        gtk_render_option(context.get(), paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());

    if (theme->isFocused(renderObject)) {
        IntRect indicatorRect(rect);
        gint indicatorSpacing;
        gtk_style_context_get_style(context.get(), "indicator-spacing", &indicatorSpacing, nullptr);
        indicatorRect.inflate(indicatorSpacing);
        gtk_render_focus(context.get(), paintInfo.context->platformContext()->cr(), indicatorRect.x(), indicatorRect.y(),
                         indicatorRect.width(), indicatorRect.height());
    }
}

void RenderThemeGtk::setCheckboxSize(RenderStyle* style) const
{
    setToggleSize(CheckButton, style);
}

bool RenderThemeGtk::paintCheckbox(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    paintToggle(this, CheckButton, renderObject, paintInfo, rect);
    return false;
}

void RenderThemeGtk::setRadioSize(RenderStyle* style) const
{
    setToggleSize(RadioButton, style);
}

bool RenderThemeGtk::paintRadio(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    paintToggle(this, RadioButton, renderObject, paintInfo, rect);
    return false;
}

static void renderButton(RenderTheme* theme, GtkStyleContext* context, RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    IntRect buttonRect(rect);

    guint flags = 0;
    if (!theme->isEnabled(renderObject) || theme->isReadOnlyControl(renderObject))
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    else if (theme->isHovered(renderObject))
        flags |= GTK_STATE_FLAG_PRELIGHT;
    if (theme->isPressed(renderObject))
        flags |= GTK_STATE_FLAG_ACTIVE;
    gtk_style_context_set_state(context, static_cast<GtkStateFlags>(flags));

    if (theme->isDefault(renderObject)) {
        GtkBorder* borderPtr = 0;
        GtkBorder border = { 1, 1, 1, 1 };

        gtk_style_context_get_style(context, "default-border", &borderPtr, NULL);
        if (borderPtr) {
            border = *borderPtr;
            gtk_border_free(borderPtr);
        }

        buttonRect.move(border.left, border.top);
        buttonRect.setWidth(buttonRect.width() - (border.left + border.right));
        buttonRect.setHeight(buttonRect.height() - (border.top + border.bottom));

        gtk_style_context_add_class(context, GTK_STYLE_CLASS_DEFAULT);
    }

    gtk_render_background(context, paintInfo.context->platformContext()->cr(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());
    gtk_render_frame(context, paintInfo.context->platformContext()->cr(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());

    if (theme->isFocused(renderObject)) {
        gint focusWidth, focusPad;
        gboolean displaceFocus, interiorFocus;
        gtk_style_context_get_style(context,
                                    "focus-line-width", &focusWidth,
                                    "focus-padding", &focusPad,
                                    "interior-focus", &interiorFocus,
                                    "displace-focus", &displaceFocus,
                                    NULL);

        if (interiorFocus) {
            GtkBorder borderWidth;
            gtk_style_context_get_border(context, gtk_style_context_get_state(context), &borderWidth);

            buttonRect = IntRect(buttonRect.x() + borderWidth.left + focusPad, buttonRect.y() + borderWidth.top + focusPad,
                                 buttonRect.width() - (2 * focusPad + borderWidth.left + borderWidth.right),
                                 buttonRect.height() - (2 * focusPad + borderWidth.top + borderWidth.bottom));
        } else
            buttonRect.inflate(focusWidth + focusPad);

        if (displaceFocus && theme->isPressed(renderObject)) {
            gint childDisplacementX;
            gint childDisplacementY;
            gtk_style_context_get_style(context,
                                        "child-displacement-x", &childDisplacementX,
                                        "child-displacement-y", &childDisplacementY,
                                        NULL);
            buttonRect.move(childDisplacementX, childDisplacementY);
        }

        gtk_render_focus(context, paintInfo.context->platformContext()->cr(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());
    }
}
bool RenderThemeGtk::paintButton(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    GRefPtr<GtkStyleContext> context = createStyleContext(Button);
    gtk_style_context_set_direction(context.get(), static_cast<GtkTextDirection>(gtkTextDirection(renderObject->style().direction())));

    renderButton(this, context.get(), renderObject, paintInfo, rect);

    return false;
}

static void getComboBoxMetrics(RenderStyle* style, GtkBorder& border, int& focus)
{
    // If this menu list button isn't drawn using the native theme, we
    // don't add any extra padding beyond what WebCore already uses.
    if (style->appearance() == NoControlPart)
        return;

    GRefPtr<GtkStyleContext> parentContext = createStyleContext(ComboBox);
    GRefPtr<GtkStyleContext> context = createStyleContext(ComboBoxButton, parentContext.get());
    gtk_style_context_set_direction(context.get(), static_cast<GtkTextDirection>(gtkTextDirection(style->direction())));
    gtk_style_context_set_state(context.get(), static_cast<GtkStateFlags>(0));
    gtk_style_context_get_border(context.get(), gtk_style_context_get_state(context.get()), &border);

    gboolean interiorFocus;
    gint focusWidth, focusPad;
    gtk_style_context_get_style(context.get(), "interior-focus", &interiorFocus, "focus-line-width", &focusWidth, "focus-padding", &focusPad, nullptr);
    focus = interiorFocus ? focusWidth + focusPad : 0;
}

int RenderThemeGtk::popupInternalPaddingLeft(RenderStyle* style) const
{
    GtkBorder borderWidth = { 0, 0, 0, 0 };
    int focusWidth = 0;
    getComboBoxMetrics(style, borderWidth, focusWidth);
    int left = borderWidth.left + focusWidth;
    if (style->direction() == RTL)
        left += minArrowSize;
    return left;
}

int RenderThemeGtk::popupInternalPaddingRight(RenderStyle* style) const
{
    GtkBorder borderWidth = { 0, 0, 0, 0 };
    int focusWidth = 0;
    getComboBoxMetrics(style, borderWidth, focusWidth);
    int right = borderWidth.right + focusWidth;
    if (style->direction() == LTR)
        right += minArrowSize;
    return right;
}

int RenderThemeGtk::popupInternalPaddingTop(RenderStyle* style) const
{
    GtkBorder borderWidth = { 0, 0, 0, 0 };
    int focusWidth = 0;
    getComboBoxMetrics(style, borderWidth, focusWidth);
    return borderWidth.top + focusWidth;
}

int RenderThemeGtk::popupInternalPaddingBottom(RenderStyle* style) const
{
    GtkBorder borderWidth = { 0, 0, 0, 0 };
    int focusWidth = 0;
    getComboBoxMetrics(style, borderWidth, focusWidth);
    return borderWidth.bottom + focusWidth;
}

bool RenderThemeGtk::paintMenuList(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    cairo_t* cairoContext = paintInfo.context->platformContext()->cr();
    GtkTextDirection direction = static_cast<GtkTextDirection>(gtkTextDirection(renderObject->style().direction()));

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
    IntRect innerRect(rect.x() + innerBorder.left + borderWidth.left + focusWidth,
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

#if GTK_CHECK_VERSION(3, 19, 2)
    // arrow-scaling style property is now deprecated and ignored.
    gfloat arrowScaling = 1.;
#else
    gfloat arrowScaling;
    gtk_style_context_get_style(parentStyleContext.get(), "arrow-scaling", &arrowScaling, nullptr);
#endif

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

bool RenderThemeGtk::paintTextField(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    GRefPtr<GtkStyleContext> context = createStyleContext(Entry);
    gtk_style_context_set_direction(context.get(), static_cast<GtkTextDirection>(gtkTextDirection(renderObject->style().direction())));

    guint flags = 0;
    if (!isEnabled(renderObject) || isReadOnlyControl(renderObject))
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    else if (isFocused(renderObject))
        flags |= GTK_STATE_FLAG_FOCUSED;
    gtk_style_context_set_state(context.get(), static_cast<GtkStateFlags>(flags));

    gtk_render_background(context.get(), paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(context.get(), paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());

    if (isFocused(renderObject) && isEnabled(renderObject)) {
        gboolean interiorFocus;
        gint focusWidth, focusPad;
        gtk_style_context_get_style(context.get(), "interior-focus", &interiorFocus, "focus-line-width", &focusWidth, "focus-padding", &focusPad, nullptr);
        if (!interiorFocus) {
            IntRect focusRect(rect);
            focusRect.inflate(focusWidth + focusPad);
            gtk_render_focus(context.get(), paintInfo.context->platformContext()->cr(), focusRect.x(), focusRect.y(), focusRect.width(), focusRect.height());
        }
    }

    return false;
}

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

static void adjustSearchFieldIconStyle(RenderThemePart themePart, RenderStyle* style)
{
    style->resetBorder();
    style->resetPadding();

    GRefPtr<GtkStyleContext> parentContext = createStyleContext(Entry);
    GRefPtr<GtkStyleContext> context = createStyleContext(themePart, parentContext.get());

    GtkBorder padding;
    gtk_style_context_get_padding(context.get(), gtk_style_context_get_state(context.get()), &padding);

    // Get the icon size based on the font size.
    int fontSize = style->fontSize();
    if (fontSize < gtkIconSizeMenu) {
        style->setWidth(Length(fontSize + (padding.left + padding.right), Fixed));
        style->setHeight(Length(fontSize + (padding.top + padding.bottom), Fixed));
        return;
    }
    gint width = 0, height = 0;
    gtk_icon_size_lookup(getIconSizeForPixelSize(fontSize), &width, &height);
    style->setWidth(Length(width + (padding.left + padding.right), Fixed));
    style->setHeight(Length(height + (padding.top + padding.bottom), Fixed));
}

void RenderThemeGtk::adjustSearchFieldResultsDecorationPartStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    adjustSearchFieldIconStyle(EntryIconLeft, style);
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
    GtkIconInfo* iconInfo = gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_default(), icon.get(), std::min(width, height), static_cast<GtkIconLookupFlags>(lookupFlags));
    if (!iconInfo)
        return nullptr;

    GdkPixbuf* pixbuf = gtk_icon_info_load_symbolic_for_context(iconInfo, context, nullptr, nullptr);
    gtk_icon_info_free(iconInfo);

    return adoptGRef(pixbuf);
}

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

static IntRect centerRectVerticallyInParentInputElement(RenderObject* renderObject, const IntRect& rect)
{
    // Get the renderer of <input> element.
    Node* input = renderObject->node()->shadowHost();
    if (!input)
        input = renderObject->node();
    if (!input->renderer()->isBox())
        return IntRect();

    // If possible center the y-coordinate of the rect vertically in the parent input element.
    // We also add one pixel here to ensure that the y coordinate is rounded up for box heights
    // that are even, which looks in relation to the box text.
    IntRect inputContentBox = toRenderBox(input->renderer())->absoluteContentBox();

    // Make sure the scaled decoration stays square and will fit in its parent's box.
    int iconSize = std::min(inputContentBox.width(), std::min(inputContentBox.height(), rect.height()));
    IntRect scaledRect(rect.x(), inputContentBox.y() + (inputContentBox.height() - iconSize + 1) / 2, iconSize, iconSize);
    return scaledRect;
}

bool RenderThemeGtk::paintSearchFieldResultsDecorationPart(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    IntRect iconRect = centerRectVerticallyInParentInputElement(renderObject, rect);
    if (iconRect.isEmpty())
        return true;

    return !paintEntryIcon(EntryIconLeft, "edit-find-symbolic", *paintInfo.context, iconRect, gtkTextDirection(renderObject->style().direction()),
        gtkIconStateFlags(this, renderObject));
}

void RenderThemeGtk::adjustSearchFieldCancelButtonStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    adjustSearchFieldIconStyle(EntryIconRight, style);
}

bool RenderThemeGtk::paintSearchFieldCancelButton(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    IntRect iconRect = centerRectVerticallyInParentInputElement(renderObject, rect);
    if (iconRect.isEmpty())
        return true;

    return !paintEntryIcon(EntryIconRight, "edit-clear-symbolic", *paintInfo.context, iconRect, gtkTextDirection(renderObject->style().direction()),
        gtkIconStateFlags(this, renderObject));
}

bool RenderThemeGtk::paintCapsLockIndicator(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    // The other paint methods don't need to check whether painting is disabled because RenderTheme already checks it
    // before calling them, but paintCapsLockIndicator() is called by RenderTextControlSingleLine which doesn't check it.
    if (paintInfo.context->paintingDisabled())
        return true;

    int iconSize = std::min(rect.width(), rect.height());
    // GTK+ locates the icon right aligned in the entry. The given rectangle is already
    // centered vertically by RenderTextControlSingleLine.
    IntRect iconRect(rect.x() + rect.width() - iconSize,
                     rect.y() + (rect.height() - iconSize) / 2,
                     iconSize, iconSize);

    return !paintEntryIcon(EntryIconRight, "dialog-warning-symbolic", *paintInfo.context, iconRect, gtkTextDirection(renderObject->style().direction()),
        gtkIconStateFlags(this, renderObject));
}

bool RenderThemeGtk::paintSliderTrack(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    ControlPart part = renderObject->style().appearance();
    ASSERT_UNUSED(part, part == SliderHorizontalPart || part == SliderVerticalPart);

    GRefPtr<GtkStyleContext> parentContext = createStyleContext(Scale);
    gtk_style_context_add_class(parentContext.get(), part == SliderHorizontalPart ? GTK_STYLE_CLASS_HORIZONTAL : GTK_STYLE_CLASS_VERTICAL);
    GRefPtr<GtkStyleContext> context = createStyleContext(ScaleTrough, parentContext.get());
    gtk_style_context_set_direction(context.get(), gtkTextDirection(renderObject->style().direction()));

    if (!isEnabled(renderObject) || isReadOnlyControl(renderObject))
        gtk_style_context_set_state(context.get(), GTK_STATE_FLAG_INSENSITIVE);

    gtk_render_background(context.get(), paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(context.get(), paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());

    if (isFocused(renderObject)) {
        gint focusWidth, focusPad;
        gtk_style_context_get_style(context.get(), "focus-line-width", &focusWidth, "focus-padding", &focusPad, nullptr);
        IntRect focusRect(rect);
        focusRect.inflate(focusWidth + focusPad);
        gtk_render_focus(context.get(), paintInfo.context->platformContext()->cr(), focusRect.x(), focusRect.y(), focusRect.width(), focusRect.height());
    }

    return false;
}

bool RenderThemeGtk::paintSliderThumb(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    ControlPart part = renderObject->style().appearance();
    ASSERT(part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart);

    GRefPtr<GtkStyleContext> parentContext = createStyleContext(Scale);
    gtk_style_context_add_class(parentContext.get(), part == SliderThumbHorizontalPart ? GTK_STYLE_CLASS_HORIZONTAL : GTK_STYLE_CLASS_VERTICAL);
    GRefPtr<GtkStyleContext> troughContext = createStyleContext(ScaleTrough, parentContext.get());
    GRefPtr<GtkStyleContext> context = createStyleContext(ScaleSlider, troughContext.get());
    gtk_style_context_set_direction(context.get(), gtkTextDirection(renderObject->style().direction()));

    guint flags = 0;
    if (!isEnabled(renderObject) || isReadOnlyControl(renderObject))
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    else if (isHovered(renderObject))
        flags |= GTK_STATE_FLAG_PRELIGHT;
    if (isPressed(renderObject))
        flags |= GTK_STATE_FLAG_ACTIVE;
    gtk_style_context_set_state(context.get(), static_cast<GtkStateFlags>(flags));

    gtk_render_slider(context.get(), paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height(),
        part == SliderThumbHorizontalPart ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);

    return false;
}

void RenderThemeGtk::adjustSliderThumbSize(RenderStyle* style, Element*) const
{
    ControlPart part = style->appearance();
    if (part != SliderThumbHorizontalPart && part != SliderThumbVerticalPart)
        return;

    GRefPtr<GtkStyleContext> context = createStyleContext(Scale);
    gint sliderWidth, sliderLength;
    gtk_style_context_get_style(context.get(), "slider-width", &sliderWidth, "slider-length", &sliderLength, nullptr);
    if (part == SliderThumbHorizontalPart) {
        style->setWidth(Length(sliderLength, Fixed));
        style->setHeight(Length(sliderWidth, Fixed));
        return;
    }
    ASSERT(part == SliderThumbVerticalPart);
    style->setWidth(Length(sliderWidth, Fixed));
    style->setHeight(Length(sliderLength, Fixed));
}

#if ENABLE(PROGRESS_ELEMENT)
bool RenderThemeGtk::paintProgressBar(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!renderObject->isProgress())
        return true;

    GRefPtr<GtkStyleContext> parentContext = createStyleContext(ProgressBar);
    GRefPtr<GtkStyleContext> troughContext = createStyleContext(ProgressBarTrough, parentContext.get());
    GRefPtr<GtkStyleContext> context = createStyleContext(ProgressBarProgress, troughContext.get());

    gtk_render_background(troughContext.get(), paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(troughContext.get(), paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());

    gtk_style_context_set_state(context.get(), static_cast<GtkStateFlags>(0));

    GtkBorder padding;
    gtk_style_context_get_padding(context.get(), gtk_style_context_get_state(context.get()), &padding);
    IntRect progressRect(rect.x() + padding.left, rect.y() + padding.top,
                         rect.width() - (padding.left + padding.right),
                         rect.height() - (padding.top + padding.bottom));
    progressRect = RenderThemeGtk::calculateProgressRect(renderObject, progressRect);

    if (!progressRect.isEmpty()) {
#if GTK_CHECK_VERSION(3, 13, 7)
        gtk_render_background(context.get(), paintInfo.context->platformContext()->cr(), progressRect.x(), progressRect.y(), progressRect.width(), progressRect.height());
        gtk_render_frame(context.get(), paintInfo.context->platformContext()->cr(), progressRect.x(), progressRect.y(), progressRect.width(), progressRect.height());
#else
        gtk_render_activity(context.get(), paintInfo.context->platformContext()->cr(), progressRect.x(), progressRect.y(), progressRect.width(), progressRect.height());
#endif
    }

    return false;
}
#endif

static gint spinButtonArrowSize(GtkStyleContext* context)
{
    PangoFontDescription* fontDescription;
    gtk_style_context_get(context, gtk_style_context_get_state(context), "font", &fontDescription, nullptr);
    gint fontSize = pango_font_description_get_size(fontDescription);
    gint arrowSize = std::max(PANGO_PIXELS(fontSize), minSpinButtonArrowSize);
    pango_font_description_free(fontDescription);

    return arrowSize - arrowSize % 2; // Force even.
}

void RenderThemeGtk::adjustInnerSpinButtonStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    GRefPtr<GtkStyleContext> context = createStyleContext(SpinButton);

    GtkBorder padding;
    gtk_style_context_get_padding(context.get(), gtk_style_context_get_state(context.get()), &padding);

    int width = spinButtonArrowSize(context.get()) + padding.left + padding.right;
    style->setWidth(Length(width, Fixed));
    style->setMinWidth(Length(width, Fixed));
}

static void paintSpinArrowButton(RenderTheme* theme, GtkStyleContext* parentContext, RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect, GtkArrowType arrowType)
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

    gtk_render_background(context.get(), paintInfo.context->platformContext()->cr(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());
    gtk_render_frame(context.get(), paintInfo.context->platformContext()->cr(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());

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
    gtk_render_arrow(context.get(), paintInfo.context->platformContext()->cr(), angle, arrowRect.x(), arrowRect.y(), width);
}

bool RenderThemeGtk::paintInnerSpinButton(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    GRefPtr<GtkStyleContext> context = createStyleContext(SpinButton);
    gtk_style_context_set_direction(context.get(), gtkTextDirection(renderObject->style().direction()));

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

enum StyleColorType { StyleColorBackground, StyleColorForeground };

static Color styleColor(RenderThemePart themePart, GtkStateFlags state, StyleColorType colorType)
{
    GRefPtr<GtkStyleContext> parentContext;
    RenderThemePart part = themePart;
    if (themePart == Entry && (state & GTK_STATE_FLAG_SELECTED)) {
        parentContext = createStyleContext(Entry);
        part = EntrySelection;
    }

    GRefPtr<GtkStyleContext> context = createStyleContext(part, parentContext.get());
    gtk_style_context_set_state(context.get(), state);

    GdkRGBA gdkRGBAColor;
    if (colorType == StyleColorBackground)
        gtk_style_context_get_background_color(context.get(), state, &gdkRGBAColor);
    else
        gtk_style_context_get_color(context.get(), state, &gdkRGBAColor);
    return gdkRGBAColor;
}

Color RenderThemeGtk::platformActiveSelectionBackgroundColor() const
{
    return styleColor(Entry, static_cast<GtkStateFlags>(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_FOCUSED), StyleColorBackground);
}

Color RenderThemeGtk::platformInactiveSelectionBackgroundColor() const
{
    return styleColor(Entry, GTK_STATE_FLAG_SELECTED, StyleColorBackground);
}

Color RenderThemeGtk::platformActiveSelectionForegroundColor() const
{
    return styleColor(Entry, static_cast<GtkStateFlags>(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_FOCUSED), StyleColorForeground);
}

Color RenderThemeGtk::platformInactiveSelectionForegroundColor() const
{
    return styleColor(Entry, GTK_STATE_FLAG_SELECTED, StyleColorForeground);
}

Color RenderThemeGtk::activeListBoxSelectionBackgroundColor() const
{
    return styleColor(ListBox, static_cast<GtkStateFlags>(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_FOCUSED), StyleColorBackground);
}

Color RenderThemeGtk::inactiveListBoxSelectionBackgroundColor() const
{
    return styleColor(ListBox, GTK_STATE_FLAG_SELECTED, StyleColorBackground);
}

Color RenderThemeGtk::activeListBoxSelectionForegroundColor() const
{
    return styleColor(ListBox, static_cast<GtkStateFlags>(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_FOCUSED), StyleColorForeground);
}

Color RenderThemeGtk::inactiveListBoxSelectionForegroundColor() const
{
    return styleColor(ListBox, GTK_STATE_FLAG_SELECTED, StyleColorForeground);
}

Color RenderThemeGtk::systemColor(CSSValueID cssValueId) const
{
    switch (cssValueId) {
    case CSSValueButtontext:
        return styleColor(Button, GTK_STATE_FLAG_ACTIVE, StyleColorForeground);
    case CSSValueCaptiontext:
        return styleColor(Entry, GTK_STATE_FLAG_ACTIVE, StyleColorForeground);
    default:
        return RenderTheme::systemColor(cssValueId);
    }
}

#if ENABLE(VIDEO)
bool RenderThemeGtk::paintMediaButton(RenderObject* renderObject, GraphicsContext* graphicsContext, const IntRect& rect, const char* iconName, const char*)
{
    GRefPtr<GtkStyleContext> context = createStyleContext(MediaButton);
    gtk_style_context_set_direction(context.get(), gtkTextDirection(renderObject->style().direction()));
    gtk_style_context_set_state(context.get(), gtkIconStateFlags(this, renderObject));
    static const unsigned mediaIconSize = 16;
    IntRect iconRect(rect.x() + (rect.width() - mediaIconSize) / 2, rect.y() + (rect.height() - mediaIconSize) / 2, mediaIconSize, mediaIconSize);
    return !paintIcon(context.get(), *graphicsContext, iconRect, iconName);
}
#endif

} // namespace WebCore

#endif // !GTK_API_VERSION_2
