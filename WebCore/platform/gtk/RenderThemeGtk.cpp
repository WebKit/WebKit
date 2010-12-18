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

#include "AffineTransform.h"
#include "CSSValueKeywords.h"
#include "GOwnPtr.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "GtkVersioning.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "MediaControlElements.h"
#include "NotImplemented.h"
#include "PlatformMouseEvent.h"
#include "RenderBox.h"
#include "RenderObject.h"
#include "Scrollbar.h"
#include "TimeRanges.h"
#include "UserAgentStyleSheets.h"
#include "WidgetRenderingContext.h"
#include "gtkdrawing.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <wtf/text/CString.h>

#if ENABLE(PROGRESS_TAG)
#include "RenderProgress.h"
#endif

namespace WebCore {

using namespace HTMLNames;

static void paintStockIcon(GraphicsContext* context, const IntPoint& iconPoint, GtkStyle* style, const char* iconName,
                           GtkTextDirection direction, GtkStateType state, GtkIconSize iconSize)
{
    GtkIconSet* iconSet = gtk_style_lookup_icon_set(style, iconName);
    PlatformRefPtr<GdkPixbuf> icon = adoptPlatformRef(gtk_icon_set_render_icon(iconSet, style, direction, state, iconSize, 0, 0));

    cairo_t* cr = context->platformContext();
    cairo_save(cr);
    gdk_cairo_set_source_pixbuf(cr, icon.get(), iconPoint.x(), iconPoint.y());
    cairo_paint(cr);
    cairo_restore(cr);
}

#if ENABLE(VIDEO)
static HTMLMediaElement* getMediaElementFromRenderObject(RenderObject* o)
{
    Node* node = o->node();
    Node* mediaNode = node ? node->shadowAncestorNode() : 0;
    if (!mediaNode || (!mediaNode->hasTagName(videoTag) && !mediaNode->hasTagName(audioTag)))
        return 0;

    return static_cast<HTMLMediaElement*>(mediaNode);
}

static GtkIconSize getMediaButtonIconSize(int mediaIconSize)
{
    GtkIconSize iconSize = gtk_icon_size_from_name("webkit-media-button-size");
    if (!iconSize)
        iconSize = gtk_icon_size_register("webkit-media-button-size", mediaIconSize, mediaIconSize);
    return iconSize;
}

void RenderThemeGtk::initMediaColors()
{
    GtkStyle* style = gtk_widget_get_style(GTK_WIDGET(gtkContainer()));
    m_panelColor = style->bg[GTK_STATE_NORMAL];
    m_sliderColor = style->bg[GTK_STATE_ACTIVE];
    m_sliderThumbColor = style->bg[GTK_STATE_SELECTED];
}

void RenderThemeGtk::initMediaButtons()
{
    static bool iconsInitialized = false;

    if (iconsInitialized)
        return;

    PlatformRefPtr<GtkIconFactory> iconFactory = adoptPlatformRef(gtk_icon_factory_new());
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

PassRefPtr<RenderTheme> RenderThemeGtk::create()
{
    return adoptRef(new RenderThemeGtk());
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    static RenderTheme* rt = RenderThemeGtk::create().releaseRef();
    return rt;
}

static int mozGtkRefCount = 0;

RenderThemeGtk::RenderThemeGtk()
    : m_gtkWindow(0)
    , m_gtkContainer(0)
    , m_gtkButton(0)
    , m_gtkEntry(0)
    , m_gtkTreeView(0)
    , m_panelColor(Color::white)
    , m_sliderColor(Color::white)
    , m_sliderThumbColor(Color::white)
    , m_mediaIconSize(16)
    , m_mediaSliderHeight(14)
    , m_mediaSliderThumbWidth(12)
    , m_mediaSliderThumbHeight(12)
#ifdef GTK_API_VERSION_2
    , m_themePartsHaveRGBAColormap(true)
#endif
{

    memset(&m_themeParts, 0, sizeof(GtkThemeParts));
#ifdef GTK_API_VERSION_2
    GdkColormap* colormap = gdk_screen_get_rgba_colormap(gdk_screen_get_default());
    if (!colormap) {
        m_themePartsHaveRGBAColormap = false;
        colormap = gdk_screen_get_default_colormap(gdk_screen_get_default());
    }
    m_themeParts.colormap = colormap;
#endif

    // Initialize the Mozilla theme drawing code.
    if (!mozGtkRefCount) {
        moz_gtk_init();
        moz_gtk_use_theme_parts(&m_themeParts);
    }
    ++mozGtkRefCount;

#if ENABLE(VIDEO)
    initMediaColors();
    initMediaButtons();
#endif
}

RenderThemeGtk::~RenderThemeGtk()
{
    --mozGtkRefCount;

    if (!mozGtkRefCount)
        moz_gtk_shutdown();

    gtk_widget_destroy(m_gtkWindow);
}

void RenderThemeGtk::getIndicatorMetrics(ControlPart part, int& indicatorSize, int& indicatorSpacing) const
{
    ASSERT(part == CheckboxPart || part == RadioPart);
    if (part == CheckboxPart) {
        moz_gtk_checkbox_get_metrics(&indicatorSize, &indicatorSpacing);
        return;
    }

    // RadioPart
    moz_gtk_radio_get_metrics(&indicatorSize, &indicatorSpacing);
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

GtkStateType RenderThemeGtk::getGtkStateType(RenderObject* object)
{
    if (!isEnabled(object) || isReadOnlyControl(object))
        return GTK_STATE_INSENSITIVE;
    if (isPressed(object))
        return GTK_STATE_ACTIVE;
    if (isHovered(object))
        return GTK_STATE_PRELIGHT;
    return GTK_STATE_NORMAL;
}

bool RenderThemeGtk::supportsFocusRing(const RenderStyle* style) const
{
    return supportsFocus(style->appearance());
}

bool RenderThemeGtk::controlSupportsTints(const RenderObject* o) const
{
    return isEnabled(o);
}

int RenderThemeGtk::baselinePosition(const RenderObject* o) const
{
    if (!o->isBox())
        return 0;

    // FIXME: This strategy is possibly incorrect for the GTK+ port.
    if (o->style()->appearance() == CheckboxPart
        || o->style()->appearance() == RadioPart) {
        const RenderBox* box = toRenderBox(o);
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

GtkStateType RenderThemeGtk::gtkIconState(RenderObject* renderObject)
{
    if (!isEnabled(renderObject))
        return GTK_STATE_INSENSITIVE;
    if (isPressed(renderObject))
        return GTK_STATE_ACTIVE;
    if (isHovered(renderObject))
        return GTK_STATE_PRELIGHT;

    return GTK_STATE_NORMAL;
}

bool RenderThemeGtk::paintRenderObject(GtkThemeWidgetType type, RenderObject* renderObject, GraphicsContext* context, const IntRect& rect, int flags)
{
    // Painting is disabled so just claim to have succeeded
    if (context->paintingDisabled())
        return false;

    GtkWidgetState widgetState;
    widgetState.active = isPressed(renderObject);
    widgetState.focused = isFocused(renderObject);

    // https://bugs.webkit.org/show_bug.cgi?id=18364
    // The Mozilla theme drawing code, only paints a button as pressed when it's pressed 
    // while hovered. Until we move away from the Mozila code, work-around the issue by
    // forcing a pressed button into the hovered state. This ensures that buttons activated
    // via the keyboard have the proper rendering.
    widgetState.inHover = isHovered(renderObject) || (type == MOZ_GTK_BUTTON && isPressed(renderObject));

    // FIXME: Disabled does not always give the correct appearance for ReadOnly
    widgetState.disabled = !isEnabled(renderObject) || isReadOnlyControl(renderObject);
    widgetState.isDefault = false;
    widgetState.canDefault = false;
    widgetState.depressed = false;

    WidgetRenderingContext widgetContext(context, rect);
    return !widgetContext.paintMozillaWidget(type, &widgetState, flags, gtkTextDirection(renderObject->style()->direction()));
}

static void setToggleSize(const RenderThemeGtk* theme, RenderStyle* style, ControlPart appearance)
{
    // The width and height are both specified, so we shouldn't change them.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // FIXME: This is probably not correct use of indicatorSize and indicatorSpacing.
    gint indicatorSize, indicatorSpacing;
    theme->getIndicatorMetrics(appearance, indicatorSize, indicatorSpacing);

    // Other ports hard-code this to 13, but GTK+ users tend to demand the native look.
    // It could be made a configuration option values other than 13 actually break site compatibility.
    int length = indicatorSize + indicatorSpacing;
    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(length, Fixed));

    if (style->height().isAuto())
        style->setHeight(Length(length, Fixed));
}

void RenderThemeGtk::setCheckboxSize(RenderStyle* style) const
{
    setToggleSize(this, style, RadioPart);
}

bool RenderThemeGtk::paintCheckbox(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintRenderObject(MOZ_GTK_CHECKBUTTON, o, i.context, rect, isChecked(o));
}

void RenderThemeGtk::setRadioSize(RenderStyle* style) const
{
    setToggleSize(this, style, RadioPart);
}

bool RenderThemeGtk::paintRadio(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintRenderObject(MOZ_GTK_RADIOBUTTON, o, i.context, rect, isChecked(o));
}

void RenderThemeGtk::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const
{
    // Some layout tests check explicitly that buttons ignore line-height.
    if (style->appearance() == PushButtonPart)
        style->setLineHeight(RenderStyle::initialLineHeight());
}

bool RenderThemeGtk::paintButton(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    if (info.context->paintingDisabled())
        return false;

    GtkWidget* widget = gtkButton();
    IntRect buttonRect(IntPoint(), rect.size());
    IntRect focusRect(buttonRect);

    GtkStateType state = getGtkStateType(object);
    gtk_widget_set_state(widget, state);
    gtk_widget_set_direction(widget, gtkTextDirection(object->style()->direction()));

    if (isFocused(object)) {
        if (isEnabled(object)) {
#if !GTK_CHECK_VERSION(2, 22, 0)
            GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_FOCUS);
#endif
            g_object_set(widget, "has-focus", TRUE, NULL);
        }

        gboolean interiorFocus = 0, focusWidth = 0, focusPadding = 0;
        gtk_widget_style_get(widget,
                             "interior-focus", &interiorFocus,
                             "focus-line-width", &focusWidth,
                             "focus-padding", &focusPadding, NULL);
        // If we are using exterior focus, we shrink the button rect down before
        // drawing. If we are using interior focus we shrink the focus rect. This
        // approach originates from the Mozilla theme drawing code (gtk2drawing.c).
        if (interiorFocus) {
            GtkStyle* style = gtk_widget_get_style(widget);
            focusRect.inflateX(-style->xthickness - focusPadding);
            focusRect.inflateY(-style->ythickness - focusPadding);
        } else {
            buttonRect.inflateX(-focusWidth - focusPadding);
            buttonRect.inflateY(-focusPadding - focusPadding);
        }
    }

    WidgetRenderingContext widgetContext(info.context, rect);
    GtkShadowType shadowType = state == GTK_STATE_ACTIVE ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
    widgetContext.gtkPaintBox(buttonRect, widget, state, shadowType, "button");
    if (isFocused(object))
        widgetContext.gtkPaintFocus(focusRect, widget, state, "button");

#if !GTK_CHECK_VERSION(2, 22, 0)
    GTK_WIDGET_UNSET_FLAGS(widget, GTK_HAS_FOCUS);
#endif
    g_object_set(widget, "has-focus", FALSE, NULL);
    return false;
}

static void getComboBoxPadding(RenderStyle* style, int& left, int& top, int& right, int& bottom)
{
    // If this menu list button isn't drawn using the native theme, we
    // don't add any extra padding beyond what WebCore already uses.
    if (style->appearance() == NoControlPart)
        return;
    moz_gtk_get_widget_border(MOZ_GTK_DROPDOWN, &left, &top, &right, &bottom,
                              gtkTextDirection(style->direction()), TRUE);
}

int RenderThemeGtk::popupInternalPaddingLeft(RenderStyle* style) const
{
    int left = 0, top = 0, right = 0, bottom = 0;
    getComboBoxPadding(style, left, top, right, bottom);
    return left;
}

int RenderThemeGtk::popupInternalPaddingRight(RenderStyle* style) const
{
    int left = 0, top = 0, right = 0, bottom = 0;
    getComboBoxPadding(style, left, top, right, bottom);
    return right;
}

int RenderThemeGtk::popupInternalPaddingTop(RenderStyle* style) const
{
    int left = 0, top = 0, right = 0, bottom = 0;
    getComboBoxPadding(style, left, top, right, bottom);
    return top;
}

int RenderThemeGtk::popupInternalPaddingBottom(RenderStyle* style) const
{
    int left = 0, top = 0, right = 0, bottom = 0;
    getComboBoxPadding(style, left, top, right, bottom);
    return bottom;
}

void RenderThemeGtk::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    // The tests check explicitly that select menu buttons ignore line height.
    style->setLineHeight(RenderStyle::initialLineHeight());

    // We cannot give a proper rendering when border radius is active, unfortunately.
    style->resetBorderRadius();
}

void RenderThemeGtk::adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustMenuListStyle(selector, style, e);
}

bool RenderThemeGtk::paintMenuList(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintRenderObject(MOZ_GTK_DROPDOWN, o, i.context, rect);
}

bool RenderThemeGtk::paintMenuListButton(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintMenuList(object, info, rect);
}

static void setTextInputBorders(RenderStyle* style)
{
    // If this control isn't drawn using the native theme, we don't touch the borders.
    if (style->appearance() == NoControlPart)
        return;

    // We cannot give a proper rendering when border radius is active, unfortunately.
    style->resetBorderRadius();

    int left = 0, top = 0, right = 0, bottom = 0;
    moz_gtk_get_widget_border(MOZ_GTK_ENTRY, &left, &top, &right, &bottom,
                              gtkTextDirection(style->direction()), TRUE);
    style->setBorderLeftWidth(left);
    style->setBorderTopWidth(top);
    style->setBorderRightWidth(right);
    style->setBorderBottomWidth(bottom);
}

void RenderThemeGtk::adjustTextFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    setTextInputBorders(style);
}

