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

#include <qnamespace.h>
#include <qregion.h>
#include <qpoint.h>
#include <qstring.h>

class QEvent : public Qt {
public:

    enum Type {
	None,
	Enter,
	Leave,
        Timer,
        MouseButtonPress,
        MouseButtonRelease,
        MouseButtonDblClick,
        MouseMove,
        FocusIn,
        FocusOut,
        AccelAvailable,
        KeyPress,
	KeyRelease,
        Paint,
    };

    QEvent( Type t ) : _type(t) {}
    virtual ~QEvent();

    Type type() const;

private:
    QEvent(const QEvent &);
    QEvent &operator=(const QEvent &);

    Type  _type;
};

class QMouseEvent : public QEvent {
public:
    QMouseEvent(Type type, const QPoint &pos, int button, int state);
    QMouseEvent(Type type, const QPoint &pos, const QPoint &global, int button, int state);

    int x();
    int y();
    int globalX();
    int globalY();
    const QPoint &pos() const;
    ButtonState button();
    ButtonState state();
    ButtonState stateAfter();

private:
    QPoint _position;
    ButtonState _button;
    ButtonState _state;

};

class QTimerEvent : public QEvent {
public:
    QTimerEvent(int timerId);
    ~QTimerEvent();

    int timerId() const { return _timerId; }

private:
    int _timerId;
};

class QKeyEvent : public QEvent {
public:
    QKeyEvent(Type type, int key, int ascii, int buttonState, const QString &textVal = QString::null, bool autoRepeat = FALSE, ushort countVal = 1);

    int key() const;
    ButtonState state() const;
    void accept();
    void ignore();
    bool isAutoRepeat() const;
    bool isAccepted() const;
    int count()  const;
    QString text() const;
    int ascii() const;
};

class QFocusEvent : public QEvent {
public:
    enum Reason { Mouse, Tab, ActiveWindow, Popup, Shortcut, Other };
    
    QFocusEvent(Type);

    static Reason reason();
};

class QHideEvent : public QEvent {
public:
    QHideEvent(Type);
};

class QResizeEvent : public QEvent {
public:
    QResizeEvent(Type);
};

class QShowEvent : public QEvent {
public:
    QShowEvent(Type);
};

class QWheelEvent : public QEvent {
public:
    QWheelEvent(Type);

    void accept();
    void ignore();
};

class QCustomEvent : public QEvent {
public:
    QCustomEvent( int type );
    QCustomEvent( Type type, void *data )
	: QEvent(type), d(data) {};

    void       *data()	const	{ return d; }
    void	setData( void* data )	{ d = data; }

private:
    void       *d;
};

class QContextMenuEvent : public QEvent
{
 public:
    QContextMenuEvent(int, const QPoint &, const QPoint &, Qt::ButtonState);

    int x() const;
    int y() const;
    ButtonState state() const;
    int reason() const;
    QPoint globalPos() const;

 private:
    int m_reason;
    QPoint m_pos;
    QPoint m_globalPos;
    ButtonState m_state;
};

#endif
