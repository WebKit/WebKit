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

#ifndef QFILE_H_
#define QFILE_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <KWQDef.h>

// USING_BORROWED_QFILE =======================================================

#ifdef USING_BORROWED_QFILE

#include <_qfile.h>

#else // !USING_BORROWED_QFILE

#include <qstring.h>
#include <qiodevice.h>

class QFile 
{
public:
    QFile();
    QFile(const QString &);
    ~QFile();

    bool exists() const;
    bool open(int);
    void close();
    int readBlock(char *, uint);
    uint size() const;

    static bool exists(const QString &path);

private:
    // no copying or assignment
    // note that these are "standard" (no pendantic stuff needed)
    QFile(const QFile &);
    QFile &operator=(const QFile &);

    class KWQFilePrivate;

    KWQFilePrivate *d;
};


#endif // USING_BORROWED_QFILE

#endif // QFILE_H_
