/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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
#include <qhbox.h>

#include <KWQView.h>
#include <KWQNSTextField.h>

#include <kwqdebug.h>

// This class is ONLY used by FORM <input type=file> elements.  It's used
// to stretch a LineEditWidget.  We don't need it.

QHBox::QHBox()
{
    KWQView *view = (KWQView *)getView();
    [view setIsFlipped: NO];
}


QHBox::QHBox(QWidget *)
{
    _logNotYetImplemented();
}


QHBox::~QHBox()
{
    _logNotYetImplemented();
}


// Override the focus proxy to simply add the line edit widget as a subview.
void QHBox::setFocusProxy( QWidget *lineEdit)
{
    KWQView *view = (KWQView *)getView();
    KWQNSTextField *fieldView = (KWQNSTextField *)lineEdit->getView();

    // Do we also need to set size, or will layout take care of that?
    [view addSubview: fieldView];
}


bool QHBox::setStretchFactor(QWidget*, int stretch)
{
    _logNotYetImplemented();
    return FALSE;
}

