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

#ifndef BROWSEREXTENSION_H_
#define BROWSEREXTENSION_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qpoint.h>
#include <qevent.h>
#include <KWQDataStream.h>

#include <kurl.h>

#include "part.h"
#include "browserinterface.h"

// class KXMLGUIClient =========================================================

class KXMLGUIClient {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
    KXMLGUIClient();
    ~KXMLGUIClient();
    
    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    KXMLGUIClient(const KXMLGUIClient &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    KXMLGUIClient &operator=(const KXMLGUIClient &);
#endif

}; // class KXMLGUIClient ======================================================


namespace KParts {

// struct URLArgs ==============================================================

struct URLArgs {

    QString frameName;
    QString serviceType;
    
    URLArgs();
    URLArgs( const URLArgs &);
    URLArgs &operator=(const URLArgs &);    
    virtual ~URLArgs();

}; // struct URLArgs ===========================================================


// struct WindowArgs ===========================================================

struct WindowArgs {

    int x;
    int y;
    int width;
    int height;
    bool menuBarVisible;
    bool statusBarVisible;
    bool toolBarsVisible;
    bool resizable;
    bool fullscreen;

    WindowArgs();
    WindowArgs(const WindowArgs &);
    WindowArgs &operator=(const WindowArgs &);

}; // struct WindowArgs ========================================================


// class BrowserExtension ======================================================

class BrowserExtension : public QObject {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    BrowserExtension() {}
#endif

    virtual ~BrowserExtension();
    
    // member functions --------------------------------------------------------

     BrowserInterface *browserInterface() const;
     
     void openURLRequest(const KURL &, 
        const KParts::URLArgs &args=KParts::URLArgs());
     
     void createNewWindow(const KURL &);
     
     void createNewWindow(const KURL &, const KParts::URLArgs &, 
        const KParts::WindowArgs &, KParts::ReadOnlyPart *&);

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    BrowserExtension(const BrowserExtension &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    BrowserExtension &operator=(const BrowserExtension &);
#endif

}; // class BrowserExtension ===================================================


// class BrowserHostExtension ==================================================

class BrowserHostExtension : public QObject {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
    BrowserHostExtension();
    virtual ~BrowserHostExtension();
    
    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    BrowserHostExtension(const BrowserHostExtension &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    BrowserHostExtension &operator=(const BrowserHostExtension &);
#endif

}; // class BrowserHostExtension ===============================================

} // namespace KParts

#endif


