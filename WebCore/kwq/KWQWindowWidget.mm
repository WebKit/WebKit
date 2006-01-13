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
#import "KWQWindowWidget.h"

#import "WebCoreFrameBridge.h"

#import <Cocoa/Cocoa.h>

// The NSScreen calls in this file can't throw, so no need to block
// Cocoa exceptions.

class KWQWindowWidgetPrivate
{
public:
    WebCoreFrameBridge *bridge;
};

KWQWindowWidget::KWQWindowWidget(WebCoreFrameBridge *bridge) :
    d(new KWQWindowWidgetPrivate())
{
    // intentionally not retained, since the bridge owns the window widget
    d->bridge = bridge;
}

KWQWindowWidget::~KWQWindowWidget()
{
    delete d;
}

IntSize KWQWindowWidget::sizeHint() const
{
    return size();
}

QRect KWQWindowWidget::frameGeometry() const
{
    NSRect frame = [d->bridge windowFrame];
    return QRect((int)frame.origin.x, (int)(NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - NSMaxY(frame)),
                 (int)frame.size.width, (int)frame.size.height);
}

QWidget *KWQWindowWidget::topLevelWidget() const
{
    return (QWidget *)this;
}

// Note these routines work on QT window coords - origin upper left
QPoint KWQWindowWidget::mapFromGlobal(const QPoint &p) const
{
    NSPoint screenPoint = NSMakePoint(p.x(), NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - p.y());
    NSPoint windowPoint = [[d->bridge window] convertScreenToBase:screenPoint];
    return QPoint((int)windowPoint.x, (int)([d->bridge windowFrame].size.height - windowPoint.y));
}

// maps "viewport" (actually Cocoa window coords) to screen coords
QPoint KWQWindowWidget::viewportToGlobal(const QPoint &p) const
{
    NSPoint windowPoint = NSMakePoint(p.x(), p.y());
    NSPoint screenPoint = [[d->bridge window] convertBaseToScreen:windowPoint];
    return QPoint((int)screenPoint.x, (int)(NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - screenPoint.y));
}

void KWQWindowWidget::setFrameGeometry(const QRect &r)
{
    // FIXME: Could do something to make it easy for the browser to avoid saving this change.
    [d->bridge setWindowFrame:NSMakeRect(r.x(), NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - (r.y() + r.height()),
        r.width(), r.height())];
}
