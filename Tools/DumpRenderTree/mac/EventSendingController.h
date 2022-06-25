/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <WebKit/WebKitLegacy.h>

@interface EventSendingController : NSObject <DOMEventListener>
{
    BOOL leftMouseButtonDown;
    BOOL dragMode;
    int clickCount;
    NSTimeInterval lastClick;
    int eventNumber;
    double timeOffset;
#if PLATFORM(MAC)
    BOOL _sentWheelPhaseEndOrCancel;
    BOOL _sentMomentumPhaseEnd;
#endif
#if PLATFORM(IOS_FAMILY)
    NSMutableArray* touches;
    unsigned currentTouchIdentifier;
    unsigned nextEventFlags;
#endif
}

+ (void)saveEvent:(NSInvocation *)event;
+ (void)replaySavedEvents;
+ (void)clearSavedEvents;

- (void)scheduleAsynchronousClick;

- (void)enableDOMUIEventLogging:(WebScriptObject *)node;

- (void)handleEvent:(DOMEvent *)event;

#if PLATFORM(MAC)
// Timestamp is mach_absolute_time() units.
- (void)sendScrollEventAt:(NSPoint)mouseLocation deltaX:(double)deltaX deltaY:(double)deltaY units:(CGScrollEventUnit)units wheelPhase:(CGGesturePhase)wheelPhase momentumPhase:(CGMomentumScrollPhase)momentumPhase timestamp:(uint64_t)timestamp;
#endif

@end

extern NSPoint lastMousePosition;
extern NSPoint lastClickPosition;
