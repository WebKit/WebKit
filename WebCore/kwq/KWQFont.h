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

#ifndef QFONT_H_
#define QFONT_H_

class QString;

#ifdef __OBJC__
@class NSFont;
@class NSString;
#else
class NSFont;
class NSString;
#endif

class QFontFamily {
public:
    QFontFamily();
    ~QFontFamily() { if (_next) _next->deref();  }
    
    QFontFamily(const QFontFamily& other);    
    QFontFamily& operator=(const QFontFamily& other);
        
    void setFamily(const QString&);
    QString family() const;
    
    NSString* getNSFamily() const { return _family; }

    QFontFamily* next() { return _next; }
    void appendFamily(QFontFamily* family) 
    { 
        if (_next) 
            _next->deref(); 
        _next = family; 
        if (_next)
            _next->ref();
    }
    
    bool operator==(const QFontFamily &compareFontFamily) const;
    bool operator!=(const QFontFamily &x) const { return !(*this == x); }
    
    void ref() { _refCnt++; };
    void deref() { _refCnt--; if (_refCnt == 0) delete this; };
    
private:
    NSString* _family;
    QFontFamily* _next;
    int _refCnt;
};

class QFont {
public:
    enum Weight { Normal = 50, Bold = 63 };

    QFont();
    ~QFont();
    
    void setFamily(const QString &);
    QString family() const;

    QFontFamily* firstFamily() { return &_family; }
    void setFirstFamily(const QFontFamily& family) ;
    
    void setWeight(int);
    int weight() const;
    bool bold() const;

    void setItalic(bool);
    bool italic() const;

    void setPixelSize(float s);
    int pixelSize() const { return (int)_size; }

    bool operator==(const QFont &x) const;
    bool operator!=(const QFont &x) const { return !(*this == x); }
    
    NSString *getNSFamily() const { return _family.getNSFamily(); }
    int getNSTraits() const { return _trait; }
    float getNSSize() const { return _size; }
    
    NSFont *getNSFont() const;
        
private:
    QFontFamily _family;
    int _trait;
    float _size;
    mutable NSFont *_nsfont;
};

// Macro to create a stack array containing non-retained NSString names
// of CSS font families.  This can be used to avoid allocations in
// performance critical code.  Create a NSSString ** name families
// and populates with a NSString * for each family name.  Null terminates
// the array.
#define CREATE_FAMILY_ARRAY(font,families)\
int __numFamilies = 0;\
{\
    QFontFamily *__ff = ((QFont)font).firstFamily();\
    while (__ff)\
    {\
        __numFamilies++;\
            __ff = __ff->next();\
    }\
}\
NSString *families[__numFamilies+1];\
{\
    int __i = 0;\
    QFontFamily *__ff = ((QFont)font).firstFamily();\
    while (__ff)\
    {\
        families[__i++] = __ff->getNSFamily();\
            __ff = __ff->next();\
    }\
    families[__i] = 0;\
}


#endif
