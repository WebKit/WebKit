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

#import "KWQPushButton.h"

// We empirically determined that buttons have these extra pixels on all
// sides. It would be better to get this info from AppKit somehow.
#define TOP_MARGIN 4
#define BOTTOM_MARGIN 6
#define LEFT_MARGIN 5
#define RIGHT_MARGIN 5

// AppKit calls this kThemePushButtonSmallTextOffset.
#define VERTICAL_FUDGE_FACTOR 2

QPushButton::QPushButton(QWidget *)
{
    NSButton *button = (NSButton *)getView();
    [button setBezelStyle:NSRoundedBezelStyle];
}

QPushButton::QPushButton(const QString &text, QWidget *)
{
    NSButton *button = (NSButton *)getView();
    [button setBezelStyle:NSRoundedBezelStyle];

    setText(text);
}

QSize QPushButton::sizeHint() const 
{
    NSButton *button = (NSButton *)getView();
    return QSize((int)[[button cell] cellSize].width - (LEFT_MARGIN + RIGHT_MARGIN),
        (int)[[button cell] cellSize].height - (TOP_MARGIN + BOTTOM_MARGIN));
}

QRect QPushButton::frameGeometry() const
{
    QRect r = QWidget::frameGeometry();
    return QRect(r.x() + LEFT_MARGIN, r.y() + TOP_MARGIN,
        r.width() - (LEFT_MARGIN + RIGHT_MARGIN),
        r.height() - (TOP_MARGIN + BOTTOM_MARGIN));
}

void QPushButton::setFrameGeometry(const QRect &r)
{
    QWidget::setFrameGeometry(QRect(r.x() - LEFT_MARGIN, r.y() - TOP_MARGIN,
        r.width() + LEFT_MARGIN + RIGHT_MARGIN,
        r.height() + TOP_MARGIN + BOTTOM_MARGIN));
}

int QPushButton::baselinePosition() const
{
    // Button text is centered vertically, with a fudge factor to account for the shadow.
    NSButton *button = (NSButton *)getView();
    NSFont *font = [button font];
    float ascender = [font ascender];
    float descender = [font descender];
    return (int)ceil(-TOP_MARGIN
        + ((height() + TOP_MARGIN + BOTTOM_MARGIN) - (ascender - descender)) / 2.0
        + ascender - VERTICAL_FUDGE_FACTOR);
}
