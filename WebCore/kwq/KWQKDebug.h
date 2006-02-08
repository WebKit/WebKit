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

#ifndef KDEBUG_H_
#define KDEBUG_H_

#include "QString.h"
#include "KWQTextStream.h"

#define                k_funcinfo      "[" << __FILE__ << ":" << __LINE__ << "] "

class kdbgstream;

typedef kdbgstream & (*KDBGFUNC)(kdbgstream &);

class kdbgstream {
public:
    kdbgstream(unsigned int area = 0, unsigned int level = 0, bool print = true) { }
    
    kdbgstream &operator<<(int) { return *this; }
    kdbgstream &operator<<(unsigned int) { return *this; }
    kdbgstream &operator<<(long) { return *this; }
    kdbgstream &operator<<(unsigned long) { return *this; }
    kdbgstream &operator<<(double) { return *this; }
    kdbgstream &operator<<(const char *) { return *this; }
    kdbgstream &operator<<(const void *) { return *this; }
    kdbgstream &operator<<(const QChar &) { return *this; }
    kdbgstream &operator<<(const QString &) { return *this; }
    kdbgstream &operator<<(const QCString &) { return *this; }
    kdbgstream &operator<<(KDBGFUNC) { return *this; }
};

inline kdbgstream &endl(kdbgstream &s) { return s; }

inline kdbgstream kdDebug(int area = 0) { return kdbgstream(); }
inline kdbgstream kdWarning(int area = 0) { return kdbgstream(); }
inline kdbgstream kdWarning(bool cond, int area = 0) { return kdbgstream(); }
inline kdbgstream kdError(int area = 0) { return kdbgstream(); }
inline kdbgstream kdError(bool cond, int area = 0) { return kdbgstream(); }
inline kdbgstream kdFatal(int area = 0) { return kdbgstream(); }
inline kdbgstream kdFatal(bool cond, int area = 0) { return kdbgstream(); }
inline QString kdBacktrace() { return QString(); }

#endif
