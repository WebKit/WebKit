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

#ifndef KMIMETYPE_H_
#define KMIMETYPE_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <kdecore/kglobal.h>

#include <kdecore/kurl.h>
#include <sys/types.h>
#include <qstring.h>

// class KSharedPtr ============================================================

template<class T> class KSharedPtr {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    KSharedPtr() {}
    KSharedPtr(const KSharedPtr &);
    ~KSharedPtr() {}

    // member functions --------------------------------------------------------

    // operators ---------------------------------------------------------------

    KSharedPtr &operator=(const KSharedPtr &);
    T *operator->();


// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class KSharedPtr =========================================================


// class KMimeType =============================================================

class KMimeType {
public:

    // structs -----------------------------------------------------------------

    // typedefs ----------------------------------------------------------------

    typedef KSharedPtr<KMimeType> Ptr;

    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    static Ptr findByURL(const KURL &, mode_t=0, bool=false, bool=false);
    
    // constructors, copy constructors, and destructors ------------------------

// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    KMimeType() {}
#endif

    virtual ~KMimeType();

    // member functions --------------------------------------------------------

    QString name() const;

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    KMimeType(const KMimeType &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    KMimeType &operator=(const KMimeType &);
#endif

}; // class KMimeType ==========================================================


#endif