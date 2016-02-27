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
#include "PaintInfo.h"
#include "PlatformContextCairo.h"
#include "RenderBox.h"
#include "RenderObject.h"
#include "StringTruncator.h"
#include "TimeRanges.h"
#include "UserAgentStyleSheets.h"
#include <cmath>
#include <gdk/gdk.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <wtf/gobject/GUniquePtr.h>
#include <wtf/text/CString.h>

#if ENABLE(PROGRESS_ELEMENT)
#include "RenderProgress.h"
#endif

namespace WebCore {

#if ENABLE(VIDEO)
static HTMLMediaElement* getMediaElementFromRenderObject(RenderObject* o)
{
    Node* node = o->node();
    Node* mediaNode = node ? node->shadowHost() : 0;
    if (!mediaNode)
        mediaNode = node;
    if (!mediaNode || !mediaNode->isElementNode() || !toElement(mediaNode)->isMediaElement())
        return 0;

    return toHTMLMediaElement(mediaNode);
}
#endif

PassRefPtr<RenderTheme> RenderThemeGtk::create()
{
    return adoptRef(new RenderThemeGtk());
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    static RenderTheme* rt = RenderThemeGtk::create().leakRef();
    return rt;
}

RenderThemeGtk::RenderThemeGtk()
{
    platformInit();
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
    case MediaPlayButtonPart:
    case MediaVolumeSliderPart:
    case MediaMuteButtonPart:
    case MediaEnterFullscreenButtonPart:
    case MediaSliderPart:
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
    if (o->style().appearance() == CheckboxPart
        || o->style().appearance() == RadioPart) {
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

void RenderThemeGtk::adjustButtonStyle(StyleResolver*, RenderStyle* style, WebCore::Element*) const
{
    // Some layout tests check explicitly that buttons ignore line-height.
    if (style->appearance() == PushButtonPart)
        style->setLineHeight(RenderStyle::initialLineHeight());
}

void RenderThemeGtk::adjustMenuListStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    // The tests check explicitly that select menu buttons ignore line height.
    style->setLineHeight(RenderStyle::initialLineHeight());

    // We cannot give a proper rendering when border radius is active, unfortunately.
    style->resetBorderRadius();
}

void RenderThemeGtk::adjustMenuListButtonStyle(StyleResolver* styleResolver, RenderStyle* style, Element* e) const
{
    adjustMenuListStyle(styleResolver, style, e);
}

bool RenderThemeGtk::paintMenuListButtonDecorations(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintMenuList(object, info, rect);
}

bool RenderThemeGtk::paintTextArea(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeGtk::adjustSearchFieldResultsButtonStyle(StyleResolver* styleResolver, RenderStyle* style, Element* e) const
{
    adjustSearchFieldCancelButtonStyle(styleResolver, style, e);
}

bool RenderThemeGtk::paintSearchFieldResultsButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintSearchFieldResultsDecorationPart(o, i, rect);
}

void RenderThemeGtk::adjustSearchFieldStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    // We cannot give a proper rendering when border radius is active, unfortunately.
    style->resetBorderRadius();
    style->setLineHeight(RenderStyle::initialLineHeight());
}

bool RenderThemeGtk::paintSearchField(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintTextField(o, i, rect);
}

void RenderThemeGtk::adjustSliderTrackStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(nullptr);
}

