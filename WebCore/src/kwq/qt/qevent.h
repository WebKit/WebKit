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

#ifndef QEVENT_H_
#define QEVENT_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "qnamespace.h"
#include "qregion.h"
#include "qpoint.h"

// class QEvent ================================================================

class QEvent : public Qt {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------

    enum Type {
        Timer,
        MouseButtonPress,
        MouseButtonRelease,
        MouseButtonDblClick,
        MouseMove,
        FocusIn,
        FocusOut,
        AccelAvailable,
        KeyPress,
    };

    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    // constructors, copy constructors, and destructors ------------------------
    
// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    QEvent() {}
#endif

    QEvent(Type);
    virtual ~QEvent();

    // member functions --------------------------------------------------------

    Type type() const;

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QEvent(const QEvent &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QEvent &operator=(const QEvent &);
#endif

}; // class QEvent =============================================================


// class QMouseEvent ===========================================================

class QMouseEvent : public QEvent {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    QMouseEvent() {}
#endif

    QMouseEvent(Type type, const QPoint &pos, int button, int state);

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QMouseEvent() {}
#endif

    // member functions --------------------------------------------------------

    int x();
    int y();
    int globalX();
    int globalY();
    const QPoint &pos() const;
    ButtonState button();
    ButtonState state();

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QMouseEvent(const QMouseEvent &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QMouseEvent &operator=(const QMouseEvent &);
#endif

}; // class QMouseEvent ========================================================


// class QTimerEvent ===========================================================

class QTimerEvent : public QEvent {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    QTimerEvent() {}
#endif

    QTimerEvent(int timerId);

    ~QTimerEvent();

    // member functions --------------------------------------------------------

    int timerId() const;

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QTimerEvent(const QTimerEvent &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QTimerEvent &operator=(const QTimerEvent &);
#endif

}; // class QTimerEvent ========================================================


// class QKeyEvent =============================================================

class QKeyEvent : public QEvent {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    // constructors, copy constructors, and destructors ------------------------

// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    QKeyEvent() {}
#endif

    QKeyEvent(Type, Key, int, int);

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QKeyEvent() {}
#endif

    // member functions --------------------------------------------------------

    int key() const;
    ButtonState state() const;
    void accept();

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QKeyEvent(const QKeyEvent &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QKeyEvent &operator=(const QKeyEvent &);
#endif

}; // class QKeyEvent ==========================================================


// class QFocusEvent ===========================================================

class QFocusEvent : public QEvent {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    QFocusEvent() {}
#endif

    QFocusEvent(Type);

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QFocusEvent() {}
#endif

    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QFocusEvent(const QFocusEvent &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QFocusEvent &operator=(const QFocusEvent &);
#endif

}; // class QFocusEvent ========================================================


// class QHideEvent ============================================================

class QHideEvent : public QEvent {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    QHideEvent() {}
#endif

    QHideEvent(Type);

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QHideEvent() {}
#endif

    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QHideEvent(const QHideEvent &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QHideEvent &operator=(const QHideEvent &);
#endif

}; // class QHideEvent =========================================================


// class QResizeEvent ==========================================================

class QResizeEvent : public QEvent {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    QResizeEvent() {}
#endif

    QResizeEvent(Type);
    
// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QResizeEvent() {}
#endif
    
    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QResizeEvent(const QResizeEvent &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QResizeEvent &operator=(const QResizeEvent &);
#endif

}; // class QResizeEvent =======================================================


// class QShowEvent ============================================================

class QShowEvent : public QEvent {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    QShowEvent() {}
#endif

    QShowEvent(Type);

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QShowEvent() {}
#endif

    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QShowEvent(const QShowEvent &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QShowEvent &operator=(const QShowEvent &);
#endif

}; // class QShowEvent =========================================================


// class QWheelEvent ===========================================================

class QWheelEvent : public QEvent {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    QWheelEvent() {}
#endif

    QWheelEvent(Type);

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QWheelEvent() {}
#endif
    
    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QWheelEvent(const QWheelEvent &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QWheelEvent &operator=(const QWheelEvent &);
#endif

}; // class QWheelEvent ========================================================

#endif
