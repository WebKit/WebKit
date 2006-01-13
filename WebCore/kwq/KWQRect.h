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

#ifndef QRECT_H_
#define QRECT_H_

#include "KWQSize.h"
#include "KWQPointArray.h"

#ifdef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
typedef struct CGRect NSRect;
#else
typedef struct _NSRect NSRect;
#endif
typedef struct CGRect CGRect;

class QRect {
public:
    QRect();
    QRect(QPoint p, QSize s);
    QRect(int, int, int, int);
    QRect(const QPoint &, const QPoint &);
#ifndef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
    explicit QRect(const NSRect &); // don't do this implicitly since it's lossy
#endif
    explicit QRect(const CGRect &); // don't do this implicitly since it's lossy

    bool isNull() const;
    bool isValid() const;
    bool isEmpty() const;

    int x() const { return xp; }
    int y() const { return yp; }
    int left() const { return xp; }
    int top() const { return yp; }
    int right() const;
    int bottom() const;
    int width() const { return w; }
    int height() const { return h; }

    QPoint topLeft() const;
    QPoint topRight() const;
    QPoint bottomRight() const;
    QPoint bottomLeft() const;
    
    void moveTopLeft(const QPoint &p) { xp += p.x(); yp += p.y(); } 

    QSize size() const;
    void setX(int x) { xp = x; }
    void setY(int y) { yp = y; }
    void setWidth(int width) { w = width; }
    void setHeight(int height) { h = height; }
    void setRect(int x, int y, int width, int height) { xp = x; yp = y; w = width; h = height; }
    QRect intersect(const QRect &) const;
    bool intersects(const QRect &) const;
    QRect unite(const QRect &) const;
    QRect normalize() const;

    bool contains(const QPoint &point) const { return contains(point.x(), point.y()); }

    bool contains(int x, int y, bool proper = false) const {
        if (proper)
            return x > xp && (x < (xp + w - 1)) && y > yp && y < (yp + h - 1);
        return x >= xp && x < (xp + w) && y >= yp && y < (yp + h);
    }

    bool contains(const QRect &rect) const { return intersect(rect) == rect; }
    
    void inflate(int s);

    inline QRect operator&(const QRect &r) const { return intersect(r); }

#ifndef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
    operator NSRect() const;
#endif
    operator CGRect() const;

private:
    int xp;
    int yp;
    int w;
    int h;

    friend bool operator==(const QRect &, const QRect &);
    friend bool operator!=(const QRect &, const QRect &);
};

#endif
