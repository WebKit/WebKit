/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source exceptionCode must retain the above copyright
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

#import "DOMEvents.h"

#import "KWQAssertions.h"

@implementation DOMEvent

- (NSString *)type
{
    ERROR("unimplemented");
    return nil;
}

- (DOMEventTarget *)target
{
    ERROR("unimplemented");
    return nil;
}

- (DOMEventTarget *)currentTarget
{
    ERROR("unimplemented");
    return nil;
}

- (unsigned short)eventPhase
{
    ERROR("unimplemented");
    return 0;
}

- (BOOL)bubbles
{
    ERROR("unimplemented");
    return NO;
}

- (BOOL)cancelable
{
    ERROR("unimplemented");
    return NO;
}

- (DOMTimeStamp)timeStamp
{
    ERROR("unimplemented");
    return nil;
}

- (void)stopPropagation
{
    ERROR("unimplemented");
}

- (void)preventDefault
{
    ERROR("unimplemented");
}

- (void)initEvent:(NSString *)eventTypeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg
{
    ERROR("unimplemented");
}

@end

@implementation DOMEventTarget

- (void)addEventListener:(NSString *)type :(DOMEventListener *)listener :(BOOL)useCapture
{
    ERROR("unimplemented");
}

- (void)removeEventListener:(NSString *)type :(DOMEventListener *)listener :(BOOL)useCapture
{
    ERROR("unimplemented");
}

- (BOOL)dispatchEvent:(DOMEvent *)event
{
    ERROR("unimplemented");
    return NO;
}

@end

@implementation DOMMouseEvent

- (long)screenX
{
    ERROR("unimplemented");
    return 0;
}

- (long)screenY
{
    ERROR("unimplemented");
    return 0;
}

- (long)clientX
{
    ERROR("unimplemented");
    return 0;
}

- (long)clientY
{
    ERROR("unimplemented");
    return 0;
}

- (BOOL)ctrlKey
{
    ERROR("unimplemented");
    return NO;
}

- (BOOL)shiftKey
{
    ERROR("unimplemented");
    return NO;
}

- (BOOL)altKey
{
    ERROR("unimplemented");
    return NO;
}

- (BOOL)metaKey
{
    ERROR("unimplemented");
    return NO;
}

- (unsigned short)button
{
    ERROR("unimplemented");
    return 0;
}

- (DOMEventTarget *)relatedTarget
{
    ERROR("unimplemented");
    return nil;
}

- (void)initMouseEvent:(NSString *)typeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg :(DOMAbstractView *)viewArg :(long)detailArg :(long)screenXArg :(long)screenYArg :(long)clientX :(long)clientY :(BOOL)ctrlKeyArg :(BOOL)altKeyArg :(BOOL)shiftKeyArg :(BOOL)metaKeyArg :(unsigned short)buttonArg :(DOMEventTarget *)relatedTargetArg
{
    ERROR("unimplemented");
}

@end

@implementation DOMMutationEvent

- (DOMNode *)relatedNode
{
    ERROR("unimplemented");
    return nil;
}

- (NSString *)prevValue
{
    ERROR("unimplemented");
    return nil;
}

- (NSString *)newValue
{
    ERROR("unimplemented");
    return nil;
}

- (NSString *)attrName
{
    ERROR("unimplemented");
    return nil;
}

- (unsigned short)attrChange
{
    ERROR("unimplemented");
    return 0;
}

- (void)initMutationEvent:(NSString *)typeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg :(DOMNode *)relatedNodeArg :(NSString *)prevValueArg :(NSString *)newValueArg :(NSString *)attrNameArg :(unsigned short)attrChangeArg
{
    ERROR("unimplemented");
}

@end

@implementation DOMUIEvent

- (DOMAbstractView *)view
{
    ERROR("unimplemented");
    return nil;
}

- (long)detail
{
    ERROR("unimplemented");
    return 0;
}

- (void)initUIEvent:(NSString *)typeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg :(DOMAbstractView *)viewArg :(long)detailArg
{
    ERROR("unimplemented");
}

@end

@implementation DOMDocument (DOMDocumentEvent)

- (DOMEvent *)createEvent:(NSString *)eventType
{
    ERROR("unimplemented");
    return nil;
}

@end
