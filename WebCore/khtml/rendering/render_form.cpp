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

#include <kdebug.h>
#include <kurl.h>
#include <klocale.h>
#include <kfiledialog.h>
#include <kapp.h>
#include <kcompletionbox.h>
#include <kcursor.h>

#include <qcombobox.h>
#include "misc/helper.h"

#include "dom_nodeimpl.h"
#include "dom_textimpl.h"
#include "dom_docimpl.h"
#include "dom2_eventsimpl.h"

#include "html/html_formimpl.h"
#include "html/html_documentimpl.h"
#include "misc/htmlhashes.h"

#include "rendering/render_form.h"
#include "rendering/render_style.h"
#include "rendering/render_root.h"
#include <assert.h>

#include "khtmlview.h"
#include "khtml_part.h"
#include "khtml_ext.h"

using namespace khtml;

RenderFormElement::RenderFormElement(QScrollView *view,
                                     HTMLGenericFormElementImpl *element)
    : RenderWidget(view)
{
    // init RenderObject attributes
    setInline(true);   // our object is Inline

    m_element = element;
    m_clickCount = 0;
    m_state = 0;
    m_button = 0;
    m_isDoubleClick = false;
}

RenderFormElement::~RenderFormElement()
{
}

short RenderFormElement::baselinePosition( bool f ) const
{
    return RenderWidget::baselinePosition( f ) - 2 - QFontMetrics( style()->font() ).descent();
}

short RenderFormElement::calcReplacedWidth(bool*) const
{
    Length w = style()->width();
    if ( w.isVariable() )
        return intrinsicWidth();
    else
        return RenderReplaced::calcReplacedWidth();
}

int RenderFormElement::calcReplacedHeight() const
{
    Length h = style()->height();
    if ( h.isVariable() )
        return intrinsicHeight();
    else
        return RenderReplaced::calcReplacedHeight();
}

void RenderFormElement::layout()
{
    if ( layouted() ) return;

    // minimum height
    m_height = 0;

    calcWidth();
    calcHeight();

    if ( m_widget ) {
        m_widget->resize( m_width-borderLeft()-borderRight()-paddingLeft()-paddingRight(),
                          m_height-borderLeft()-borderRight()-paddingLeft()-paddingRight());
        m_widget->setEnabled(!m_element->disabled());

	QColor color = style()->color();
	QColor backgroundColor = style()->backgroundColor();

	if ( color.isValid() || backgroundColor.isValid() ) {
	    QPalette pal(m_widget->palette());

	    int contrast_ = KGlobalSettings::contrast();
	    int highlightVal = 100 + (2*contrast_+4)*16/10;
	    int lowlightVal = 100 + (2*contrast_+4)*10;

	    if (backgroundColor.isValid()) {
		pal.setColor(QPalette::Active,QColorGroup::Background,backgroundColor);
		pal.setColor(QPalette::Active,QColorGroup::Light,backgroundColor.light(highlightVal));
		pal.setColor(QPalette::Active,QColorGroup::Dark,backgroundColor.dark(lowlightVal));
		pal.setColor(QPalette::Active,QColorGroup::Mid,backgroundColor.dark(120));
		pal.setColor(QPalette::Active,QColorGroup::Midlight, backgroundColor.light(110));
		pal.setColor(QPalette::Active,QColorGroup::Button,backgroundColor);
		pal.setColor(QPalette::Active,QColorGroup::Base,backgroundColor);
		pal.setColor(QPalette::Inactive,QColorGroup::Background,backgroundColor);
		pal.setColor(QPalette::Inactive,QColorGroup::Light,backgroundColor.light(highlightVal));
		pal.setColor(QPalette::Inactive,QColorGroup::Dark,backgroundColor.dark(lowlightVal));
		pal.setColor(QPalette::Inactive,QColorGroup::Mid,backgroundColor.dark(120));
		pal.setColor(QPalette::Inactive,QColorGroup::Midlight, backgroundColor.light(110));
		pal.setColor(QPalette::Inactive,QColorGroup::Button,backgroundColor);
		pal.setColor(QPalette::Inactive,QColorGroup::Base,backgroundColor);
		pal.setColor(QPalette::Disabled,QColorGroup::Background,backgroundColor);
		pal.setColor(QPalette::Disabled,QColorGroup::Light,backgroundColor.light(highlightVal));
		pal.setColor(QPalette::Disabled,QColorGroup::Dark,backgroundColor.dark(lowlightVal));
		pal.setColor(QPalette::Disabled,QColorGroup::Mid,backgroundColor.dark(120));
		pal.setColor(QPalette::Disabled,QColorGroup::Text,backgroundColor.dark(120));
		pal.setColor(QPalette::Disabled,QColorGroup::Midlight, backgroundColor.light(110));
		pal.setColor(QPalette::Disabled,QColorGroup::Button, backgroundColor);
		pal.setColor(QPalette::Disabled,QColorGroup::Base,backgroundColor);
	    }
            if ( color.isValid() ) {
		pal.setColor(QPalette::Active,QColorGroup::Foreground,color);
		pal.setColor(QPalette::Active,QColorGroup::ButtonText,color);
		pal.setColor(QPalette::Active,QColorGroup::Text,color);
		pal.setColor(QPalette::Inactive,QColorGroup::Foreground,color);
		pal.setColor(QPalette::Inactive,QColorGroup::ButtonText,color);
		pal.setColor(QPalette::Inactive,QColorGroup::Text,color);

		QColor disfg = color;
		int h, s, v;
		disfg.hsv( &h, &s, &v );
		if (v > 128)
		    // dark bg, light fg - need a darker disabled fg
		    disfg = disfg.dark(lowlightVal);
		else if (disfg != black)
		    // light bg, dark fg - need a lighter disabled fg - but only if !black
		    disfg = disfg.light(highlightVal);
		else
		    // black fg - use darkgrey disabled fg
		    disfg = Qt::darkGray;
		pal.setColor(QPalette::Disabled,QColorGroup::Foreground,disfg);
		pal.setColor(QPalette::Disabled,QColorGroup::ButtonText, color);
	    }

	    m_widget->setPalette(pal);
	}
        else
            m_widget->unsetPalette();
    }

    if ( !style()->width().isPercent() )
        setLayouted();
}

