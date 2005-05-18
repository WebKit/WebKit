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

#include "KWQDef.h"
#include "KWQSignal.h"

#include "KWQNamespace.h"
#include "KWQString.h"
#include "KWQEvent.h"
#include "KWQStringList.h"
#include "KWQPtrList.h"

#define slots : public
#define SLOT(x) "SLOT:" #x
#define signals protected
#define SIGNAL(x) "SIGNAL:" #x
#define emit
#define Q_OBJECT
#define Q_PROPERTY(text)

class QEvent;
class QPaintDevice;
class QPaintDeviceMetrics;
class QWidget;
class QColor;
class QColorGroup;
class QPalette;
class QPainter;
class QRegion;
class QSize;
class QSizePolicy;
class QRect;
class QFont;
class QFontMetrics;
class QBrush;
class QBitmap;
class QMovie;
class QTimer;
class QImage;
class QVariant;

class KWQGuardedPtrBase;
class KWQSignal;

#ifdef __OBJC__
@class NSTimer;
#else
class NSTimer;
#endif

class QObject : public Qt {
public:
    QObject(QObject *parent = 0, const char *name = 0);
    virtual ~QObject();

    static void connect(const QObject *sender, const char *signal, const QObject *receiver, const char *member);
    static void disconnect(const QObject *sender, const char *signal, const QObject *receiver, const char *member);
    void connect(const QObject *sender, const char *signal, const char *member) const
        { connect(sender, signal, this, member); }

    bool inherits(const char *className) const;

    int startTimer(int);
    void killTimer(int);
    void killTimers();
    void pauseTimer(int _timerId, const void *key);
    void resumeTimers(const void *key, QObject *target);
    static void clearPausedTimers (const void *key);
    
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
    void _addTimer(NSTimer *timer, int _timerId);

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
    KWQObjectSenderScope(const QObject *);
    ~KWQObjectSenderScope();

private:
    const QObject *_savedSender;
};

#endif