bool RenderThemeGtk::paintTextField(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintRenderObject(MOZ_GTK_ENTRY, o, i.context, rect);
}

void RenderThemeGtk::adjustTextAreaStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    setTextInputBorders(style);
}

bool RenderThemeGtk::paintTextArea(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeGtk::adjustSearchFieldResultsButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustSearchFieldCancelButtonStyle(selector, style, e);
}

bool RenderThemeGtk::paintSearchFieldResultsButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintSearchFieldResultsDecoration(o, i, rect);
}

void RenderThemeGtk::adjustSearchFieldResultsDecorationStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->resetBorder();
    style->resetPadding();

    gint width = 0, height = 0;
    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);
    style->setWidth(Length(width, Fixed));
    style->setHeight(Length(height, Fixed));
}

static IntPoint centerRectVerticallyInParentInputElement(RenderObject* object, const IntRect& rect)
{
    Node* input = object->node()->shadowAncestorNode(); // Get the renderer of <input> element.
    if (!input->renderer()->isBox())
        return rect.topLeft();

    // If possible center the y-coordinate of the rect vertically in the parent input element.
    // We also add one pixel here to ensure that the y coordinate is rounded up for box heights
    // that are even, which looks in relation to the box text.
    IntRect inputContentBox = toRenderBox(input->renderer())->absoluteContentBox();

    return IntPoint(rect.x(), inputContentBox.y() + (inputContentBox.height() - rect.height() + 1) / 2);
}

