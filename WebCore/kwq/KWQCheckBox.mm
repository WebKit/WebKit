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

#import "KWQCheckBox.h"
#import "KWQExceptions.h"

enum {
    topMargin,
    bottomMargin,
    leftMargin,
    rightMargin,
    baselineFudgeFactor,
    dimWidth,
    dimHeight
};

QCheckBox::QCheckBox(QWidget *w)
    : m_stateChanged(this, SIGNAL(stateChanged(int)))
{
    KWQ_BLOCK_EXCEPTIONS;

    NSButton *button = (NSButton *)getView();
    [button setButtonType:NSSwitchButton];

    KWQ_UNBLOCK_EXCEPTIONS;
}

QSize QCheckBox::sizeHint() const 
{
    return QSize(dimensions()[dimWidth], dimensions()[dimHeight]);
}

QRect QCheckBox::frameGeometry() const
{
    QRect r = QWidget::frameGeometry();
    return QRect(r.x() + dimensions()[leftMargin], r.y() + dimensions()[topMargin],
        r.width() - (dimensions()[leftMargin] + dimensions()[rightMargin]),
        r.height() - (dimensions()[topMargin] + dimensions()[bottomMargin]));
}

void QCheckBox::setFrameGeometry(const QRect &r)
{
    QWidget::setFrameGeometry(QRect(r.x() - dimensions()[leftMargin], r.y() - dimensions()[topMargin],
        r.width() + dimensions()[leftMargin] + dimensions()[rightMargin],
        r.height() + dimensions()[topMargin] + dimensions()[bottomMargin]));
}

void QCheckBox::setChecked(bool isChecked)
{
    KWQ_BLOCK_EXCEPTIONS;

    NSButton *button = (NSButton *)getView();
    [button setState:isChecked ? NSOnState : NSOffState];

    KWQ_UNBLOCK_EXCEPTIONS;
}

bool QCheckBox::isChecked()
{
    KWQ_BLOCK_EXCEPTIONS;
    NSButton *button = (NSButton *)getView();
    return [button state] == NSOnState;
    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
}

void QCheckBox::clicked()
{
    // Note that it's important to give the stateChanged signal before
    // the clicked signal so that the corresponding JavaScript messages
    // go in the right order. A test for this at the time of this writing
    // was the languages radio buttons and check boxes at google.com prefs.
    
    m_stateChanged.call(isChecked() ? 2 : 0);
    QButton::clicked();
}

int QCheckBox::baselinePosition(int height) const
{
    return height - dimensions()[baselineFudgeFactor];
}

const int *QCheckBox::dimensions() const
{
    // We empirically determined these dimensions.
    // It would be better to get this info from AppKit somehow.
    static const int w[3][7] = {
        { 3, 4, 2, 4, 2, 14, 14 },
        { 4, 3, 3, 3, 2, 12, 12 },
        { 4, 3, 3, 3, 2, 10, 10 },
    };
    NSControl * const button = static_cast<NSControl *>(getView());

    KWQ_BLOCK_EXCEPTIONS;
    return w[[[button cell] controlSize]];
    KWQ_UNBLOCK_EXCEPTIONS;

    return w[NSSmallControlSize];
}
