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

#ifndef KWQLISTBOX_H_
#define KWQLISTBOX_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qscrollview.h>
#include <qstring.h>

class QListBoxItem;
class QListBoxText;

// class QListBox ==============================================================

class QListBox : public QScrollView {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------

    // enums -------------------------------------------------------------------

    enum SelectionMode { Single, Multi, Extended, NoSelection };

    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------

    QListBox();
    ~QListBox();

    // member functions --------------------------------------------------------

    uint count() const;
    void clear();
    virtual void setSelectionMode(SelectionMode);
    QListBoxItem *firstItem() const;
    int currentItem() const;
    void insertItem(const QString &, int index=-1);
    void insertItem(const QListBoxItem *, int index=-1);
    void setSelected(int, bool);
    bool isSelected(int);

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    // note that these are "standard" (no pendantic stuff needed)
    QListBox(const QListBox &);
    QListBox &operator=(const QListBox &);

}; // class QListBox ===========================================================


// class QListBoxItem ==========================================================

class QListBoxItem {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QListBoxItem();
    virtual ~QListBoxItem();

    // member functions --------------------------------------------------------

    void setSelectable(bool);
    QListBox *listBox() const;
    virtual int width(const QListBox *) const;
    virtual int height(const QListBox *) const;
    QListBoxItem *next() const;
    QListBoxItem *prev() const;

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    // note that these are "standard" (no pendantic stuff needed)
    QListBoxItem(const QListBoxItem &);
    QListBoxItem &operator=(const QListBoxItem &);

}; // class QListBoxItem =======================================================


// class QListBoxText ==========================================================

class QListBoxText : public QListBoxItem {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QListBoxText(const QString &text=QString::null);
    ~QListBoxText();

    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    // note that these are "standard" (no pendantic stuff needed)
    QListBoxText(const QListBoxText &);
    QListBoxText &operator=(const QListBoxText &);

}; // class QListBoxText =======================================================

#endif
