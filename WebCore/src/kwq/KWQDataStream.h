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

#ifndef KWQDATASTREAM_H_
#define KWQDATASTREAM_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "qarray.h"
#include "qstring.h"
#include "qcstring.h"

#define IO_ReadOnly             0x0001          // readable device
#define IO_WriteOnly            0x0002          // writable device
#define IO_ReadWrite            0x0003          // read+write device
#define IO_Append               0x0004          // append
#define IO_Truncate             0x0008          // truncate device
#define IO_Translate            0x0010          // translate CR+LF
#define IO_ModeMask             0x00ff

// class QDataStream ===========================================================

class QDataStream {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QDataStream();
    QDataStream(QByteArray, int);
    virtual ~QDataStream();

    // member functions --------------------------------------------------------

    // operators ---------------------------------------------------------------

    QDataStream &operator<<(long int);
    QDataStream &operator<<(const char *);
    QDataStream &operator<<(const QString &);
    QDataStream &operator<<(const QCString &);
    QDataStream &operator>>(const QString &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    // note that these are "standard" (no pendantic stuff needed)
    QDataStream(const QDataStream &);
    QDataStream &operator=(const QDataStream &);

}; // class QDataStream ========================================================

#endif
