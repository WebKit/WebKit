/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQButton.h"

#import "KWQCheckBox.h"

#import "render_form.h"

@interface KWQButton : NSButton
{
    QButton *button;
    BOOL processingMouseEvent;
    BOOL clickedDuringMouseEvent;
}

- initWithQButton:(QButton *)b;
- (void)action:(id)sender;
@end

@implementation KWQButton


- initWithQButton:(QButton *)b
{
    button = b;
    return [super init];
}

- (void)action:(id)sender
{
    if (processingMouseEvent) {
	clickedDuringMouseEvent = true;
	button->sendConsumedMouseUp();
    } 

    button->clicked();
}

-(void)mouseDown:(NSEvent *)event
{
    processingMouseEvent = true;
    [super mouseDown:event];
    processingMouseEvent = false;

    if (clickedDuringMouseEvent) {
	clickedDuringMouseEvent = false;
    } else {
	button->sendConsumedMouseUp();
    }
}


@end

QButton::QButton()
    : m_clicked(this, SIGNAL(clicked()))
{
    KWQButton *button = [[KWQButton alloc] initWithQButton:this];
    
    [button setTarget:button];
    [button setAction:@selector(action:)];

    [button setTitle:@""];
    [[button cell] setControlSize:NSSmallControlSize];
    [button setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];

    setView(button);

    [button release];
}

QButton::~QButton()
{
    NSButton *button = (NSButton *)getView();
    [button setTarget:nil];
}

void QButton::setText(const QString &s)
{
    NSButton *button = (NSButton *)getView();
    [button setTitle:s.getNSString()];
}

QString QButton::text() const
{
    NSButton *button = (NSButton *)getView();
    return QString::fromNSString([button title]);
}

void QButton::clicked()
{
    m_clicked.call();
}


