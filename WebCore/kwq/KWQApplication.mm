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

#include <qapplication.h>
#include <kwqdebug.h>

QPalette QApplication::palette(const QWidget *p)
{
    static QPalette pal;
    return pal;
}

// FIXME: Do we need to handle multiple screens?

int QDesktopWidget::width() const
{
    return (int)[[NSScreen mainScreen] frame].size.width;
}
    
int QDesktopWidget::height() const
{
    return (int)[[NSScreen mainScreen] frame].size.height;
}

int QDesktopWidget::screenNumber(QWidget *) const
{
    return 0;
}

QRect QDesktopWidget::screenGeometry(int screenNumber)
{
    NSRect rect = [[NSScreen mainScreen] frame];
    return QRect((int)rect.origin.x, (int)rect.origin.y, (int)rect.size.width, (int)rect.size.height);
}

QDesktopWidget *QApplication::desktop()
{
    static QDesktopWidget desktopWidget;
    return &desktopWidget;
}

int QApplication::startDragDistance()
{
     return 2;
}

QSize QApplication::globalStrut()
{
    _logNotYetImplemented();
    return QSize(0,0);
}

void QApplication::setOverrideCursor(const QCursor &c)
{
    _logNotYetImplemented();
}

void QApplication::restoreOverrideCursor()
{
    _logNotYetImplemented();
}

bool QApplication::sendEvent(QObject *o, QEvent *e)
{
    return o ? o->event(e) : false;
}

void QApplication::sendPostedEvents(QObject *receiver, int event_type)
{
    _logNotYetImplemented();
}

QApplication *qApp = NULL;

QStyle &QApplication::style() const
{
    static QStyle style;
    return style;
}