bool RenderFormElement::eventFilter(QObject* /*o*/, QEvent* e)
{
    if ( !m_element || !m_element->view || !m_element->view->part() ) return true;

    ref();
    m_element->ref();

    switch(e->type()) {
    case QEvent::FocusOut:
        m_element->dispatchHTMLEvent(EventImpl::BLUR_EVENT,false,false);
        if ( isEditable() ) {
            KHTMLPartBrowserExtension *ext = static_cast<KHTMLPartBrowserExtension *>( m_element->view->part()->browserExtension() );
            if ( ext )  ext->editableWidgetBlurred( m_widget );
        }
        break;
    case QEvent::FocusIn:
        m_element->ownerDocument()->setFocusNode(m_element);
        if ( isEditable() ) {
            KHTMLPartBrowserExtension *ext = static_cast<KHTMLPartBrowserExtension *>( m_element->view->part()->browserExtension() );
            if ( ext )  ext->editableWidgetFocused( m_widget );
        }
        break;
    case QEvent::MouseButtonPress:
        handleMousePressed(static_cast<QMouseEvent*>(e));
        break;
    case QEvent::MouseButtonRelease:
    {
        int absX, absY;
        absolutePosition(absX,absY);
        QMouseEvent* _e = static_cast<QMouseEvent*>(e);
        m_button = _e->button();
        m_state  = _e->state();
        QMouseEvent e2(e->type(),QPoint(absX,absY)+_e->pos(),_e->button(),_e->state());

        m_element->dispatchMouseEvent(&e2,EventImpl::MOUSEUP_EVENT,m_clickCount);

        if((m_mousePos - e2.pos()).manhattanLength() <= QApplication::startDragDistance()) {
            // DOM2 Events section 1.6.2 says that a click is if the mouse was pressed
            // and released in the "same screen location"
            // As people usually can't click on the same pixel, we're a bit tolerant here
            m_element->dispatchMouseEvent(&e2,EventImpl::CLICK_EVENT,m_clickCount);
        }

        if(!isRenderButton()) {
            // ### DOMActivate is also dispatched for thigs like selects & textareas -
            // not sure if this is correct
            m_element->dispatchUIEvent(EventImpl::DOMACTIVATE_EVENT,m_isDoubleClick ? 2 : 1);
            m_element->dispatchMouseEvent(&e2, m_isDoubleClick ? EventImpl::KHTML_DBLCLICK_EVENT : EventImpl::KHTML_CLICK_EVENT, m_clickCount);
            m_isDoubleClick = false;
        }
        else
            // save position for slotClicked - see below -
            m_mousePos = e2.pos();
    }
    break;
    case QEvent::MouseButtonDblClick:
    {
        m_isDoubleClick = true;
        handleMousePressed(static_cast<QMouseEvent*>(e));
    }
    break;
    case QEvent::MouseMove:
    {
        int absX, absY;
        absolutePosition(absX,absY);
        QMouseEvent* _e = static_cast<QMouseEvent*>(e);
        QMouseEvent e2(e->type(),QPoint(absX,absY)+_e->pos(),_e->button(),_e->state());
        m_element->dispatchMouseEvent(&e2);
        // ### change cursor like in KHTMLView?
    }
    break;
    default: break;
    };

    m_element->deref();
    bool deleted = hasOneRef();
    deref();

    return deleted;
}

void RenderFormElement::slotClicked()
{
    if(isRenderButton()) {
        QMouseEvent e2( QEvent::MouseButtonRelease, m_mousePos, m_button, m_state);

        m_element->dispatchMouseEvent(&e2, m_isDoubleClick ? EventImpl::KHTML_DBLCLICK_EVENT : EventImpl::KHTML_CLICK_EVENT, m_clickCount);

	m_element->dispatchUIEvent(EventImpl::DOMACTIVATE_EVENT,m_isDoubleClick ? 2 : 1);
	m_isDoubleClick = false;
    }
}