bool RenderThemeGtk::paintSearchFieldResultsDecoration(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    GtkStyle* style = gtk_widget_get_style(GTK_WIDGET(gtkEntry()));
    IntPoint iconPoint(centerRectVerticallyInParentInputElement(renderObject, rect));
    paintStockIcon(paintInfo.context, iconPoint, style, GTK_STOCK_FIND,
                   gtkTextDirection(renderObject->style()->direction()),
                   gtkIconState(renderObject), GTK_ICON_SIZE_MENU);
    return false;
}

void RenderThemeGtk::adjustSearchFieldCancelButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->resetBorder();
    style->resetPadding();

    gint width = 0, height = 0;
    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);
    style->setWidth(Length(width, Fixed));
    style->setHeight(Length(height, Fixed));
}

bool RenderThemeGtk::paintSearchFieldCancelButton(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    GtkStyle* style = gtk_widget_get_style(GTK_WIDGET(gtkEntry()));
    IntPoint iconPoint(centerRectVerticallyInParentInputElement(renderObject, rect));
    paintStockIcon(paintInfo.context, iconPoint, style, GTK_STOCK_CLEAR,
                   gtkTextDirection(renderObject->style()->direction()),
                   gtkIconState(renderObject), GTK_ICON_SIZE_MENU);
    return false;
}

