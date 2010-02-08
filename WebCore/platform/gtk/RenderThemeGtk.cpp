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
#include "CString.h"
#include "GOwnPtr.h"
#include "GraphicsContext.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "NotImplemented.h"
#include "RenderBox.h"
#include "RenderObject.h"
#include "UserAgentStyleSheets.h"
#include "gtkdrawing.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>

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
        m_muteButton = Image::loadPlatformThemeIcon("audio-volume-muted", m_mediaIconSize);
        m_unmuteButton = Image::loadPlatformThemeIcon("audio-volume-high", m_mediaIconSize);
        m_playButton = Image::loadPlatformThemeIcon(reinterpret_cast<const char*>(playButtonIconName), m_mediaIconSize);
        m_pauseButton = Image::loadPlatformThemeIcon("gtk-media-pause", m_mediaIconSize).releaseRef();
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
    , m_partsTable(adoptGRef(g_hash_table_new_full(0, 0, 0, g_free)))
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

static void setMozillaState(const RenderTheme* theme, GtkWidgetState* state, RenderObject* o)
{
    state->active = theme->isPressed(o);
    state->focused = theme->isFocused(o);
    state->inHover = theme->isHovered(o);
    // FIXME: Disabled does not always give the correct appearance for ReadOnly
    state->disabled = !theme->isEnabled(o) || theme->isReadOnlyControl(o);
    state->isDefault = false;
    state->canDefault = false;
    state->depressed = false;
}

static bool paintMozillaGtkWidget(const RenderThemeGtk* theme, GtkThemeWidgetType type, RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    // No GdkWindow to render to, so return true to fall back
    if (!i.context->gdkDrawable())
        return true;

    // Painting is disabled so just claim to have succeeded
    if (i.context->paintingDisabled())
        return false;

    GtkWidgetState mozState;
    setMozillaState(theme, &mozState, o);

    int flags;

    // We might want to make setting flags the caller's job at some point rather than doing it here.
    switch (type) {
    case MOZ_GTK_BUTTON:
        flags = GTK_RELIEF_NORMAL;
        break;
    case MOZ_GTK_CHECKBUTTON:
    case MOZ_GTK_RADIOBUTTON:
        flags = theme->isChecked(o);
        break;
    default:
        flags = 0;
        break;
    }

    AffineTransform ctm = i.context->getCTM();

    IntPoint pos = ctm.mapPoint(rect.location());
    GdkRectangle gdkRect = IntRect(pos.x(), pos.y(), rect.width(), rect.height());
    GtkTextDirection direction = gtkTextDirection(o->style()->direction());

    // Find the clip rectangle
    cairo_t* cr = i.context->platformContext();
    double clipX1, clipX2, clipY1, clipY2;
    cairo_clip_extents(cr, &clipX1, &clipY1, &clipX2, &clipY2);

    GdkRectangle gdkClipRect;
    gdkClipRect.width = clipX2 - clipX1;
    gdkClipRect.height = clipY2 - clipY1;
    IntPoint clipPos = ctm.mapPoint(IntPoint(clipX1, clipY1));
    gdkClipRect.x = clipPos.x();
    gdkClipRect.y = clipPos.y();

    gdk_rectangle_intersect(&gdkRect, &gdkClipRect, &gdkClipRect);

    // Since the theme renderer is going to be drawing onto this GdkDrawable,
    // select the appropriate widgets for the drawable depth.
    moz_gtk_use_theme_parts(theme->partsForDrawable(i.context->gdkDrawable()));
    return moz_gtk_widget_paint(type, i.context->gdkDrawable(), &gdkRect, &gdkClipRect, &mozState, flags, direction) != MOZ_GTK_SUCCESS;
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

bool RenderThemeGtk::paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozillaGtkWidget(this, MOZ_GTK_CHECKBUTTON, o, i, rect);
}

void RenderThemeGtk::setRadioSize(RenderStyle* style) const
{
    setToggleSize(this, style, RadioPart);
}

bool RenderThemeGtk::paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozillaGtkWidget(this, MOZ_GTK_RADIOBUTTON, o, i, rect);
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

bool RenderThemeGtk::paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozillaGtkWidget(this, MOZ_GTK_BUTTON, o, i, rect);
}

void RenderThemeGtk::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const
{
    style->resetBorder();
    style->resetPadding();
    style->setHeight(Length(Auto));
    style->setWhiteSpace(PRE);
    adjustMozillaStyle(this, style, MOZ_GTK_DROPDOWN);
}

bool RenderThemeGtk::paintMenuList(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozillaGtkWidget(this, MOZ_GTK_DROPDOWN, o, i, rect);
}

