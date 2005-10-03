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

#include "config.h"
#import "KWQPushButton.h"

#import "KWQExceptions.h"

enum {
    topMargin,
    bottomMargin,
    leftMargin,
    rightMargin,
    baselineFudgeFactor
};

QPushButton::QPushButton(QWidget *)
{
    NSButton *button = (NSButton *)getView();
    KWQ_BLOCK_EXCEPTIONS;
    [button setBezelStyle:NSRoundedBezelStyle];
    KWQ_UNBLOCK_EXCEPTIONS;
}

QPushButton::QPushButton(const QString &text, QWidget *)
{
    NSButton *button = (NSButton *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    [button setBezelStyle:NSRoundedBezelStyle];
    KWQ_UNBLOCK_EXCEPTIONS;

    setText(text);
}

QSize QPushButton::sizeHint() const 
{
    NSButton *button = (NSButton *)getView();

    QSize size;

    KWQ_BLOCK_EXCEPTIONS;
    size = QSize((int)[[button cell] cellSize].width - (dimensions()[leftMargin] + dimensions()[rightMargin]),
        (int)[[button cell] cellSize].height - (dimensions()[topMargin] + dimensions()[bottomMargin]));
    KWQ_UNBLOCK_EXCEPTIONS;

    return size;
}

QRect QPushButton::frameGeometry() const
{
    QRect r = QWidget::frameGeometry();
    return QRect(r.x() + dimensions()[leftMargin], r.y() + dimensions()[topMargin],
        r.width() - (dimensions()[leftMargin] + dimensions()[rightMargin]),
        r.height() - (dimensions()[topMargin] + dimensions()[bottomMargin]));
}

void QPushButton::setFrameGeometry(const QRect &r)
{
    QWidget::setFrameGeometry(QRect(r.x() - dimensions()[leftMargin], r.y() - dimensions()[topMargin],
        r.width() + dimensions()[leftMargin] + dimensions()[rightMargin],
        r.height() + dimensions()[topMargin] + dimensions()[bottomMargin]));
}

int QPushButton::baselinePosition(int height) const
{
    // Button text is centered vertically, with a fudge factor to account for the shadow.
    NSButton *button = (NSButton *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    NSFont *font = [button font];
    float ascender = [font ascender];
    float descender = [font descender];
    return (int)ceil(-dimensions()[topMargin]
        + ((height + dimensions()[topMargin] + dimensions()[bottomMargin]) - (ascender - descender)) / 2.0
        + ascender - dimensions()[baselineFudgeFactor]);
    KWQ_UNBLOCK_EXCEPTIONS;

    return (int)ceil(-dimensions()[topMargin]
        + ((height + dimensions()[topMargin] + dimensions()[bottomMargin])) / 2.0
        - dimensions()[baselineFudgeFactor]);
}

const int *QPushButton::dimensions() const
{
    // We empirically determined these dimensions.
    // It would be better to get this info from AppKit somehow.
    static const int w[3][5] = {
        { 4, 7, 6, 6, 2 },
        { 4, 6, 5, 5, 2 },
        { 0, 1, 1, 1, 1 }
    };
    NSControl * const button = static_cast<NSControl *>(getView());

    KWQ_BLOCK_EXCEPTIONS;
    return w[[[button cell] controlSize]];
    KWQ_UNBLOCK_EXCEPTIONS;

    return w[NSSmallControlSize];
}
