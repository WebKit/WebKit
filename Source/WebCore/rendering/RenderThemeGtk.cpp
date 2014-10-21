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
#include "ExceptionCodePlaceholder.h"
#include "FileList.h"
#include "FileSystem.h"
#include "FontDescription.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "GtkVersioning.h"
#include "HTMLMediaElement.h"
#include "LocalizedStrings.h"
#include "MediaControlElements.h"
#include "NamedNodeMap.h"
#include "Page.h"
#include "PaintInfo.h"
#include "PlatformContextCairo.h"
#include "RenderBox.h"
#include "RenderObject.h"
#include "RenderProgress.h"
#include "ScrollbarThemeGtk.h"
#include "StringTruncator.h"
#include "TimeRanges.h"
#include "UserAgentScripts.h"
#include "UserAgentStyleSheets.h"
#include <cmath>
#include <gdk/gdk.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/gobject/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

PassRefPtr<RenderTheme> RenderThemeGtk::create()
{
    return adoptRef(new RenderThemeGtk());
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page*)
{
    static RenderTheme* rt = RenderThemeGtk::create().leakRef();
    return rt;
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

void RenderThemeGtk::systemFont(CSSValueID, FontDescription& fontDescription) const
{
    GtkSettings* settings = gtk_settings_get_default();
    if (!settings)
        return;

    // This will be a font selection string like "Sans 10" so we cannot use it as the family name.
    GUniqueOutPtr<gchar> fontName;
    g_object_get(settings, "gtk-font-name", &fontName.outPtr(), nullptr);

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
    fontDescription.setGenericFamily(FontDescription::NoFamily);
    fontDescription.setWeight(FontWeightNormal);
    fontDescription.setItalic(false);
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

// This is the default value defined by GTK+, where it was defined as MIN_ARROW_SIZE in gtkarrow.c.
static const int minArrowSize = 15;
// This is the default value defined by GTK+, where it was defined as MIN_ARROW_WIDTH in gtkspinbutton.c.
static const int minSpinButtonArrowSize = 6;

typedef HashMap<GType, GRefPtr<GtkStyleContext>> StyleContextMap;
static StyleContextMap& styleContextMap();

static void gtkStyleChangedCallback(GObject*, GParamSpec*)
{
    for (const auto& styleContext : styleContextMap())
        gtk_style_context_invalidate(styleContext.value.get());
    static_cast<ScrollbarThemeGtk*>(ScrollbarTheme::theme())->themeChanged();
    Page::updateStyleForAllPagesAfterGlobalChangeInEnvironment();
}

static StyleContextMap& styleContextMap()
{
    static NeverDestroyed<StyleContextMap> map;
    static bool initialized = false;
    if (!initialized) {
        GtkSettings* settings = gtk_settings_get_default();
        g_signal_connect(settings, "notify::gtk-theme-name", G_CALLBACK(gtkStyleChangedCallback), nullptr);
        g_signal_connect(settings, "notify::gtk-color-scheme", G_CALLBACK(gtkStyleChangedCallback), nullptr);
        initialized = true;
    }
    return map;
}

static GtkStyleContext* getStyleContext(GType widgetType)
{
    StyleContextMap::AddResult result = styleContextMap().add(widgetType, nullptr);
    if (!result.isNewEntry)
        return result.iterator->value.get();

    GtkWidgetPath* path = gtk_widget_path_new();
    gtk_widget_path_append_type(path, widgetType);

    if (widgetType == GTK_TYPE_ENTRY)
        gtk_widget_path_iter_add_class(path, 0, GTK_STYLE_CLASS_ENTRY);
    else if (widgetType == GTK_TYPE_ARROW)
        gtk_widget_path_iter_add_class(path, 0, "arrow");
    else if (widgetType == GTK_TYPE_BUTTON) {
        gtk_widget_path_iter_add_class(path, 0, GTK_STYLE_CLASS_BUTTON);
        gtk_widget_path_iter_add_class(path, 1, "text-button");
    } else if (widgetType == GTK_TYPE_SCALE)
        gtk_widget_path_iter_add_class(path, 0, GTK_STYLE_CLASS_SCALE);
    else if (widgetType == GTK_TYPE_SEPARATOR)
        gtk_widget_path_iter_add_class(path, 0, GTK_STYLE_CLASS_SEPARATOR);
    else if (widgetType == GTK_TYPE_PROGRESS_BAR)
        gtk_widget_path_iter_add_class(path, 0, GTK_STYLE_CLASS_PROGRESSBAR);
    else if (widgetType == GTK_TYPE_SPIN_BUTTON)
        gtk_widget_path_iter_add_class(path, 0, GTK_STYLE_CLASS_SPINBUTTON);
    else if (widgetType == GTK_TYPE_TREE_VIEW)
        gtk_widget_path_iter_add_class(path, 0, GTK_STYLE_CLASS_VIEW);
    else if (widgetType == GTK_TYPE_CHECK_BUTTON)
        gtk_widget_path_iter_add_class(path, 0, GTK_STYLE_CLASS_CHECK);
    else if (widgetType == GTK_TYPE_RADIO_BUTTON)
        gtk_widget_path_iter_add_class(path, 0, GTK_STYLE_CLASS_RADIO);

    GRefPtr<GtkStyleContext> context = adoptGRef(gtk_style_context_new());
    gtk_style_context_set_path(context.get(), path);
    gtk_widget_path_free(path);

    result.iterator->value = context;
    return context.get();
}

static GRefPtr<GdkPixbuf> getStockIconForWidgetType(GType widgetType, const char* iconName, gint direction, gint state, gint iconSize)
{
    ASSERT(iconName);

    GtkStyleContext* context = getStyleContext(widgetType);
    GtkIconSet* iconSet = gtk_style_context_lookup_icon_set(context, iconName);

    gtk_style_context_save(context);

    guint flags = 0;
    if (state == GTK_STATE_PRELIGHT)
        flags |= GTK_STATE_FLAG_PRELIGHT;
    else if (state == GTK_STATE_INSENSITIVE)
        flags |= GTK_STATE_FLAG_INSENSITIVE;

    gtk_style_context_set_state(context, static_cast<GtkStateFlags>(flags));
    gtk_style_context_set_direction(context, static_cast<GtkTextDirection>(direction));
    GdkPixbuf* icon = gtk_icon_set_render_icon_pixbuf(iconSet, context, static_cast<GtkIconSize>(iconSize));

    gtk_style_context_restore(context);

    return adoptGRef(icon);
}

static GRefPtr<GdkPixbuf> getStockSymbolicIconForWidgetType(GType widgetType, const char* symbolicIconName, const char* fallbackStockIconName, gint direction, gint state, gint iconSize)
{
    GtkStyleContext* context = getStyleContext(widgetType);

    gtk_style_context_save(context);

    guint flags = 0;
    if (state == GTK_STATE_PRELIGHT)
        flags |= GTK_STATE_FLAG_PRELIGHT;
    else if (state == GTK_STATE_INSENSITIVE)
        flags |= GTK_STATE_FLAG_INSENSITIVE;

    gtk_style_context_set_state(context, static_cast<GtkStateFlags>(flags));
    gtk_style_context_set_direction(context, static_cast<GtkTextDirection>(direction));
    GtkIconInfo* info = gtk_icon_theme_lookup_icon(gtk_icon_theme_get_default(), symbolicIconName, iconSize,
        static_cast<GtkIconLookupFlags>(GTK_ICON_LOOKUP_FORCE_SVG | GTK_ICON_LOOKUP_FORCE_SIZE));
    GdkPixbuf* icon = 0;
    if (info) {
        icon = gtk_icon_info_load_symbolic_for_context(info, context, 0, 0);
        gtk_icon_info_free(info);
    }

    gtk_style_context_restore(context);

    if (!icon) {
        if (!fallbackStockIconName)
            return nullptr;
        return getStockIconForWidgetType(widgetType, fallbackStockIconName, direction, state, iconSize);
    }

    return adoptGRef(icon);
}

#if ENABLE(VIDEO)
static HTMLMediaElement* getMediaElementFromRenderObject(const RenderObject& o)
{
    Node* node = o.node();
    Node* mediaNode = node ? node->shadowHost() : 0;
    if (!mediaNode)
        mediaNode = node;
    if (!mediaNode || !mediaNode->isElementNode() || !toElement(mediaNode)->isMediaElement())
        return 0;

    return toHTMLMediaElement(mediaNode);
}

void RenderThemeGtk::initMediaColors()
{
    GdkRGBA color;
    GtkStyleContext* containerContext = getStyleContext(GTK_TYPE_CONTAINER);

    gtk_style_context_get_background_color(containerContext, GTK_STATE_FLAG_NORMAL, &color);
    m_panelColor = color;
    gtk_style_context_get_background_color(containerContext, GTK_STATE_FLAG_ACTIVE, &color);
    m_sliderColor = color;
    gtk_style_context_get_background_color(containerContext, GTK_STATE_FLAG_SELECTED, &color);
    m_sliderThumbColor = color;
}

void RenderThemeGtk::initMediaButtons()
{
    static bool iconsInitialized = false;

    if (iconsInitialized)
        return;

    GRefPtr<GtkIconFactory> iconFactory = adoptGRef(gtk_icon_factory_new());
    GtkIconSource* iconSource = gtk_icon_source_new();
    const char* icons[] = { "audio-volume-high", "audio-volume-muted" };

    gtk_icon_factory_add_default(iconFactory.get());

    for (size_t i = 0; i < G_N_ELEMENTS(icons); ++i) {
        gtk_icon_source_set_icon_name(iconSource, icons[i]);
        GtkIconSet* iconSet = gtk_icon_set_new();
        gtk_icon_set_add_source(iconSet, iconSource);
        gtk_icon_factory_add(iconFactory.get(), icons[i], iconSet);
        gtk_icon_set_unref(iconSet);
    }

    gtk_icon_source_free(iconSource);

    iconsInitialized = true;
}
#endif

static bool nodeHasPseudo(Node* node, const char* pseudo)
{
    RefPtr<Node> attributeNode = node->attributes()->getNamedItem("pseudo");

    return attributeNode ? attributeNode->nodeValue() == pseudo : false;
}

static bool nodeHasClass(Node* node, const char* className)
{
    if (!node->isElementNode())
        return false;

    Element* element = toElement(node);

    if (!element->hasClass())
        return false;

    return element->classNames().contains(className);
}

RenderThemeGtk::RenderThemeGtk()
    : m_panelColor(Color::white)
    , m_sliderColor(Color::white)
    , m_sliderThumbColor(Color::white)
    , m_mediaIconSize(16)
    , m_mediaSliderHeight(14)
{
#if ENABLE(VIDEO)
    initMediaColors();
    initMediaButtons();
#endif
}

RenderThemeGtk::~RenderThemeGtk()
{
}

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

int RenderThemeGtk::baselinePosition(const RenderObject& o) const
{
    if (!o.isBox())
        return 0;

    // FIXME: This strategy is possibly incorrect for the GTK+ port.
    if (o.style().appearance() == CheckboxPart
        || o.style().appearance() == RadioPart) {
        const RenderBox* box = toRenderBox(&o);
        return box->marginTop() + box->height() - 2;
    }

    return RenderTheme::baselinePosition(o);
}

static GtkTextDirection gtkTextDirection(TextDirection direction)
{
    switch (direction) {
    case RTL:
        return GTK_TEXT_DIR_RTL;
    case LTR:
        return GTK_TEXT_DIR_LTR;
    default:
        return GTK_TEXT_DIR_NONE;
    }
}

static GtkStateType gtkIconState(RenderTheme* theme, const RenderObject& renderObject)
{
    if (!theme->isEnabled(renderObject))
        return GTK_STATE_INSENSITIVE;
    if (theme->isPressed(renderObject))
        return GTK_STATE_ACTIVE;
    if (theme->isHovered(renderObject))
        return GTK_STATE_PRELIGHT;

    return GTK_STATE_NORMAL;
}

static void adjustRectForFocus(GtkStyleContext* context, FloatRect& rect)
{
    gint focusWidth, focusPad;
    gtk_style_context_get_style(context, "focus-line-width", &focusWidth, "focus-padding", &focusPad, nullptr);
    rect.inflate(focusWidth + focusPad);
}

void RenderThemeGtk::adjustRepaintRect(const RenderObject& renderObject, FloatRect& rect)
{
    GtkStyleContext* context = 0;
    bool checkInteriorFocus = false;
    ControlPart part = renderObject.style().appearance();
    switch (part) {
    case CheckboxPart:
    case RadioPart:
        context = getStyleContext(part == CheckboxPart ? GTK_TYPE_CHECK_BUTTON : GTK_TYPE_RADIO_BUTTON);

        gint indicatorSpacing;
        gtk_style_context_get_style(context, "indicator-spacing", &indicatorSpacing, nullptr);
        rect.inflate(indicatorSpacing);

        return;
    case SliderVerticalPart:
    case SliderHorizontalPart:
        context = getStyleContext(GTK_TYPE_SCALE);
        break;
    case ButtonPart:
    case MenulistButtonPart:
    case MenulistPart:
        context = getStyleContext(GTK_TYPE_BUTTON);
        checkInteriorFocus = true;
        break;
    case TextFieldPart:
    case TextAreaPart:
        context = getStyleContext(GTK_TYPE_ENTRY);
        checkInteriorFocus = true;
        break;
    default:
        return;
    }

    ASSERT(context);
    if (checkInteriorFocus) {
        gboolean interiorFocus;
        gtk_style_context_get_style(context, "interior-focus", &interiorFocus, nullptr);
        if (interiorFocus)
            return;
    }
    adjustRectForFocus(context, rect);
}

void RenderThemeGtk::adjustButtonStyle(StyleResolver&, RenderStyle& style, Element*) const
{
    // Some layout tests check explicitly that buttons ignore line-height.
    if (style.appearance() == PushButtonPart)
        style.setLineHeight(RenderStyle::initialLineHeight());
}

static void setToggleSize(GtkStyleContext* context, RenderStyle& style)
{
    // The width and height are both specified, so we shouldn't change them.
    if (!style.width().isIntrinsicOrAuto() && !style.height().isAuto())
        return;

    // Other ports hard-code this to 13 which is also the default value defined by GTK+.
    // GTK+ users tend to demand the native look.
    // It could be made a configuration option values other than 13 actually break site compatibility.
    gint indicatorSize;
    gtk_style_context_get_style(context, "indicator-size", &indicatorSize, nullptr);

    if (style.width().isIntrinsicOrAuto())
        style.setWidth(Length(indicatorSize, Fixed));

    if (style.height().isAuto())
        style.setHeight(Length(indicatorSize, Fixed));
}

static void paintToggle(const RenderThemeGtk* theme, GType widgetType, const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& fullRect)
{
    GtkStyleContext* context = getStyleContext(widgetType);
    gtk_style_context_save(context);

    // Some themes do not render large toggle buttons properly, so we simply
    // shrink the rectangle back down to the default size and then center it
    // in the full toggle button region. The reason for not simply forcing toggle
    // buttons to be a smaller size is that we don't want to break site layouts.
    gint indicatorSize;
    gtk_style_context_get_style(context, "indicator-size", &indicatorSize, nullptr);
    IntRect rect(fullRect);
    if (rect.width() > indicatorSize) {
        rect.inflateX(-(rect.width() - indicatorSize) / 2);
        rect.setWidth(indicatorSize); // In case rect.width() was equal to indicatorSize + 1.
    }

    if (rect.height() > indicatorSize) {
        rect.inflateY(-(rect.height() - indicatorSize) / 2);
        rect.setHeight(indicatorSize); // In case rect.height() was equal to indicatorSize + 1.
    }

    gtk_style_context_set_direction(context, static_cast<GtkTextDirection>(gtkTextDirection(renderObject.style().direction())));
    gtk_style_context_add_class(context, widgetType == GTK_TYPE_CHECK_BUTTON ? GTK_STYLE_CLASS_CHECK : GTK_STYLE_CLASS_RADIO);

    guint flags = 0;
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
    gtk_style_context_set_state(context, static_cast<GtkStateFlags>(flags));

    if (widgetType == GTK_TYPE_CHECK_BUTTON)
        gtk_render_check(context, paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());
    else
        gtk_render_option(context, paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());

    if (theme->isFocused(renderObject)) {
        IntRect indicatorRect(rect);
        gint indicatorSpacing;
        gtk_style_context_get_style(context, "indicator-spacing", &indicatorSpacing, nullptr);
        indicatorRect.inflate(indicatorSpacing);
        gtk_render_focus(context, paintInfo.context->platformContext()->cr(), indicatorRect.x(), indicatorRect.y(),
            indicatorRect.width(), indicatorRect.height());
    }

    gtk_style_context_restore(context);
}

void RenderThemeGtk::setCheckboxSize(RenderStyle& style) const
{
    setToggleSize(getStyleContext(GTK_TYPE_CHECK_BUTTON), style);
}

bool RenderThemeGtk::paintCheckbox(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    paintToggle(this, GTK_TYPE_CHECK_BUTTON, renderObject, paintInfo, rect);
    return false;
}

void RenderThemeGtk::setRadioSize(RenderStyle& style) const
{
    setToggleSize(getStyleContext(GTK_TYPE_RADIO_BUTTON), style);
}

bool RenderThemeGtk::paintRadio(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    paintToggle(this, GTK_TYPE_RADIO_BUTTON, renderObject, paintInfo, rect);
    return false;
}

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

    gtk_render_background(context, paintInfo.context->platformContext()->cr(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());
    gtk_render_frame(context, paintInfo.context->platformContext()->cr(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());

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
            gtk_style_context_get_border(context, static_cast<GtkStateFlags>(flags), &borderWidth);

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

        gtk_render_focus(context, paintInfo.context->platformContext()->cr(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());
    }
}
bool RenderThemeGtk::paintButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    GtkStyleContext* context = getStyleContext(GTK_TYPE_BUTTON);
    gtk_style_context_save(context);

    gtk_style_context_set_direction(context, static_cast<GtkTextDirection>(gtkTextDirection(renderObject.style().direction())));
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_BUTTON);

    renderButton(this, context, renderObject, paintInfo, rect);

    gtk_style_context_restore(context);

    return false;
}

void RenderThemeGtk::adjustMenuListStyle(StyleResolver&, RenderStyle& style, Element*) const
{
    // The tests check explicitly that select menu buttons ignore line height.
    style.setLineHeight(RenderStyle::initialLineHeight());

    // We cannot give a proper rendering when border radius is active, unfortunately.
    style.resetBorderRadius();
}

void RenderThemeGtk::adjustMenuListButtonStyle(StyleResolver& styleResolver, RenderStyle& style, Element* e) const
{
    adjustMenuListStyle(styleResolver, style, e);
}

static void getComboBoxMetrics(RenderStyle& style, GtkBorder& border, int& focus, int& separator)
{
    // If this menu list button isn't drawn using the native theme, we
    // don't add any extra padding beyond what WebCore already uses.
    if (style.appearance() == NoControlPart)
        return;

    GtkStyleContext* context = getStyleContext(GTK_TYPE_COMBO_BOX);
    gtk_style_context_save(context);

    gtk_style_context_add_class(context, GTK_STYLE_CLASS_BUTTON);
    gtk_style_context_set_direction(context, static_cast<GtkTextDirection>(gtkTextDirection(style.direction())));

    gtk_style_context_get_border(context, static_cast<GtkStateFlags>(0), &border);

    gboolean interiorFocus;
    gint focusWidth, focusPad;
    gtk_style_context_get_style(context, "interior-focus", &interiorFocus, "focus-line-width", &focusWidth, "focus-padding", &focusPad, nullptr);
    focus = interiorFocus ? focusWidth + focusPad : 0;

    gtk_style_context_restore(context);

    context = getStyleContext(GTK_TYPE_SEPARATOR);
    gtk_style_context_save(context);

    GtkTextDirection direction = static_cast<GtkTextDirection>(gtkTextDirection(style.direction()));
    gtk_style_context_set_direction(context, direction);
    gtk_style_context_add_class(context, "separator");

    gboolean wideSeparators;
    gint separatorWidth;
    gtk_style_context_get_style(context, "wide-separators", &wideSeparators, "separator-width", &separatorWidth, nullptr);

    // GTK+ always uses border.left, regardless of text direction. See gtkseperator.c.
    if (!wideSeparators)
        separatorWidth = border.left;

    separator = separatorWidth;

    gtk_style_context_restore(context);
}

int RenderThemeGtk::popupInternalPaddingLeft(RenderStyle& style) const
{
    GtkBorder borderWidth = { 0, 0, 0, 0 };
    int focusWidth = 0, separatorWidth = 0;
    getComboBoxMetrics(style, borderWidth, focusWidth, separatorWidth);
    int left = borderWidth.left + focusWidth;
    if (style.direction() == RTL)
        left += separatorWidth + minArrowSize;
    return left;
}

int RenderThemeGtk::popupInternalPaddingRight(RenderStyle& style) const
{
    GtkBorder borderWidth = { 0, 0, 0, 0 };
    int focusWidth = 0, separatorWidth = 0;
    getComboBoxMetrics(style, borderWidth, focusWidth, separatorWidth);
    int right = borderWidth.right + focusWidth;
    if (style.direction() == LTR)
        right += separatorWidth + minArrowSize;
    return right;
}

int RenderThemeGtk::popupInternalPaddingTop(RenderStyle& style) const
{
    GtkBorder borderWidth = { 0, 0, 0, 0 };
    int focusWidth = 0, separatorWidth = 0;
    getComboBoxMetrics(style, borderWidth, focusWidth, separatorWidth);
    return borderWidth.top + focusWidth;
}

int RenderThemeGtk::popupInternalPaddingBottom(RenderStyle& style) const
{
    GtkBorder borderWidth = { 0, 0, 0, 0 };
    int focusWidth = 0, separatorWidth = 0;
    getComboBoxMetrics(style, borderWidth, focusWidth, separatorWidth);
    return borderWidth.bottom + focusWidth;
}

bool RenderThemeGtk::paintMenuList(const RenderObject& renderObject, const PaintInfo& paintInfo, const FloatRect& r)
{
    // FIXME: adopt subpixel themes.
    IntRect rect = IntRect(r);

    cairo_t* cairoContext = paintInfo.context->platformContext()->cr();
    GtkTextDirection direction = static_cast<GtkTextDirection>(gtkTextDirection(renderObject.style().direction()));

    // Paint the button.
    GtkStyleContext* buttonStyleContext = getStyleContext(GTK_TYPE_BUTTON);
    gtk_style_context_save(buttonStyleContext);
    gtk_style_context_set_direction(buttonStyleContext, direction);
    gtk_style_context_add_class(buttonStyleContext, GTK_STYLE_CLASS_BUTTON);
    renderButton(this, buttonStyleContext, renderObject, paintInfo, rect);

    // Get the inner rectangle.
    gint focusWidth, focusPad;
    GtkBorder* innerBorderPtr = 0;
    GtkBorder innerBorder = { 1, 1, 1, 1 };
    gtk_style_context_get_style(buttonStyleContext, "inner-border", &innerBorderPtr, "focus-line-width", &focusWidth, "focus-padding", &focusPad, nullptr);
    if (innerBorderPtr) {
        innerBorder = *innerBorderPtr;
        gtk_border_free(innerBorderPtr);
    }

    GtkBorder borderWidth;
    GtkStateFlags state = gtk_style_context_get_state(buttonStyleContext);
    gtk_style_context_get_border(buttonStyleContext, state, &borderWidth);

    focusWidth += focusPad;
    IntRect innerRect(
        rect.x() + innerBorder.left + borderWidth.left + focusWidth,
        rect.y() + innerBorder.top + borderWidth.top + focusWidth,
        rect.width() - borderWidth.left - borderWidth.right - innerBorder.left - innerBorder.right - (2 * focusWidth),
        rect.height() - borderWidth.top - borderWidth.bottom - innerBorder.top - innerBorder.bottom - (2 * focusWidth));

    if (isPressed(renderObject)) {
        gint childDisplacementX;
        gint childDisplacementY;
        gtk_style_context_get_style(buttonStyleContext, "child-displacement-x", &childDisplacementX, "child-displacement-y", &childDisplacementY, nullptr);
        innerRect.move(childDisplacementX, childDisplacementY);
    }
    innerRect.setWidth(std::max(1, innerRect.width()));
    innerRect.setHeight(std::max(1, innerRect.height()));

    gtk_style_context_restore(buttonStyleContext);

    // Paint the arrow.
    GtkStyleContext* arrowStyleContext = getStyleContext(GTK_TYPE_ARROW);
    gtk_style_context_save(arrowStyleContext);

    gtk_style_context_set_direction(arrowStyleContext, direction);
    gtk_style_context_add_class(arrowStyleContext, "arrow");
    gtk_style_context_add_class(arrowStyleContext, GTK_STYLE_CLASS_BUTTON);

    gfloat arrowScaling;
    gtk_style_context_get_style(arrowStyleContext, "arrow-scaling", &arrowScaling, nullptr);

    IntSize arrowSize(minArrowSize, innerRect.height());
    FloatPoint arrowPosition(innerRect.location());
    if (direction == GTK_TEXT_DIR_LTR)
        arrowPosition.move(innerRect.width() - arrowSize.width(), 0);

    // GTK+ actually fetches the xalign and valign values from the widget, but since we
    // don't have a widget here, we are just using the default xalign and valign values of 0.5.
    gint extent = std::min(arrowSize.width(), arrowSize.height()) * arrowScaling;
    arrowPosition.move((arrowSize.width() - extent) / 2, (arrowSize.height() - extent) / 2);

    gtk_style_context_set_state(arrowStyleContext, state);
    gtk_render_arrow(arrowStyleContext, cairoContext, G_PI, arrowPosition.x(), arrowPosition.y(), extent);

    gtk_style_context_restore(arrowStyleContext);

    // Paint the separator if needed.
    GtkStyleContext* separatorStyleContext = getStyleContext(GTK_TYPE_COMBO_BOX);
    gtk_style_context_save(separatorStyleContext);

    gtk_style_context_set_direction(separatorStyleContext, direction);
    gtk_style_context_add_class(separatorStyleContext, "separator");

    gboolean wideSeparators;
    gint separatorWidth;
    gtk_style_context_get_style(separatorStyleContext, "wide-separators", &wideSeparators, "separator-width", &separatorWidth, nullptr);
    if (wideSeparators && !separatorWidth) {
        gtk_style_context_restore(separatorStyleContext);
        return false;
    }

    gtk_style_context_set_state(separatorStyleContext, state);
    IntPoint separatorPosition(arrowPosition.x(), innerRect.y());
    if (wideSeparators) {
        if (direction == GTK_TEXT_DIR_LTR)
            separatorPosition.move(-separatorWidth, 0);
        else
            separatorPosition.move(arrowSize.width(), 0);

        gtk_render_frame(separatorStyleContext, cairoContext, separatorPosition.x(), separatorPosition.y(), separatorWidth, innerRect.height());
    } else {
        GtkBorder padding;
        gtk_style_context_get_padding(separatorStyleContext, state, &padding);
        GtkBorder border;
        gtk_style_context_get_border(separatorStyleContext, state, &border);

        if (direction == GTK_TEXT_DIR_LTR)
            separatorPosition.move(-(padding.left + border.left), 0);
        else
            separatorPosition.move(arrowSize.width(), 0);

        cairo_save(cairoContext);

        // An extra clip prevents the separator bleeding outside of the specified rectangle because of subpixel positioning.
        cairo_rectangle(cairoContext, separatorPosition.x(), separatorPosition.y(), border.left, innerRect.height());
        cairo_clip(cairoContext);
        gtk_render_line(separatorStyleContext, cairoContext, separatorPosition.x(), separatorPosition.y(), separatorPosition.x(), innerRect.maxY());
        cairo_restore(cairoContext);
    }

    gtk_style_context_restore(separatorStyleContext);
    return false;
}

bool RenderThemeGtk::paintMenuListButtonDecorations(const RenderObject& object, const PaintInfo& info, const FloatRect& rect)
{
    return paintMenuList(object, info, rect);
}

bool RenderThemeGtk::paintTextField(const RenderObject& renderObject, const PaintInfo& paintInfo, const FloatRect& rect)
{
    GtkStyleContext* context = getStyleContext(GTK_TYPE_ENTRY);
    gtk_style_context_save(context);

    gtk_style_context_set_direction(context, static_cast<GtkTextDirection>(gtkTextDirection(renderObject.style().direction())));
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_ENTRY);

    guint flags = 0;
    if (!isEnabled(renderObject) || isReadOnlyControl(renderObject))
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    else if (isFocused(renderObject))
        flags |= GTK_STATE_FLAG_FOCUSED;
    gtk_style_context_set_state(context, static_cast<GtkStateFlags>(flags));

    gtk_render_background(context, paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(context, paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());

    if (isFocused(renderObject) && isEnabled(renderObject)) {
        gboolean interiorFocus;
        gint focusWidth, focusPad;
        gtk_style_context_get_style(context, "interior-focus", &interiorFocus, "focus-line-width", &focusWidth, "focus-padding", &focusPad, nullptr);
        if (!interiorFocus) {
            IntRect focusRect(rect);
            focusRect.inflate(focusWidth + focusPad);
            gtk_render_focus(context, paintInfo.context->platformContext()->cr(), focusRect.x(), focusRect.y(), focusRect.width(), focusRect.height());
        }
    }

    gtk_style_context_restore(context);

    return false;
}

bool RenderThemeGtk::paintTextArea(const RenderObject& o, const PaintInfo& i, const FloatRect& r)
{
    return paintTextField(o, i, r);
}

static void paintGdkPixbuf(GraphicsContext* context, const GdkPixbuf* icon, const IntRect& iconRect)
{
    IntSize iconSize(gdk_pixbuf_get_width(icon), gdk_pixbuf_get_height(icon));
    GRefPtr<GdkPixbuf> scaledIcon;
    if (iconRect.size() != iconSize) {
        // We could use cairo_scale() here but cairo/pixman downscale quality is quite bad.
        scaledIcon = adoptGRef(gdk_pixbuf_scale_simple(icon, iconRect.width(), iconRect.height(), GDK_INTERP_BILINEAR));
        icon = scaledIcon.get();
    }

    cairo_t* cr = context->platformContext()->cr();
    cairo_save(cr);
    gdk_cairo_set_source_pixbuf(cr, icon, iconRect.x(), iconRect.y());
    cairo_paint(cr);
    cairo_restore(cr);
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

void RenderThemeGtk::adjustSearchFieldResultsButtonStyle(StyleResolver& styleResolver, RenderStyle& style, Element* e) const
{
    adjustSearchFieldCancelButtonStyle(styleResolver, style, e);
}

bool RenderThemeGtk::paintSearchFieldResultsButton(const RenderObject& o, const PaintInfo& i, const IntRect& rect)
{
    return paintSearchFieldResultsDecorationPart(o, i, rect);
}

static void adjustSearchFieldIconStyle(RenderStyle& style)
{
    style.resetBorder();
    style.resetPadding();

    // Get the icon size based on the font size.
    int fontSize = style.fontSize();
    if (fontSize < gtkIconSizeMenu) {
        style.setWidth(Length(fontSize, Fixed));
        style.setHeight(Length(fontSize, Fixed));
        return;
    }
    gint width = 0, height = 0;
    gtk_icon_size_lookup(getIconSizeForPixelSize(fontSize), &width, &height);
    style.setWidth(Length(width, Fixed));
    style.setHeight(Length(height, Fixed));
}

void RenderThemeGtk::adjustSearchFieldResultsDecorationPartStyle(StyleResolver&, RenderStyle& style, Element*) const
{
    adjustSearchFieldIconStyle(style);
}

static IntRect centerRectVerticallyInParentInputElement(const RenderObject& renderObject, const IntRect& rect)
{
    // Get the renderer of <input> element.
    Node* input = renderObject.node()->shadowHost();
    if (!input)
        input = renderObject.node();
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

bool RenderThemeGtk::paintSearchFieldResultsDecorationPart(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    IntRect iconRect = centerRectVerticallyInParentInputElement(renderObject, rect);
    if (iconRect.isEmpty())
        return false;

    GRefPtr<GdkPixbuf> icon = getStockIconForWidgetType(GTK_TYPE_ENTRY, GTK_STOCK_FIND,
        gtkTextDirection(renderObject.style().direction()),
        gtkIconState(this, renderObject),
        getIconSizeForPixelSize(rect.height()));
    paintGdkPixbuf(paintInfo.context, icon.get(), iconRect);
    return false;
}

void RenderThemeGtk::adjustSearchFieldCancelButtonStyle(StyleResolver&, RenderStyle& style, Element*) const
{
    adjustSearchFieldIconStyle(style);
}

bool RenderThemeGtk::paintSearchFieldCancelButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    IntRect iconRect = centerRectVerticallyInParentInputElement(renderObject, rect);
    if (iconRect.isEmpty())
        return false;

    GRefPtr<GdkPixbuf> icon = getStockIconForWidgetType(GTK_TYPE_ENTRY, GTK_STOCK_CLEAR,
        gtkTextDirection(renderObject.style().direction()),
        gtkIconState(this, renderObject),
        getIconSizeForPixelSize(rect.height()));
    paintGdkPixbuf(paintInfo.context, icon.get(), iconRect);
    return false;
}

void RenderThemeGtk::adjustSearchFieldStyle(StyleResolver&, RenderStyle& style, Element*) const
{
    // We cannot give a proper rendering when border radius is active, unfortunately.
    style.resetBorderRadius();
    style.setLineHeight(RenderStyle::initialLineHeight());
}

bool RenderThemeGtk::paintSearchField(const RenderObject& o, const PaintInfo& i, const IntRect& rect)
{
    return paintTextField(o, i, rect);
}

bool RenderThemeGtk::paintCapsLockIndicator(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    // The other paint methods don't need to check whether painting is disabled because RenderTheme already checks it
    // before calling them, but paintCapsLockIndicator() is called by RenderTextControlSingleLine which doesn't check it.
    if (paintInfo.context->paintingDisabled())
        return true;

    int iconSize = std::min(rect.width(), rect.height());
    GRefPtr<GdkPixbuf> icon = getStockIconForWidgetType(GTK_TYPE_ENTRY, GTK_STOCK_CAPS_LOCK_WARNING, gtkTextDirection(renderObject.style().direction()), 0, getIconSizeForPixelSize(iconSize));

    // Only re-scale the icon when it's smaller than the minimum icon size.
    if (iconSize >= gtkIconSizeMenu)
        iconSize = gdk_pixbuf_get_height(icon.get());

    // GTK+ locates the icon right aligned in the entry. The given rectangle is already
    // centered vertically by RenderTextControlSingleLine.
    IntRect iconRect(
        rect.x() + rect.width() - iconSize,
        rect.y() + (rect.height() - iconSize) / 2,
        iconSize, iconSize);
    paintGdkPixbuf(paintInfo.context, icon.get(), iconRect);
    return true;
}

void RenderThemeGtk::adjustSliderTrackStyle(StyleResolver&, RenderStyle& style, Element*) const
{
    style.setBoxShadow(nullptr);
}

void RenderThemeGtk::adjustSliderThumbStyle(StyleResolver& styleResolver, RenderStyle& style, Element* element) const
{
    RenderTheme::adjustSliderThumbStyle(styleResolver, style, element);
    style.setBoxShadow(nullptr);
}

static void applySliderStyleContextClasses(GtkStyleContext* context, ControlPart part)
{
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_SCALE);
    if (part == SliderHorizontalPart || part == SliderThumbHorizontalPart)
        gtk_style_context_add_class(context, GTK_STYLE_CLASS_HORIZONTAL);
    else if (part == SliderVerticalPart || part == SliderThumbVerticalPart)
        gtk_style_context_add_class(context, GTK_STYLE_CLASS_VERTICAL);
}

bool RenderThemeGtk::paintSliderTrack(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    ControlPart part = renderObject.style().appearance();
    ASSERT_UNUSED(part, part == SliderHorizontalPart || part == SliderVerticalPart || part == MediaVolumeSliderPart);

    GtkStyleContext* context = getStyleContext(GTK_TYPE_SCALE);
    gtk_style_context_save(context);

    gtk_style_context_set_direction(context, gtkTextDirection(renderObject.style().direction()));
    applySliderStyleContextClasses(context, part);
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_TROUGH);

    if (!isEnabled(renderObject))
        gtk_style_context_set_state(context, GTK_STATE_FLAG_INSENSITIVE);

    gtk_render_background(context, paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(context, paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());

    if (isFocused(renderObject)) {
        gint focusWidth, focusPad;
        gtk_style_context_get_style(context, "focus-line-width", &focusWidth, "focus-padding", &focusPad, nullptr);
        IntRect focusRect(rect);
        focusRect.inflate(focusWidth + focusPad);
        gtk_render_focus(context, paintInfo.context->platformContext()->cr(), focusRect.x(), focusRect.y(), focusRect.width(), focusRect.height());
    }

    gtk_style_context_restore(context);
    return false;
}

bool RenderThemeGtk::paintSliderThumb(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    ControlPart part = renderObject.style().appearance();
    ASSERT(part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart || part == MediaVolumeSliderThumbPart);

    GtkStyleContext* context = getStyleContext(GTK_TYPE_SCALE);
    gtk_style_context_save(context);

    gtk_style_context_set_direction(context, gtkTextDirection(renderObject.style().direction()));
    applySliderStyleContextClasses(context, part);
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_SLIDER);

    guint flags = 0;
    if (!isEnabled(renderObject))
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    else if (isHovered(renderObject))
        flags |= GTK_STATE_FLAG_PRELIGHT;
    if (isPressed(renderObject))
        flags |= GTK_STATE_FLAG_ACTIVE;
    gtk_style_context_set_state(context, static_cast<GtkStateFlags>(flags));

    gtk_render_slider(context, paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height(),
        part == SliderThumbHorizontalPart ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);

    gtk_style_context_restore(context);

    return false;
}

