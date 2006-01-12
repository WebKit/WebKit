/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#ifndef ObjectContents_H
#define ObjectContents_H

#include "KWQKURL.h"
#include "KWQObject.h"
#include "KWQTimer.h"

class ObjectContents : public QObject {
public:
    ObjectContents()
        : _parent(0), _name(0), _widget(0), _ref(1) { }
    
    KURL url() const { return m_url; }
    
    void setParent(QObject *parent) { _parent = parent; }
    QObject *parent() const { return _parent; }

    virtual void setName(const QString &name) { _name = name; }
    QString name() { return _name; }
    
    virtual bool openURL(const KURL &) = 0;
    virtual bool closeURL() = 0;

    QWidget *widget() const { return _widget; }
    void setWidget(QWidget *widget) { _widget = widget; }
    
    void ref() { ++_ref; }
    void deref() { if (!--_ref) delete this; }
    
    bool event(QEvent *event) { customEvent(event); return true; }
    virtual void customEvent(QCustomEvent *) { }

protected:
    KURL m_url;

private:
    virtual bool isObjectContents() const { return true; }
    QObject *_parent;
    QString _name;
    QWidget *_widget;
    unsigned int _ref;
};


#endif
