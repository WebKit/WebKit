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

#ifndef QLINEEDIT_H_
#define QLINEEDIT_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "qwidget.h"
#include "qevent.h"
#include "qstring.h"

// class QLineEdit =============================================================

class QLineEdit : public QWidget {
public:

    // typedefs ----------------------------------------------------------------

    // enums -------------------------------------------------------------------

    enum EchoMode { Normal, NoEcho, Password };

    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    // constructors, copy constructors, and destructors ------------------------

    QLineEdit(QWidget *parent=0, const char *name=0);

    virtual ~QLineEdit();
    
    // member functions --------------------------------------------------------

    virtual void setEchoMode(EchoMode);
    virtual void setCursorPosition(int);
    virtual void setText(const QString &);
    virtual QString text();
    virtual void setMaxLength(int);

    bool isReadOnly() const;
    void setReadOnly(bool);
    bool event(QEvent *);
    bool frame() const;
    int cursorPosition() const;
    int maxLength() const;
    void selectAll();

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    // note that these are "standard" (no pendantic stuff needed)
    QLineEdit(const QLineEdit &);
    QLineEdit &operator=(const QLineEdit &);

}; // class QLineEdit ==========================================================

#endif
