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
#import <qcheckbox.h>

@implementation KWQView

- initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];
    isFlipped = YES;
    return self;
}

- initWithWidget:(QWidget *)w 
{
    [super init];
    widget = w;
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

@implementation KWQNSButton

- initWithFrame:(NSRect)frame 
{
    [super initWithFrame:frame];

    [self setTarget:self];
    [self setAction:@selector(action:)];

    [self setTitle:@""];
    [self setBezelStyle:NSRoundedBezelStyle];
    [[self cell] setControlSize:NSSmallControlSize];
    [self setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];

    return self;
}

- initWithWidget:(QWidget *)w 
{
    [super init];
    widget = w;
    return self;
}

- (void)action:(id)sender
{
    QButton *button = dynamic_cast<QButton *>(widget);
    if (button) {
        button->clicked();
    }
}

- (void)stateChanged:(id)sender
{
    // Note that it's important to give the stateChanged signal before
    // the clicked signal so that the corresponding JavaScript messages
    // go in the right order. A test for this at the time of this writing
    // was the languages radio buttons and check boxes at google.com prefs.
    
    QCheckBox *checkBox = dynamic_cast<QCheckBox *>(widget);
    if (checkBox) {
        checkBox->stateChanged();
    }
    [self action:sender];
}

@end

@implementation KWQNSComboBox

- initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];

    [self setTarget:self];
    [self setAction:@selector(action:)];

    [[self cell] setControlSize:NSSmallControlSize];
    [self setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];

    return self;
}

- initWithWidget:(QWidget *)w 
{
    [super init];
    widget = w;
    return self;
}

- (void)action:(id)sender
{
    widget->emitAction(QObject::ACTION_COMBOBOX_CLICKED);
}

@end

@implementation KWQNSScrollView

- initWithWidget:(QWidget *)w 
{
    [super init];
    widget = w;
    return self;
}

@end
