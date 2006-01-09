/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#ifndef QOBJECT_H_
#define QOBJECT_H_

#include "KWQEvent.h"
#include "KWQPtrList.h"
#include "KWQSignal.h"
#include "KWQStringList.h"

#define slots : public
#define SLOT(x) "SLOT:" #x
#define signals protected
#define SIGNAL(x) "SIGNAL:" #x
#define emit
#define Q_OBJECT
#define Q_PROPERTY(text)

class QBitmap;
class QBrush;
class QColor;
class QColorGroup;
class QEvent;
class QFont;
class QFontMetrics;
class QPaintDevice;
class QPaintDeviceMetrics;
class QPainter;
class QPalette;
class QRect;
class QRegion;
class QSize;
class QSizePolicy;
class QTimer;
class QVariant;
class QWidget;

class KWQGuardedPtrBase;
class KWQSignal;

class QObject : public Qt {
public:
    QObject(QObject *parent = 0, const char *name = 0);
    virtual ~QObject();

    static void connect(const QObject *sender, const char *signal, const QObject *receiver, const char *member);
    static void disconnect(const QObject *sender, const char *signal, const QObject *receiver, const char *member);
    void connect(const QObject *sender, const char *signal, const char *member) const
        { connect(sender, signal, this, member); }

    bool inherits(const char *className) const;

    int startTimer(int interval);
    void killTimer(int timerId);
    void killTimers();
    void timerIntervals(int timerId, int& nextFireInterval, int& repeatInterval) const;
    void restartTimer(int timerId, int nextFireInterval, int repeatInterval);
    
    virtual void timerEvent(QTimerEvent *);

    void installEventFilter(const QObject *o) { _eventFilterObject = o; }
    void removeEventFilter(const QObject *) { _eventFilterObject = 0; }
    const QObject *eventFilterObject() const { return _eventFilterObject; }

    virtual bool eventFilter(QObject *object, QEvent *event) { return false; }

    void blockSignals(bool b) { _signalsBlocked = b; }

    virtual bool event(QEvent *);

    static const QObject *sender() { return _sender; }
    
    static bool defersTimers() { return _defersTimers; }
    static void setDefersTimers(bool defers);

    virtual bool isKHTMLLoader() const;
    virtual bool isKHTMLPart() const;
    virtual bool isKHTMLView() const;
    virtual bool isKPartsReadOnlyPart() const;
    virtual bool isQFrame() const;
    virtual bool isQScrollView() const;

private:
    // no copying or assignment
    QObject(const QObject &);
    QObject &operator=(const QObject &);
    
    KWQSignal *findSignal(const char *signalName) const;
    
    QPtrList<QObject> _guardedPtrDummyList;
    
    mutable KWQSignal *_signalListHead;
    bool _signalsBlocked;
    static const QObject *_sender;

    KWQSignal _destroyed;
    
    const QObject *_eventFilterObject;
    
    static bool _defersTimers;

    friend class KWQGuardedPtrBase;
    friend class KWQSignal;
    friend class KWQObjectSenderScope;
};

class KWQObjectSenderScope
{
public:
    KWQObjectSenderScope(const QObject *o) : m_savedSender(QObject::_sender) { QObject::_sender = o; }
    ~KWQObjectSenderScope() { QObject::_sender = m_savedSender; }

private:
    const QObject *m_savedSender;
};

#endif
