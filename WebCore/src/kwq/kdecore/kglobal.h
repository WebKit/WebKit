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

#ifndef KGLOBAL_H_
#define KGLOBAL_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qstring.h>
#include <kinstance.h>

class KCharsets;
class KLocale;
class KStandardDirs;
class KConfig;
class KWQStaticStringDict;

#define kMin(a, b) ((a) < (b) ? (a) : (b))
#define kMax(a, b) ((a) > (b) ? (a) : (b))

// class KGlobal ===============================================================

class KGlobal {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------

    static KWQStaticStringDict *staticStringDict;

    // static member functions -------------------------------------------------

    static KInstance *instance();
    static KCharsets *charsets();
    static KLocale *locale();
    static KStandardDirs *dirs();
    static KConfig *config();

    static const QString &staticQString(const QString &);

    // constructors, copy constructors, and destructors ------------------------

// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    KGlobal() {}
#endif

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~KGlobal() {}
#endif
        
    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    static KWQStaticStringDict *_stringDict;
    static KInstance *_instance;
    static KLocale *_locale;
    static KCharsets *_charsets;

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    KGlobal(const KGlobal &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    KGlobal &operator=(const KGlobal &);
#endif

}; // class KGlobal ============================================================

#endif
