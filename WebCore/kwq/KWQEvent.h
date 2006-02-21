/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "KWQNamespace.h"
#include "IntPoint.h"
#include "QString.h"

#ifdef __OBJC__
@class NSEvent;
#else
class NSEvent;
#endif

class QEvent {
public:

    enum Type {
        MouseButtonPress,
        MouseButtonRelease,
        MouseButtonDblClick,
        MouseMove,
        FocusIn,
        FocusOut,
        KeyPress,
        KeyRelease,
        Wheel
    };

    QEvent(Type type) : _type(type) { }
    virtual ~QEvent();

    Type type() const { return _type; }

private:
    Type  _type;
};

class QMouseEvent : public QEvent {
public:
    QMouseEvent(Type, NSEvent *);
    explicit QMouseEvent(Type); // uses AppKit's current event

    const IntPoint &pos() const { return _position; }
    int x() const { return _position.x(); }
    int y() const { return _position.y(); }
    int globalX() const { return _globalPosition.x(); }
    int globalY() const { return _globalPosition.y(); }
    Qt::ButtonState button() const { return static_cast<Qt::ButtonState>(_button); }
    Qt::ButtonState state() const { return static_cast<Qt::ButtonState>(_state); }
    Qt::ButtonState stateAfter() const { return static_cast<Qt::ButtonState>(_stateAfter); }

    int clickCount() const { return _clickCount; }

private:
    void fixState();

    IntPoint _position;
    IntPoint _globalPosition;
    int _button;
    int _state;
    int _stateAfter;
    int _clickCount;
};

class QKeyEvent : public QEvent {
public:
    QKeyEvent(NSEvent *, bool forceAutoRepeat = false);

    Qt::ButtonState state() const { return static_cast<Qt::ButtonState>(_state); }
    bool isAccepted() const { return _isAccepted; }
    QString text() const { return _text; }
    bool isAutoRepeat() const { return _autoRepeat; }
    void accept() { _isAccepted = true; }
    void ignore() { _isAccepted = false; }

    int WindowsKeyCode() const { return _WindowsKeyCode; }
    QString unmodifiedText() const { return _unmodifiedText; }
    QString keyIdentifier() const { return _keyIdentifier; }

private:
    int _state;
    QString _text;
    QString _unmodifiedText;
    QString _keyIdentifier;
    bool _autoRepeat;
    bool _isAccepted;
    int _WindowsKeyCode;
};

class QWheelEvent : public QEvent {
public:
    QWheelEvent(const IntPoint& position, const IntPoint& globalPosition, int delta, int state, Qt::Orientation orientation)
        : QEvent(Wheel), _position(position), _globalPosition(globalPosition), _delta(delta), _state(state)
        , _orientation(orientation), _isAccepted(false)
        { }
    QWheelEvent(NSEvent *);

    const IntPoint &pos() const { return _position; }
    const IntPoint &globalPos() const { return _globalPosition; }
    int delta() const { return _delta; }
    int state() const { return _state; }
    Qt::Orientation orientation() const { return _orientation; }
    bool isAccepted() const { return _isAccepted; }

    int x() const { return _position.x(); }
    int y() const { return _position.y(); }
    int globalX() const { return _globalPosition.x(); }
    int globalY() const { return _globalPosition.y(); }

    void accept() { _isAccepted = true; }
    void ignore() { _isAccepted = false; }

private:
    IntPoint _position;
    IntPoint _globalPosition;
    int _delta;
    int _state;
    Qt::Orientation _orientation;
    bool _isAccepted;
};

#endif
