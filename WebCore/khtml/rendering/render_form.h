/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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
 */
#ifndef RENDER_FORM_H
#define RENDER_FORM_H

#include "rendering/render_replaced.h"
#include "rendering/render_image.h"
#include "rendering/render_block.h"
#include "html/html_formimpl.h"

class QWidget;
class QLineEdit;
class QListboxItem;

#include <qtextedit.h>
#include <klineedit.h>
#include <qhbox.h>
#include <klistbox.h>
#include <kcombobox.h>

typedef class QTextEdit KTextEdit;
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
    RenderFormElement(DOM::HTMLGenericFormElementImpl* node);
    virtual ~RenderFormElement();

    virtual const char *renderName() const { return "RenderForm"; }

    virtual bool isFormElement() const { return true; }

#if APPLE_CHANGES
    // Aqua form controls never have border/padding.
    int borderTop() const { return 0; }
    int borderBottom() const { return 0; }
    int borderLeft() const { return 0; }
    int borderRight() const { return 0; }
    int paddingTop() const { return 0; }
    int paddingBottom() const { return 0; }
    int paddingLeft() const { return 0; }
    int paddingRight() const { return 0; }

    // Aqua controls use intrinsic margin values in order to leave space for focus rings
    // and to keep controls from butting up against one another.  This intrinsic margin
    // is only applied if the Web page allows the control to size intrinsically.  If the
    // Web page specifies an explicit width for a control, then we go ahead and honor that
    // precise width.  Similarly, if a Web page uses a specific margin value, we will go ahead
    // and honor that value.  
    void addIntrinsicMarginsIfAllowed(RenderStyle* _style);
    virtual bool canHaveIntrinsicMargins() const { return false; }
    int intrinsicMargin() const { return 2; }
#endif

    virtual void setStyle(RenderStyle *);
    virtual void updateFromElement();

    virtual void layout();
    virtual short baselinePosition( bool, bool ) const;

    DOM::HTMLGenericFormElementImpl *element() const
    { return static_cast<DOM::HTMLGenericFormElementImpl*>(RenderObject::element()); }

public slots:
    virtual void slotClicked();
    virtual void slotSelectionChanged();
    
#if APPLE_CHANGES
    // Hack to make KWQSlot code work.
    virtual void slotTextChanged(const QString &string);
#endif

protected:
    virtual bool isEditable() const { return false; }

    AlignmentFlags textAlignment() const;
};

// -------------------------------------------------------------------------

class RenderImageButton : public RenderImage
{
public:
    RenderImageButton(DOM::HTMLInputElementImpl *element);

    virtual const char *renderName() const { return "RenderImageButton"; }
    virtual bool isImageButton() const { return true; }
};

// -------------------------------------------------------------------------

class RenderLineEdit : public RenderFormElement
{
    Q_OBJECT
public:
    RenderLineEdit(DOM::HTMLInputElementImpl *element);

    virtual void calcMinMaxWidth();
#if APPLE_CHANGES
    int calcReplacedHeight() const { return intrinsicHeight(); }
    virtual bool canHaveIntrinsicMargins() const { return true; }
#endif

    virtual const char *renderName() const { return "RenderLineEdit"; }
    virtual void updateFromElement();
    virtual void setStyle(RenderStyle *);

    int selectionStart();
    int selectionEnd();
    void setSelectionStart(int);
    void setSelectionEnd(int);
    
    void select();
    void setSelectionRange(int, int);

    KLineEdit *widget() const { return static_cast<KLineEdit*>(m_widget); }
    DOM::HTMLInputElementImpl* element() const
    { return static_cast<DOM::HTMLInputElementImpl*>(RenderObject::element()); }

public slots:
    void slotReturnPressed();
    void slotTextChanged(const QString &string);
    void slotSelectionChanged();
#if APPLE_CHANGES
    void slotPerformSearch();
public:
    void addSearchResult();
#endif

protected:
    virtual void handleFocusOut();

private:
    virtual bool isEditable() const { return true; }

    bool m_updating;
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

class RenderFieldset : public RenderBlock
{
public:
    RenderFieldset(DOM::HTMLGenericFormElementImpl *element);

    virtual const char *renderName() const { return "RenderFieldSet"; }

    virtual RenderObject* layoutLegend(bool relayoutChildren);

