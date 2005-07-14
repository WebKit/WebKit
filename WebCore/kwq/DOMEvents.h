/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import <WebCore/DOMViews.h>

@class DOMEvent;

extern NSString * const DOMEventException;

enum DOMEventExceptionCode {
    DOM_UNSPECIFIED_EVENT_TYPE_ERR = 0
};

@protocol DOMEventListener <NSObject>
- (void)handleEvent:(DOMEvent *)event;
@end

@protocol DOMEventTarget <NSObject, NSCopying>
- (void)addEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture;
- (void)removeEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture;
- (BOOL)dispatchEvent:(DOMEvent *)event;
@end

enum {
    DOM_CAPTURING_PHASE = 1,
    DOM_AT_TARGET = 2,
    DOM_BUBBLING_PHASE = 3
};

@interface DOMEvent : DOMObject
- (NSString *)type;
- (id <DOMEventTarget>)target;
- (id <DOMEventTarget>)currentTarget;
- (unsigned short)eventPhase;
- (BOOL)bubbles;
- (BOOL)cancelable;
- (DOMTimeStamp)timeStamp;
- (void)stopPropagation;
- (void)preventDefault;
- (void)initEvent:(NSString *)eventTypeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg;
@end

@interface DOMDocument (DOMDocumentEvent)
- (DOMEvent *)createEvent:(NSString *)eventType;
@end

@interface DOMUIEvent : DOMEvent
- (DOMAbstractView *)view;
- (long)detail;
- (void)initUIEvent:(NSString *)typeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg :(DOMAbstractView *)viewArg :(long)detailArg;
@end

@interface DOMMouseEvent : DOMUIEvent
- (long)screenX;
- (long)screenY;
- (long)clientX;
- (long)clientY;
- (BOOL)ctrlKey;
- (BOOL)shiftKey;
- (BOOL)altKey;
- (BOOL)metaKey;
- (unsigned short)button;
- (id <DOMEventTarget>)relatedTarget;
- (void)initMouseEvent:(NSString *)typeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg :(DOMAbstractView *)viewArg :(long)detailArg :(long)screenXArg :(long)screenYArg :(long)clientX :(long)clientY :(BOOL)ctrlKeyArg :(BOOL)altKeyArg :(BOOL)shiftKeyArg :(BOOL)metaKeyArg :(unsigned short)buttonArg :(id <DOMEventTarget>)relatedTargetArg;
@end

enum {
    DOM_MODIFICATION = 1,
    DOM_ADDITION = 2,
    DOM_REMOVAL = 3
};

@interface DOMMutationEvent : DOMEvent
- (DOMNode *)relatedNode;
- (NSString *)prevValue;
- (NSString *)newValue;
- (NSString *)attrName;
- (unsigned short)attrChange;
- (void)initMutationEvent:(NSString *)typeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg :(DOMNode *)relatedNodeArg :(NSString *)prevValueArg :(NSString *)newValueArg :(NSString *)attrNameArg :(unsigned short)attrChangeArg;
@end
