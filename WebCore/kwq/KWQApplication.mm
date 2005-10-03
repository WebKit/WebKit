/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#import "KWQApplication.h"
#import "KWQStyle.h"

// The NSScreen Cocoa calls in this file should never throw, so
// there's no need to block exceptions.

int QDesktopWidget::width()
{
    return (int)[[NSScreen mainScreen] frame].size.width;
}
    
int QDesktopWidget::height()
{
    return (int)[[NSScreen mainScreen] frame].size.height;
}

// FIXME: Some day we may want to handle the case where the widget is in a window that's on
// a different screen from the key window at the time the script is run. If so, keep in mind
// that the result of screenGeometry is intersected with the result of KWinModule::workArea.

int QDesktopWidget::screenNumber(QWidget *)
{
    // Always returns 0, which is fine since this is only passed to screenGeometry,
    // which ignores the screen number parameter.
    return 0;
}

QRect QDesktopWidget::screenGeometry(int screenNumber)
{
    // Ignores the screen number, and always returns the geometry of the main screen,
    // which is the screen that the key window is on.
    NSRect rect = [[NSScreen mainScreen] frame];
    rect.origin.y = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - NSMaxY(rect);
    return QRect(rect);
}

QRect QDesktopWidget::availableGeometry(QWidget *widget)
{
    NSScreen *screen = widget ? [[widget->getView() window] screen] : nil;
    if (!screen)
        screen = [NSScreen mainScreen];
    NSRect rect = [screen visibleFrame];
    rect.origin.y = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - NSMaxY(rect);
    return QRect(rect);
}

void QApplication::setOverrideCursor(const QCursor &c)
{
    // FIXME: Should implement this so that frame border dragging has the proper cursor.
}

void QApplication::restoreOverrideCursor()
{
    // FIXME: Should implement this so that frame border dragging has the proper cursor.
}

QStyle &QApplication::style()
{
    static QStyle style;
    return style;
}
