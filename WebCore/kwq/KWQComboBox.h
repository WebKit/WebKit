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

#ifndef QCOMBOBOX_H_
#define QCOMBOBOX_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <KWQListBox.h>
#include "qwidget.h"

#if (defined(__APPLE__) && defined(__OBJC__) && defined(__cplusplus))
#define Fixed MacFixed
#define Rect MacRect
#define Boolean MacBoolean
#import <Cocoa/Cocoa.h>
#undef Fixed
#undef Rect
#undef Boolean
#endif

// class QComboBox =============================================================

class QComboBox : public QWidget {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QComboBox(QWidget *parent=0, const char *name=0);
    QComboBox(bool rw, QWidget *parent=0, const char *name=0);
    ~QComboBox();
     
    // member functions --------------------------------------------------------

    int count() const;
    QListBox *listBox() const;
    void popup();
    bool eventFilter(QObject *object, QEvent *event);
    void insertItem(const QString &text, int index=-1);
    void clear();
#ifdef _KWQ_
    int indexOfCurrentItem();
#endif
    virtual void setCurrentItem(int);
    QSize sizeHint() const;

#if (defined(__APPLE__) && defined(__OBJC__) && defined(__cplusplus))
    NSMutableArray *items;
#else
    void *items;
#endif
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------

// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    // note that these are "standard" (no pendantic stuff needed)
    QComboBox(const QComboBox &);
    QComboBox &operator=(const QComboBox &);

#ifdef _KWQ_
    void init(bool isEditable);

#endif

}; // class QComboBox ==========================================================

#endif
