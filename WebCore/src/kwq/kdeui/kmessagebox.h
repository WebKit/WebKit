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

#ifndef KMESSAGEBOX_H_
#define KMESSAGEBOX_H_

#include <qwidget.h>
#include <qstring.h>

// class KMessageBox ===========================================================

class KMessageBox {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------

    // enums -------------------------------------------------------------------

    enum { Ok = 1, Cancel = 2, Yes = 3, No = 4, Continue = 5 };

    // constants ---------------------------------------------------------------

    // static member functions -------------------------------------------------

    static void error(QWidget *, const QString &, 
        const QString &caption=QString::null, bool notify=true);
   
    static int warningYesNo(QWidget *, const QString &, 
        const QString &caption=QString::null, 
        const QString &buttonYes=QString::null, 
        const QString &buttonNo=QString::null, 
        bool notify=true);
   
    static int questionYesNo(QWidget *, const QString &, 
        const QString &caption=QString::null, 
        const QString &buttonYes=QString::null, 
        const QString &buttonNo=QString::null, bool notify=true);
   
    static void sorry(QWidget *, const QString &, 
        const QString &caption=QString::null, bool notify=true);

    // constructors, copy constructors, and destructors ------------------------
    
// add no-op constructor
#ifdef _KWQ_PEDANTIC_
    KMessageBox() {}
#endif

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~KMessageBox() {}
#endif
        
    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    KMessageBox(const KMessageBox &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    KMessageBox &operator=(const KMessageBox &);
#endif

}; // class KMessageBox ========================================================

#endif