void RenderThemeGtk::adjustSearchFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->setLineHeight(RenderStyle::initialLineHeight());
    setTextInputBorders(style);
}

bool RenderThemeGtk::paintSearchField(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintTextField(o, i, rect);
}

bool RenderThemeGtk::paintSliderTrack(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    if (info.context->paintingDisabled())
        return false;

    ControlPart part = object->style()->appearance();
    ASSERT(part == SliderHorizontalPart || part == SliderVerticalPart);

    // We shrink the trough rect slightly to make room for the focus indicator.
    IntRect troughRect(IntPoint(), rect.size()); // This is relative to rect.
    GtkWidget* widget = 0;
    if (part == SliderVerticalPart) {
        widget = gtkVScale();
        troughRect.inflateY(-gtk_widget_get_style(widget)->ythickness);
    } else {
        widget = gtkHScale();
        troughRect.inflateX(-gtk_widget_get_style(widget)->xthickness);
    }
    gtk_widget_set_direction(widget, gtkTextDirection(object->style()->direction()));

    WidgetRenderingContext widgetContext(info.context, rect);
    widgetContext.gtkPaintBox(troughRect, widget, GTK_STATE_ACTIVE, GTK_SHADOW_OUT, "trough");
    if (isFocused(object))
        widgetContext.gtkPaintFocus(IntRect(IntPoint(), rect.size()), widget, getGtkStateType(object), "trough");

    return false;
}