void RenderFormElement::handleMousePressed(QMouseEvent *e)
{
    int absX, absY;
    absolutePosition(absX,absY);
    QMouseEvent e2(e->type(),QPoint(absX,absY)+e->pos(),e->button(),e->state());

    if((m_mousePos - e2.pos()).manhattanLength() > QApplication::startDragDistance()) {
        m_mousePos = e2.pos();
        m_clickCount = 1;
    }
    else
        m_clickCount++;

    m_element->dispatchMouseEvent(&e2,EventImpl::MOUSEDOWN_EVENT,m_clickCount);
}

// -------------------------------------------------------------------------

RenderButton::RenderButton(QScrollView *view,
                           HTMLGenericFormElementImpl *element)
    : RenderFormElement(view, element)
{
}

short RenderButton::baselinePosition( bool f ) const
{
    return RenderWidget::baselinePosition( f ) - 2;
}

// -------------------------------------------------------------------------------

RenderCheckBox::RenderCheckBox(QScrollView *view,
                               HTMLInputElementImpl *element)
    : RenderButton(view, element)
{
    QCheckBox* b = new QCheckBox(view->viewport());
    b->setAutoMask(true);
    b->setMouseTracking(true);
    setQWidget(b);
    b->installEventFilter(this);
    connect(b,SIGNAL(stateChanged(int)),this,SLOT(slotStateChanged(int)));
    connect(b, SIGNAL(clicked()), this, SLOT(slotClicked()));
}


void RenderCheckBox::calcMinMaxWidth()
{
    if ( minMaxKnown() ) return;

    QSize s = static_cast<QCheckBox*>( m_widget )->style().indicatorSize();
    setIntrinsicWidth( s.width() );
    setIntrinsicHeight( s.height() );

    RenderButton::calcMinMaxWidth();
}

void RenderCheckBox::layout()
{
    if ( layouted() ) return;

    QCheckBox* cb = static_cast<QCheckBox*>( m_widget );
    cb->setChecked(static_cast<HTMLInputElementImpl*>(m_element)->checked());

    RenderButton::layout();
}

void RenderCheckBox::slotStateChanged(int state)
{
    m_element->setAttribute(ATTR_CHECKED,state == 2 ? "" : 0);
}

// -------------------------------------------------------------------------------

RenderRadioButton::RenderRadioButton(QScrollView *view,
                                     HTMLInputElementImpl *element)
    : RenderButton(view, element)
{
    QRadioButton* b = new QRadioButton(view->viewport());
    b->setAutoMask(true);
    b->setMouseTracking(true);
    setQWidget(b);
    b->installEventFilter(this);
    connect(b, SIGNAL(clicked()), this, SLOT(slotClicked()));
}

void RenderRadioButton::setChecked(bool checked)
{
    static_cast<QRadioButton *>(m_widget)->setChecked(checked);
}

void RenderRadioButton::slotClicked()
{
    m_element->setAttribute(ATTR_CHECKED,"");

    // emit mouseClick event etc
    RenderButton::slotClicked();
}

void RenderRadioButton::calcMinMaxWidth()
{
    if ( minMaxKnown() ) return;

    QSize s = static_cast<QRadioButton*>( m_widget )->style().exclusiveIndicatorSize();
    setIntrinsicWidth( s.width() );
    setIntrinsicHeight( s.height() );

    RenderButton::calcMinMaxWidth();
}

void RenderRadioButton::layout()
{
    if ( layouted() ) return;

    QRadioButton* rb = static_cast<QRadioButton*>( m_widget );
    rb->setChecked(static_cast<HTMLInputElementImpl*>(m_element)->checked());

    RenderButton::layout();
}

// -------------------------------------------------------------------------------


RenderSubmitButton::RenderSubmitButton(QScrollView *view, HTMLInputElementImpl *element)
    : RenderButton(view, element)
{
    QPushButton* p = new QPushButton(view->viewport());
    setQWidget(p);
    p->setMouseTracking(true);
    p->installEventFilter(this);
    connect(p, SIGNAL(clicked()), this, SLOT(slotClicked()));
}

void RenderSubmitButton::calcMinMaxWidth()
{
    if ( minMaxKnown() ) return;

    QString value = static_cast<HTMLInputElementImpl*>(m_element)->value().isEmpty() ?
        defaultLabel() : static_cast<HTMLInputElementImpl*>(m_element)->value().string();
    value = value.visual();
    value = value.stripWhiteSpace();
    QString raw;
    for(unsigned int i = 0; i < value.length(); i++) {
        raw += value[i];
        if(value[i] == '&')
            raw += '&';
    }

    static_cast<QPushButton*>(m_widget)->setText(raw);
    static_cast<QPushButton*>(m_widget)->setFont(style()->font());

    // this is a QLineEdit/RenderLineEdit compatible sizehint
    QFontMetrics fm = fontMetrics( m_widget->font() );
    m_widget->constPolish();
    QSize ts = fm.size( ShowPrefix, raw );
    int h = ts.height() + 8;
    int w = ts.width() + 2*fm.width( ' ' );
    if ( m_widget->style().guiStyle() == Qt::WindowsStyle && h < 26 )
        h = 22;
    QSize s = QSize( w + 8, h ).expandedTo( m_widget->minimumSizeHint()).expandedTo( QApplication::globalStrut() );

    setIntrinsicWidth( s.width() );
    setIntrinsicHeight( s.height() );

    RenderButton::calcMinMaxWidth();
}

