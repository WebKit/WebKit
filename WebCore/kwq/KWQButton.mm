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

#import <qbutton.h>

#import <KWQView.h>

// We empirically determined that buttons have these extra pixels on all
// sides. It would be better to get this info from AppKit somehow.
#define TOP_MARGIN 4
#define BOTTOM_MARGIN 6
#define LEFT_MARGIN 5
#define RIGHT_MARGIN 5

QButton::QButton(QWidget *parent)
    : m_clicked(this, SIGNAL(clicked()))
{
    KWQNSButton *button = [[KWQNSButton alloc] initWithWidget:this];
    setView(button);
    [button release];
}

QSize QButton::sizeHint() const 
{
    KWQNSButton *button = (KWQNSButton *)getView();
    return QSize((int)[[button cell] cellSize].width - (LEFT_MARGIN + RIGHT_MARGIN),
        (int)[[button cell] cellSize].height - (TOP_MARGIN + BOTTOM_MARGIN));
}

QRect QButton::frameGeometry() const
{
    QRect r = QWidget::frameGeometry();
    return QRect(r.x() + LEFT_MARGIN, r.y() + TOP_MARGIN,
        r.width() - (LEFT_MARGIN + RIGHT_MARGIN),
        r.height() - (TOP_MARGIN + BOTTOM_MARGIN));
}

void QButton::setFrameGeometry(const QRect &r)
{
    QWidget::setFrameGeometry(QRect(r.x() - LEFT_MARGIN, r.y() - TOP_MARGIN,
        r.width() + LEFT_MARGIN + RIGHT_MARGIN,
        r.height() + TOP_MARGIN + BOTTOM_MARGIN));
}

void QButton::setText(const QString &s)
{
    KWQNSButton *button = (KWQNSButton *)getView();
    [button setTitle:s.getNSString()];
}

QString QButton::text() const
{
    KWQNSButton *button = (KWQNSButton *)getView();
    return QString::fromNSString([button title]);
}