void RenderThemeGtk::adjustSliderTrackStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(0);
}

bool RenderThemeGtk::paintSliderThumb(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    if (info.context->paintingDisabled())
        return false;

    ControlPart part = object->style()->appearance();
    ASSERT(part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart);

    GtkWidget* widget = 0;
    const char* detail = 0;
    GtkOrientation orientation;
    if (part == SliderThumbVerticalPart) {
        widget = gtkVScale();
        detail = "vscale";
        orientation = GTK_ORIENTATION_VERTICAL;
    } else {
        widget = gtkHScale();
        detail = "hscale";
        orientation = GTK_ORIENTATION_HORIZONTAL;
    }
    gtk_widget_set_direction(widget, gtkTextDirection(object->style()->direction()));

    // Only some themes have slider thumbs respond to clicks and some don't. This information is
    // gathered via the 'activate-slider' property, but it's deprecated in GTK+ 2.22 and removed in
    // GTK+ 3.x. The drawback of not honoring it is that slider thumbs change color when you click
    // on them. 
    IntRect thumbRect(IntPoint(), rect.size());
    WidgetRenderingContext widgetContext(info.context, rect);
    widgetContext.gtkPaintSlider(thumbRect, widget, getGtkStateType(object), GTK_SHADOW_OUT, detail, orientation);
    return false;
}

void RenderThemeGtk::adjustSliderThumbStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(0);
}

void RenderThemeGtk::adjustSliderThumbSize(RenderObject* o) const
{
    ControlPart part = o->style()->appearance();
#if ENABLE(VIDEO)
    if (part == MediaSliderThumbPart) {
        o->style()->setWidth(Length(m_mediaSliderThumbWidth, Fixed));
        o->style()->setHeight(Length(m_mediaSliderThumbHeight, Fixed));
        return;
    }
    if (part == MediaVolumeSliderThumbPart)
        return;
#endif

    GtkWidget* widget = part == SliderThumbHorizontalPart ? gtkHScale() : gtkVScale();
    int length = 0, width = 0;
    gtk_widget_style_get(widget,
                         "slider_length", &length,
                         "slider_width", &width,
                         NULL);

    if (part == SliderThumbHorizontalPart) {
        o->style()->setWidth(Length(length, Fixed));
        o->style()->setHeight(Length(width, Fixed));
        return;
    }
    ASSERT(part == SliderThumbVerticalPart);
    o->style()->setWidth(Length(width, Fixed));
    o->style()->setHeight(Length(length, Fixed));
}