QString RenderSubmitButton::defaultLabel() {
    return i18n("Submit");
}

short RenderSubmitButton::baselinePosition( bool f ) const
{
    return RenderFormElement::baselinePosition( f );
}

// -------------------------------------------------------------------------------

RenderImageButton::RenderImageButton(HTMLInputElementImpl *element)
    : RenderImage(element)
{
    m_element = element;
    // ### support DOMActivate event when clicked
}


// -------------------------------------------------------------------------------

RenderResetButton::RenderResetButton(QScrollView *view, HTMLInputElementImpl *element)
    : RenderSubmitButton(view, element)
{
}

QString RenderResetButton::defaultLabel() {
    return i18n("Reset");
}


// -------------------------------------------------------------------------------

RenderPushButton::RenderPushButton(QScrollView *view, HTMLInputElementImpl *element)
    : RenderSubmitButton(view, element)
{
}

QString RenderPushButton::defaultLabel()
{
    return QString::null;
}

// -------------------------------------------------------------------------------

LineEditWidget::LineEditWidget(QWidget *parent)
        : KLineEdit(parent)
{
    setMouseTracking(true);
}

bool LineEditWidget::event( QEvent *e )
{
    if ( e->type() == QEvent::AccelAvailable && isReadOnly() ) {
        QKeyEvent* ke = (QKeyEvent*) e;
        if ( ke->state() & ControlButton ) {
            switch ( ke->key() ) {
                case Key_Left:
                case Key_Right:
                case Key_Up:
                case Key_Down:
                case Key_Home:
                case Key_End:
                    ke->accept();
                default:
                break;
            }
        }
    }
    return KLineEdit::event( e );
}

// -----------------------------------------------------------------------------

RenderLineEdit::RenderLineEdit(QScrollView *view, HTMLInputElementImpl *element)
    : RenderFormElement(view, element)
{
    LineEditWidget *edit = new LineEditWidget(view->viewport());
    edit->installEventFilter(this);
    connect(edit,SIGNAL(returnPressed()), this, SLOT(slotReturnPressed()));
    connect(edit,SIGNAL(textChanged(const QString &)),this,SLOT(slotTextChanged(const QString &)));

    if(element->inputType() == HTMLInputElementImpl::PASSWORD)
        edit->setEchoMode( QLineEdit::Password );

    if ( element->autoComplete() ) {
        QStringList completions =
            static_cast<KHTMLView *>(view)->formCompletionItems(element->name().string());
        if (completions.count()) {
            edit->completionObject()->setItems(completions);
            edit->setContextMenuEnabled(true);
        }
    }

    setQWidget(edit);
}

void RenderLineEdit::slotReturnPressed()
{
    // don't submit the form when return was pressed in a completion-popup
    KCompletionBox *box = (static_cast<KLineEdit*>(m_widget))->completionBox(false);
    if ( box && box->isVisible() && box->currentItem() != -1 )
	return;

    HTMLFormElementImpl* fe = m_element->form();
    if ( fe )
	fe->prepareSubmit();
}

void RenderLineEdit::calcMinMaxWidth()
{
    if ( minMaxKnown() ) return;

    QFontMetrics fm = fontMetrics( style()->font() );
    QSize s;

    KLineEdit *edit = static_cast<KLineEdit*>(m_widget);
    HTMLInputElementImpl *input = static_cast<HTMLInputElementImpl*>(m_element);

    int size = input->size();

    int h = fm.height();
    int w = fm.width( 'x' ) * (size > 0 ? size : 17); // "some"
    if ( edit->frame() ) {
        h += 8;
        // ### this is not really portable between all styles.
        // I think one should try to find a generic solution which
        // works with all possible styles. Lars.
        // ### well, it is. it's the 1:1 copy of QLineEdit::sizeHint()
        // the only reason that made me including this thingie is
        // that I cannot get a sizehint for a specific number of characters
        // in the lineedit from it. It's not my fault, it's Qt's. Dirk
        if ( m_widget->style().guiStyle() == Qt::WindowsStyle && h < 26 )
            h = 22;
        s = QSize( w + 8, h ).expandedTo( QApplication::globalStrut() );
    } else
	s = QSize( w + 4, h + 4 ).expandedTo( QApplication::globalStrut() );

    setIntrinsicWidth( s.width() );
    setIntrinsicHeight( s.height() );

    RenderFormElement::calcMinMaxWidth();
}

void RenderLineEdit::layout()
{
    if ( layouted() ) return;

    KLineEdit *edit = static_cast<KLineEdit*>(m_widget);
    HTMLInputElementImpl *input = static_cast<HTMLInputElementImpl*>(m_element);
    edit->blockSignals(true);
    int pos = edit->cursorPosition();
    edit->setText(static_cast<HTMLInputElementImpl*>(m_element)->value().string().visual());
    edit->setCursorPosition(pos);
    edit->blockSignals(false);

    int ml = input->maxLength();
    if ( ml < 0 || ml > 1024 )
        ml = 1024;
    edit->setMaxLength( ml );

    edit->setReadOnly(m_element->readOnly());

    RenderFormElement::layout();
}

