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
    
    QEvent(Type);
    
    virtual ~QEvent();

    // member functions --------------------------------------------------------

    Type type() const;

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    QEvent(const QEvent &);
    QEvent &operator=(const QEvent &);

}; // class QEvent =============================================================


// class QMouseEvent ===========================================================

class QMouseEvent : public QEvent {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QMouseEvent(Type type, const QPoint &pos, int button, int state);

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
    // no copying or assignment
    QMouseEvent(const QMouseEvent &);
    QMouseEvent &operator=(const QMouseEvent &);

}; // class QMouseEvent ========================================================


// class QTimerEvent ===========================================================

class QTimerEvent : public QEvent {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
    QTimerEvent(int timerId);

    // member functions --------------------------------------------------------

    int timerId() const;

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    QTimerEvent(const QTimerEvent &);
    QTimerEvent &operator=(const QTimerEvent &);

}; // class QTimerEvent ========================================================


// class QKeyEvent =============================================================

class QKeyEvent : public QEvent {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    // constructors, copy constructors, and destructors ------------------------

    QKeyEvent();
    QKeyEvent(Type, Key, int, int);

    // member functions --------------------------------------------------------

    int key() const;
    ButtonState state() const;
    void accept();

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    QKeyEvent(const QKeyEvent &);
    QKeyEvent &operator=(const QKeyEvent &);

}; // class QKeyEvent ==========================================================


// class QFocusEvent ===========================================================

class QFocusEvent : public QEvent {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QFocusEvent();

    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    QFocusEvent(const QFocusEvent &);
    QFocusEvent &operator=(const QFocusEvent &);

}; // class QFocusEvent ========================================================


// class QHideEvent ============================================================

class QHideEvent : public QEvent {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
    QHideEvent();
    
    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    QHideEvent(const QHideEvent &);
    QHideEvent &operator=(const QHideEvent &);

}; // class QHideEvent =========================================================


// class QResizeEvent ==========================================================

class QResizeEvent : public QEvent {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
    QResizeEvent();
    
    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    QResizeEvent(const QResizeEvent &);
    QResizeEvent &operator=(const QResizeEvent &);

}; // class QResizeEvent =======================================================


// class QShowEvent ============================================================

class QShowEvent : public QEvent {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
    QShowEvent();
    
    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    QShowEvent(const QShowEvent &);
    QShowEvent &operator=(const QShowEvent &);

}; // class QShowEvent =========================================================


// class QWheelEvent ===========================================================

class QWheelEvent : public QEvent {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
    QWheelEvent();
    
    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    QWheelEvent(const QWheelEvent &);
    QWheelEvent &operator=(const QWheelEvent &);

}; // class QWheelEvent ========================================================

#endif
