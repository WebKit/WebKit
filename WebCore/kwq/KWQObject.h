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

#ifndef QOBJECT_H_
#define QOBJECT_H_

#include <KWQDef.h>

#include "qnamespace.h"
#include "qstring.h"
#include "qevent.h"
#include "qstringlist.h"
#include "qptrlist.h"

// FIXME: should these macros be in "kwq.h" or other header file?
#define slots
#define SLOT(x) """## x ##"""
#define signals protected
#define SIGNAL(x) """## x ##"""
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

// class QObject ===============================================================

class QObject : public Qt {
public:

    enum Actions {
        // Standard button action, maps to RenderFormElement::slotClicked
        ACTION_BUTTON_CLICKED = 1,
        
        // Checkbox button action, maps to RenderCheckBox::slotStateChanged
        ACTION_CHECKBOX_CLICKED = 2,
        
        // Text field actions, map to RenderLineEdit::slotReturnPressed and
        // RenderLineEdit::slotTextChanged
        ACTION_TEXT_FIELD = 3,  // corresponds to [NSTextField action]
        ACTION_TEXT_FIELD_END_EDITING = 4, // corresponds to NSTextField's delegate textDidEndEditing:

        ACTION_TEXT_AREA_END_EDITING = 5,
        
        ACTION_LISTBOX_CLICKED = 6,
        
        ACTION_COMBOBOX_CLICKED = 7
    };

    static bool connect(const QObject *, const char *, const QObject *, const char *);
    static bool disconnect( const QObject *, const char *, const QObject *, const char *);

    QObject(QObject *parent=0, const char *name=0);
    virtual ~QObject();

    const char *name() const;
    virtual void setName(const char *);

    QVariant property(const char *name) const;
    bool inherits(const char *) const;
    bool connect(const QObject *src, const char *signal, const char *slot) const;

    int startTimer(int);
    void killTimer(int);
    void killTimers();
	virtual void timerEvent( QTimerEvent * );
    
    void installEventFilter(const QObject *);
    void removeEventFilter(const QObject *);
    bool eventFilter(QObject *o, QEvent *e);

    void blockSignals(bool);

    virtual void performAction(QObject::Actions action);
    void emitAction(QObject::Actions action);
    void setTarget (QObject *obj);
    
private:
    // no copying or assignment
    QObject(const QObject &);
    QObject &operator=(const QObject &);

    QObject *target;
    QPtrList<QObject> guardedPtrDummyList;
    
    friend class KWQGuardedPtrBase;
};

#endif
