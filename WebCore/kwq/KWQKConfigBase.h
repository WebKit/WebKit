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

#ifndef KCONFIG_H_
#define KCONFIG_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qobject.h>
#include <qcolor.h>
#include <qfont.h>
#include <qdatetime.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qvariant.h>
#include <qmap.h>
#include <qcolor.h>
#include <KWQStrList.h>

// class KConfigBase ===========================================================

class KConfigBase {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------

    KConfigBase();
    virtual ~KConfigBase();

    // member functions --------------------------------------------------------

    void setGroup(const QString &pGroup);
    
    void writeEntry(const QString &pKey, const QStringList &rValue, 
        char sep=',', bool bPersistent=true, bool bGlobal=false, 
        bool bNLS=false);

    QString readEntry(const char *pKey, 
        const QString& aDefault=QString::null) const;
    
    int readNumEntry(const char *pKey, int nDefault=0) const;
    
    unsigned int readUnsignedNumEntry(const char *pKey, 
        unsigned int nDefault=0) const;
    
    bool readBoolEntry(const char *pKey, bool nDefault=0) const;
    
    QColor readColorEntry(const char *pKey, const QColor *pDefault=0L) const;

    QStringList readListEntry(const QString &pKey, char sep=',') const;

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    KConfigBase(const KConfigBase &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    KConfigBase &operator=(const KConfigBase &);
#endif

}; // class KConfigBase ========================================================


// class KConfig ===============================================================

class KConfig : public KConfigBase {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    KConfig(const QString &n, bool bReadOnly=false, bool bUseKDEGlobals = true);
    virtual ~KConfig();

    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    KConfig(const KConfig &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    KConfig &operator=(const KConfig &);
#endif

}; // class KConfig ============================================================

#endif