void RenderLineEdit::slotTextChanged(const QString &string)
{
    // don't use setValue here!
    static_cast<HTMLInputElementImpl*>(m_element)->m_value = string;
}

void RenderLineEdit::select()
{
    static_cast<LineEditWidget*>(m_widget)->selectAll();
}

// ---------------------------------------------------------------------------

RenderFieldset::RenderFieldset(QScrollView *view,
                               HTMLGenericFormElementImpl *element)
    : RenderFormElement(view, element)
{
}

// -------------------------------------------------------------------------

RenderFileButton::RenderFileButton(QScrollView *view, HTMLInputElementImpl *element)
    : RenderFormElement(view, element)
{
    QHBox *w = new QHBox(view->viewport());

    m_edit = new LineEditWidget(w);

    m_edit->installEventFilter(this);

    connect(m_edit, SIGNAL(returnPressed()), this, SLOT(slotReturnPressed()));
    connect(m_edit, SIGNAL(textChanged(const QString &)),this,SLOT(slotTextChanged(const QString &)));

    m_button = new QPushButton(i18n("Browse..."), w);
    m_button->setFocusPolicy(QWidget::ClickFocus);
    connect(m_button,SIGNAL(clicked()), this, SLOT(slotClicked()));

    w->setStretchFactor(m_edit, 2);
    w->setFocusProxy(m_edit);

    setQWidget(w);
    m_haveFocus = false;
}

void RenderFileButton::calcMinMaxWidth()
{
    if ( minMaxKnown() ) return;

    QFontMetrics fm = fontMetrics( style()->font() );
    QSize s;
    HTMLInputElementImpl *input = static_cast<HTMLInputElementImpl*>(m_element);
    int size = input->size();

    int h = fm.height();
    int w = fm.width( 'x' ) * (size > 0 ? size : 17);
    w += fm.width( m_button->text() ) + 2*fm.width( ' ' );

    if ( m_edit->frame() ) {
        h += 8;
        if ( m_widget->style().guiStyle() == Qt::WindowsStyle && h < 26 )
            h = 22;
        s = QSize( w + 8, h );
    } else
        s = QSize( w + 4, h + 4 );

    setIntrinsicWidth( s.width() );
    setIntrinsicHeight( s.height() );

    RenderFormElement::calcMinMaxWidth();
}

void RenderFileButton::slotClicked()
{
    QString file_name = KFileDialog::getOpenFileName(QString::null, QString::null, 0, i18n("Browse..."));
    if (!file_name.isNull()) {
        // ### truncate if > maxLength
        static_cast<HTMLInputElementImpl*>(m_element)->setFilename(DOMString(file_name));
        m_edit->setText(file_name);
    }
}

void RenderFileButton::layout( )
{
    // this is largely taken from the RenderLineEdit layout
    QFontMetrics fm = fontMetrics( m_edit->font() );
    HTMLInputElementImpl *input = static_cast<HTMLInputElementImpl*>(m_element);

    m_edit->blockSignals(true);
    m_edit->setText(static_cast<HTMLInputElementImpl*>(m_element)->filename().string());
    m_edit->blockSignals(false);
    int ml = input->maxLength();
    if ( ml < 0 || ml > 1024 )
        ml = 1024;
    m_edit->setMaxLength( ml );
    m_edit->setReadOnly(m_element->readOnly());

    RenderFormElement::layout();
}

void RenderFileButton::slotReturnPressed()
{
    if (m_element->form())
	m_element->form()->prepareSubmit();
}

void RenderFileButton::slotTextChanged(const QString &string)
{
    static_cast<HTMLInputElementImpl*>(m_element)->setFilename(DOMString(string));
}

void RenderFileButton::select()
{
    m_edit->selectAll();
}


// -------------------------------------------------------------------------

RenderLabel::RenderLabel(QScrollView *view,
                         HTMLGenericFormElementImpl *element)
    : RenderFormElement(view, element)
{

}

// -------------------------------------------------------------------------

RenderLegend::RenderLegend(QScrollView *view,
                           HTMLGenericFormElementImpl *element)
    : RenderFormElement(view, element)
{
}

// -------------------------------------------------------------------------------

ComboBoxWidget::ComboBoxWidget(QWidget *parent)
    : KComboBox(false, parent)
{
    setAutoMask(true);
    if (listBox()) listBox()->installEventFilter(this);
    setMouseTracking(true);
}

bool ComboBoxWidget::event(QEvent *e)
{
    if (e->type()==QEvent::KeyPress)
    {
	QKeyEvent *ke = static_cast<QKeyEvent *>(e);
	switch(ke->key())
	{
	case Key_Return:
	case Key_Enter:
	    popup();
	    ke->accept();
	    return true;
	default:
	    return KComboBox::event(e);
	}
    }
    return KComboBox::event(e);
}

