/*
 * Copyright (C) 2004-6 Apple Computer, Inc.  All rights reserved.
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

#ifndef INTPOINTARRAY_H_
#define INTPOINTARRAY_H_

#include "KWQMemArray.h"
#include "IntPoint.h"

class QRect;

namespace WebCore {

class IntPointArray : public QMemArray<IntPoint> {
public:
    IntPointArray() { }
    IntPointArray(int size) : QMemArray<IntPoint>(size) { }
    IntPointArray(const QRect &rect);
    IntPointArray(int, const int *);
    
    QRect boundingRect() const;
    
    IntPointArray copy() const;
    
    void point(uint, int *, int *);
    void setPoint(uint, int, int);
    bool setPoints(int, int, int, int, int, int, int, int, int);
    bool setPoints(int nPoints, const int *points);    
};

}

// FIXME: Remove when everything is in the WebCore namespace.
using WebCore::IntPointArray;

#endif