void RenderThemeGtk::adjustSliderThumbStyle(StyleResolver* styleResolver, RenderStyle* style, Element* element) const
{
    RenderTheme::adjustSliderThumbStyle(styleResolver, style, element);
    style->setBoxShadow(nullptr);
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

double RenderThemeGtk::getScreenDPI()
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
    g_object_get(settings, "gtk-font-name", &fontName.outPtr(), NULL);

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

bool RenderThemeGtk::hasOwnDisabledStateHandlingFor(ControlPart part) const
{
    return (part != MediaMuteButtonPart);
}

bool RenderThemeGtk::paintMediaFullscreenButton(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintMediaButton(renderObject, paintInfo.context, rect, "view-fullscreen-symbolic", GTK_STOCK_FULLSCREEN);
}

bool RenderThemeGtk::paintMediaMuteButton(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = getMediaElementFromRenderObject(renderObject);
    if (!mediaElement)
        return true;

    bool muted = mediaElement->muted();
    return paintMediaButton(renderObject, paintInfo.context, rect,
        muted ? "audio-volume-muted-symbolic" : "audio-volume-high-symbolic",
        muted ? "audio-volume-muted" : "audio-volume-high");
}

bool RenderThemeGtk::paintMediaPlayButton(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    Node* node = renderObject->node();
    if (!node)
        return true;
    if (!node->isMediaControlElement())
        return true;

    bool play = mediaControlElementType(node) == MediaPlayButton;
    return paintMediaButton(renderObject, paintInfo.context, rect,
        play ? "media-playback-start-symbolic" : "media-playback-pause-symbolic",
        play ? GTK_STOCK_MEDIA_PLAY : GTK_STOCK_MEDIA_PAUSE);
}

bool RenderThemeGtk::paintMediaSeekBackButton(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintMediaButton(renderObject, paintInfo.context, rect, "media-seek-backward-symbolic", GTK_STOCK_MEDIA_REWIND);
}

bool RenderThemeGtk::paintMediaSeekForwardButton(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintMediaButton(renderObject, paintInfo.context, rect, "media-seek-forward-symbolic", GTK_STOCK_MEDIA_FORWARD);
}

static RoundedRect::Radii borderRadiiFromStyle(RenderStyle* style)
{
    return RoundedRect::Radii(
        IntSize(style->borderTopLeftRadius().width().intValue(), style->borderTopLeftRadius().height().intValue()),
        IntSize(style->borderTopRightRadius().width().intValue(), style->borderTopRightRadius().height().intValue()),
        IntSize(style->borderBottomLeftRadius().width().intValue(), style->borderBottomLeftRadius().height().intValue()),
        IntSize(style->borderBottomRightRadius().width().intValue(), style->borderBottomRightRadius().height().intValue()));
}

bool RenderThemeGtk::paintMediaSliderTrack(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    HTMLMediaElement* mediaElement = parentMediaElement(*o);
    if (!mediaElement)
        return true;

    GraphicsContext* context = paintInfo.context;
    context->save();
    context->setStrokeStyle(NoStroke);

    float mediaDuration = mediaElement->duration();
    float totalTrackWidth = r.width();
    RenderStyle* style = &o->style();
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
        context->fillRoundedRect(RoundedRect(rangeRect, borderRadiiFromStyle(style)), style->visitedDependentColor(CSSPropertyColor), style->colorSpace());
    }

    context->restore();
    return false;
}

bool RenderThemeGtk::paintMediaSliderThumb(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    RenderStyle* style = &o->style();
    paintInfo.context->fillRoundedRect(RoundedRect(r, borderRadiiFromStyle(style)), style->visitedDependentColor(CSSPropertyColor), style->colorSpace());
    return false;
}

bool RenderThemeGtk::paintMediaVolumeSliderTrack(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = parentMediaElement(*renderObject);
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
    RenderStyle* style = &renderObject->style();
    IntRect volumeRect(rect);
    volumeRect.move(0, rectHeight - trackHeight);
    volumeRect.setHeight(ceil(trackHeight));

    context->fillRoundedRect(RoundedRect(volumeRect, borderRadiiFromStyle(style)),
        style->visitedDependentColor(CSSPropertyColor), style->colorSpace());
    context->restore();

    return false;
}

bool RenderThemeGtk::paintMediaVolumeSliderThumb(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintMediaSliderThumb(renderObject, paintInfo, rect);
}

String RenderThemeGtk::formatMediaControlsCurrentTime(float currentTime, float duration) const
{
    return formatMediaControlsTime(currentTime) + " / " + formatMediaControlsTime(duration);
}

bool RenderThemeGtk::paintMediaCurrentTime(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    return false;
}
#endif

#if ENABLE(PROGRESS_ELEMENT)
void RenderThemeGtk::adjustProgressBarStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(nullptr);
}

// These values have been copied from RenderThemeChromiumSkia.cpp
static const int progressActivityBlocks = 5;
static const int progressAnimationFrames = 10;
static const double progressAnimationInterval = 0.125;
double RenderThemeGtk::animationRepeatIntervalForProgressBar(RenderProgress*) const
{
    return progressAnimationInterval;
}

double RenderThemeGtk::animationDurationForProgressBar(RenderProgress*) const
{
    return progressAnimationInterval * progressAnimationFrames * 2; // "2" for back and forth;
}

IntRect RenderThemeGtk::calculateProgressRect(RenderObject* renderObject, const IntRect& fullBarRect)
{
    IntRect progressRect(fullBarRect);
    RenderProgress* renderProgress = toRenderProgress(renderObject);
    if (renderProgress->isDeterminate()) {
        int progressWidth = progressRect.width() * renderProgress->position();
        if (renderObject->style().direction() == RTL)
            progressRect.setX(progressRect.x() + progressRect.width() - progressWidth);
        progressRect.setWidth(progressWidth);
        return progressRect;
    }

    double animationProgress = renderProgress->animationProgress();

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
#endif

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

}
