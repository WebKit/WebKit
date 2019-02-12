/*
 * Copyright (C) 2009-2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"

#if ENABLE(VIDEO)

#include "AccessibilityMediaControls.h"

#include "AXObjectCache.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "LocalizedStrings.h"
#include "MediaControlElements.h"
#include "RenderObject.h"
#include "RenderSlider.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

using namespace HTMLNames;


AccessibilityMediaControl::AccessibilityMediaControl(RenderObject* renderer)
    : AccessibilityRenderObject(renderer)
{
}

Ref<AccessibilityObject> AccessibilityMediaControl::create(RenderObject* renderer)
{
    ASSERT(renderer->node());

    switch (mediaControlElementType(renderer->node())) {
    case MediaSlider:
        return AccessibilityMediaTimeline::create(renderer);

    case MediaCurrentTimeDisplay:
    case MediaTimeRemainingDisplay:
        return AccessibilityMediaTimeDisplay::create(renderer);

    case MediaControlsPanel:
        return AccessibilityMediaControlsContainer::create(renderer);

    default:
        return adoptRef(*new AccessibilityMediaControl(renderer));
    }
}

MediaControlElementType AccessibilityMediaControl::controlType() const
{
    if (!renderer() || !renderer()->node())
        return MediaTimelineContainer; // Timeline container is not accessible.

    return mediaControlElementType(renderer()->node());
}

const String& AccessibilityMediaControl::controlTypeName() const
{
    static NeverDestroyed<const String> mediaEnterFullscreenButtonName(MAKE_STATIC_STRING_IMPL("EnterFullscreenButton"));
    static NeverDestroyed<const String> mediaExitFullscreenButtonName(MAKE_STATIC_STRING_IMPL("ExitFullscreenButton"));
    static NeverDestroyed<const String> mediaMuteButtonName(MAKE_STATIC_STRING_IMPL("MuteButton"));
    static NeverDestroyed<const String> mediaPlayButtonName(MAKE_STATIC_STRING_IMPL("PlayButton"));
    static NeverDestroyed<const String> mediaSeekBackButtonName(MAKE_STATIC_STRING_IMPL("SeekBackButton"));
    static NeverDestroyed<const String> mediaSeekForwardButtonName(MAKE_STATIC_STRING_IMPL("SeekForwardButton"));
    static NeverDestroyed<const String> mediaRewindButtonName(MAKE_STATIC_STRING_IMPL("RewindButton"));
    static NeverDestroyed<const String> mediaReturnToRealtimeButtonName(MAKE_STATIC_STRING_IMPL("ReturnToRealtimeButton"));
    static NeverDestroyed<const String> mediaUnMuteButtonName(MAKE_STATIC_STRING_IMPL("UnMuteButton"));
    static NeverDestroyed<const String> mediaPauseButtonName(MAKE_STATIC_STRING_IMPL("PauseButton"));
    static NeverDestroyed<const String> mediaStatusDisplayName(MAKE_STATIC_STRING_IMPL("StatusDisplay"));
    static NeverDestroyed<const String> mediaCurrentTimeDisplay(MAKE_STATIC_STRING_IMPL("CurrentTimeDisplay"));
    static NeverDestroyed<const String> mediaTimeRemainingDisplay(MAKE_STATIC_STRING_IMPL("TimeRemainingDisplay"));
    static NeverDestroyed<const String> mediaShowClosedCaptionsButtonName(MAKE_STATIC_STRING_IMPL("ShowClosedCaptionsButton"));
    static NeverDestroyed<const String> mediaHideClosedCaptionsButtonName(MAKE_STATIC_STRING_IMPL("HideClosedCaptionsButton"));

    switch (controlType()) {
    case MediaEnterFullscreenButton:
        return mediaEnterFullscreenButtonName;
    case MediaExitFullscreenButton:
        return mediaExitFullscreenButtonName;
    case MediaMuteButton:
        return mediaMuteButtonName;
    case MediaPlayButton:
        return mediaPlayButtonName;
    case MediaSeekBackButton:
        return mediaSeekBackButtonName;
    case MediaSeekForwardButton:
        return mediaSeekForwardButtonName;
    case MediaRewindButton:
        return mediaRewindButtonName;
    case MediaReturnToRealtimeButton:
        return mediaReturnToRealtimeButtonName;
    case MediaUnMuteButton:
        return mediaUnMuteButtonName;
    case MediaPauseButton:
        return mediaPauseButtonName;
    case MediaStatusDisplay:
        return mediaStatusDisplayName;
    case MediaCurrentTimeDisplay:
        return mediaCurrentTimeDisplay;
    case MediaTimeRemainingDisplay:
        return mediaTimeRemainingDisplay;
    case MediaShowClosedCaptionsButton:
        return mediaShowClosedCaptionsButtonName;
    case MediaHideClosedCaptionsButton:
        return mediaHideClosedCaptionsButtonName;

    default:
        break;
    }

    return nullAtom();
}

void AccessibilityMediaControl::accessibilityText(Vector<AccessibilityText>& textOrder) const
{
    String description = accessibilityDescription();
    if (!description.isEmpty())
        textOrder.append(AccessibilityText(description, AccessibilityTextSource::Alternative));

    String title = this->title();
    if (!title.isEmpty())
        textOrder.append(AccessibilityText(title, AccessibilityTextSource::Alternative));

    String helptext = helpText();
    if (!helptext.isEmpty())
        textOrder.append(AccessibilityText(helptext, AccessibilityTextSource::Help));
}
    

String AccessibilityMediaControl::title() const
{
    static NeverDestroyed<const String> controlsPanel(MAKE_STATIC_STRING_IMPL("ControlsPanel"));

    if (controlType() == MediaControlsPanel)
        return localizedMediaControlElementString(controlsPanel);

    return AccessibilityRenderObject::title();
}

String AccessibilityMediaControl::accessibilityDescription() const
{
    return localizedMediaControlElementString(controlTypeName());
}

String AccessibilityMediaControl::helpText() const
{
    return localizedMediaControlElementHelpText(controlTypeName());
}

bool AccessibilityMediaControl::computeAccessibilityIsIgnored() const
{
    if (!m_renderer || m_renderer->style().visibility() != Visibility::Visible || controlType() == MediaTimelineContainer)
        return true;

    return accessibilityIsIgnoredByDefault();
}

AccessibilityRole AccessibilityMediaControl::roleValue() const
{
    switch (controlType()) {
    case MediaEnterFullscreenButton:
    case MediaExitFullscreenButton:
    case MediaMuteButton:
    case MediaPlayButton:
    case MediaSeekBackButton:
    case MediaSeekForwardButton:
    case MediaRewindButton:
    case MediaReturnToRealtimeButton:
    case MediaUnMuteButton:
    case MediaPauseButton:
    case MediaShowClosedCaptionsButton:
    case MediaHideClosedCaptionsButton:
        return AccessibilityRole::Button;

    case MediaStatusDisplay:
        return AccessibilityRole::StaticText;

    case MediaTimelineContainer:
        return AccessibilityRole::Group;

    default:
        break;
    }

    return AccessibilityRole::Unknown;
}


//
// AccessibilityMediaControlsContainer

AccessibilityMediaControlsContainer::AccessibilityMediaControlsContainer(RenderObject* renderer)
    : AccessibilityMediaControl(renderer)
{
}

Ref<AccessibilityObject> AccessibilityMediaControlsContainer::create(RenderObject* renderer)
{
    return adoptRef(*new AccessibilityMediaControlsContainer(renderer));
}

String AccessibilityMediaControlsContainer::accessibilityDescription() const
{
    return localizedMediaControlElementString(elementTypeName());
}

String AccessibilityMediaControlsContainer::helpText() const
{
    return localizedMediaControlElementHelpText(elementTypeName());
}

bool AccessibilityMediaControlsContainer::controllingVideoElement() const
{
    auto element = parentMediaElement(*m_renderer);
    return !element || element->isVideo();
}

const String& AccessibilityMediaControlsContainer::elementTypeName() const
{
    static NeverDestroyed<const String> videoElement(MAKE_STATIC_STRING_IMPL("VideoElement"));
    static NeverDestroyed<const String> audioElement(MAKE_STATIC_STRING_IMPL("AudioElement"));

    if (controllingVideoElement())
        return videoElement;
    return audioElement;
}

bool AccessibilityMediaControlsContainer::computeAccessibilityIsIgnored() const
{
    return accessibilityIsIgnoredByDefault();
}

//
// AccessibilityMediaTimeline

AccessibilityMediaTimeline::AccessibilityMediaTimeline(RenderObject* renderer)
    : AccessibilitySlider(renderer)
{
}

Ref<AccessibilityObject> AccessibilityMediaTimeline::create(RenderObject* renderer)
{
    return adoptRef(*new AccessibilityMediaTimeline(renderer));
}

String AccessibilityMediaTimeline::valueDescription() const
{
    Node* node = m_renderer->node();
    if (!is<HTMLInputElement>(*node))
        return String();

    float time = downcast<HTMLInputElement>(*node).value().toFloat();
    return localizedMediaTimeDescription(time);
}

String AccessibilityMediaTimeline::helpText() const
{
    static NeverDestroyed<const String> slider(MAKE_STATIC_STRING_IMPL("Slider"));
    return localizedMediaControlElementHelpText(slider);
}


//
// AccessibilityMediaTimeDisplay

AccessibilityMediaTimeDisplay::AccessibilityMediaTimeDisplay(RenderObject* renderer)
    : AccessibilityMediaControl(renderer)
{
}

Ref<AccessibilityObject> AccessibilityMediaTimeDisplay::create(RenderObject* renderer)
{
    return adoptRef(*new AccessibilityMediaTimeDisplay(renderer));
}

bool AccessibilityMediaTimeDisplay::computeAccessibilityIsIgnored() const
{
    if (!m_renderer || m_renderer->style().visibility() != Visibility::Visible)
        return true;

    if (!m_renderer->style().width().value())
        return true;
    
    return accessibilityIsIgnoredByDefault();
}

String AccessibilityMediaTimeDisplay::accessibilityDescription() const
{
    static NeverDestroyed<const String> currentTimeDisplay(MAKE_STATIC_STRING_IMPL("CurrentTimeDisplay"));
    static NeverDestroyed<const String> timeRemainingDisplay(MAKE_STATIC_STRING_IMPL("TimeRemainingDisplay"));

    if (controlType() == MediaCurrentTimeDisplay)
        return localizedMediaControlElementString(currentTimeDisplay);

    return localizedMediaControlElementString(timeRemainingDisplay);
}

String AccessibilityMediaTimeDisplay::stringValue() const
{
    if (!m_renderer || !m_renderer->node())
        return String();

    float time = static_cast<MediaControlTimeDisplayElement*>(m_renderer->node())->currentValue();
    return localizedMediaTimeDescription(std::abs(time));
}

} // namespace WebCore

#endif // ENABLE(VIDEO)
