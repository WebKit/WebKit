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

#import <KWQWindowWidget.h>

#import <Cocoa/Cocoa.h>

@interface KWQWindowWidgetDeleter : NSObject
{
    KWQWindowWidget *_widget;
}
- initWithWindowWidget:(KWQWindowWidget *)widget;
- (void)deleteWindowWidget;
@end

class KWQWindowWidgetPrivate
{
public:
    NSWindow *window;
    KWQWindowWidgetDeleter *deleter;
};

static CFMutableDictionaryRef windowWidgets = NULL;

KWQWindowWidget::KWQWindowWidget(NSWindow *window) :
    d(new KWQWindowWidgetPrivate())
{
    d->window = [window retain];
    d->deleter = [[KWQWindowWidgetDeleter alloc] initWithWindowWidget:this];

    [[NSNotificationCenter defaultCenter] addObserver:d->deleter
        selector:@selector(deleteWindowWidget) name:NSWindowWillCloseNotification object:window];
}

KWQWindowWidget::~KWQWindowWidget()
{
    CFDictionaryRemoveValue(windowWidgets, d->window);
    [[NSNotificationCenter defaultCenter] removeObserver:d->deleter];
    
    [d->window release];
    [d->deleter release];
    
    delete d;
}

KWQWindowWidget *KWQWindowWidget::fromNSWindow(NSWindow *window)
{
    if (windowWidgets == NULL) {
	windowWidgets = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    }
    
    KWQWindowWidget *widget = (KWQWindowWidget *)CFDictionaryGetValue(windowWidgets, window);
    if (widget == NULL) {
	widget = new KWQWindowWidget(window);
	CFDictionarySetValue(windowWidgets, window, widget);
    }

    return widget;
}

QSize KWQWindowWidget::sizeHint() const
{
    return size();
}

QRect KWQWindowWidget::frameGeometry() const
{
    NSRect frame = [d->window frame];
    return QRect((int)frame.origin.x, (int)(NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - frame.origin.y),
		 (int)frame.size.width, (int)frame.size.height);
}

QWidget *KWQWindowWidget::topLevelWidget() const
{
    return (QWidget *)this;
}

QPoint KWQWindowWidget::mapToGlobal(const QPoint &p) const
{
    NSPoint windowPoint = NSMakePoint(p.x(), [d->window frame].size.height - p.y());
    NSPoint screenPoint = [d->window convertBaseToScreen:windowPoint];
    return QPoint((int)screenPoint.x, (int)(NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - screenPoint.y));
}

QPoint KWQWindowWidget::mapFromGlobal(const QPoint &p) const
{
    NSPoint screenPoint = NSMakePoint(p.x(), NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - p.y());
    NSPoint windowPoint = [d->window convertScreenToBase:screenPoint];
    return QPoint((int)windowPoint.x, (int)([d->window frame].size.height - windowPoint.y));
}

void KWQWindowWidget::setFrameGeometry(const QRect &r)
{
    // FIXME: should try to avoid saving changes
    [d->window setFrame:NSMakeRect(r.x(), NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - r.y() - r.height(), r.width(), r.height()) display:NO];
}

@implementation KWQWindowWidgetDeleter

- initWithWindowWidget:(KWQWindowWidget *)widget
{
    [super init];
    _widget = widget;
    return self;
}

- (void)deleteWindowWidget
{
    delete _widget;
}

@end
