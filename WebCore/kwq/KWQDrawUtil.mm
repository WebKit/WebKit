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

#import <kwqdebug.h>
#import <qdrawutil.h>

void qDrawShadePanel(QPainter *p, int x, int y, int w, int h, const QColorGroup &g, bool
        sunken, int lineWidth, const QBrush *fill)
{
    if ( w == 0 || h == 0 ) {
        return;
    }

    QPen oldPen = p->pen();                     // save pen
    QPointArray a(4 * lineWidth);
    
    if (sunken) {
        p->setPen(g.dark());
    }
    else {
        p->setPen(g.light());
    }

    int x1, y1, x2, y2;
    int i;
    int n = 0;
    x1 = x;
    y1 = y2 = y;
    x2 = x+w-2;

    for (i = 0; i < lineWidth; i++) {             // top shadow
        a.setPoint(n++, x1, y1++);
        a.setPoint(n++, x2--, y2++);
    }

    x2 = x1;
    y1 = y+h-2;

    for (i = 0; i < lineWidth; i++) {             // left shadow
        a.setPoint(n++, x1++, y1);
        a.setPoint(n++, x2++, y2--);
    }

    p->drawLineSegments(a);
    n = 0;

    if (sunken) {
        p->setPen(g.light());
    }
    else {
        p->setPen(g.dark());
    }

    x1 = x;
    y1 = y2 = y+h-1;
    x2 = x+w-1;

    for (i = 0; i < lineWidth; i++) {             // bottom shadow
        a.setPoint(n++, x1++, y1--);
        a.setPoint(n++, x2, y2--);
    }

    x1 = x2;
    y1 = y;
    y2 = y+h-lineWidth-1;

    for (i = 0; i < lineWidth; i++) {             // right shadow
        a.setPoint(n++, x1--, y1++);
        a.setPoint(n++, x2--, y2);
    }

    p->drawLineSegments(a);
    if (fill) {                               // fill with fill color
        QBrush oldBrush = p->brush();
        p->setPen(Qt::NoPen);
        p->setBrush(*fill);
        p->drawRect(x+lineWidth, y+lineWidth, w-lineWidth*2, h-lineWidth*2);
        p->setBrush(oldBrush);
    }
    p->setPen(oldPen);                        // restore pen
}

