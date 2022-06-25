/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)
#include "AccessibilityMediaObject.h"

#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HTMLVideoElement.h"
#include "LocalizedStrings.h"


namespace WebCore {
    
using namespace HTMLNames;

AccessibilityMediaObject::AccessibilityMediaObject(RenderObject* renderer)
    : AccessibilityRenderObject(renderer)
{
}

AccessibilityMediaObject::~AccessibilityMediaObject() = default;

Ref<AccessibilityMediaObject> AccessibilityMediaObject::create(RenderObject* renderer)
{
    return adoptRef(*new AccessibilityMediaObject(renderer));
}

bool AccessibilityMediaObject::computeAccessibilityIsIgnored() const
{
    return accessibilityIsIgnoredByDefault();
}

HTMLMediaElement* AccessibilityMediaObject::mediaElement() const
{
    Node* node = this->node();
    if (!is<HTMLMediaElement>(*node))
        return nullptr;
    return downcast<HTMLMediaElement>(node);
}

String AccessibilityMediaObject::stringValue() const
{
    if (HTMLMediaElement* element = mediaElement())
        return localizedMediaTimeDescription(element->currentTime());
    return AccessibilityRenderObject::stringValue();
}

String AccessibilityMediaObject::interactiveVideoDuration() const
{
    if (HTMLMediaElement* element = mediaElement())
        return localizedMediaTimeDescription(element->duration());
    return String();
}
    
void AccessibilityMediaObject::mediaSeek(AXSeekDirection direction)
{
    HTMLMediaElement* element = mediaElement();
    if (!element)
        return;
    
    // Step 5% each time.
    const double seekStep = .05;
    double current = element->currentTime();
    double duration = element->duration();
    double timeDelta = ceil(duration * seekStep);

    double time = direction == AXSeekForward ? std::min(current + timeDelta, duration) : std::max(current - timeDelta, 0.0);
    element->setCurrentTime(time);
}

void AccessibilityMediaObject::toggleMute()
{
    HTMLMediaElement* element = mediaElement();
    if (!element)
        return;
    
    element->setMuted(!element->muted());
}

void AccessibilityMediaObject::increment()
{
    mediaSeek(AXSeekForward);
}

void AccessibilityMediaObject::decrement()
{
    mediaSeek(AXSeekBackward);
}

bool AccessibilityMediaObject::press()
{
    HTMLMediaElement* element = mediaElement();
    if (!element)
        return false;
    
    // We can safely call the internal togglePlayState method, which doesn't check restrictions,
    // because this method is only called from user interaction.
    element->togglePlayState();
    return true;
}

bool AccessibilityMediaObject::hasControlsAttributeSet() const
{
    HTMLMediaElement* element = mediaElement();
    if (!element)
        return false;
    
    return element->controls();
}
    
bool AccessibilityMediaObject::isPlaying() const
{
    HTMLMediaElement* element = mediaElement();
    if (!element)
        return false;
    
    return element->isPlaying();
}

bool AccessibilityMediaObject::isMuted() const
{
    HTMLMediaElement* element = mediaElement();
    if (!element)
        return false;
    
    return element->muted();
}

bool AccessibilityMediaObject::isAutoplayEnabled() const
{
    HTMLMediaElement* element = mediaElement();
    if (!element)
        return false;
    
    return element->autoplay();
}

bool AccessibilityMediaObject::isPlayingInline() const
{
    HTMLMediaElement* element = mediaElement();
    if (!element)
        return false;
    
    return !element->mediaSession().requiresFullscreenForVideoPlayback();
}

void AccessibilityMediaObject::enterFullscreen() const
{
    Node* node = this->node();
    if (!is<HTMLVideoElement>(node))
        return;
    
    HTMLVideoElement* element = downcast<HTMLVideoElement>(node);
    element->enterFullscreen();
}
    
} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