void RenderThemeGtk::adjustSliderThumbSize(RenderStyle& style, Element*) const
{
    ControlPart part = style.appearance();
    if (part != SliderThumbHorizontalPart && part != SliderThumbVerticalPart)
        return;

    gint sliderWidth, sliderLength;
    gtk_style_context_get_style(getStyleContext(GTK_TYPE_SCALE), "slider-width", &sliderWidth, "slider-length", &sliderLength, nullptr);
    if (part == SliderThumbHorizontalPart) {
        style.setWidth(Length(sliderLength, Fixed));
        style.setHeight(Length(sliderWidth, Fixed));
        return;
    }
    ASSERT(part == SliderThumbVerticalPart || part == MediaVolumeSliderThumbPart);
    style.setWidth(Length(sliderWidth, Fixed));
    style.setHeight(Length(sliderLength, Fixed));
}

bool RenderThemeGtk::paintProgressBar(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!renderObject.isProgress())
        return true;

    GtkStyleContext* context = getStyleContext(GTK_TYPE_PROGRESS_BAR);
    gtk_style_context_save(context);

    gtk_style_context_add_class(context, GTK_STYLE_CLASS_TROUGH);

    gtk_render_background(context, paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(context, paintInfo.context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());

    gtk_style_context_restore(context);

    gtk_style_context_save(context);
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_PROGRESSBAR);


    GtkBorder padding;
    gtk_style_context_get_padding(context, static_cast<GtkStateFlags>(0), &padding);
    IntRect progressRect(
        rect.x() + padding.left,
        rect.y() + padding.top,
        rect.width() - (padding.left + padding.right),
        rect.height() - (padding.top + padding.bottom));
    progressRect = RenderThemeGtk::calculateProgressRect(renderObject, progressRect);

    if (!progressRect.isEmpty())
        gtk_render_activity(context, paintInfo.context->platformContext()->cr(), progressRect.x(), progressRect.y(), progressRect.width(), progressRect.height());

    gtk_style_context_restore(context);
    return false;
}

