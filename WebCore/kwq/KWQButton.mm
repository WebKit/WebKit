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

#import "KWQAssertions.h"
#import "KWQCheckBox.h"
#import "KWQKHTMLPart.h"
#import "KWQNSViewExtras.h"
#import "WebCoreBridge.h"

#import "render_form.h"

@interface KWQButton : NSButton
{
    QButton *button;
    BOOL needToSendConsumedMouseUp;
    BOOL inNextValidKeyView;
}

- (id)initWithQButton:(QButton *)b;
- (void)sendConsumedMouseUpIfNeeded;
- (void)simulateClick;

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

-(void)simulateClick
{
    [self performClick:self];
}

-(void)mouseDown:(NSEvent *)event
{
    needToSendConsumedMouseUp = YES;
    [super mouseDown:event];
    [self sendConsumedMouseUpIfNeeded];
}

- (BOOL)becomeFirstResponder
{
    BOOL become = [super becomeFirstResponder];
    if (become) {
        if (!KWQKHTMLPart::currentEventIsMouseDownInWidget(button)) {
            [self _KWQ_scrollFrameToVisible];
        }
        QFocusEvent event(QEvent::FocusIn);
        const_cast<QObject *>(button->eventFilterObject())->eventFilter(button, &event);
    }
    return become;
}

- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign) {
        QFocusEvent event(QEvent::FocusOut);
        const_cast<QObject *>(button->eventFilterObject())->eventFilter(button, &event);
    }
    return resign;
}

-(NSView *)nextKeyView
{
    return button && inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget(button, KWQSelectingNext)
        : [super nextKeyView];
}

-(NSView *)previousKeyView
{
    return button && inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget(button, KWQSelectingPrevious)
        : [super previousKeyView];
}

-(NSView *)nextValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super nextValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

-(NSView *)previousValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super previousValidKeyView];
    inNextValidKeyView = NO;
    return view;
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
    [button setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSSmallControlSize]]];

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

void QButton::simulateClick()
{
    KWQButton *button = (KWQButton *)getView();
    [button simulateClick];
}

void QButton::setFont(const QFont &f)
{
    QWidget::setFont(f);

    const NSControlSize size = KWQNSControlSizeForFont(f);    
    NSControl * const button = static_cast<NSControl *>(getView());
    [[button cell] setControlSize:size];
    [button setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:size]]];
}

NSControlSize KWQNSControlSizeForFont(const QFont &f)
{
    return NSSmallControlSize;
    // Dave is going to turn this on once he figures out what he wants to do
    // about mini controls.
#if 0
    const int fontSize = f.pixelSize();
    if (fontSize >= 20) {
        return NSRegularControlSize;
    }
    if (fontSize >= 10) {
        return NSSmallControlSize;
    }
    return NSMiniControlSize;
#endif
}

QWidget::FocusPolicy QButton::focusPolicy() const
{
    // Add an additional check here.
    // For now, buttons are only focused when full
    // keyboard access is turned on.
    if ([KWQKHTMLPart::bridgeForWidget(this) keyboardUIMode] != WebCoreFullKeyboardAccess)
        return NoFocus;

    return QWidget::focusPolicy();
}