Color RenderThemeGtk::platformActiveSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return gtk_widget_get_style(widget)->base[GTK_STATE_SELECTED];
}

Color RenderThemeGtk::platformInactiveSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return gtk_widget_get_style(widget)->base[GTK_STATE_ACTIVE];
}

Color RenderThemeGtk::platformActiveSelectionForegroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return gtk_widget_get_style(widget)->text[GTK_STATE_SELECTED];
}

Color RenderThemeGtk::platformInactiveSelectionForegroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return gtk_widget_get_style(widget)->text[GTK_STATE_ACTIVE];
}

Color RenderThemeGtk::activeListBoxSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return gtk_widget_get_style(widget)->base[GTK_STATE_SELECTED];
}

Color RenderThemeGtk::inactiveListBoxSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return gtk_widget_get_style(widget)->base[GTK_STATE_ACTIVE];
}

Color RenderThemeGtk::activeListBoxSelectionForegroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return gtk_widget_get_style(widget)->text[GTK_STATE_SELECTED];
}

Color RenderThemeGtk::inactiveListBoxSelectionForegroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return gtk_widget_get_style(widget)->text[GTK_STATE_ACTIVE];
}

double RenderThemeGtk::caretBlinkInterval() const
{
    GtkSettings* settings = gtk_settings_get_default();

    gboolean shouldBlink;
    gint time;

    g_object_get(settings, "gtk-cursor-blink", &shouldBlink, "gtk-cursor-blink-time", &time, NULL);

    if (!shouldBlink)
        return 0;

    return time / 2000.;
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

void RenderThemeGtk::systemFont(int, FontDescription& fontDescription) const
{
    GtkSettings* settings = gtk_settings_get_default();
    if (!settings)
        return;

    // This will be a font selection string like "Sans 10" so we cannot use it as the family name.
    GOwnPtr<gchar> fontName;
    g_object_get(settings, "gtk-font-name", &fontName.outPtr(), NULL);

    PangoFontDescription* pangoDescription = pango_font_description_from_string(fontName.get());
    if (!pangoDescription)
        return;

    fontDescription.firstFamily().setFamily(pango_font_description_get_family(pangoDescription));

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

Color RenderThemeGtk::systemColor(int cssValueId) const
{
    switch (cssValueId) {
    case CSSValueButtontext:
        return Color(gtk_widget_get_style(gtkButton())->fg[GTK_STATE_NORMAL]);
    case CSSValueCaptiontext:
        return Color(gtk_widget_get_style(gtkEntry())->fg[GTK_STATE_NORMAL]);
    default:
        return RenderTheme::systemColor(cssValueId);
    }
}

static void gtkStyleSetCallback(GtkWidget* widget, GtkStyle* previous, RenderTheme* renderTheme)
{
    // FIXME: Make sure this function doesn't get called many times for a single GTK+ style change signal.
    renderTheme->platformColorsDidChange();
}

void RenderThemeGtk::setupWidgetAndAddToContainer(GtkWidget* widget, GtkWidget* window) const
{
    gtk_container_add(GTK_CONTAINER(window), widget);
    gtk_widget_realize(widget);
    g_object_set_data(G_OBJECT(widget), "transparent-bg-hint", GINT_TO_POINTER(TRUE));

    // FIXME: Perhaps this should only be called for the containing window or parent container.
    g_signal_connect(widget, "style-set", G_CALLBACK(gtkStyleSetCallback), const_cast<RenderThemeGtk*>(this));
}

GtkWidget* RenderThemeGtk::gtkContainer() const
{
    if (m_gtkContainer)
        return m_gtkContainer;

    m_gtkWindow = gtk_window_new(GTK_WINDOW_POPUP);
#ifdef GTK_API_VERSION_2
    gtk_widget_set_colormap(m_gtkWindow, m_themeParts.colormap);
#endif
    gtk_widget_realize(m_gtkWindow);
    gtk_widget_set_name(m_gtkWindow, "MozillaGtkWidget");

    m_gtkContainer = gtk_fixed_new();
    setupWidgetAndAddToContainer(m_gtkContainer, m_gtkWindow);
    return m_gtkContainer;
}

GtkWidget* RenderThemeGtk::gtkButton() const
{
    if (m_gtkButton)
        return m_gtkButton;
    m_gtkButton = gtk_button_new();
    setupWidgetAndAddToContainer(m_gtkButton, gtkContainer());
    return m_gtkButton;
}

GtkWidget* RenderThemeGtk::gtkEntry() const
{
    if (m_gtkEntry)
        return m_gtkEntry;
    m_gtkEntry = gtk_entry_new();
    setupWidgetAndAddToContainer(m_gtkEntry, gtkContainer());
    return m_gtkEntry;
}

GtkWidget* RenderThemeGtk::gtkTreeView() const
{
    if (m_gtkTreeView)
        return m_gtkTreeView;
    m_gtkTreeView = gtk_tree_view_new();
    setupWidgetAndAddToContainer(m_gtkTreeView, gtkContainer());
    return m_gtkTreeView;
}

GtkWidget* RenderThemeGtk::gtkVScale() const
{
    if (m_gtkVScale)
        return m_gtkVScale;
    m_gtkVScale = gtk_vscale_new(0);
    setupWidgetAndAddToContainer(m_gtkVScale, gtkContainer());
    return m_gtkVScale;
}

GtkWidget* RenderThemeGtk::gtkHScale() const
{
    if (m_gtkHScale)
        return m_gtkHScale;
    m_gtkHScale = gtk_hscale_new(0);
    setupWidgetAndAddToContainer(m_gtkHScale, gtkContainer());
    return m_gtkHScale;
}

GtkWidget* RenderThemeGtk::gtkScrollbar()
{
    return moz_gtk_get_scrollbar_widget();
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

bool RenderThemeGtk::paintMediaButton(RenderObject* renderObject, GraphicsContext* context, const IntRect& rect, const char* iconName)
{
    GtkStyle* style = gtk_widget_get_style(GTK_WIDGET(gtkContainer()));
    IntPoint iconPoint(rect.x() + (rect.width() - m_mediaIconSize) / 2,
                       rect.y() + (rect.height() - m_mediaIconSize) / 2);
    context->fillRect(FloatRect(rect), m_panelColor, ColorSpaceDeviceRGB);
    paintStockIcon(context, iconPoint, style, iconName, gtkTextDirection(renderObject->style()->direction()),
                   gtkIconState(renderObject), getMediaButtonIconSize(m_mediaIconSize));

    return false;
}

bool RenderThemeGtk::paintMediaFullscreenButton(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintMediaButton(renderObject, paintInfo.context, rect, GTK_STOCK_FULLSCREEN);
}

bool RenderThemeGtk::paintMediaMuteButton(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = getMediaElementFromRenderObject(renderObject);
    if (!mediaElement)
        return false;

    return paintMediaButton(renderObject, paintInfo.context, rect, mediaElement->muted() ? "audio-volume-muted" : "audio-volume-high");
}

bool RenderThemeGtk::paintMediaPlayButton(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    Node* node = renderObject->node();
    if (!node)
        return false;

    MediaControlPlayButtonElement* button = static_cast<MediaControlPlayButtonElement*>(node);
    return paintMediaButton(renderObject, paintInfo.context, rect, button->displayType() == MediaPlayButton ? GTK_STOCK_MEDIA_PLAY : GTK_STOCK_MEDIA_PAUSE);
}

bool RenderThemeGtk::paintMediaSeekBackButton(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintMediaButton(renderObject, paintInfo.context, rect, GTK_STOCK_MEDIA_REWIND);
}

bool RenderThemeGtk::paintMediaSeekForwardButton(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintMediaButton(renderObject, paintInfo.context, rect, GTK_STOCK_MEDIA_FORWARD);
}

bool RenderThemeGtk::paintMediaSliderTrack(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    GraphicsContext* context = paintInfo.context;

    context->fillRect(FloatRect(r), m_panelColor, ColorSpaceDeviceRGB);
    context->fillRect(FloatRect(IntRect(r.x(), r.y() + (r.height() - m_mediaSliderHeight) / 2,
                                        r.width(), m_mediaSliderHeight)), m_sliderColor, ColorSpaceDeviceRGB);

    RenderStyle* style = o->style();
    HTMLMediaElement* mediaElement = toParentMediaElement(o);

    if (!mediaElement)
        return false;

    // Draw the buffered ranges. This code is highly inspired from
    // Chrome for the gradient code.
    float mediaDuration = mediaElement->duration();
    RefPtr<TimeRanges> timeRanges = mediaElement->buffered();
    IntRect trackRect = r;
    int totalWidth = trackRect.width();

    trackRect.inflate(-style->borderLeftWidth());
    context->save();
    context->setStrokeStyle(NoStroke);

    for (unsigned index = 0; index < timeRanges->length(); ++index) {
        ExceptionCode ignoredException;
        float start = timeRanges->start(index, ignoredException);
        float end = timeRanges->end(index, ignoredException);
        int width = ((end - start) * totalWidth) / mediaDuration;
        IntRect rangeRect;
        if (!index) {
            rangeRect = trackRect;
            rangeRect.setWidth(width);
        } else {
            rangeRect.setLocation(IntPoint(trackRect.x() + start / mediaDuration* totalWidth, trackRect.y()));
            rangeRect.setSize(IntSize(width, trackRect.height()));
        }

        // Don't bother drawing empty range.
        if (rangeRect.isEmpty())
            continue;

        IntPoint sliderTopLeft = rangeRect.location();
        IntPoint sliderTopRight = sliderTopLeft;
        sliderTopRight.move(0, rangeRect.height());

        RefPtr<Gradient> gradient = Gradient::create(sliderTopLeft, sliderTopRight);
        Color startColor = m_panelColor;
        gradient->addColorStop(0.0, startColor);
        gradient->addColorStop(1.0, Color(startColor.red() / 2, startColor.green() / 2, startColor.blue() / 2, startColor.alpha()));

        context->setFillGradient(gradient);
        context->fillRect(rangeRect);
    }

    context->restore();
    return false;
}

bool RenderThemeGtk::paintMediaSliderThumb(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    // Make the thumb nicer with rounded corners.
    paintInfo.context->fillRoundedRect(r, IntSize(3, 3), IntSize(3, 3), IntSize(3, 3), IntSize(3, 3), m_sliderThumbColor, ColorSpaceDeviceRGB);
    return false;
}
#endif

#if ENABLE(PROGRESS_TAG)
double RenderThemeGtk::animationRepeatIntervalForProgressBar(RenderProgress*) const
{
    // FIXME: It doesn't look like there is a good way yet to support animated
    // progress bars with the Mozilla theme drawing code.
    return 0;
}

double RenderThemeGtk::animationDurationForProgressBar(RenderProgress*) const
{
    // FIXME: It doesn't look like there is a good way yet to support animated
    // progress bars with the Mozilla theme drawing code.
    return 0;
}

void RenderThemeGtk::adjustProgressBarStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(0);
}

bool RenderThemeGtk::paintProgressBar(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!renderObject->isProgress())
        return true;

    GtkWidget* progressBarWidget = moz_gtk_get_progress_widget();
    if (!progressBarWidget)
        return true;

    if (paintRenderObject(MOZ_GTK_PROGRESSBAR, renderObject, paintInfo.context, rect))
        return true;

    IntRect chunkRect(rect);
    RenderProgress* renderProgress = toRenderProgress(renderObject);

    GtkStyle* style = gtk_widget_get_style(progressBarWidget);
    chunkRect.setHeight(chunkRect.height() - (2 * style->ythickness));
    chunkRect.setY(chunkRect.y() + style->ythickness);
    chunkRect.setWidth((chunkRect.width() - (2 * style->xthickness)) * renderProgress->position());
    if (renderObject->style()->direction() == RTL)
        chunkRect.setX(rect.x() + rect.width() - chunkRect.width() - style->xthickness);
    else
        chunkRect.setX(chunkRect.x() + style->xthickness);

    return paintRenderObject(MOZ_GTK_PROGRESS_CHUNK, renderObject, paintInfo.context, chunkRect);
}
#endif

}
