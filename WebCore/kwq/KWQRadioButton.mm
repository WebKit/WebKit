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

#import "KWQRadioButton.h"

#import "KWQView.h"

// We empirically determined that radio buttons have these dimensions.
// It would be better to get this info from AppKit somehow.

#define TOP_MARGIN 3
#define BOTTOM_MARGIN 3
#define LEFT_MARGIN 2
#define RIGHT_MARGIN 2

#define WIDTH 14
#define HEIGHT 13

#define BASELINE_MARGIN 2

QRadioButton::QRadioButton(QWidget *w)
{
    NSButton *button = (NSButton *)getView();
    [button setButtonType:NSRadioButton];
}

QSize QRadioButton::sizeHint() const 
{
    return QSize(WIDTH, HEIGHT);
}

QRect QRadioButton::frameGeometry() const
{
    QRect r = QWidget::frameGeometry();
    return QRect(r.x() + LEFT_MARGIN, r.y() + TOP_MARGIN,
        r.width() - (LEFT_MARGIN + RIGHT_MARGIN),
        r.height() - (TOP_MARGIN + BOTTOM_MARGIN));
}

void QRadioButton::setFrameGeometry(const QRect &r)
{
    QWidget::setFrameGeometry(QRect(r.x() - LEFT_MARGIN, r.y() - TOP_MARGIN,
        r.width() + LEFT_MARGIN + RIGHT_MARGIN,
        r.height() + TOP_MARGIN + BOTTOM_MARGIN));
}

void QRadioButton::setChecked(bool isChecked)
{
    NSButton *button = (NSButton *)getView();
    [button setState:isChecked ? NSOnState : NSOffState];
}

bool QRadioButton::isChecked() const
{
    NSButton *button = (NSButton *)getView();
    return [button state] == NSOnState;
}

int QRadioButton::baselinePosition() const
{
    return height() - BASELINE_MARGIN;
}