static gint spinButtonArrowSize(GtkStyleContext* context)
{
    PangoFontDescription* fontDescription;
    gtk_style_context_get(context, static_cast<GtkStateFlags>(0), "font", &fontDescription, nullptr);
    gint fontSize = pango_font_description_get_size(fontDescription);
    gint arrowSize = std::max(PANGO_PIXELS(fontSize), minSpinButtonArrowSize);
    pango_font_description_free(fontDescription);

    return arrowSize - arrowSize % 2; // Force even.
}

void RenderThemeGtk::adjustInnerSpinButtonStyle(StyleResolver&, RenderStyle& style, Element*) const
{
    GtkStyleContext* context = getStyleContext(GTK_TYPE_SPIN_BUTTON);

    GtkBorder padding;
    gtk_style_context_get_padding(context, static_cast<GtkStateFlags>(0), &padding);

    int width = spinButtonArrowSize(context) + padding.left + padding.right;
    style.setWidth(Length(width, Fixed));
    style.setMinWidth(Length(width, Fixed));
}

static void paintSpinArrowButton(RenderTheme* theme, GtkStyleContext* context, const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect, GtkArrowType arrowType)
{
    ASSERT(arrowType == GTK_ARROW_UP || arrowType == GTK_ARROW_DOWN);

    gtk_style_context_save(context);
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_BUTTON);

    GtkTextDirection direction = gtk_style_context_get_direction(context);
    guint state = static_cast<guint>(gtk_style_context_get_state(context));
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
    gtk_style_context_set_state(context, static_cast<GtkStateFlags>(state));

    // Paint button.
    IntRect buttonRect(rect);
    guint junction = gtk_style_context_get_junction_sides(context);
    if (arrowType == GTK_ARROW_UP)
        junction |= GTK_JUNCTION_BOTTOM;
    else {
        junction |= GTK_JUNCTION_TOP;
        buttonRect.move(0, rect.height() / 2);
    }
    buttonRect.setHeight(rect.height() / 2);
    gtk_style_context_set_junction_sides(context, static_cast<GtkJunctionSides>(junction));

    gtk_render_background(context, paintInfo.context->platformContext()->cr(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());
    gtk_render_frame(context, paintInfo.context->platformContext()->cr(), buttonRect.x(), buttonRect.y(), buttonRect.width(), buttonRect.height());

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
    gtk_render_arrow(context, paintInfo.context->platformContext()->cr(), angle, arrowRect.x(), arrowRect.y(), width);

    gtk_style_context_restore(context);
}

