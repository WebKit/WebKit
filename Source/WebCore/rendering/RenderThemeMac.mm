/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
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
 */

#import "config.h"
#import "RenderThemeMac.h"

#import "Element.h"
#import "ExceptionCodePlaceholder.h"
#import "GraphicsContextCG.h"
#import "HTMLMediaElement.h"
#import "LocalCurrentGraphicsContext.h"
#import "MediaControlElements.h"
#import "PaintInfo.h"
#import "RenderMedia.h"
#import "RenderMediaControlElements.h"
#import "RenderMediaControls.h"
#import "RenderView.h"
#import "TimeRanges.h"
#import "ThemeMac.h"
#import "WebCoreSystemInterface.h"
#import "UserAgentStyleSheets.h"
#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <wtf/RetainPtr.h>

namespace WebCore {

using namespace HTMLNames;

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page*)
{
    static RenderTheme* rt = RenderThemeMac::create().leakRef();
    return rt;
}

PassRefPtr<RenderTheme> RenderThemeMac::create()
{
    return adoptRef(new RenderThemeMac);
}

RenderThemeMac::RenderThemeMac()
{
}

RenderThemeMac::~RenderThemeMac()
{
}

NSView* RenderThemeMac::documentViewFor(RenderObject* o) const
{
    return ThemeMac::ensuredView(o->view()->frameView());
}

#if ENABLE(VIDEO)

typedef enum {
    MediaControllerThemeClassic   = 1,
    MediaControllerThemeQuickTime = 2
} MediaControllerThemeStyle;

static int mediaControllerTheme()
{
    static int controllerTheme = -1;

    if (controllerTheme != -1)
        return controllerTheme;

    controllerTheme = MediaControllerThemeClassic;

    Boolean validKey;
    Boolean useQTMediaUIPref = CFPreferencesGetAppBooleanValue(CFSTR("UseQuickTimeMediaUI"), CFSTR("com.apple.WebCore"), &validKey);

    if (validKey && !useQTMediaUIPref)
        return controllerTheme;

    controllerTheme = MediaControllerThemeQuickTime;
    return controllerTheme;
}

const int mediaSliderThumbWidth = 13;
const int mediaSliderThumbHeight = 14;

void RenderThemeMac::adjustMediaSliderThumbSize(RenderStyle* style) const
{
    int wkPart;
    switch (style->appearance()) {
    case MediaSliderThumbPart:
        wkPart = MediaSliderThumb;
        break;
    case MediaVolumeSliderThumbPart:
        wkPart = MediaVolumeSliderThumb;
        break;
    case MediaFullScreenVolumeSliderThumbPart:
        wkPart = MediaFullScreenVolumeSliderThumb;
        break;
    default:
        return;
    }

    int width = mediaSliderThumbWidth;
    int height = mediaSliderThumbHeight;

    if (mediaControllerTheme() == MediaControllerThemeQuickTime) {
        CGSize size;
        wkMeasureMediaUIPart(wkPart, MediaControllerThemeQuickTime, NULL, &size);
        width = size.width;
        height = size.height;
    }

    float zoomLevel = style->effectiveZoom();
    style->setWidth(Length(static_cast<int>(width * zoomLevel), Fixed));
    style->setHeight(Length(static_cast<int>(height * zoomLevel), Fixed));
}

enum WKMediaControllerThemeState {
    MediaUIPartDisabledFlag = 1 << 0,
    MediaUIPartPressedFlag = 1 << 1,
    MediaUIPartDrawEndCapsFlag = 1 << 3,
};

static unsigned getMediaUIPartStateFlags(Node* node)
{
    unsigned flags = 0;

    if (node->isElementNode() && toElement(node)->disabled())
        flags |= MediaUIPartDisabledFlag;
    else if (node->active())
        flags |= MediaUIPartPressedFlag;
    return flags;
}

// Utility to scale when the UI part are not scaled by wkDrawMediaUIPart
static FloatRect getUnzoomedRectAndAdjustCurrentContext(RenderObject* o, const PaintInfo& paintInfo, const IntRect &originalRect)
{
    float zoomLevel = o->style()->effectiveZoom();
    FloatRect unzoomedRect(originalRect);
    if (zoomLevel != 1.0f && mediaControllerTheme() == MediaControllerThemeQuickTime) {
        unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
        unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
        paintInfo.context->translate(unzoomedRect.x(), unzoomedRect.y());
        paintInfo.context->scale(FloatSize(zoomLevel, zoomLevel));
        paintInfo.context->translate(-unzoomedRect.x(), -unzoomedRect.y());
    }
    return unzoomedRect;
}

