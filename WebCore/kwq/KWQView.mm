/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#import "KWQView.h"
#import <qwidget.h>

@implementation KWQView

- initWithFrame:(NSRect)r widget:(QWidget *)w 
{
    [super initWithFrame:r];
    widget = w;
    isFlipped = YES;
    return self;
}

- (void)setIsFlipped:(bool)flag
{
    isFlipped = flag;
}

- (BOOL)isFlipped 
{
    return isFlipped;
}

@end

@interface  NSButtonCell (Whacky)
- (NSRect)_insetRect:(NSRect)theRect;
- (NSSize)_titleSizeWithSize:(NSSize)maxSize;
@end

@interface KWQNSButtonCell : NSButtonCell
@end

@implementation KWQNSButtonCell

// cellSizeForBounds is a sizeToFit !
- (NSSize)cellSizeForBounds:(NSRect)theRect
{
// XXX Find a way to get these from the system? -dwh
#define	kPushButtonBorderSize			 4
#define kThemePushButtonTextOffset		 4
#define kPushButtonInset			 1

    // XXX This is going to get way more complex before I'm done here. :(
    // -dwh
    NSSize theSize = [super cellSizeForBounds:theRect];
    theSize.height -= (kPushButtonBorderSize)*2.0;
    return theSize;
}

@end

@implementation KWQNSButton

+ (void)initialize {
    if (self == [KWQNSButton class]) {
        [self setCellClass:[KWQNSButtonCell class]];
    }
}

- initWithFrame:(NSRect)r widget:(QWidget *)w 
{
    [super initWithFrame:r];
    [self setBordered:YES];
    [self setBezelStyle:NSRoundedBezelStyle];
    widget = w;
    
    [self setTarget:self];
    [self setAction:@selector(action:)];
    return self;
}

- (void)action:(id)sender
{
    widget->emitAction(QObject::ACTION_BUTTON_CLICKED);
}

- (void)stateChanged:(id)sender
{
    widget->emitAction(QObject::ACTION_CHECKBOX_CLICKED);
}

@end

@implementation KWQNSComboBox

- initWithFrame:(NSRect)r widget:(QWidget *)w 
{
    [super initWithFrame:r];
    widget = w;
    [self setTarget:self];
    [self setAction:@selector(action:)];
    return self;
}

- (void)action:(id)sender
{
    widget->emitAction(QObject::ACTION_COMBOBOX_CLICKED);
}

@end

@implementation KWQNSScrollView

- initWithFrame:(NSRect)r widget:(QWidget *)w 
{
    [super initWithFrame:r];
    widget = w;
    return self;
}

@end