bool RenderThemeGtk::paintInnerSpinButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    GtkStyleContext* context = getStyleContext(GTK_TYPE_SPIN_BUTTON);
    gtk_style_context_save(context);

    GtkTextDirection direction = static_cast<GtkTextDirection>(gtkTextDirection(renderObject.style().direction()));
    gtk_style_context_set_direction(context, direction);

    guint flags = 0;
    if (!isEnabled(renderObject) || isReadOnlyControl(renderObject))
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    else if (isFocused(renderObject))
        flags |= GTK_STATE_FLAG_FOCUSED;
    gtk_style_context_set_state(context, static_cast<GtkStateFlags>(flags));
    gtk_style_context_remove_class(context, GTK_STYLE_CLASS_ENTRY);

    paintSpinArrowButton(this, context, renderObject, paintInfo, rect, GTK_ARROW_UP);
    paintSpinArrowButton(this, context, renderObject, paintInfo, rect, GTK_ARROW_DOWN);

    gtk_style_context_restore(context);

    return false;
}

double RenderThemeGtk::caretBlinkInterval() const
{
    GtkSettings* settings = gtk_settings_get_default();

    gboolean shouldBlink;
    gint time;

    g_object_get(settings, "gtk-cursor-blink", &shouldBlink, "gtk-cursor-blink-time", &time, nullptr);

    if (!shouldBlink)
        return 0;

    return time / 2000.;
}

