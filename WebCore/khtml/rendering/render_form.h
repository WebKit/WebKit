/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id$
 */
#ifndef RENDER_FORM_H
#define RENDER_FORM_H

#include "render_replaced.h"
#include "render_image.h"
#include "render_flow.h"

class QWidget;
class QScrollView;
class QLineEdit;
class QListboxItem;

#include <keditcl.h>
#include <klineedit.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qpushbutton.h>
#include <qhbox.h>
#include <klistbox.h>
#include <kcombobox.h>
#include "dom/dom_misc.h"

class KHTMLPartBrowserExtension;

namespace DOM {
    class HTMLFormElementImpl;
    class HTMLInputElementImpl;
    class HTMLSelectElementImpl;
    class HTMLGenericFormElementImpl;
    class HTMLTextAreaElementImpl;
};

namespace khtml {

class DocLoader;

// -------------------------------------------------------------------------

class RenderFormElement : public khtml::RenderWidget
{
    Q_OBJECT
public:
    RenderFormElement(QScrollView *view, DOM::HTMLGenericFormElementImpl *element);
    virtual ~RenderFormElement();

    virtual const char *renderName() const { return "RenderForm"; }

    virtual bool isRendered() const  { return true; }
    virtual bool isFormElement() const { return true; }

    // IE does not scale according to intrinsicWidth/Height
    // aspect ratio :-(
    virtual short calcReplacedWidth(bool* ieHack=0) const;
    virtual int   calcReplacedHeight() const;

    virtual void layout();
    virtual short baselinePosition( bool ) const;

    DOM::HTMLGenericFormElementImpl *element() { return m_element; }

    virtual bool eventFilter(QObject*, QEvent*);

    void performAction(QObject::Actions action);

public slots:
    virtual void slotClicked();

protected:
    virtual bool isRenderButton() const { return false; }
    virtual bool isEditable() const { return false; }

    void handleMousePressed(QMouseEvent* e);

    DOM::HTMLGenericFormElementImpl *m_element;
    QPoint m_mousePos;
    int m_state;
    int m_button;
    int m_clickCount;
    bool m_isDoubleClick;
};


// -------------------------------------------------------------------------

// generic class for all buttons
class RenderButton : public RenderFormElement
{
    Q_OBJECT
public:
    RenderButton(QScrollView *view, DOM::HTMLGenericFormElementImpl *element);

    virtual const char *renderName() const { return "RenderButton"; }
    virtual short baselinePosition( bool ) const;

protected:
    virtual bool isRenderButton() const { return true; }
};

// -------------------------------------------------------------------------

class RenderCheckBox : public RenderButton
{
    Q_OBJECT
public:
    RenderCheckBox(QScrollView *view, DOM::HTMLInputElementImpl *element);

    virtual const char *renderName() const { return "RenderCheckBox"; }
    virtual void calcMinMaxWidth();
    virtual void layout( );
    
public slots:
    virtual void slotStateChanged(int state);
};

// -------------------------------------------------------------------------

class RenderRadioButton : public RenderButton
{
    Q_OBJECT
public:
    RenderRadioButton(QScrollView *view, DOM::HTMLInputElementImpl *element);

    virtual const char *renderName() const { return "RenderRadioButton"; }

    virtual void setChecked(bool);

    virtual void calcMinMaxWidth();
    virtual void layout();

public slots:
    void slotClicked();
};

// -------------------------------------------------------------------------

class RenderSubmitButton : public RenderButton
{
public:
    RenderSubmitButton(QScrollView *view, DOM::HTMLInputElementImpl *element);

    virtual const char *renderName() const { return "RenderSubmitButton"; }

    virtual QString defaultLabel();

    virtual void calcMinMaxWidth();
    virtual short baselinePosition( bool ) const;

};

// -------------------------------------------------------------------------

class RenderImageButton : public RenderImage
{
public:
    RenderImageButton(DOM::HTMLInputElementImpl *element);

    virtual const char *renderName() const { return "RenderImageButton"; }

    DOM::HTMLInputElementImpl *m_element;
};


// -------------------------------------------------------------------------

class RenderResetButton : public RenderSubmitButton
{
public:
    RenderResetButton(QScrollView *view, DOM::HTMLInputElementImpl *element);

    virtual const char *renderName() const { return "RenderResetButton"; }

    virtual QString defaultLabel();
};

// -------------------------------------------------------------------------

class RenderPushButton : public RenderSubmitButton
{
public:
    RenderPushButton(QScrollView *view, DOM::HTMLInputElementImpl *element);

