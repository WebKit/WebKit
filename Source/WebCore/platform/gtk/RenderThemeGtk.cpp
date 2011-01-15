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
#include "GOwnPtr.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "GtkVersioning.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "MediaControlElements.h"
#include "PaintInfo.h"
#include "RenderBox.h"
#include "RenderObject.h"
#include "TimeRanges.h"
#include "UserAgentStyleSheets.h"
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

static GtkIconSize getMediaButtonIconSize(int mediaIconSize)
{
    GtkIconSize iconSize = gtk_icon_size_from_name("webkit-media-button-size");
    if (!iconSize)
        iconSize = gtk_icon_size_register("webkit-media-button-size", mediaIconSize, mediaIconSize);
    return iconSize;
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

PassRefPtr<RenderTheme> RenderThemeGtk::create()
{
    return adoptRef(new RenderThemeGtk());
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    static RenderTheme* rt = RenderThemeGtk::create().releaseRef();
    return rt;
}

RenderThemeGtk::RenderThemeGtk()
    : m_panelColor(Color::white)
    , m_sliderColor(Color::white)
    , m_sliderThumbColor(Color::white)
    , m_mediaIconSize(16)
    , m_mediaSliderHeight(14)
    , m_mediaSliderThumbWidth(12)
    , m_mediaSliderThumbHeight(12)
{
    platformInit();
#if ENABLE(VIDEO)
    initMediaColors();
    initMediaButtons();
#endif
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

// This is used in RenderThemeGtk2 and RenderThemeGtk3. Normally, it would be in
// the RenderThemeGtk header (perhaps as a static method), but we want to avoid
// having to include GTK+ headers only for the GtkTextDirection enum.
GtkTextDirection gtkTextDirection(TextDirection direction)
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

static GtkStateType gtkIconState(RenderTheme* theme, RenderObject* renderObject)
{
    if (!theme->isEnabled(renderObject))
        return GTK_STATE_INSENSITIVE;
    if (theme->isPressed(renderObject))
        return GTK_STATE_ACTIVE;
    if (theme->isHovered(renderObject))
        return GTK_STATE_PRELIGHT;

    return GTK_STATE_NORMAL;
}

void RenderThemeGtk::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const
{
    // Some layout tests check explicitly that buttons ignore line-height.
    if (style->appearance() == PushButtonPart)
        style->setLineHeight(RenderStyle::initialLineHeight());
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

bool RenderThemeGtk::paintMenuListButton(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintMenuList(object, info, rect);
}

bool RenderThemeGtk::paintTextArea(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

static void paintGdkPixbuf(GraphicsContext* context, const GdkPixbuf* icon, const IntPoint& iconPoint)
{
    cairo_t* cr = context->platformContext();
    cairo_save(cr);
    gdk_cairo_set_source_pixbuf(cr, icon, iconPoint.x(), iconPoint.y());
    cairo_paint(cr);
    cairo_restore(cr);
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
    GRefPtr<GdkPixbuf> icon = getStockIcon(GTK_TYPE_ENTRY, GTK_STOCK_FIND,
                                           gtkTextDirection(renderObject->style()->direction()),
                                           gtkIconState(this, renderObject), GTK_ICON_SIZE_MENU);
    paintGdkPixbuf(paintInfo.context, icon.get(), centerRectVerticallyInParentInputElement(renderObject, rect));
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
    GRefPtr<GdkPixbuf> icon = getStockIcon(GTK_TYPE_ENTRY, GTK_STOCK_CLEAR,
                                           gtkTextDirection(renderObject->style()->direction()),
                                           gtkIconState(this, renderObject), GTK_ICON_SIZE_MENU);
    paintGdkPixbuf(paintInfo.context, icon.get(), centerRectVerticallyInParentInputElement(renderObject, rect));
    return false;
}

void RenderThemeGtk::adjustSearchFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    // We cannot give a proper rendering when border radius is active, unfortunately.
    style->resetBorderRadius();
    style->setLineHeight(RenderStyle::initialLineHeight());
}

bool RenderThemeGtk::paintSearchField(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintTextField(o, i, rect);
}

void RenderThemeGtk::adjustSliderTrackStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(0);
}

void RenderThemeGtk::adjustSliderThumbStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(0);
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

void RenderThemeGtk::adjustMediaSliderThumbSize(RenderObject* renderObject) const
{
    ASSERT(renderObject->style()->appearance() == MediaSliderThumbPart);
    renderObject->style()->setWidth(Length(m_mediaSliderThumbWidth, Fixed));
    renderObject->style()->setHeight(Length(m_mediaSliderThumbHeight, Fixed));
}

bool RenderThemeGtk::paintMediaButton(RenderObject* renderObject, GraphicsContext* context, const IntRect& rect, const char* iconName)
{
    GRefPtr<GdkPixbuf> icon = getStockIcon(GTK_TYPE_CONTAINER, iconName,
                                           gtkTextDirection(renderObject->style()->direction()),
                                           gtkIconState(this, renderObject),
                                           getMediaButtonIconSize(m_mediaIconSize));
    IntPoint iconPoint(rect.x() + (rect.width() - m_mediaIconSize) / 2,
                       rect.y() + (rect.height() - m_mediaIconSize) / 2);
    context->fillRect(FloatRect(rect), m_panelColor, ColorSpaceDeviceRGB);
    paintGdkPixbuf(context, icon.get(), iconPoint);
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

bool RenderThemeGtk::paintMediaVolumeSliderContainer(RenderObject*, const PaintInfo& paintInfo, const IntRect& rect)
{
    GraphicsContext* context = paintInfo.context;
    context->fillRect(FloatRect(rect), m_panelColor, ColorSpaceDeviceRGB);
    return false;
}

bool RenderThemeGtk::paintMediaVolumeSliderTrack(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintSliderTrack(renderObject, paintInfo, rect);
}

bool RenderThemeGtk::paintMediaVolumeSliderThumb(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintSliderThumb(renderObject, paintInfo, rect);
}

String RenderThemeGtk::formatMediaControlsCurrentTime(float currentTime, float duration) const
{
    return formatMediaControlsTime(currentTime) + " / " + formatMediaControlsTime(duration);
}

bool RenderThemeGtk::paintMediaCurrentTime(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    GraphicsContext* context = paintInfo.context;

    context->fillRect(FloatRect(rect), m_panelColor, ColorSpaceDeviceRGB);
    return false;
}
#endif

#if ENABLE(PROGRESS_TAG)
void RenderThemeGtk::adjustProgressBarStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(0);
}
#endif

}
