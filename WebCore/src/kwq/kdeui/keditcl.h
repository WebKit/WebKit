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

#ifndef KEDITCL_H_
#define KEDITCL_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qwidget.h>
#include <KWQFrame.h>
#include <KWQScrollBar.h>
#include <qstring.h>


const uint Tbl_vScrollBar       = 0x00000001;
const uint Tbl_hScrollBar       = 0x00000002;
const uint Tbl_autoVScrollBar   = 0x00000004;
const uint Tbl_autoHScrollBar   = 0x00000008;
const uint Tbl_autoScrollBars   = 0x0000000C;


// class QTableView ============================================================

class QTableView : public QFrame {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QTableView();
    ~QTableView();

    // member functions --------------------------------------------------------

    QScrollBar *verticalScrollBar() const;
    QScrollBar *horizontalScrollBar() const;

    virtual void setTableFlags(uint);
    void clearTableFlags(uint f = ~0);

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QTableView(const QTableView &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QTableView &operator=(const QTableView &);
#endif

}; // class QTableView =========================================================


// class QMultiLineEdit ========================================================

class QMultiLineEdit : public QTableView {
public:

    // structs -----------------------------------------------------------------

    enum WordWrap {
        NoWrap,
        WidgetWidth
    };    

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    // constructors, copy constructors, and destructors ------------------------

    QMultiLineEdit();
    ~QMultiLineEdit();

    // member functions --------------------------------------------------------

    void setWordWrap(WordWrap);
    WordWrap wordWrap() const;
    bool hasMarkedText() const;
    bool isReadOnly() const;
    virtual void setReadOnly(bool);
    virtual void setCursorPosition(int line, int col, bool mark = FALSE);
    void getCursorPosition(int *line, int *col) const;
    virtual void setText(const QString &);
    QString text();
    QString textLine(int line) const;
    int numLines() const;
    void selectAll();

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    // note that these are "standard" (no pendantic stuff needed)
    QMultiLineEdit(const QMultiLineEdit &);
    QMultiLineEdit &operator=(const QMultiLineEdit &);

}; // class QMultiLineEdit =====================================================


// class KEdit =================================================================

class KEdit : public QMultiLineEdit {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    KEdit();
    KEdit(QWidget *);
    ~KEdit();

    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    KEdit(const KEdit &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    KEdit &operator=(const KEdit &);
#endif

}; // class KEdit ==============================================================

#endif