bool ComboBoxWidget::eventFilter(QObject *dest, QEvent *e)
{
    if (dest==listBox() &&  e->type()==QEvent::KeyPress)
    {
	QKeyEvent *ke = static_cast<QKeyEvent *>(e);
	bool forward = false;
	switch(ke->key())
	{
	case Key_Tab:
	    forward=true;
	case Key_BackTab:
	    // ugly hack. emulate popdownlistbox() (private in QComboBox)
	    // we re-use ke here to store the reference to the generated event.
	    ke = new QKeyEvent(QEvent::KeyPress, Key_Escape, 0, 0);
	    QApplication::sendEvent(dest,ke);
	    focusNextPrevChild(forward);
	    delete ke;
	    return true;
	default:
	    return KComboBox::eventFilter(dest, e);
	}
    }
    return KComboBox::eventFilter(dest, e);
}

// -------------------------------------------------------------------------

RenderSelect::RenderSelect(QScrollView *view, HTMLSelectElementImpl *element)
    : RenderFormElement(view, element)
{
    m_ignoreSelectEvents = false;
    m_multiple = element->multiple();
    m_size = element->size();
    m_useListBox = (m_multiple || m_size > 1);

    if(m_useListBox)
        setQWidget(createListBox());
    else
	setQWidget(createComboBox());
}


void RenderSelect::calcMinMaxWidth()
{
    // ### ugly HACK FIXME!!!
    if ( !layouted() )
        layout();

    setLayouted( false );

    RenderFormElement::calcMinMaxWidth();
}

void RenderSelect::layout( )
{
    // ### maintain selection properly between type/size changes, and work
    // out how to handle multiselect->singleselect (probably just select
    // first selected one)

    // ### reconnect mouse signals when we change widget type

    HTMLSelectElementImpl* f = static_cast<HTMLSelectElementImpl*>(m_element);
    m_ignoreSelectEvents = true;

    // change widget type

    bool oldMultiple = m_multiple;
    unsigned oldSize = m_size;
    bool oldListbox = m_useListBox;

    m_multiple = f->multiple();
    m_size = f->size();
    m_useListBox = (m_multiple || m_size > 1);

    if (oldMultiple != m_multiple || oldSize != m_size) {
        if (m_useListBox != oldListbox) {
            // type of select has changed
            delete m_widget;

            if(m_useListBox)
                setQWidget(createListBox());
            else
                setQWidget(createComboBox());
        }

        if (m_useListBox && oldMultiple != m_multiple) {
            static_cast<KListBox*>(m_widget)->setSelectionMode(m_multiple ? QListBox::Multi : QListBox::Single);
        }
        m_selectionChanged = true;
        m_optionsChanged = true;
    }

    HTMLSelectElementImpl *select = static_cast<HTMLSelectElementImpl*>(m_element);

    // update contents listbox/combobox based on options in m_element
    if ( m_optionsChanged ) {
        QArray<HTMLGenericFormElementImpl*> listItems = select->listItems();
        int listIndex;

        if(m_useListBox)
            static_cast<KListBox*>(m_widget)->clear();
        else
            static_cast<KComboBox*>(m_widget)->clear();

        for (listIndex = 0; listIndex < int(listItems.size()); listIndex++) {
            if (listItems[listIndex]->id() == ID_OPTGROUP) {
                DOMString text = listItems[listIndex]->getAttribute(ATTR_LABEL);
                if (text.isNull())
                    text = "";

                if(m_useListBox) {
                    QListBoxText *item = new QListBoxText(QString(text.implementation()->s, text.implementation()->l).visual());
                    static_cast<KListBox*>(m_widget)
                        ->insertItem(item, listIndex);
                    item->setSelectable(false);
                }
                else
                    static_cast<KComboBox*>(m_widget)
                        ->insertItem(QString(text.implementation()->s, text.implementation()->l).visual(), listIndex);
            }
            else if (listItems[listIndex]->id() == ID_OPTION) {
                DOMString text = static_cast<HTMLOptionElementImpl*>(listItems[listIndex])->text();
                if (text.isNull())
                    text = "";
                if (listItems[listIndex]->parentNode()->id() == ID_OPTGROUP)
                    text = DOMString("    ")+text;

                if(m_useListBox)
                    static_cast<KListBox*>(m_widget)
                        ->insertItem(QString(text.implementation()->s, text.implementation()->l).visual(), listIndex);
                else
                    static_cast<KComboBox*>(m_widget)
                        ->insertItem(QString(text.implementation()->s, text.implementation()->l).visual(), listIndex);
            }
            else
                assert(false);
            m_selectionChanged = true;
        }
        m_optionsChanged = false;
    }

    // update selection
    if (m_selectionChanged)
        updateSelection();

    // calculate size
    if(m_useListBox) {
        KListBox* w = static_cast<KListBox*>(m_widget);

        QListBoxItem* p = w->firstItem();
        int width = 0;
        int height = 0;
        while(p) {
            width = QMAX(width, p->width(p->listBox()));
            height = QMAX(height, p->height(p->listBox()));
            p = p->next();
        }

        int size = m_size;
        // check if multiple and size was not given or invalid
        // Internet Exploder sets size to QMIN(number of elements, 4)
        // Netscape seems to simply set it to "number of elements"
        // the average of that is IMHO QMIN(number of elements, 10)
        // so I did that ;-)
        if(size < 1)
            size = QMIN(static_cast<KListBox*>(m_widget)->count(), 10);

        width += 2*w->frameWidth() + w->verticalScrollBar()->sizeHint().width();
        height = size*height + 2*w->frameWidth();

        setIntrinsicWidth( width );
        setIntrinsicHeight( height );
    }
    else {
        QSize s(m_widget->sizeHint());
        setIntrinsicWidth( s.width() );
        setIntrinsicHeight( s.height() );
    }

    /// uuh, ignore the following line..
    setLayouted( false );
    RenderFormElement::layout();

    // and now disable the widget in case there is no <option> given
    QArray<HTMLGenericFormElementImpl*> listItems = select->listItems();
    bool foundOption = false;
    for (uint i = 0; i < listItems.size() && !foundOption; i++)
	foundOption = (listItems[i]->id() == ID_OPTION);

    if (!foundOption)
	m_widget->setEnabled(false);

    m_ignoreSelectEvents = false;
}

