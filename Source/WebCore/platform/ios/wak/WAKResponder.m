/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WAKResponder.h"

#if PLATFORM(IOS)

#import "WAKViewPrivate.h"
#import "WKViewPrivate.h"

@implementation WAKResponder

// FIXME: the functions named handleEvent generally do not forward event to the parent chain.
// This method should ideally be removed, or renamed "sendEvent".
- (void)handleEvent:(WebEvent *)event
{
    UNUSED_PARAM(event);
}

- (void)_forwardEvent:(WebEvent *)event
{
    [[self nextResponder] handleEvent:event];
}

- (void)scrollWheel:(WebEvent *)event 
{ 
    [self _forwardEvent:event];
}

- (void)mouseEntered:(WebEvent *)event
{ 
    [self _forwardEvent:event];
}

- (void)mouseExited:(WebEvent *)event 
{ 
    [self _forwardEvent:event];
}

- (void)mouseMoved:(WebEvent *)theEvent
{
    [self _forwardEvent:theEvent];
}

- (void)keyDown:(WebEvent *)event
{ 
    [self _forwardEvent:event];
}
- (void)keyUp:(WebEvent *)event
{ 
    [self _forwardEvent:event];
}

#if ENABLE(TOUCH_EVENTS)
- (void)touch:(WebEvent *)event
{
    [self _forwardEvent:event];
}
#endif

- (WAKResponder *)nextResponder { return nil; }

- (void)insertText:(NSString *)text
{
    UNUSED_PARAM(text);
}

- (void)deleteBackward:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)deleteForward:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)insertParagraphSeparator:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)moveDown:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)moveDownAndModifySelection:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)moveLeft:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)moveLeftAndModifySelection:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)moveRight:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)moveRightAndModifySelection:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)moveUp:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)moveUpAndModifySelection:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)mouseUp:(WebEvent *)event 
{
    [self _forwardEvent:event];
}

- (void)mouseDown:(WebEvent *)event 
{
    [self _forwardEvent:event];
}

- (BOOL)acceptsFirstResponder { return true; }
- (BOOL)becomeFirstResponder { return true; }
- (BOOL)resignFirstResponder { return true; }

- (BOOL)tryToPerform:(SEL)anAction with:(id)anObject 
{ 
    if ([self respondsToSelector:anAction]) {
        [self performSelector:anAction withObject:anObject];
        return YES;
    }
    return NO; 
}

@end

#endif // PLATFORM(IOS)