enum StyleColorType { StyleColorBackground, StyleColorForeground };

static Color styleColor(GType widgetType, GtkStateFlags state, StyleColorType colorType)
{

    GtkStyleContext* context = getStyleContext(widgetType);
    // Recent GTK+ versions (> 3.14) require to explicitly set the state before getting the color.
    gtk_style_context_set_state(context, state);

    GdkRGBA gdkRGBAColor;
    if (colorType == StyleColorBackground)
        gtk_style_context_get_background_color(context, state, &gdkRGBAColor);
    else
        gtk_style_context_get_color(context, state, &gdkRGBAColor);
    return gdkRGBAColor;
}

Color RenderThemeGtk::platformActiveSelectionBackgroundColor() const
{
    return styleColor(GTK_TYPE_ENTRY, static_cast<GtkStateFlags>(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_FOCUSED), StyleColorBackground);
}

Color RenderThemeGtk::platformInactiveSelectionBackgroundColor() const
{
    return styleColor(GTK_TYPE_ENTRY, GTK_STATE_FLAG_SELECTED, StyleColorBackground);
}

Color RenderThemeGtk::platformActiveSelectionForegroundColor() const
{
    return styleColor(GTK_TYPE_ENTRY, static_cast<GtkStateFlags>(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_FOCUSED), StyleColorForeground);
}