    virtual void setStyle(RenderStyle* _style);
    
protected:
    virtual void paintBoxDecorations(PaintInfo& i, int _tx, int _ty);
    void paintBorderMinusLegend(QPainter *p, int _tx, int _ty, int w,
                                int h, const RenderStyle *style, int lx, int lw);
    RenderObject* findLegend();
};

// -------------------------------------------------------------------------

class RenderFileButton : public RenderFormElement
{
    Q_OBJECT
public:
    RenderFileButton(DOM::HTMLInputElementImpl *element);

    virtual const char *renderName() const { return "RenderFileButton"; }
    virtual void calcMinMaxWidth();
    virtual void updateFromElement();
    void select();

    int calcReplacedHeight() const { return intrinsicHeight(); }

    DOM::HTMLInputElementImpl *element() const
    { return static_cast<DOM::HTMLInputElementImpl*>(RenderObject::element()); }

    void click(bool sendMouseEvents);

public slots:
    virtual void slotClicked();
    virtual void slotReturnPressed();
    virtual void slotTextChanged(const QString &string);

protected:
    virtual bool isEditable() const { return true; }
};


// -------------------------------------------------------------------------

class RenderLabel : public RenderFormElement
{
public:
    RenderLabel(DOM::HTMLGenericFormElementImpl *element);

    virtual const char *renderName() const { return "RenderLabel"; }
};


// -------------------------------------------------------------------------

class RenderLegend : public RenderBlock
{
public:
    RenderLegend(DOM::HTMLGenericFormElementImpl *element);

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
    RenderSelect(DOM::HTMLSelectElementImpl *element);

    virtual const char *renderName() const { return "RenderSelect"; }

#if APPLE_CHANGES
    short baselinePosition( bool f, bool b ) const;
    int calcReplacedHeight() const { if (!m_useListBox) return intrinsicHeight(); return RenderFormElement::calcReplacedHeight(); }
    virtual bool canHaveIntrinsicMargins() const { return true; }
#endif

    virtual void calcMinMaxWidth();
    virtual void layout();

    void setOptionsChanged(bool _optionsChanged);

    bool selectionChanged() { return m_selectionChanged; }
    void setSelectionChanged(bool _selectionChanged) { m_selectionChanged = _selectionChanged; }
    virtual void updateFromElement();
#if APPLE_CHANGES
    virtual void setStyle(RenderStyle *);
#endif

    void updateSelection();

    DOM::HTMLSelectElementImpl *element() const
    { return static_cast<DOM::HTMLSelectElementImpl*>(RenderObject::element()); }

protected:
    KListBox *createListBox();
    ComboBoxWidget *createComboBox();
#if APPLE_CHANGES
    void setWidgetWritingDirection();
#endif

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

class TextAreaWidget : public KTextEdit
{
public:
    TextAreaWidget(QWidget* parent);

protected:
    virtual bool event (QEvent *e );
};


// -------------------------------------------------------------------------

class RenderTextArea : public RenderFormElement
{
    Q_OBJECT
public:
    RenderTextArea(DOM::HTMLTextAreaElementImpl *element);

    virtual void destroy();

    virtual const char *renderName() const { return "RenderTextArea"; }
    virtual void calcMinMaxWidth();
    virtual void updateFromElement();
    virtual void setStyle(RenderStyle *);

    virtual bool isTextArea() const { return true; }
    
    // don't even think about making this method virtual!
    DOM::HTMLTextAreaElementImpl* element() const
    { return static_cast<DOM::HTMLTextAreaElementImpl*>(RenderObject::element()); }

    QString text();

    int selectionStart();
    int selectionEnd();
    void setSelectionStart(int);
    void setSelectionEnd(int);
    
    void select();
    void setSelectionRange(int, int);
    
#if APPLE_CHANGES
    virtual bool canHaveIntrinsicMargins() const { return true; }
#endif

protected slots:
    void slotTextChanged();
    void slotSelectionChanged();
    
protected:
    virtual void handleFocusOut();

    virtual bool isEditable() const { return true; }

    bool m_dirty;
};

// -------------------------------------------------------------------------

#if APPLE_CHANGES
class RenderSlider : public RenderFormElement
{
public:
    RenderSlider(DOM::HTMLInputElementImpl *element);
    
    DOM::HTMLInputElementImpl* element() const
    { return static_cast<DOM::HTMLInputElementImpl*>(RenderObject::element()); }

    virtual const char *renderName() const { return "RenderSlider"; }
    virtual bool canHaveIntrinsicMargins() const { return true; }
    virtual void calcMinMaxWidth();
    virtual void updateFromElement();

protected slots:
    void slotSliderValueChanged();
    void slotClicked();
};
#endif

}; //namespace

#endif
