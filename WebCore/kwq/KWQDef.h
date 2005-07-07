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

#ifndef KWQDEF_H_
#define KWQDEF_H_

#include <sys/types.h>
#ifndef KWQ_UNSIGNED_TYPES_DEFINED
#define KWQ_UNSIGNED_TYPES_DEFINED
typedef unsigned char uchar;
typedef unsigned long ulong;
#endif

typedef int64_t Q_INT64;
typedef u_int64_t Q_UINT64;

typedef int Q_INT32;
typedef unsigned int Q_UINT32;

typedef short Q_INT16;
typedef unsigned short Q_UINT16;

template<class T>
inline const T& kMin ( const T& a, const T& b ) { return a < b ? a : b; }

template<class T>
inline const T& kMax ( const T& a, const T& b ) { return b < a ? a : b; }

#define QABS(a) (((a) >= 0) ? (a) : -(a))

inline int qRound(double a) { return lround(a); }

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

void qDebug(const char *msg, ...);
void qWarning(const char *msg, ...);

/* Silly hack to avoid "unused parameter" warnings */
#define Q_UNUSED(x) (void)(x)

#define Q_ASSERT(arg) do {} while(0)

#ifndef NDEBUG
#define _KWQ_IOSTREAM_
#endif

#endif
