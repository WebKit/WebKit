/*
 * Copyright 2006-2017 Apple Inc. All rights reserved.
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

#import "config.h"
#pragma GCC visibility push(default)
#import "WebCoreSystemInterface.h"
#pragma GCC visibility pop

#import <Foundation/Foundation.h>

void (*wkDrawBezeledTextArea)(NSRect, BOOL enabled);
void (*wkDrawMediaSliderTrack)(CGContextRef context, CGRect rect, float timeLoaded, float currentTime,
    float duration, unsigned state);
BOOL (*wkHitTestMediaUIPart)(int part, CGRect bounds, CGPoint point);
void (*wkDrawMediaUIPart)(int part, CGContextRef context, CGRect rect, unsigned state);
void (*wkMeasureMediaUIPart)(int part, CGRect *bounds, CGSize *naturalSize);
NSTimeInterval (*wkGetNSURLResponseCalculatedExpiration)(NSURLResponse *response);
BOOL (*wkGetNSURLResponseMustRevalidate)(NSURLResponse *response);
void (*wkPopupMenu)(NSMenu*, NSPoint location, float width, NSView*, int selectedItem, NSFont*, NSControlSize controlSize, bool usesCustomAppearance);

void* wkGetHyphenationLocationBeforeIndex;

bool (*wkExecutableWasLinkedOnOrBeforeSnowLeopard)(void);

CFStringRef (*wkCopyDefaultSearchProviderDisplayName)(void);

NSCursor *(*wkCursor)(const char*);

#if !PLATFORM(IOS)
CGFloat (*wkNSElasticDeltaForTimeDelta)(CGFloat initialPosition, CGFloat initialVelocity, CGFloat elapsedTime);
CGFloat (*wkNSElasticDeltaForReboundDelta)(CGFloat delta);
CGFloat (*wkNSReboundDeltaForElasticDelta)(CGFloat delta);
#endif

int (*wkExernalDeviceTypeForPlayer)(AVPlayer *);
NSString *(*wkExernalDeviceDisplayNameForPlayer)(AVPlayer *);

bool (*wkQueryDecoderAvailability)(void);