void RenderSelect::close()
{
    HTMLSelectElementImpl* f = static_cast<HTMLSelectElementImpl*>(m_element);

    // Restore state
    QString state = f->ownerDocument()->registerElement(f);
    if ( !state.isEmpty())
        static_cast<HTMLSelectElementImpl*>(m_element)->restoreState( state );

    setLayouted(false);
    static_cast<HTMLSelectElementImpl*>(m_element)->recalcListItems();

    RenderFormElement::close();
}

void RenderSelect::slotSelected(int index)
{
    if ( m_ignoreSelectEvents ) return;

    assert( !m_useListBox );

    QArray<HTMLGenericFormElementImpl*> listItems = static_cast<HTMLSelectElementImpl*>(m_element)->listItems();
    if(index >= 0 && index < int(listItems.size()))
    {
        if ( listItems[index]->id() == ID_OPTGROUP ) {

            // this one is not selectable,  we need to find an option element
            bool found = false;
            while ( ( unsigned ) index < listItems.size() ) {
                if ( listItems[index]->id() == ID_OPTION ) {
                    found = true;
                    break;
                }
                ++index;
            }

            if ( !found ) {
                while ( index >= 0 ) {
                    if ( listItems[index]->id() == ID_OPTION ) {
                        found = true;
                        break;
                    }
                    --index;
                }
            }

            if ( found )
                static_cast<HTMLOptionElementImpl*>(listItems[index])->setSelected(true);
        }
        else {
            static_cast<HTMLOptionElementImpl*>(listItems[index])->setSelected(true);
        }
    }

    static_cast<HTMLSelectElementImpl*>(m_element)->onChange();
}


void RenderSelect::slotSelectionChanged()
{
    if ( m_ignoreSelectEvents ) return;

    QArray<HTMLGenericFormElementImpl*> listItems = static_cast<HTMLSelectElementImpl*>(m_element)->listItems();
    for ( unsigned i = 0; i < listItems.count(); i++ )
        // don't use setSelected() here because it will cause us to be called
        // again with updateSelection.
        if ( listItems[i]->id() == ID_OPTION )
            static_cast<HTMLOptionElementImpl*>( listItems[i] )
                ->m_selected = static_cast<KListBox*>( m_widget )->isSelected( i );

    static_cast<HTMLSelectElementImpl*>(m_element)->onChange();
}


void RenderSelect::setOptionsChanged(bool _optionsChanged)
{
    m_optionsChanged = _optionsChanged;
}

KListBox* RenderSelect::createListBox()
{
    KListBox *lb = new KListBox(m_view->viewport());
    lb->installEventFilter(this);
    lb->setSelectionMode(m_multiple ? QListBox::Extended : QListBox::Single);
    // ### looks broken
    //lb->setAutoMask(true);
    connect( lb, SIGNAL( selectionChanged() ), this, SLOT( slotSelectionChanged() ) );
    m_ignoreSelectEvents = false;
    lb->setMouseTracking(true);

    return lb;
}

ComboBoxWidget *RenderSelect::createComboBox()
{
    ComboBoxWidget *cb = new ComboBoxWidget(m_view->viewport());
    cb->installEventFilter(this);
    connect(cb, SIGNAL(activated(int)), this, SLOT(slotSelected(int)));
    return cb;
}

void RenderSelect::updateSelection()
{
    QArray<HTMLGenericFormElementImpl*> listItems = static_cast<HTMLSelectElementImpl*>(m_element)->listItems();
    int i;
    if (m_useListBox) {
        // if multi-select, we select only the new selected index
        KListBox *listBox = static_cast<KListBox*>(m_widget);
        for (i = 0; i < int(listItems.size()); i++)
            listBox->setSelected(i,listItems[i]->id() == ID_OPTION &&
                                static_cast<HTMLOptionElementImpl*>(listItems[i])->selected());
    }
    else {
        for (i = 0; i < int(listItems.size()); i++)
            if (listItems[i]->id() == ID_OPTION && static_cast<HTMLOptionElementImpl*>(listItems[i])->selected()) {
                static_cast<KComboBox*>(m_widget)->setCurrentItem(i);
                return;
            }
        static_cast<KComboBox*>(m_widget)->setCurrentItem(0); // ### wrong if item 0 is an optgroup
    }

    m_selectionChanged = false;
}


