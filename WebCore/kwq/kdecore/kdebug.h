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

#ifndef KDEBUG_H_
#define KDEBUG_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qstring.h>

class kdbgstream;

typedef kdbgstream & (*KDBGFUNC)(kdbgstream &);

// class kdbgstream ============================================================

class kdbgstream {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
    kdbgstream(unsigned int area, unsigned int level, bool print=true);
    
// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~kdbgstream() {}
#endif    
    
    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

	kdbgstream &operator<<(int);
	kdbgstream &operator<<(const char *);
	kdbgstream &operator<<(const void *);
	kdbgstream &operator<<(const QString &);
	kdbgstream &operator<<(const QCString &);
	kdbgstream &operator<<(KDBGFUNC);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    kdbgstream(const kdbgstream &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    kdbgstream &operator=(const kdbgstream &);
#endif

}; // class kdbgstream =========================================================

inline kdbgstream &endl(kdbgstream &s) { s << "\n"; return s; }

kdbgstream kdDebug(int area = 0);
kdbgstream kdWarning(int area = 0);
kdbgstream kdWarning(bool cond, int area = 0);
kdbgstream kdError(int area = 0);
kdbgstream kdError(bool cond, int area = 0);
kdbgstream kdFatal(int area = 0);
kdbgstream kdFatal(bool cond, int area = 0);

#endif
