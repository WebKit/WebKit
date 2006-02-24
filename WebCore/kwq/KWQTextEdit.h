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

#ifndef QTEXTEDIT_H_
#define QTEXTEDIT_H_

#include "KWQSignal.h"
#include "KWQPainter.h"
#include "ScrollView.h"

class QTextEdit : public WebCore::ScrollView {
 public:
    typedef enum { 
        NoWrap,
        WidgetWidth
    } WrapStyle;

    typedef enum {
        PlainText,
    } TextFormat;

    QTextEdit(Widget *parent);
    ~QTextEdit();

    void setColors(const Color& background, const Color& foreground);

    void setAlignment(AlignmentFlags);
    void setLineHeight(int lineHeight);

    void setCursorPosition(int, int);
    void getCursorPosition(int *, int *) const;

    void setFont(const QFont &);

    void setReadOnly(bool);
    bool isReadOnly() const;

    void setDisabled(bool);
    bool isDisabled() const;

    bool hasSelectedText() const;
    
    void setText(const DOM::DOMString&);
    DOM::DOMString text() const;
    DOM::DOMString textWithHardLineBreaks() const;

    void setTextFormat(TextFormat) { }

    void setWordWrap(WrapStyle);
    WrapStyle wordWrap() const;

    void setScrollBarModes(WebCore::ScrollBarMode hMode, WebCore::ScrollBarMode vMode);

    void setWritingDirection(QPainter::TextDirection);

    int selectionStart();
    int selectionEnd();
    void setSelectionStart(int);
    void setSelectionEnd(int);
    
    void selectAll();
    void setSelectionRange(int, int);

    IntSize sizeWithColumnsAndRows(int numColumns, int numRows) const;

    void textChanged() { _textChanged.call(); }

    void selectionChanged() { _selectionChanged.call(); }
    void clicked();

    virtual FocusPolicy focusPolicy() const;
    virtual bool checksDescendantsForFocus() const;

  private:
    KWQSignal _clicked;
    KWQSignal _textChanged;
    KWQSignal _selectionChanged;
};

#endif /* QTEXTEDIT_H_ */
