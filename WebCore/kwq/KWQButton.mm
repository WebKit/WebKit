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
    BOOL needToSendConsumedMouseUp;
}

- (id)initWithQButton:(QButton *)b;
- (void)sendConsumedMouseUpIfNeeded;

@end

@implementation KWQButton

- (id)initWithQButton:(QButton *)b
{
    button = b;
    return [self init];
}

- (void)action:(id)sender
{
    button->clicked();
}

- (void)sendConsumedMouseUpIfNeeded
{
    if (needToSendConsumedMouseUp) {
	needToSendConsumedMouseUp = NO;
	if ([self target]) {
            button->sendConsumedMouseUp();
        }
    } 
}

-(void)mouseDown:(NSEvent *)event
{
    needToSendConsumedMouseUp = YES;
    [super mouseDown:event];
    [self sendConsumedMouseUpIfNeeded];
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
    // Order of signals is:
    //   1) signals in subclasses (stateChanged, not sure if there are any others)
    //   2) mouse up
    //   3) clicked
    // Proper behavior of check boxes, at least, depends on this order.
    
    KWQButton *button = (KWQButton *)getView();
    [button sendConsumedMouseUpIfNeeded];

    // Don't call clicked if the button was destroyed inside of sendConsumedMouseUpIfNeeded.
    if ([button target]) {
        m_clicked.call();
    }
}