bool RenderThemeMac::paintMediaFullscreenButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    if (!node)
        return false;

    if (node->isMediaControlElement()) {
        LocalCurrentGraphicsContext localContext(paintInfo.context);
        wkDrawMediaUIPart(mediaControlElementType(node), mediaControllerTheme(), localContext.cgContext(), r, getMediaUIPartStateFlags(node));
    }
    return false;
}

bool RenderThemeMac::paintMediaMuteButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    Node* mediaNode = node ? node->shadowHost() : 0;
    if (!mediaNode || (!mediaNode->hasTagName(videoTag) && !mediaNode->hasTagName(audioTag)))
        return false;

    if (node->isMediaControlElement()) {
        LocalCurrentGraphicsContext localContext(paintInfo.context);
        wkDrawMediaUIPart(mediaControlElementType(node), mediaControllerTheme(), localContext.cgContext(), r, getMediaUIPartStateFlags(node));
    }
    return false;
}

bool RenderThemeMac::paintMediaPlayButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    Node* mediaNode = node ? node->shadowHost() : 0;
    if (!mediaNode || (!mediaNode->hasTagName(videoTag) && !mediaNode->hasTagName(audioTag)))
        return false;

    if (node->isMediaControlElement()) {
        LocalCurrentGraphicsContext localContext(paintInfo.context);
        wkDrawMediaUIPart(mediaControlElementType(node), mediaControllerTheme(), localContext.cgContext(), r, getMediaUIPartStateFlags(node));
    }
    return false;
}

bool RenderThemeMac::paintMediaSeekBackButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    if (!node)
        return false;

    LocalCurrentGraphicsContext localContext(paintInfo.context);
    wkDrawMediaUIPart(MediaSeekBackButton, mediaControllerTheme(), localContext.cgContext(), r, getMediaUIPartStateFlags(node));
    return false;
}

bool RenderThemeMac::paintMediaSeekForwardButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    if (!node)
        return false;

    LocalCurrentGraphicsContext localContext(paintInfo.context);
    wkDrawMediaUIPart(MediaSeekForwardButton, mediaControllerTheme(), localContext.cgContext(), r, getMediaUIPartStateFlags(node));
    return false;
}

bool RenderThemeMac::paintMediaSliderTrack(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    Element* mediaNode = node ? node->shadowHost() : 0;
    if (!mediaNode || !mediaNode->isMediaElement())
        return false;

    HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(mediaNode);
    if (!mediaElement)
        return false;

    RefPtr<TimeRanges> timeRanges = mediaElement->buffered();
    float timeLoaded = timeRanges->length() ? timeRanges->end(0, IGNORE_EXCEPTION) : 0;
    float currentTime = mediaElement->currentTime();
    float duration = mediaElement->duration();
    if (std::isnan(duration))
        duration = 0;

    ContextContainer cgContextContainer(paintInfo.context);
    CGContextRef context = cgContextContainer.context();
    GraphicsContextStateSaver stateSaver(*paintInfo.context);
    FloatRect unzoomedRect = getUnzoomedRectAndAdjustCurrentContext(o, paintInfo, r);
    wkDrawMediaSliderTrack(mediaControllerTheme(), context, unzoomedRect,
        timeLoaded, currentTime, duration, getMediaUIPartStateFlags(node));
    return false;
}

bool RenderThemeMac::paintMediaSliderThumb(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    if (!node)
        return false;

    LocalCurrentGraphicsContext localContext(paintInfo.context);
    wkDrawMediaUIPart(MediaSliderThumb, mediaControllerTheme(), localContext.cgContext(), r, getMediaUIPartStateFlags(node));
    return false;
}

bool RenderThemeMac::paintMediaRewindButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    if (!node)
        return false;

    LocalCurrentGraphicsContext localContext(paintInfo.context);
    wkDrawMediaUIPart(MediaRewindButton, mediaControllerTheme(), localContext.cgContext(), r, getMediaUIPartStateFlags(node));
    return false;
}

bool RenderThemeMac::paintMediaReturnToRealtimeButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    if (!node)
        return false;

    LocalCurrentGraphicsContext localContext(paintInfo.context);
    wkDrawMediaUIPart(MediaReturnToRealtimeButton, mediaControllerTheme(), localContext.cgContext(), r, getMediaUIPartStateFlags(node));
    return false;
}

bool RenderThemeMac::paintMediaControlsBackground(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    if (!node)
        return false;

    LocalCurrentGraphicsContext localContext(paintInfo.context);
    wkDrawMediaUIPart(MediaTimelineContainer, mediaControllerTheme(), localContext.cgContext(), r, getMediaUIPartStateFlags(node));
    return false;
}

