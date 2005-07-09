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

#ifndef QFONT_H_
#define QFONT_H_

#include "KWQFontFamily.h"

#ifdef __OBJC__
@class NSFont;
#else
class NSFont;
#endif

class QFont {
public:
    enum Weight { Normal = 50, Bold = 63 };
    enum Pitch { Unknown, Fixed, Variable };

    QFont();
    ~QFont();
    
    QFont(const QFont &);
    QFont &operator=(const QFont &);
    
    void setFamily(const QString &);
    QString family() const;

    const KWQFontFamily *firstFamily() const { return &_family; }
    KWQFontFamily *firstFamily() { return &_family; }
    void setFirstFamily(const KWQFontFamily &family);
    
    void setWeight(int);
    int weight() const;
    bool bold() const;

    void setItalic(bool);
    bool italic() const;

    void setPixelSize(float s);
    int pixelSize() const { return (int)_size; }

    bool isFixedPitch() const { if (_pitch == Unknown) determinePitch(); return _pitch == Fixed; };
    void determinePitch() const;
    
    void setPrinterFont(bool);
    bool isPrinterFont() const { return _isPrinterFont; }
    
    bool operator==(const QFont &x) const;
    bool operator!=(const QFont &x) const { return !(*this == x); }
    
    NSString *getNSFamily() const { return _family.getNSFamily(); }
    int getNSTraits() const { return _trait; }
    float getNSSize() const { return _size; }
    
    NSFont *getNSFont() const;

private:
    KWQFontFamily _family;
    int _trait;
    float _size;
    bool _isPrinterFont : 1;
    mutable Pitch _pitch : 2;
    mutable NSFont *_NSFont;
};

#endif