Color RenderThemeGtk::platformInactiveSelectionForegroundColor() const
{
    return styleColor(GTK_TYPE_ENTRY, GTK_STATE_FLAG_SELECTED, StyleColorForeground);
}

Color RenderThemeGtk::platformActiveListBoxSelectionBackgroundColor() const
{
    return styleColor(GTK_TYPE_TREE_VIEW, static_cast<GtkStateFlags>(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_FOCUSED), StyleColorBackground);
}

Color RenderThemeGtk::platformInactiveListBoxSelectionBackgroundColor() const
{
    return styleColor(GTK_TYPE_TREE_VIEW, GTK_STATE_FLAG_SELECTED, StyleColorBackground);
}

Color RenderThemeGtk::platformActiveListBoxSelectionForegroundColor() const
{
    return styleColor(GTK_TYPE_TREE_VIEW, static_cast<GtkStateFlags>(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_FOCUSED), StyleColorForeground);
}

Color RenderThemeGtk::platformInactiveListBoxSelectionForegroundColor() const
{
    return styleColor(GTK_TYPE_TREE_VIEW, GTK_STATE_FLAG_SELECTED, StyleColorForeground);
}

Color RenderThemeGtk::systemColor(CSSValueID cssValueId) const
{
    switch (cssValueId) {
    case CSSValueButtontext:
        return styleColor(GTK_TYPE_BUTTON, GTK_STATE_FLAG_ACTIVE, StyleColorForeground);
    case CSSValueCaptiontext:
        return styleColor(GTK_TYPE_ENTRY, GTK_STATE_FLAG_ACTIVE, StyleColorForeground);
    default:
        return RenderTheme::systemColor(cssValueId);
    }
}

