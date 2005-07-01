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

#ifndef QSIZE_H_
#define QSIZE_H_

#include "KWQDef.h"

#ifdef _KWQ_IOSTREAM_
#include <iosfwd>
#endif

typedef struct _NSSize NSSize;
typedef struct CGSize CGSize;

class QSize {
public:
    QSize();
    QSize(int,int);
    explicit QSize(const NSSize &);

    bool isValid() const;
    int width() const { return w; }
    int height() const { return h; }
    void setWidth(int width) { w = width; }
    void setHeight(int height) { h = height; }
    QSize expandedTo(const QSize &) const;
    
    operator NSSize() const;
    operator CGSize() const;

    friend QSize operator+(const QSize &, const QSize &);
    friend bool operator==(const QSize &, const QSize &);
    friend bool operator!=(const QSize &, const QSize &);

#ifdef _KWQ_IOSTREAM_
    friend std::ostream &operator<<(std::ostream &, const QSize &);
#endif

private:
    int w;
    int h;
};

#endif