void RenderThemeGtk::adjustTextFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->resetBorder();
    style->resetPadding();
    style->setHeight(Length(Auto));
    style->setWhiteSpace(PRE);
    adjustMozillaStyle(this, style, MOZ_GTK_ENTRY);
}

bool RenderThemeGtk::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozillaGtkWidget(this, MOZ_GTK_ENTRY, o, i, rect);
}

bool RenderThemeGtk::paintTextArea(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeGtk::adjustSearchFieldResultsButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustSearchFieldCancelButtonStyle(selector, style, e);
}

bool RenderThemeGtk::paintSearchFieldResultsButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozillaGtkWidget(this, MOZ_GTK_DROPDOWN_ARROW, o, i, rect);
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

bool RenderThemeGtk::paintSearchFieldResultsDecoration(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozillaGtkWidget(this, MOZ_GTK_CHECKMENUITEM, o, i, rect);
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

bool RenderThemeGtk::paintSearchFieldCancelButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozillaGtkWidget(this, MOZ_GTK_CHECKMENUITEM, o, i, rect);
}

void RenderThemeGtk::adjustSearchFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustTextFieldStyle(selector, style, e);
}

bool RenderThemeGtk::paintSearchField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintTextField(o, i, rect);
}

void RenderThemeGtk::adjustSliderThumbSize(RenderObject* o) const
{
#if ENABLE(VIDEO)
    if (o->style()->appearance() == MediaSliderThumbPart) {
        o->style()->setWidth(Length(m_mediaSliderThumbWidth, Fixed));
        o->style()->setHeight(Length(m_mediaSliderThumbHeight, Fixed));
    }
#endif
}

Color RenderThemeGtk::platformActiveSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return widget->style->base[GTK_STATE_SELECTED];
}

Color RenderThemeGtk::platformInactiveSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return widget->style->base[GTK_STATE_ACTIVE];
}

Color RenderThemeGtk::platformActiveSelectionForegroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return widget->style->text[GTK_STATE_SELECTED];
}

Color RenderThemeGtk::platformInactiveSelectionForegroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return widget->style->text[GTK_STATE_ACTIVE];
}

Color RenderThemeGtk::activeListBoxSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return widget->style->base[GTK_STATE_SELECTED];
}

Color RenderThemeGtk::inactiveListBoxSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return widget->style->base[GTK_STATE_ACTIVE];
}

Color RenderThemeGtk::activeListBoxSelectionForegroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return widget->style->text[GTK_STATE_SELECTED];
}

Color RenderThemeGtk::inactiveListBoxSelectionForegroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return widget->style->text[GTK_STATE_ACTIVE];
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

bool RenderThemeGtk::paintMediaFullscreenButton(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    return paintMediaButton(paintInfo.context, r, m_fullscreenButton.get(), m_panelColor, m_mediaIconSize);
}

bool RenderThemeGtk::paintMediaMuteButton(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    HTMLMediaElement* mediaElement = getMediaElementFromRenderObject(o);
    if (!mediaElement)
        return false;

    return paintMediaButton(paintInfo.context, r, mediaElement->muted() ? m_unmuteButton.get() : m_muteButton.get(), m_panelColor, m_mediaIconSize);
}

bool RenderThemeGtk::paintMediaPlayButton(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    HTMLMediaElement* mediaElement = getMediaElementFromRenderObject(o);
    if (!mediaElement)
        return false;

    return paintMediaButton(paintInfo.context, r, mediaElement->canPlay() ? m_playButton.get() : m_pauseButton.get(), m_panelColor, m_mediaIconSize);
}

bool RenderThemeGtk::paintMediaSeekBackButton(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    return paintMediaButton(paintInfo.context, r, m_seekBackButton.get(), m_panelColor, m_mediaIconSize);
}

bool RenderThemeGtk::paintMediaSeekForwardButton(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    return paintMediaButton(paintInfo.context, r, m_seekForwardButton.get(), m_panelColor, m_mediaIconSize);
}

bool RenderThemeGtk::paintMediaSliderTrack(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    paintInfo.context->fillRect(FloatRect(r), m_panelColor, DeviceColorSpace);
    paintInfo.context->fillRect(FloatRect(IntRect(r.x(), r.y() + (r.height() - m_mediaSliderHeight) / 2,
                                                  r.width(), m_mediaSliderHeight)), m_sliderColor, DeviceColorSpace);
    return false;
}

bool RenderThemeGtk::paintMediaSliderThumb(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    // Make the thumb nicer with rounded corners.
    paintInfo.context->fillRoundedRect(r, IntSize(3, 3), IntSize(3, 3), IntSize(3, 3), IntSize(3, 3), m_sliderThumbColor, DeviceColorSpace);
    return false;
}
#endif

}
