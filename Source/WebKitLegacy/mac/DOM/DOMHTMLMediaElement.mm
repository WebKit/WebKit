/*
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#if ENABLE(VIDEO)

#import "DOMHTMLMediaElement.h"

#import "DOMMediaErrorInternal.h"
#import "DOMNodeInternal.h"
#import "DOMTimeRangesInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/HTMLMediaElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/JSExecState.h>
#import <WebCore/MediaError.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/TimeRanges.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL static_cast<WebCore::HTMLMediaElement*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMHTMLMediaElement

- (DOMMediaError *)error
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->error()));
}

- (NSString *)src
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getURLAttribute(WebCore::HTMLNames::srcAttr);
}

- (void)setSrc:(NSString *)newSrc
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::srcAttr, newSrc);
}

- (NSString *)currentSrc
{
    WebCore::JSMainThreadNullState state;
    return IMPL->currentSrc();
}

- (NSString *)crossOrigin
{
    WebCore::JSMainThreadNullState state;
    return IMPL->crossOrigin();
}

- (void)setCrossOrigin:(NSString *)newCrossOrigin
{
    WebCore::JSMainThreadNullState state;
    IMPL->setCrossOrigin(newCrossOrigin);
}

- (unsigned short)networkState
{
    WebCore::JSMainThreadNullState state;
    return IMPL->networkState();
}

- (NSString *)preload
{
    WebCore::JSMainThreadNullState state;
    return IMPL->preload();
}

- (void)setPreload:(NSString *)newPreload
{
    WebCore::JSMainThreadNullState state;
    IMPL->setPreload(newPreload);
}

- (DOMTimeRanges *)buffered
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->buffered()));
}

- (unsigned short)readyState
{
    WebCore::JSMainThreadNullState state;
    return IMPL->readyState();
}

- (BOOL)seeking
{
    WebCore::JSMainThreadNullState state;
    return IMPL->seeking();
}

- (double)currentTime
{
    WebCore::JSMainThreadNullState state;
    return IMPL->currentTime();
}

- (void)setCurrentTime:(double)newCurrentTime
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->setCurrentTimeForBindings(newCurrentTime));
}

- (double)duration
{
    WebCore::JSMainThreadNullState state;
    return IMPL->duration();
}

- (BOOL)paused
{
    WebCore::JSMainThreadNullState state;
    return IMPL->paused();
}

- (double)defaultPlaybackRate
{
    WebCore::JSMainThreadNullState state;
    return IMPL->defaultPlaybackRate();
}

- (void)setDefaultPlaybackRate:(double)newDefaultPlaybackRate
{
    WebCore::JSMainThreadNullState state;
    IMPL->setDefaultPlaybackRate(newDefaultPlaybackRate);
}

- (double)playbackRate
{
    WebCore::JSMainThreadNullState state;
    return IMPL->playbackRate();
}

- (void)setPlaybackRate:(double)newPlaybackRate
{
    WebCore::JSMainThreadNullState state;
    IMPL->setPlaybackRate(newPlaybackRate);
}

- (DOMTimeRanges *)played
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->played()));
}

- (DOMTimeRanges *)seekable
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->seekable()));
}

- (BOOL)ended
{
    WebCore::JSMainThreadNullState state;
    return IMPL->ended();
}

- (BOOL)autoplay
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::autoplayAttr);
}

- (void)setAutoplay:(BOOL)newAutoplay
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::autoplayAttr, newAutoplay);
}

- (BOOL)loop
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::loopAttr);
}

- (void)setLoop:(BOOL)newLoop
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::loopAttr, newLoop);
}

- (BOOL)controls
{
    WebCore::JSMainThreadNullState state;
    return IMPL->controls();
}

- (void)setControls:(BOOL)newControls
{
    WebCore::JSMainThreadNullState state;
    IMPL->setControls(newControls);
}

- (double)volume
{
    WebCore::JSMainThreadNullState state;
    return IMPL->volume();
}

- (void)setVolume:(double)newVolume
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->setVolume(newVolume));
}

- (BOOL)muted
{
    WebCore::JSMainThreadNullState state;
    return IMPL->muted();
}

- (void)setMuted:(BOOL)newMuted
{
    WebCore::JSMainThreadNullState state;
    IMPL->setMuted(newMuted);
}

- (BOOL)defaultMuted
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasAttributeWithoutSynchronization(WebCore::HTMLNames::mutedAttr);
}

- (void)setDefaultMuted:(BOOL)newDefaultMuted
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBooleanAttribute(WebCore::HTMLNames::mutedAttr, newDefaultMuted);
}

- (BOOL)webkitPreservesPitch
{
    WebCore::JSMainThreadNullState state;
    return IMPL->webkitPreservesPitch();
}

- (void)setWebkitPreservesPitch:(BOOL)newWebkitPreservesPitch
{
    WebCore::JSMainThreadNullState state;
    IMPL->setWebkitPreservesPitch(newWebkitPreservesPitch);
}

- (BOOL)webkitHasClosedCaptions
{
    WebCore::JSMainThreadNullState state;
    return IMPL->webkitHasClosedCaptions();
}

- (BOOL)webkitClosedCaptionsVisible
{
    WebCore::JSMainThreadNullState state;
    return IMPL->webkitClosedCaptionsVisible();
}

- (void)setWebkitClosedCaptionsVisible:(BOOL)newWebkitClosedCaptionsVisible
{
    WebCore::JSMainThreadNullState state;
    IMPL->setWebkitClosedCaptionsVisible(newWebkitClosedCaptionsVisible);
}

- (NSString *)mediaGroup
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getAttribute(WebCore::HTMLNames::mediagroupAttr);
}

- (void)setMediaGroup:(NSString *)newMediaGroup
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAttributeWithoutSynchronization(WebCore::HTMLNames::mediagroupAttr, newMediaGroup);
}

- (void)load
{
    WebCore::JSMainThreadNullState state;
    IMPL->load();
}

- (NSString *)canPlayType:(NSString *)type
{
    WebCore::JSMainThreadNullState state;
    return IMPL->canPlayType(type);
}

- (NSTimeInterval)getStartDate
{
    WebCore::JSMainThreadNullState state;
    return IMPL->getStartDate();
}

- (void)play
{
    WebCore::JSMainThreadNullState state;
    IMPL->play();
}

- (void)pause
{
    WebCore::JSMainThreadNullState state;
    IMPL->pause();
}

- (void)fastSeek:(double)time
{
    WebCore::JSMainThreadNullState state;
    IMPL->fastSeek(time);
}

@end

#endif // ENABLE(VIDEO)

#undef IMPL
