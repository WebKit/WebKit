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

#ifndef KWQDEF_H_
#define KWQDEF_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned uint;
typedef unsigned long ulong;

typedef char Q_INT8;
typedef short Q_INT16;  
typedef int Q_INT32;  

typedef unsigned char Q_UINT8;
typedef unsigned short Q_UINT16;
typedef unsigned int Q_UINT32;  

typedef Q_INT32 QCOORD;

typedef uint WFlags;
typedef int WId;

#define QMAX(a,b) ((a) > (b) ? (a) : (b))
#define QMIN(a,b) ((a) < (b) ? (a) : (b))

#define KMAX(a,b) QMAX(a, b)
#define KMIN(a,b) QMIN(a, b)

#define QABS(a) (((a) >= 0) ? (a) : -(a))

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#define ASSERT(a)

void qDebug(const char *msg, ...);
void qWarning(const char *msg, ...);

#ifdef NEED_BOGUS_X_DEFINES
typedef int XEvent;
#endif

/* FIXME: Let's not worrying about malloc/new failing for now.... */
#define CHECK_PTR(p)

/* Silly hack to avoid "unused parameter" warnings */
#define Q_UNUSED(x) x=x;

/* We don't handle this, it's needed in QT for wacky hacking Win32 DLLs */
#define Q_EXPORT

/* unnecessary for our platform */
#define Q_PACKED

/* Qt const defines */
#define QT_STATIC_CONST static const
#define QT_STATIC_CONST_IMPL const

#define Q_ASSERT(arg) do {} while(0)
#endif