void RenderThemeGtk::platformColorsDidChange()
{
#if ENABLE(VIDEO)
    initMediaColors();
#endif
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

bool RenderThemeGtk::paintMediaButton(const RenderObject& renderObject, GraphicsContext* context, const IntRect& rect, const char* symbolicIconName, const char* fallbackStockIconName)
{
    IntRect iconRect(
        rect.x() + (rect.width() - m_mediaIconSize) / 2,
        rect.y() + (rect.height() - m_mediaIconSize) / 2,
        m_mediaIconSize, m_mediaIconSize);
    GRefPtr<GdkPixbuf> icon = getStockSymbolicIconForWidgetType(GTK_TYPE_CONTAINER, symbolicIconName, fallbackStockIconName,
        gtkTextDirection(renderObject.style().direction()), gtkIconState(this, renderObject), iconRect.width());
    paintGdkPixbuf(context, icon.get(), iconRect);
    return true;
}

bool RenderThemeGtk::hasOwnDisabledStateHandlingFor(ControlPart part) const
{
    return (part != MediaMuteButtonPart);
}

bool RenderThemeGtk::paintMediaFullscreenButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintMediaButton(renderObject, paintInfo.context, rect, "view-fullscreen-symbolic", GTK_STOCK_FULLSCREEN);
}

bool RenderThemeGtk::paintMediaMuteButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = getMediaElementFromRenderObject(renderObject);
    if (!mediaElement)
        return false;

    bool muted = mediaElement->muted();
    return paintMediaButton(renderObject, paintInfo.context, rect,
        muted ? "audio-volume-muted-symbolic" : "audio-volume-high-symbolic",
        muted ? "audio-volume-muted" : "audio-volume-high");
}

bool RenderThemeGtk::paintMediaPlayButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    Node* node = renderObject.node();
    if (!node)
        return false;

    if (!nodeHasPseudo(node, "-webkit-media-controls-play-button"))
        return false;
    bool showPlayButton = nodeHasClass(node, "paused");

    return paintMediaButton(renderObject, paintInfo.context, rect,
        showPlayButton ? "media-playback-start-symbolic" : "media-playback-pause-symbolic",
        showPlayButton ? GTK_STOCK_MEDIA_PLAY : GTK_STOCK_MEDIA_PAUSE);
}

