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

#include <qevent.h>

// class QEvent ================================================================

QEvent::QEvent(Type)
{
}


QEvent::~QEvent()
{
}


QEvent::Type QEvent::type() const
{
}


// class QMouseEvent ===========================================================

QMouseEvent::QMouseEvent(Type type, const QPoint &pos, int button, int state)
{
}


int QMouseEvent::x()
{
}


int QMouseEvent::y()
{
}


int QMouseEvent::globalX()
{
}


int QMouseEvent::globalY()
{
}


const QPoint &QMouseEvent::pos() const
{
}


Qt::ButtonState QMouseEvent::button()
{
}


Qt::ButtonState QMouseEvent::state()
{
}


Qt::ButtonState QMouseEvent::stateAfter()
{
}


// class QTimerEvent ===========================================================

QTimerEvent::QTimerEvent(int timerId)
{
}


QTimerEvent::~QTimerEvent()
{
}


int QTimerEvent::timerId() const
{
}


// class QKeyEvent =============================================================

QKeyEvent::QKeyEvent(Type, Key, int, int)
{
}


int QKeyEvent::key() const
{
}


Qt::ButtonState QKeyEvent::state() const
{
}


void QKeyEvent::accept()
{
}


void QKeyEvent::ignore()
{
}


// class QFocusEvent ===========================================================

QFocusEvent::QFocusEvent(Type)
{
}


// class QHideEvent ============================================================

QHideEvent::QHideEvent(Type)
{
}


// class QResizeEvent ==========================================================

QResizeEvent::QResizeEvent(Type)
{
}


// class QShowEvent ============================================================

QShowEvent::QShowEvent(Type)
{
}


// class QWheelEvent ===========================================================

QWheelEvent::QWheelEvent(Type)
{
}


void QWheelEvent::accept()
{
}


void QWheelEvent::ignore()
{
}


// class QCustomEvent ===========================================================

QCustomEvent::QCustomEvent(Type)
{
}