    virtual QString defaultLabel();
};

// -------------------------------------------------------------------------

class RenderLineEdit : public RenderFormElement
{
    Q_OBJECT
public:
    RenderLineEdit(QScrollView *view, DOM::HTMLInputElementImpl *element);

    virtual void calcMinMaxWidth();
    virtual void layout();

    virtual const char *renderName() const { return "RenderLineEdit"; }
    void select();

public slots:
    void slotReturnPressed();
    void slotTextChanged(const QString &string);

private:
    virtual bool isEditable() const { return true; }
};

// -------------------------------------------------------------------------

class LineEditWidget : public KLineEdit
{
public:
    LineEditWidget(QWidget *parent);

protected:
    virtual bool event( QEvent *e );
};

// -------------------------------------------------------------------------

class RenderFieldset : public RenderFormElement
{
public:
    RenderFieldset(QScrollView *view, DOM::HTMLGenericFormElementImpl *element);

    virtual const char *renderName() const { return "RenderFieldSet"; }
};


// -------------------------------------------------------------------------

class RenderFileButton : public RenderFormElement
{
    Q_OBJECT
public:
    RenderFileButton(QScrollView *view, DOM::HTMLInputElementImpl *element);

    virtual const char *renderName() const { return "RenderFileButton"; }
    virtual void calcMinMaxWidth();
    virtual void layout();
    void select();

public slots:
    virtual void slotClicked();
    virtual void slotReturnPressed();
    virtual void slotTextChanged(const QString &string);

protected:
    virtual bool isEditable() const { return true; }
    
    bool m_clicked;
    bool m_haveFocus;
    KLineEdit   *m_edit;
    QPushButton *m_button;
};


// -------------------------------------------------------------------------

class RenderLabel : public RenderFormElement
{
public:
    RenderLabel(QScrollView *view, DOM::HTMLGenericFormElementImpl *element);

    virtual const char *renderName() const { return "RenderLabel"; }
};


// -------------------------------------------------------------------------

class RenderLegend : public RenderFormElement
{
public:
    RenderLegend(QScrollView *view, DOM::HTMLGenericFormElementImpl *element);

    virtual const char *renderName() const { return "RenderLegend"; }
};

// -------------------------------------------------------------------------

class ComboBoxWidget : public KComboBox
{
public:
    ComboBoxWidget(QWidget *parent);

protected:
    virtual bool event(QEvent *);
    virtual bool eventFilter(QObject *dest, QEvent *e);
};

// -------------------------------------------------------------------------

class RenderSelect : public RenderFormElement
{
    Q_OBJECT
public:
    RenderSelect(QScrollView *view, DOM::HTMLSelectElementImpl *element);

    virtual const char *renderName() const { return "RenderSelect"; }

    virtual void calcMinMaxWidth();
    virtual void layout();
    virtual void close( );

    void setOptionsChanged(bool _optionsChanged);

    bool selectionChanged() { return m_selectionChanged; }
    void setSelectionChanged(bool _selectionChanged) { m_selectionChanged = _selectionChanged; }

    void updateSelection();

protected:
    KListBox *createListBox();
    ComboBoxWidget *createComboBox();

    unsigned  m_size;
    bool m_multiple;
    bool m_useListBox;
    bool m_selectionChanged;
    bool m_ignoreSelectEvents;
    bool m_optionsChanged;

protected slots:
    void slotSelected(int index);
    void slotSelectionChanged();
};

// -------------------------------------------------------------------------

class TextAreaWidget : public KEdit
{
public:
    TextAreaWidget(int wrap, QWidget* parent);

    using QMultiLineEdit::verticalScrollBar;
    using QMultiLineEdit::horizontalScrollBar;
    using QMultiLineEdit::hasMarkedText;

protected:
    virtual bool event (QEvent *e );
};


// -------------------------------------------------------------------------

class RenderTextArea : public RenderFormElement
{
    Q_OBJECT
public:
    RenderTextArea(QScrollView *view, DOM::HTMLTextAreaElementImpl *element);
    ~RenderTextArea();

    virtual const char *renderName() const { return "RenderTextArea"; }
    virtual void calcMinMaxWidth();
    virtual void layout();
    virtual void close ( );

    QString text(); // ### remove

    void select();

protected slots:
    void slotTextChanged();

protected:
    virtual bool isEditable() const { return true; }
};

// -------------------------------------------------------------------------

}; //namespace

#endif
