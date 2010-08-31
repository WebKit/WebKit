/*
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2009 Kenneth Rohde Christiansen
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
#include "gtkdrawing.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <wtf/text/CString.h>

#if ENABLE(PROGRESS_TAG)
#include "RenderProgress.h"
#endif

namespace WebCore {

using namespace HTMLNames;

#if ENABLE(VIDEO)
static HTMLMediaElement* getMediaElementFromRenderObject(RenderObject* o)
{
    Node* node = o->node();
    Node* mediaNode = node ? node->shadowAncestorNode() : 0;
    if (!mediaNode || (!mediaNode->hasTagName(videoTag) && !mediaNode->hasTagName(audioTag)))
        return 0;

    return static_cast<HTMLMediaElement*>(mediaNode);
}

static gchar* getIconNameForTextDirection(const char* baseName)
{
    GString* nameWithDirection = g_string_new(baseName);
    GtkTextDirection textDirection = gtk_widget_get_default_direction();

    if (textDirection == GTK_TEXT_DIR_RTL)
        g_string_append(nameWithDirection, "-rtl");
    else if (textDirection == GTK_TEXT_DIR_LTR)
        g_string_append(nameWithDirection, "-ltr");

    return g_string_free(nameWithDirection, FALSE);
}

void RenderThemeGtk::initMediaStyling(GtkStyle* style, bool force)
{
    static bool stylingInitialized = false;

    if (!stylingInitialized || force) {
        m_panelColor = style->bg[GTK_STATE_NORMAL];
        m_sliderColor = style->bg[GTK_STATE_ACTIVE];
        m_sliderThumbColor = style->bg[GTK_STATE_SELECTED];

        // Names of these icons can vary because of text direction.
        gchar* playButtonIconName = getIconNameForTextDirection("gtk-media-play");
        gchar* seekBackButtonIconName = getIconNameForTextDirection("gtk-media-rewind");
        gchar* seekForwardButtonIconName = getIconNameForTextDirection("gtk-media-forward");

        m_fullscreenButton.clear();
        m_muteButton.clear();
        m_unmuteButton.clear();
        m_playButton.clear();
        m_pauseButton.clear();
        m_seekBackButton.clear();
        m_seekForwardButton.clear();

        m_fullscreenButton = Image::loadPlatformThemeIcon("gtk-fullscreen", m_mediaIconSize);
        // Note that the muteButton and unmuteButton take icons reflecting
        // the *current* state. Hence, the unmuteButton represents the *muted*
        // status, the muteButton represents the then current *unmuted* status.
        m_muteButton = Image::loadPlatformThemeIcon("audio-volume-high", m_mediaIconSize);
        m_unmuteButton = Image::loadPlatformThemeIcon("audio-volume-muted", m_mediaIconSize);
        m_playButton = Image::loadPlatformThemeIcon(reinterpret_cast<const char*>(playButtonIconName), m_mediaIconSize);
        m_pauseButton = Image::loadPlatformThemeIcon("gtk-media-pause", m_mediaIconSize);
        m_seekBackButton = Image::loadPlatformThemeIcon(reinterpret_cast<const char*>(seekBackButtonIconName), m_mediaIconSize);
        m_seekForwardButton = Image::loadPlatformThemeIcon(reinterpret_cast<const char*>(seekForwardButtonIconName), m_mediaIconSize);

        g_free(playButtonIconName);
        g_free(seekBackButtonIconName);
        g_free(seekForwardButtonIconName);
        stylingInitialized = true;
    }
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
    , m_fullscreenButton(0)
    , m_muteButton(0)
    , m_unmuteButton(0)
    , m_playButton(0)
    , m_pauseButton(0)
    , m_seekBackButton(0)
    , m_seekForwardButton(0)
    , m_partsTable(adoptPlatformRef(g_hash_table_new_full(0, 0, 0, g_free)))
{
    if (!mozGtkRefCount) {
        moz_gtk_init();

        // Use the theme parts for the default drawable.
        moz_gtk_use_theme_parts(partsForDrawable(0));
    }

    ++mozGtkRefCount;

#if ENABLE(VIDEO)
    initMediaStyling(gtk_rc_get_style(GTK_WIDGET(gtkContainer())), false);
#endif
}

RenderThemeGtk::~RenderThemeGtk()
{
    --mozGtkRefCount;

    if (!mozGtkRefCount)
        moz_gtk_shutdown();

    m_fullscreenButton.clear();
    m_muteButton.clear();
    m_unmuteButton.clear();
    m_playButton.clear();
    m_pauseButton.clear();
    m_seekBackButton.clear();
    m_seekForwardButton.clear();

    GList* values = g_hash_table_get_values(m_partsTable.get());
    for (guint i = 0; i < g_list_length(values); i++)
        moz_gtk_destroy_theme_parts_widgets(
            static_cast<GtkThemeParts*>(g_list_nth_data(values, i)));

    gtk_widget_destroy(m_gtkWindow);
}

GtkThemeParts* RenderThemeGtk::partsForDrawable(GdkDrawable* drawable) const
{
    // A null drawable represents the default screen colormap.
    GdkColormap* colormap = 0;
    if (!drawable)
        colormap = gdk_screen_get_default_colormap(gdk_screen_get_default());
    else
        colormap = gdk_drawable_get_colormap(drawable);

    GtkThemeParts* parts = static_cast<GtkThemeParts*>(g_hash_table_lookup(m_partsTable.get(), colormap));
    if (!parts) {
        parts = g_new0(GtkThemeParts, 1);
        parts->colormap = colormap;
        g_hash_table_insert(m_partsTable.get(), colormap, parts);
    }

    return parts;
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

static void adjustMozillaStyle(const RenderThemeGtk* theme, RenderStyle* style, GtkThemeWidgetType type)
{
    gint left, top, right, bottom;
    GtkTextDirection direction = gtkTextDirection(style->direction());
    gboolean inhtml = true;

    if (moz_gtk_get_widget_border(type, &left, &top, &right, &bottom, direction, inhtml) != MOZ_GTK_SUCCESS)
        return;

    // FIXME: This approach is likely to be incorrect. See other ports and layout tests to see the problem.
    const int xpadding = 1;
    const int ypadding = 1;

    style->setPaddingLeft(Length(xpadding + left, Fixed));
    style->setPaddingTop(Length(ypadding + top, Fixed));
    style->setPaddingRight(Length(xpadding + right, Fixed));
    style->setPaddingBottom(Length(ypadding + bottom, Fixed));
}

bool RenderThemeGtk::paintMozillaGtkWidget(GtkThemeWidgetType type, GraphicsContext* context, const IntRect& rect, GtkWidgetState* widgetState, int flags, GtkTextDirection textDirection)
{
    // Painting is disabled so just claim to have succeeded
    if (context->paintingDisabled())
        return false;

    PlatformRefPtr<GdkDrawable> drawable(context->gdkDrawable());
    GdkRectangle paintRect, clipRect;
    if (drawable) {
        AffineTransform ctm = context->getCTM();
        IntPoint pos = ctm.mapPoint(rect.location());
        paintRect = IntRect(pos.x(), pos.y(), rect.width(), rect.height());

        // Intersect the cairo rectangle with the target widget region. This  will
        // prevent the theme drawing code from drawing into regions that cairo will
        // clip anyway.
        double clipX1, clipX2, clipY1, clipY2;
        cairo_clip_extents(context->platformContext(), &clipX1, &clipY1, &clipX2, &clipY2);
        IntPoint clipPos = ctm.mapPoint(IntPoint(clipX1, clipY1));

        clipRect.width = clipX2 - clipX1;
        clipRect.height = clipY2 - clipY1;
        clipRect.x = clipPos.x();
        clipRect.y = clipPos.y();
        gdk_rectangle_intersect(&paintRect, &clipRect, &clipRect);

    } else {
        // In some situations, like during print previews, this GraphicsContext is not
        // backed by a GdkDrawable. In those situations, we render onto a pixmap and then
        // copy the rendered data back to the GraphicsContext via Cairo.
        drawable = adoptPlatformRef(gdk_pixmap_new(0, rect.width(), rect.height(), gdk_visual_get_depth(gdk_visual_get_system())));
        paintRect = clipRect = IntRect(0, 0, rect.width(), rect.height());
    }

    moz_gtk_use_theme_parts(partsForDrawable(drawable.get()));
    bool success = moz_gtk_widget_paint(type, drawable.get(), &paintRect, &clipRect, widgetState, flags, textDirection) == MOZ_GTK_SUCCESS;

    // If the drawing was successful and we rendered onto a pixmap, copy the
    // results back to the original GraphicsContext.
    if (success && !context->gdkDrawable()) {
        cairo_t* cairoContext = context->platformContext();
        cairo_save(cairoContext);
        gdk_cairo_set_source_pixmap(cairoContext, drawable.get(), rect.x(), rect.y());
        cairo_paint(cairoContext);
        cairo_restore(cairoContext);
    }

    return !success;
}

bool RenderThemeGtk::paintRenderObject(GtkThemeWidgetType type, RenderObject* renderObject, GraphicsContext* context, const IntRect& rect, int flags)
{
    // Painting is disabled so just claim to have succeeded
    if (context->paintingDisabled())
        return false;

    GtkWidgetState widgetState;
    widgetState.active = isPressed(renderObject);
    widgetState.focused = isFocused(renderObject);
    widgetState.inHover = isHovered(renderObject);

    // FIXME: Disabled does not always give the correct appearance for ReadOnly
    widgetState.disabled = !isEnabled(renderObject) || isReadOnlyControl(renderObject);
    widgetState.isDefault = false;
    widgetState.canDefault = false;

    // FIXME: The depressed value should probably apply for other theme parts too.
    // It must be used for range thumbs, because otherwise when the thumb is pressed,
    // the rendering is incorrect.
    if (type == MOZ_GTK_SCALE_THUMB_HORIZONTAL || type == MOZ_GTK_SCALE_THUMB_VERTICAL)
        widgetState.depressed = isPressed(renderObject);
    else
        widgetState.depressed = false;

    GtkTextDirection textDirection = gtkTextDirection(renderObject->style()->direction());
    return paintMozillaGtkWidget(type, context, rect, &widgetState, flags, textDirection);
}

static void setButtonPadding(RenderStyle* style)
{
    // FIXME: This looks incorrect.
    const int padding = 8;
    style->setPaddingLeft(Length(padding, Fixed));
    style->setPaddingRight(Length(padding, Fixed));
    style->setPaddingTop(Length(padding / 2, Fixed));
    style->setPaddingBottom(Length(padding / 2, Fixed));
}

static void setToggleSize(const RenderThemeGtk* theme, RenderStyle* style, ControlPart appearance)
{
    // The width and height are both specified, so we shouldn't change them.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // FIXME: This is probably not correct use of indicatorSize and indicatorSpacing.
    gint indicatorSize, indicatorSpacing;

    switch (appearance) {
    case CheckboxPart:
        if (moz_gtk_checkbox_get_metrics(&indicatorSize, &indicatorSpacing) != MOZ_GTK_SUCCESS)
            return;
        break;
    case RadioPart:
        if (moz_gtk_radio_get_metrics(&indicatorSize, &indicatorSpacing) != MOZ_GTK_SUCCESS)
            return;
        break;
    default:
        return;
    }

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
    // FIXME: Is this condition necessary?
    if (style->appearance() == PushButtonPart) {
        style->resetBorder();
        style->setHeight(Length(Auto));
        style->setWhiteSpace(PRE);
        setButtonPadding(style);
    } else {
        // FIXME: This should not be hard-coded.
        style->setMinHeight(Length(14, Fixed));
        style->resetBorderTop();
        style->resetBorderBottom();
    }
}

bool RenderThemeGtk::paintButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintRenderObject(MOZ_GTK_BUTTON, o, i.context, rect, GTK_RELIEF_NORMAL);
}

void RenderThemeGtk::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const
{
    style->resetBorder();
    style->resetPadding();
    style->setHeight(Length(Auto));
    style->setWhiteSpace(PRE);
    adjustMozillaStyle(this, style, MOZ_GTK_DROPDOWN);
}

bool RenderThemeGtk::paintMenuList(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintRenderObject(MOZ_GTK_DROPDOWN, o, i.context, rect);
}

void RenderThemeGtk::adjustTextFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->resetBorder();
    style->resetPadding();
    style->setHeight(Length(Auto));
    style->setWhiteSpace(PRE);
    adjustMozillaStyle(this, style, MOZ_GTK_ENTRY);
}

bool RenderThemeGtk::paintTextField(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintRenderObject(MOZ_GTK_ENTRY, o, i.context, rect);
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

    // FIXME: This should not be hard-coded.
    IntSize size = IntSize(14, 14);
    style->setWidth(Length(size.width(), Fixed));
    style->setHeight(Length(size.height(), Fixed));
}

bool RenderThemeGtk::paintSearchFieldResultsDecoration(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    GraphicsContext* context = i.context;

    static Image* searchImage = Image::loadPlatformThemeIcon(GTK_STOCK_FIND, rect.width()).releaseRef();
    context->drawImage(searchImage, DeviceColorSpace, rect);

    return false;
}

void RenderThemeGtk::adjustSearchFieldCancelButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->resetBorder();
    style->resetPadding();

    // FIXME: This should not be hard-coded.
    IntSize size = IntSize(14, 14);
    style->setWidth(Length(size.width(), Fixed));
    style->setHeight(Length(size.height(), Fixed));
}

bool RenderThemeGtk::paintSearchFieldCancelButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    GraphicsContext* context = i.context;

    // TODO: Brightening up the image on hover is desirable here, I believe.
    static Image* cancelImage = Image::loadPlatformThemeIcon(GTK_STOCK_CLEAR, rect.width()).releaseRef();
    context->drawImage(cancelImage, DeviceColorSpace, rect);

    return false;
}

void RenderThemeGtk::adjustSearchFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustTextFieldStyle(selector, style, e);
}

bool RenderThemeGtk::paintSearchField(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintTextField(o, i, rect);
}

bool RenderThemeGtk::paintSliderTrack(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    ControlPart part = object->style()->appearance();
    ASSERT(part == SliderHorizontalPart || part == SliderVerticalPart);

    GtkThemeWidgetType gtkPart = MOZ_GTK_SCALE_HORIZONTAL;
    if (part == SliderVerticalPart)
        gtkPart = MOZ_GTK_SCALE_VERTICAL;

    return paintRenderObject(gtkPart, object, info.context, rect);
}

void RenderThemeGtk::adjustSliderTrackStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(0);
}

bool RenderThemeGtk::paintSliderThumb(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    ControlPart part = object->style()->appearance();
    ASSERT(part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart);

    GtkThemeWidgetType gtkPart = MOZ_GTK_SCALE_THUMB_HORIZONTAL;
    if (part == SliderThumbVerticalPart)
        gtkPart = MOZ_GTK_SCALE_THUMB_VERTICAL;

    return paintRenderObject(gtkPart, object, info.context, rect);
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
    } else
#endif
    if (part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart) {
        gint width, height;
        moz_gtk_get_scalethumb_metrics(part == SliderThumbHorizontalPart ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL, &width, &height);
        o->style()->setWidth(Length(width, Fixed));
        o->style()->setHeight(Length(height, Fixed));
    }
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

void RenderThemeGtk::systemFont(int, FontDescription&) const
{
    // If you remove this notImplemented(), replace it with an comment that explains why.
    notImplemented();
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

GtkContainer* RenderThemeGtk::gtkContainer() const
{
    if (m_gtkContainer)
        return m_gtkContainer;

    m_gtkWindow = gtk_window_new(GTK_WINDOW_POPUP);
    m_gtkContainer = GTK_CONTAINER(gtk_fixed_new());
    g_signal_connect(m_gtkWindow, "style-set", G_CALLBACK(gtkStyleSetCallback), const_cast<RenderThemeGtk*>(this));
    gtk_container_add(GTK_CONTAINER(m_gtkWindow), GTK_WIDGET(m_gtkContainer));
    gtk_widget_realize(m_gtkWindow);

    return m_gtkContainer;
}

GtkWidget* RenderThemeGtk::gtkButton() const
{
    if (m_gtkButton)
        return m_gtkButton;

    m_gtkButton = gtk_button_new();
    g_signal_connect(m_gtkButton, "style-set", G_CALLBACK(gtkStyleSetCallback), const_cast<RenderThemeGtk*>(this));
    gtk_container_add(gtkContainer(), m_gtkButton);
    gtk_widget_realize(m_gtkButton);

    return m_gtkButton;
}

GtkWidget* RenderThemeGtk::gtkEntry() const
{
    if (m_gtkEntry)
        return m_gtkEntry;

    m_gtkEntry = gtk_entry_new();
    g_signal_connect(m_gtkEntry, "style-set", G_CALLBACK(gtkStyleSetCallback), const_cast<RenderThemeGtk*>(this));
    gtk_container_add(gtkContainer(), m_gtkEntry);
    gtk_widget_realize(m_gtkEntry);

    return m_gtkEntry;
}

GtkWidget* RenderThemeGtk::gtkTreeView() const
{
    if (m_gtkTreeView)
        return m_gtkTreeView;

    m_gtkTreeView = gtk_tree_view_new();
    g_signal_connect(m_gtkTreeView, "style-set", G_CALLBACK(gtkStyleSetCallback), const_cast<RenderThemeGtk*>(this));
    gtk_container_add(gtkContainer(), m_gtkTreeView);
    gtk_widget_realize(m_gtkTreeView);

    return m_gtkTreeView;
}

GtkWidget* RenderThemeGtk::gtkScrollbar()
{
    return moz_gtk_get_scrollbar_widget();
}

void RenderThemeGtk::platformColorsDidChange()
{
#if ENABLE(VIDEO)
    initMediaStyling(gtk_rc_get_style(GTK_WIDGET(gtkContainer())), true);
#endif
    RenderTheme::platformColorsDidChange();
}

#if ENABLE(VIDEO)
String RenderThemeGtk::extraMediaControlsStyleSheet()
{
    return String(mediaControlsGtkUserAgentStyleSheet, sizeof(mediaControlsGtkUserAgentStyleSheet));
}

static inline bool paintMediaButton(GraphicsContext* context, const IntRect& r, Image* image, Color panelColor, int mediaIconSize)
{
    context->fillRect(FloatRect(r), panelColor, DeviceColorSpace);
    context->drawImage(image, DeviceColorSpace,
                       IntRect(r.x() + (r.width() - mediaIconSize) / 2,
                               r.y() + (r.height() - mediaIconSize) / 2,
                               mediaIconSize, mediaIconSize));

    return false;
}

bool RenderThemeGtk::paintMediaFullscreenButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    return paintMediaButton(paintInfo.context, r, m_fullscreenButton.get(), m_panelColor, m_mediaIconSize);
}

bool RenderThemeGtk::paintMediaMuteButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    HTMLMediaElement* mediaElement = getMediaElementFromRenderObject(o);
    if (!mediaElement)
        return false;

    return paintMediaButton(paintInfo.context, r, mediaElement->muted() ? m_unmuteButton.get() : m_muteButton.get(), m_panelColor, m_mediaIconSize);
}

bool RenderThemeGtk::paintMediaPlayButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    if (!node)
        return false;

    MediaControlPlayButtonElement* button = static_cast<MediaControlPlayButtonElement*>(node);
    return paintMediaButton(paintInfo.context, r, button->displayType() == MediaPlayButton ? m_playButton.get() : m_pauseButton.get(), m_panelColor, m_mediaIconSize);
}

bool RenderThemeGtk::paintMediaSeekBackButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    return paintMediaButton(paintInfo.context, r, m_seekBackButton.get(), m_panelColor, m_mediaIconSize);
}

bool RenderThemeGtk::paintMediaSeekForwardButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    return paintMediaButton(paintInfo.context, r, m_seekForwardButton.get(), m_panelColor, m_mediaIconSize);
}

bool RenderThemeGtk::paintMediaSliderTrack(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    GraphicsContext* context = paintInfo.context;

    context->fillRect(FloatRect(r), m_panelColor, DeviceColorSpace);
    context->fillRect(FloatRect(IntRect(r.x(), r.y() + (r.height() - m_mediaSliderHeight) / 2,
                                        r.width(), m_mediaSliderHeight)), m_sliderColor, DeviceColorSpace);

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
            rangeRect.setLocation(IntPoint((start * totalWidth) / mediaDuration, trackRect.y()));
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
    paintInfo.context->fillRoundedRect(r, IntSize(3, 3), IntSize(3, 3), IntSize(3, 3), IntSize(3, 3), m_sliderThumbColor, DeviceColorSpace);
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
