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

#ifndef PART_H_
#define PART_H_

#include <kinstance.h>
#include <kurl.h>
#include <qobject.h>
#include <qvariant.h>
#include <qlist.h>
#include <qstringlist.h>
#include <qvaluelist.h>

class QWidget;

namespace KIO {
    class Job;
}

namespace KParts {

class Part : public QObject {
public:
    Part() : m_widget(0), m_ref(1) { }
    
    QWidget *widget() const { return m_widget; }
    void setWidget(QWidget *widget) { m_widget = widget; }
    
    void ref() { m_ref++; }
    void deref() { if(m_ref) m_ref--; if (!m_ref) delete this; }
    
    void event(QEvent *event) { customEvent((QCustomEvent *)event); }
    virtual void customEvent(QCustomEvent *) { }
    
private:
    QWidget *m_widget;
    unsigned int m_ref;
};

class ReadOnlyPart : public Part {
public:
    ReadOnlyPart(QObject * = 0, const char * = 0) { }
    
    KURL url() const { return m_url; }
    KURL m_url;
    
    void setXMLFile(const char *) { }
    QObject *parent() const { return 0; }
    void setInstance(KInstance *, bool) { }
    void setStatusBarText(const QString &) { }
};

class GUIActivateEvent { };

} // namespace KParts

#endif