// -------------------------------------------------------------------------

TextAreaWidget::TextAreaWidget(int wrap, QWidget* parent)
    : KEdit(parent)
{
    if(wrap != DOM::HTMLTextAreaElementImpl::ta_NoWrap) {
        setWordWrap(QMultiLineEdit::WidgetWidth);
        clearTableFlags(Tbl_autoScrollBars);
        setTableFlags(Tbl_vScrollBar);
    }
    else {
        clearTableFlags(Tbl_autoScrollBars);
        setTableFlags(Tbl_vScrollBar | Tbl_hScrollBar);
    }
    KCursor::setAutoHideCursor(this, true);
    setAutoMask(true);
    setMouseTracking(true);
}

bool TextAreaWidget::event( QEvent *e )
{
    if ( e->type() == QEvent::AccelAvailable && isReadOnly() ) {
        QKeyEvent* ke = (QKeyEvent*) e;
        if ( ke->state() & ControlButton ) {
            switch ( ke->key() ) {
                case Key_Left:
                case Key_Right:
                case Key_Up:
                case Key_Down:
                case Key_Home:
                case Key_End:
                    ke->accept();
                default:
                break;
            }
        }
    }
    return KEdit::event( e );
}

// -------------------------------------------------------------------------

// ### allow contents to be manipulated via DOM - will require updating
// of text node child

RenderTextArea::RenderTextArea(QScrollView *view, HTMLTextAreaElementImpl *element)
    : RenderFormElement(view, element)
{
    TextAreaWidget *edit = new TextAreaWidget(element->wrap(), view);
    setQWidget(edit);
    edit->installEventFilter(this);

    connect(edit,SIGNAL(textChanged()),this,SLOT(slotTextChanged()));
}

RenderTextArea::~RenderTextArea()
{
    HTMLTextAreaElementImpl* e = static_cast<HTMLTextAreaElementImpl*>( m_element );

    if ( e->m_dirtyvalue ) {
        e->m_value = text();
        e->m_dirtyvalue = false;
    }
}

void RenderTextArea::calcMinMaxWidth()
{
    TextAreaWidget* w = static_cast<TextAreaWidget*>(m_widget);
    HTMLTextAreaElementImpl* f = static_cast<HTMLTextAreaElementImpl*>(m_element);
    QFontMetrics m = fontMetrics(style()->font());
    QSize size( QMAX(f->cols(), 1)*m.width('x') + w->frameWidth()*5 +
                w->verticalScrollBar()->sizeHint().width(),
                QMAX(f->rows(), 1)*m.height() + w->frameWidth()*3 +
                (w->wordWrap() == QMultiLineEdit::NoWrap ?
                 w->horizontalScrollBar()->sizeHint().height() : 0)
        );

    setIntrinsicWidth( size.width() );
    setIntrinsicHeight( size.height() );

    RenderFormElement::calcMinMaxWidth();
}

void RenderTextArea::layout( )
{
    TextAreaWidget* w = static_cast<TextAreaWidget*>(m_widget);
    HTMLTextAreaElementImpl* f = static_cast<HTMLTextAreaElementImpl*>(m_element);

    if (!layouted()) {
	w->setReadOnly(m_element->readOnly());
	w->blockSignals(true);
	int line, col;
	w->getCursorPosition( &line, &col );
	w->setText(f->value().string().visual());
	w->setCursorPosition( line, col );
	w->blockSignals(false);
    }

    RenderFormElement::layout();
}

void RenderTextArea::close( )
{
    HTMLTextAreaElementImpl *e = static_cast<HTMLTextAreaElementImpl*>(m_element);

    e->setValue( e->defaultValue() );

    QString state = e->ownerDocument()->registerElement( e );
    if ( !state.isEmpty() )
        e->restoreState( state );

    RenderFormElement::close();
}

QString RenderTextArea::text()
{
    QString txt;
    HTMLTextAreaElementImpl* e = static_cast<HTMLTextAreaElementImpl*>(m_element);
    TextAreaWidget* w = static_cast<TextAreaWidget*>(m_widget);

    if(e->wrap() == DOM::HTMLTextAreaElementImpl::ta_Physical) {
        for(int i=0; i < w->numLines(); i++)
            txt += w->textLine(i) + QString::fromLatin1("\n");
    }
    else
        txt = w->text();

    return txt;
}

void RenderTextArea::slotTextChanged()
{
    static_cast<HTMLTextAreaElementImpl*>( m_element )->m_dirtyvalue = true;
}

void RenderTextArea::select()
{
    static_cast<TextAreaWidget *>(m_widget)->selectAll();
}

// ---------------------------------------------------------------------------

#include "render_form.moc"