bool RenderThemeMac::paintMediaCurrentTime(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    if (!node)
        return false;

    ContextContainer cgContextContainer(paintInfo.context);
    GraphicsContextStateSaver stateSaver(*paintInfo.context);
    FloatRect unzoomedRect = getUnzoomedRectAndAdjustCurrentContext(o, paintInfo, r);
    wkDrawMediaUIPart(MediaCurrentTimeDisplay, mediaControllerTheme(), cgContextContainer.context(), unzoomedRect, getMediaUIPartStateFlags(node));
    return false;
}

bool RenderThemeMac::paintMediaTimeRemaining(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    if (!node)
        return false;

    ContextContainer cgContextContainer(paintInfo.context);
    GraphicsContextStateSaver stateSaver(*paintInfo.context);
    FloatRect unzoomedRect = getUnzoomedRectAndAdjustCurrentContext(o, paintInfo, r);
    wkDrawMediaUIPart(MediaTimeRemainingDisplay, mediaControllerTheme(), cgContextContainer.context(), unzoomedRect, getMediaUIPartStateFlags(node));
    return false;
}

bool RenderThemeMac::paintMediaVolumeSliderContainer(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    if (!node)
        return false;

    LocalCurrentGraphicsContext localContext(paintInfo.context);
    wkDrawMediaUIPart(MediaVolumeSliderContainer, mediaControllerTheme(), localContext.cgContext(), r, getMediaUIPartStateFlags(node));
    return false;
}

bool RenderThemeMac::paintMediaVolumeSliderTrack(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    if (!node)
        return false;

    LocalCurrentGraphicsContext localContext(paintInfo.context);
    wkDrawMediaUIPart(MediaVolumeSlider, mediaControllerTheme(), localContext.cgContext(), r, getMediaUIPartStateFlags(node));
    return false;
}

bool RenderThemeMac::paintMediaVolumeSliderThumb(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    if (!node)
        return false;

    LocalCurrentGraphicsContext localContext(paintInfo.context);
    wkDrawMediaUIPart(MediaVolumeSliderThumb, mediaControllerTheme(), localContext.cgContext(), r, getMediaUIPartStateFlags(node));
    return false;
}

bool RenderThemeMac::paintMediaFullScreenVolumeSliderTrack(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    if (!node)
        return false;

    LocalCurrentGraphicsContext localContext(paintInfo.context);
    wkDrawMediaUIPart(MediaFullScreenVolumeSlider, mediaControllerTheme(), localContext.cgContext(), r, getMediaUIPartStateFlags(node));
    return false;
}

bool RenderThemeMac::paintMediaFullScreenVolumeSliderThumb(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    if (!node)
        return false;

    LocalCurrentGraphicsContext localContext(paintInfo.context);
    wkDrawMediaUIPart(MediaFullScreenVolumeSliderThumb, mediaControllerTheme(), localContext.cgContext(), r, getMediaUIPartStateFlags(node));
    return false;
}

String RenderThemeMac::extraMediaControlsStyleSheet()
{
    if (mediaControllerTheme() == MediaControllerThemeQuickTime)
        return String(mediaControlsQuickTimeUserAgentStyleSheet, sizeof(mediaControlsQuickTimeUserAgentStyleSheet));

    return String();
}

#if ENABLE(FULLSCREEN_API)
String RenderThemeMac::extraFullScreenStyleSheet()
{
    if (mediaControllerTheme() == MediaControllerThemeQuickTime)
        return String(fullscreenQuickTimeUserAgentStyleSheet, sizeof(fullscreenQuickTimeUserAgentStyleSheet));

    return String();
}
#endif

bool RenderThemeMac::hasOwnDisabledStateHandlingFor(ControlPart part) const
{
    if (part == MediaMuteButtonPart)
        return false;

    return mediaControllerTheme() == MediaControllerThemeClassic;
}

bool RenderThemeMac::usesMediaControlStatusDisplay()
{
    return mediaControllerTheme() == MediaControllerThemeQuickTime;
}

bool RenderThemeMac::usesMediaControlVolumeSlider() const
{
    return mediaControllerTheme() == MediaControllerThemeQuickTime;
}

IntPoint RenderThemeMac::volumeSliderOffsetFromMuteButton(RenderBox* muteButtonBox, const IntSize& size) const
{
    return RenderMediaControls::volumeSliderOffsetFromMuteButton(muteButtonBox, size);
}

#endif // ENABLE(VIDEO)

} // namespace WebCore
