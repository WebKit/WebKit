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

#ifndef QSTRINGLIST_H_
#define QSTRINGLIST_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "qstring.h"
#include "qvaluelist.h"

// class QStringList ===========================================================

class QStringList : public QValueList<QString> {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------

    // static member functions -------------------------------------------------

    static QStringList split(const QString &, const QString &, 
        bool allowEmptyEntries = FALSE );
    static QStringList split(const QChar &, const QString &, 
        bool allowEmptyEntries = FALSE );

    // constructors, copy constructors, and destructors ------------------------
    
    QStringList();
    QStringList(const QStringList &);

    ~QStringList();

    // member functions --------------------------------------------------------

    QString join(const QString &) const;

    // operators ---------------------------------------------------------------

    QStringList &operator=(const QStringList &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QStringList ========================================================

#endif
