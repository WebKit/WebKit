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
#import "KWQView.h"

#include <khtmlview.h>
#include <qwidget.h>
#include <qpainter.h>
#include <html/html_documentimpl.h>

@implementation KWQView

- initWithFrame: (NSRect) r widget: (QWidget *)w 
{
    [super initWithFrame: r];
    widget = w;
    isFlipped = YES;
}


// This should eventually be removed.
- (void)drawRect:(NSRect)rect {
    widget->paint((void *)0);
}

- (void)setIsFlipped: (bool)flag
{
    isFlipped = flag;
}


- (BOOL)isFlipped 
{
    return isFlipped;
}

@end


@implementation KWQNSButton

- initWithFrame: (NSRect) r widget: (QWidget *)w 
{
    [super initWithFrame: r];
    widget = w;
}

@end


@implementation KWQNSComboBox

- initWithFrame: (NSRect) r widget: (QWidget *)w 
{
    [super initWithFrame: r];
    widget = w;
}

@end


@implementation KWQHTMLView

- initWithFrame: (NSRect) r widget: (QWidget *)w 
{
    [super initWithFrame: r];
    widget = w;
    isFlipped = NO;
}


// This should eventually be removed.
- (void)drawRect:(NSRect)rect {
    if (widget != 0l){
        //widget->paint((void *)0);
        
        QPainter p(widget);
        NSRect frame = [self frame];
 
        if (((KHTMLView *)widget)->part()->xmlDocImpl() && 
            ((KHTMLView *)widget)->part()->xmlDocImpl()->renderer()){
            ((KHTMLView *)widget)->layout(TRUE);
        }
       
        ((KHTMLView *)widget)->drawContents( &p, (int)frame.origin.x, 
                    (int)frame.origin.y, 
                    (int)frame.size.width, 
                    (int)frame.size.height );
    }
}

- (void)setIsFlipped: (bool)flag
{
    isFlipped = flag;
}


- (BOOL)isFlipped 
{
    return isFlipped;
}

@end