bool RenderThemeGtk::paintMediaSeekBackButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintMediaButton(renderObject, paintInfo.context, rect, "media-seek-backward-symbolic", GTK_STOCK_MEDIA_REWIND);
}

bool RenderThemeGtk::paintMediaSeekForwardButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintMediaButton(renderObject, paintInfo.context, rect, "media-seek-forward-symbolic", GTK_STOCK_MEDIA_FORWARD);
}

#if ENABLE(VIDEO_TRACK)
bool RenderThemeGtk::paintMediaToggleClosedCaptionsButton(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    IntRect iconRect(rect.x() + (rect.width() - m_mediaIconSize) / 2, rect.y() + (rect.height() - m_mediaIconSize) / 2,
        m_mediaIconSize, m_mediaIconSize);
    GRefPtr<GdkPixbuf> icon = getStockSymbolicIconForWidgetType(GTK_TYPE_CONTAINER, "media-view-subtitles-symbolic", nullptr,
        gtkTextDirection(renderObject.style().direction()), gtkIconState(this, renderObject), iconRect.width());
    if (!icon) {
        icon = getStockSymbolicIconForWidgetType(GTK_TYPE_CONTAINER, "user-invisible-symbolic", GTK_STOCK_JUSTIFY_FILL,
            gtkTextDirection(renderObject.style().direction()), gtkIconState(this, renderObject), iconRect.width());
    }
    paintGdkPixbuf(paintInfo.context, icon.get(), iconRect);
    return true;
}
#endif

static FloatRoundedRect::Radii borderRadiiFromStyle(RenderStyle& style)
{
    return FloatRoundedRect::Radii(
        IntSize(style.borderTopLeftRadius().width().intValue(), style.borderTopLeftRadius().height().intValue()),
        IntSize(style.borderTopRightRadius().width().intValue(), style.borderTopRightRadius().height().intValue()),
        IntSize(style.borderBottomLeftRadius().width().intValue(), style.borderBottomLeftRadius().height().intValue()),
        IntSize(style.borderBottomRightRadius().width().intValue(), style.borderBottomRightRadius().height().intValue()));
}

bool RenderThemeGtk::paintMediaSliderTrack(const RenderObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    HTMLMediaElement* mediaElement = parentMediaElement(o);
    if (!mediaElement)
        return false;

    GraphicsContext* context = paintInfo.context;
    context->save();
    context->setStrokeStyle(NoStroke);

    float mediaDuration = mediaElement->duration();
    float totalTrackWidth = r.width();
    RenderStyle& style = o.style();
    RefPtr<TimeRanges> timeRanges = mediaElement->buffered();
    for (unsigned index = 0; index < timeRanges->length(); ++index) {
        float start = timeRanges->start(index, IGNORE_EXCEPTION);
        float end = timeRanges->end(index, IGNORE_EXCEPTION);
        float startRatio = start / mediaDuration;
        float lengthRatio = (end - start) / mediaDuration;
        if (!lengthRatio)
            continue;

        IntRect rangeRect(r);
        rangeRect.setWidth(lengthRatio * totalTrackWidth);
        if (index)
            rangeRect.move(startRatio * totalTrackWidth, 0);
        context->fillRoundedRect(FloatRoundedRect(rangeRect, borderRadiiFromStyle(style)), style.visitedDependentColor(CSSPropertyColor), style.colorSpace());
    }

    context->restore();
    return false;
}

bool RenderThemeGtk::paintMediaSliderThumb(const RenderObject& o, const PaintInfo& paintInfo, const IntRect& r)
{
    RenderStyle& style = o.style();
    paintInfo.context->fillRoundedRect(FloatRoundedRect(r, borderRadiiFromStyle(style)), style.visitedDependentColor(CSSPropertyColor), style.colorSpace());
    return false;
}

bool RenderThemeGtk::paintMediaVolumeSliderContainer(const RenderObject&, const PaintInfo&, const IntRect&)
{
    return true;
}

bool RenderThemeGtk::paintMediaVolumeSliderTrack(const RenderObject& renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = parentMediaElement(renderObject);
    if (!mediaElement)
        return true;

    float volume = mediaElement->muted() ? 0.0f : mediaElement->volume();
    if (!volume)
        return true;

    GraphicsContext* context = paintInfo.context;
    context->save();
    context->setStrokeStyle(NoStroke);

    int rectHeight = rect.height();
    float trackHeight = rectHeight * volume;
    RenderStyle& style = renderObject.style();
    IntRect volumeRect(rect);
    volumeRect.move(0, rectHeight - trackHeight);
    volumeRect.setHeight(ceil(trackHeight));

    context->fillRoundedRect(FloatRoundedRect(volumeRect, borderRadiiFromStyle(style)),
        style.visitedDependentColor(CSSPropertyColor), style.colorSpace());
    context->restore();

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

void RenderThemeGtk::adjustProgressBarStyle(StyleResolver&, RenderStyle& style, Element*) const
{
    style.setBoxShadow(nullptr);
}

// These values have been copied from RenderThemeChromiumSkia.cpp
static const int progressActivityBlocks = 5;
static const int progressAnimationFrames = 10;
static const double progressAnimationInterval = 0.125;
double RenderThemeGtk::animationRepeatIntervalForProgressBar(RenderProgress&) const
{
    return progressAnimationInterval;
}

double RenderThemeGtk::animationDurationForProgressBar(RenderProgress&) const
{
    return progressAnimationInterval * progressAnimationFrames * 2; // "2" for back and forth;
}

IntRect RenderThemeGtk::calculateProgressRect(const RenderObject& renderObject, const IntRect& fullBarRect)
{
    IntRect progressRect(fullBarRect);
    const RenderProgress& renderProgress = toRenderProgress(renderObject);
    if (renderProgress.isDeterminate()) {
        int progressWidth = progressRect.width() * renderProgress.position();
        if (renderObject.style().direction() == RTL)
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

String RenderThemeGtk::fileListNameForWidth(const FileList* fileList, const Font& font, int width, bool multipleFilesAllowed) const
{
    if (width <= 0)
        return String();

    if (fileList->length() > 1)
        return StringTruncator::rightTruncate(multipleFileUploadText(fileList->length()), width, font, StringTruncator::EnableRoundingHacks);

    String string;
    if (fileList->length())
        string = pathGetFileName(fileList->item(0)->path());
    else if (multipleFilesAllowed)
        string = fileButtonNoFilesSelectedLabel();
    else
        string = fileButtonNoFileSelectedLabel();

    return StringTruncator::centerTruncate(string, width, font, StringTruncator::EnableRoundingHacks);
}

String RenderThemeGtk::mediaControlsScript()
{
    StringBuilder scriptBuilder;
    scriptBuilder.append(mediaControlsLocalizedStringsJavaScript, sizeof(mediaControlsLocalizedStringsJavaScript));
    scriptBuilder.append(mediaControlsBaseJavaScript, sizeof(mediaControlsBaseJavaScript));
    scriptBuilder.append(mediaControlsGtkJavaScript, sizeof(mediaControlsGtkJavaScript));
    return scriptBuilder.toString();
}

#endif // GTK_API_VERSION_2
}
