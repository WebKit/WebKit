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

#import <qcheckbox.h>

#import <KWQView.h>

QCheckBox::QCheckBox(QWidget *w)
    : QButton(w)
    , m_stateChanged(this, SIGNAL(stateChanged(int)))
{
    KWQNSButton *button = (KWQNSButton *)getView();
    [button setButtonType: NSSwitchButton];
    [button setAction:@selector(stateChanged:)];
}

QSize QCheckBox::sizeHint() const 
{
    return QSize(22, 22);
}

QRect QCheckBox::frameGeometry() const
{
    return QWidget::frameGeometry();
}

void QCheckBox::setFrameGeometry(const QRect &r)
{
    QWidget::setFrameGeometry(r);
}

void QCheckBox::setChecked(bool isChecked)
{
    KWQNSButton *button = (KWQNSButton *)getView();
    [button setState:isChecked ? NSOnState : NSOffState];
}

bool QCheckBox::isChecked()
{
    KWQNSButton *button = (KWQNSButton *)getView();
    return [button state] == NSOnState;
}
