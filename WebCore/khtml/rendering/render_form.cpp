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
 */

#include <kdebug.h>
#include <klocale.h>
#include <kfiledialog.h>
#include <kcompletionbox.h>
#include <kcursor.h>

#include <qstyle.h>

#include "misc/helper.h"
#include "xml/dom2_eventsimpl.h"
#include "html/html_formimpl.h"
#include "misc/htmlhashes.h"

#include "rendering/render_form.h"
#include <assert.h>

#include "khtmlview.h"
#include "khtml_ext.h"
#include "xml/dom_docimpl.h"

#include <kdebug.h>

#ifdef APPLE_CHANGES
#include "render_root.h"
#include <KWQInvisibleButton.h>
#endif

using namespace khtml;

RenderFormElement::RenderFormElement(HTMLGenericFormElementImpl *element)
    : RenderWidget(element)
{
    // init RenderObject attributes
    setInline(true);   // our object is Inline

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
#ifdef APPLE_CHANGES
    return RenderWidget::baselinePosition( f ) - 6 - style()->fontMetrics().descent();
#else
    return RenderWidget::baselinePosition( f ) - 2 - style()->fontMetrics().descent();
#endif
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

void RenderFormElement::updateFromElement()
{
    m_widget->setEnabled(!element()->disabled());

    QColor color = style()->color();
    QColor backgroundColor = style()->backgroundColor();

    if ( color.isValid() || backgroundColor.isValid() ) {
        QPalette pal(m_widget->palette());

        int contrast_ = KGlobalSettings::contrast();
        int highlightVal = 100 + (2*contrast_+4)*16/10;
        int lowlightVal = 100 + (2*contrast_+4)*10;

        if (backgroundColor.isValid()) {
	    for ( int i = 0; i < QPalette::NColorGroups; i++ ) {
		pal.setColor( (QPalette::ColorGroup)i, QColorGroup::Background, backgroundColor );
		pal.setColor( (QPalette::ColorGroup)i, QColorGroup::Light, backgroundColor.light(highlightVal) );
		pal.setColor( (QPalette::ColorGroup)i, QColorGroup::Dark, backgroundColor.dark(lowlightVal) );
		pal.setColor( (QPalette::ColorGroup)i, QColorGroup::Mid, backgroundColor.dark(120) );
		pal.setColor( (QPalette::ColorGroup)i, QColorGroup::Midlight, backgroundColor.light(110) );
		pal.setColor( (QPalette::ColorGroup)i, QColorGroup::Button, backgroundColor );
		pal.setColor( (QPalette::ColorGroup)i, QColorGroup::Base, backgroundColor );
	    }
        }
        if ( color.isValid() ) {
	    struct ColorSet {
		QPalette::ColorGroup cg;
		QColorGroup::ColorRole cr;
	    };
	    const struct ColorSet toSet [] = {
		{ QPalette::Active, QColorGroup::Foreground },
		{ QPalette::Active, QColorGroup::ButtonText },
		{ QPalette::Active, QColorGroup::Text },
		{ QPalette::Inactive, QColorGroup::Foreground },
		{ QPalette::Inactive, QColorGroup::ButtonText },
		{ QPalette::Inactive, QColorGroup::Text },
		{ QPalette::Disabled,QColorGroup::ButtonText },
		{ QPalette::NColorGroups, QColorGroup::NColorRoles },
	    };
	    const ColorSet *set = toSet;
	    while( set->cg != QPalette::NColorGroups ) {
		pal.setColor( set->cg, set->cr, color );
		++set;
	    }

            QColor disfg = color;
            int h, s, v;
            disfg.hsv( &h, &s, &v );
            if (v > 128)
                // dark bg, light fg - need a darker disabled fg
                disfg = disfg.dark(lowlightVal);
            else if (disfg != Qt::black)
                // light bg, dark fg - need a lighter disabled fg - but only if !black
                disfg = disfg.light(highlightVal);
            else
                // black fg - use darkgrey disabled fg
                disfg = Qt::darkGray;
            pal.setColor(QPalette::Disabled,QColorGroup::Foreground,disfg);
        }

        m_widget->setPalette(pal);
    }
    else
        m_widget->unsetPalette();
}

void RenderFormElement::layout()
{
    KHTMLAssert( !layouted() );
    KHTMLAssert( minMaxKnown() );

    // minimum height
    m_height = 0;

    calcWidth();
    calcHeight();

    if ( m_widget )
        resizeWidget(m_widget,
                     m_width-borderLeft()-borderRight()-paddingLeft()-paddingRight(),
                     m_height-borderLeft()-borderRight()-paddingLeft()-paddingRight());

    if ( !style()->width().isPercent() )
        setLayouted();
}

#ifdef APPLE_CHANGES
void RenderFormElement::performAction(Actions action)
{
    if (m_widget)
        m_widget->endEditing();
        
    if (action == ACTION_BUTTON_CLICKED)
        slotClicked();
}
#endif /* APPLE_CHANGES */

void RenderFormElement::slotClicked()
{
    ref();
    QMouseEvent e2( QEvent::MouseButtonRelease, m_mousePos, m_button, m_state);

    element()->dispatchMouseEvent(&e2, m_isDoubleClick ? EventImpl::KHTML_DBLCLICK_EVENT : EventImpl::KHTML_CLICK_EVENT, m_clickCount);
    m_isDoubleClick = false;
    deref();
}

// -------------------------------------------------------------------------

RenderButton::RenderButton(HTMLGenericFormElementImpl *element)
    : RenderFormElement(element)
{
}

short RenderButton::baselinePosition( bool f ) const
{
    return RenderWidget::baselinePosition( f ) - 2;
}

// -------------------------------------------------------------------------------

RenderCheckBox::RenderCheckBox(HTMLInputElementImpl *element)
    : RenderButton(element)
{
    QCheckBox* b = new QCheckBox(view()->viewport());
    b->setAutoMask(true);
    b->setMouseTracking(true);
    setQWidget(b);
    connect(b,SIGNAL(stateChanged(int)),this,SLOT(slotStateChanged(int)));
    connect(b, SIGNAL(clicked()), this, SLOT(slotClicked()));
}


void RenderCheckBox::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

    QCheckBox *cb = static_cast<QCheckBox *>( m_widget );
    QSize s( cb->style().pixelMetric( QStyle::PM_IndicatorWidth ),
             cb->style().pixelMetric( QStyle::PM_IndicatorHeight ) );
    setIntrinsicWidth( s.width() );
    setIntrinsicHeight( s.height() );

    RenderButton::calcMinMaxWidth();
}

void RenderCheckBox::updateFromElement()
{
    widget()->setChecked(element()->checked());

    RenderButton::updateFromElement();
}

#ifdef APPLE_CHANGES
void RenderCheckBox::performAction(Actions action)
{
    QCheckBox* cb = static_cast<QCheckBox*>( m_widget );

    if (action == ACTION_CHECKBOX_CLICKED)
        slotStateChanged(cb->isChecked() ? 2 : 0);
}
#endif /* APPLE_CHANGES */

// From the Qt documentation:
// state is 2 if the button is on, 1 if it is in the "no change" state or 0 if the button is off. 
void RenderCheckBox::slotStateChanged(int state)
{
    element()->setChecked(state == 2);
}

// -------------------------------------------------------------------------------

RenderRadioButton::RenderRadioButton(HTMLInputElementImpl *element)
    : RenderButton(element)
{
    QRadioButton* b = new QRadioButton(view()->viewport());
    b->setAutoMask(true);
    b->setMouseTracking(true);
    setQWidget(b);
    connect(b, SIGNAL(clicked()), this, SLOT(slotClicked()));
}

void RenderRadioButton::updateFromElement()
{
    widget()->setChecked(element()->checked());

    RenderButton::updateFromElement();
}

void RenderRadioButton::slotClicked()
{
    element()->setChecked(true);

    // emit mouseClick event etc
    RenderButton::slotClicked();
}

void RenderRadioButton::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

    QRadioButton *rb = static_cast<QRadioButton *>( m_widget );
    QSize s( rb->style().pixelMetric( QStyle::PM_ExclusiveIndicatorWidth ),
             rb->style().pixelMetric( QStyle::PM_ExclusiveIndicatorHeight ) );
    setIntrinsicWidth( s.width() );
    setIntrinsicHeight( s.height() );

    RenderButton::calcMinMaxWidth();
}

// -------------------------------------------------------------------------------


RenderSubmitButton::RenderSubmitButton(HTMLInputElementImpl *element)
    : RenderButton(element)
{
    QPushButton* p = new QPushButton(view()->viewport());
    setQWidget(p);
    p->setAutoMask(true);
    p->setMouseTracking(true);
    connect(p, SIGNAL(clicked()), this, SLOT(slotClicked()));
#ifdef APPLE_CHANGES
    // Need to store a reference to this object and then invoke slotClicked on it.
    //p->setAction(&RenderFormElement::slotClicked);
    //p->setRenderObject(this);
    p->setTarget(this);
#endif
}

QString RenderSubmitButton::rawText()
{
    QString value = element()->value().isEmpty() ? defaultLabel() : element()->value().string();
    value = value.stripWhiteSpace();
    QString raw;
    for(unsigned int i = 0; i < value.length(); i++) {
        raw += value[i];
        if(value[i] == '&')
            raw += '&';
    }
    return raw;
}

void RenderSubmitButton::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

#ifndef APPLE_CHANGES
    // Don't use the CSS font metrics.  Don't rely on the button sizing quirks that they
    // have to use because of scalable button fonts. -dwh
    QString raw = rawText();
    QPushButton* pb = static_cast<QPushButton*>(m_widget);
    pb->setText(raw);
    pb->setFont(style()->font());

    bool empty = raw.isEmpty();
    if ( empty )
        raw = QString::fromLatin1("XXXX");
    QFontMetrics fm = pb->fontMetrics();
    int margin = pb->style().pixelMetric( QStyle::PM_ButtonMargin, pb);
    QSize s(pb->style().sizeFromContents(
                QStyle::CT_PushButton, pb, fm.size( ShowPrefix, raw))
            .expandedTo(QApplication::globalStrut()));
    
    setIntrinsicWidth( s.width() - margin / 2 );
    setIntrinsicHeight( s.height() - margin / 2);
#else
    // Instead treat a button as fully replaced.  Let it use its own system UI font
    // and tell us exactly how big it wants to be.
    QSize s(m_widget->sizeHint());
    setIntrinsicWidth( s.width() );
    setIntrinsicHeight( s.height() );
#endif

    RenderButton::calcMinMaxWidth();
}

void RenderSubmitButton::updateFromElement()
{
    QString oldText = static_cast<QPushButton*>(m_widget)->text();
    QString newText = rawText();
    static_cast<QPushButton*>(m_widget)->setText(newText);
    if ( oldText != newText ) {
        setMinMaxKnown(false);
	setLayouted(false);
    }
    RenderFormElement::updateFromElement();
}

QString RenderSubmitButton::defaultLabel() {
    return i18n("Submit");
}

short RenderSubmitButton::baselinePosition( bool f ) const
{
#ifdef APPLE_CHANGES
    return RenderWidget::baselinePosition( f ) - 7;
#else
    return RenderFormElement::baselinePosition( f );
#endif
}

// -------------------------------------------------------------------------------

RenderImageButton::RenderImageButton(HTMLInputElementImpl *element)
    : RenderImage(element)
{
    // ### support DOMActivate event when clicked
#ifdef APPLE_CHANGES
    button = new KWQInvisibleButton(this);
#endif
}

#ifdef APPLE_CHANGES
RenderImageButton::~RenderImageButton()
{
    delete button;
}
    
void RenderImageButton::printObject(QPainter *p, int x, int y, int w, int h, int tx, int ty)
{
    RenderImage::printObject(p, x, y, w, h, tx, ty);
    if (!pixmap().isNull()) {
        button->setFrameInView(tx, ty, contentWidth(), contentHeight(), root()->view());
    }
}
#endif /* APPLE_CHANGES */

// -------------------------------------------------------------------------------

RenderResetButton::RenderResetButton(HTMLInputElementImpl *element)
    : RenderSubmitButton(element)
{
}

QString RenderResetButton::defaultLabel() {
    return i18n("Reset");
}


// -------------------------------------------------------------------------------

RenderPushButton::RenderPushButton(HTMLInputElementImpl *element)
    : RenderSubmitButton(element)
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

RenderLineEdit::RenderLineEdit(HTMLInputElementImpl *element)
    : RenderFormElement(element)
{
    LineEditWidget *edit = new LineEditWidget(view()->viewport());
    connect(edit,SIGNAL(returnPressed()), this, SLOT(slotReturnPressed()));
    connect(edit,SIGNAL(textChanged(const QString &)),this,SLOT(slotTextChanged(const QString &)));

    if(element->inputType() == HTMLInputElementImpl::PASSWORD)
        edit->setEchoMode( QLineEdit::Password );

    if ( element->autoComplete() ) {
        QStringList completions = view()->formCompletionItems(element->name().string());
        if (completions.count()) {
            edit->completionObject()->setItems(completions);
            edit->setContextMenuEnabled(true);
        }
    }

    setQWidget(edit);
#ifdef APPLE_CHANGES
    edit->setTarget (this);
#endif
}

#ifdef APPLE_CHANGES
// We override the baseline position to get the textfield border and padding (before  we hit
// the font descent) taken into account.
short RenderLineEdit::baselinePosition( bool f ) const
{
    return RenderWidget::baselinePosition( f ) - 4;
}
#endif

void RenderLineEdit::slotReturnPressed()
{
    // don't submit the form when return was pressed in a completion-popup
    KCompletionBox *box = widget()->completionBox(false);
    if ( box && box->isVisible() && box->currentItem() != -1 )
	return;

    // Emit onChange if necessary
    // Works but might not be enough, dirk said he had another solution at
    // hand (can't remember which) - David
    handleFocusOut();

    HTMLFormElementImpl* fe = element()->form();
    if ( fe )
        fe->prepareSubmit();
}

void RenderLineEdit::handleFocusOut()
{
    if ( widget() && widget()->edited() ) {
        element()->onChange();
        widget()->setEdited( false );
    }
}

void RenderLineEdit::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

    const QFontMetrics &fm = style()->fontMetrics();
    QSize s;

    int size = element()->size();

    int h = fm.lineSpacing();
    int w = fm.width( 'x' ) * (size > 0 ? size : 17); // "some"
    s = QSize(w + 2 + 2*widget()->frameWidth(),
              QMAX(h, 14) + 2 + 2*widget()->frameWidth())
        .expandedTo(QApplication::globalStrut());

    setIntrinsicWidth( s.width() );
    setIntrinsicHeight( s.height() );

    RenderFormElement::calcMinMaxWidth();
}

void RenderLineEdit::updateFromElement()
{
    int ml = element()->maxLength();
    if ( ml < 0 || ml > 1024 )
        ml = 1024;
    if ( widget()->maxLength() != ml )
        widget()->setMaxLength( ml );

    if (element()->value().string() != widget()->text()) {
        widget()->blockSignals(true);
        int pos = widget()->cursorPosition();
        widget()->setText(element()->value().string());

        widget()->setEdited( false );

        widget()->setCursorPosition(pos);
        widget()->blockSignals(false);
    }
    widget()->setReadOnly(element()->readOnly());

    RenderFormElement::updateFromElement();
}

#ifdef APPLE_CHANGES
void RenderLineEdit::performAction(Actions action)
{
    KLineEdit *edit = static_cast<KLineEdit*>(m_widget);

    if (action == ACTION_TEXT_FIELD_END_EDITING)
        slotTextChanged(edit->text());
    else if (action == ACTION_TEXT_FIELD)
        slotReturnPressed();
}
#endif /* APPLE_CHANGES */

void RenderLineEdit::slotTextChanged(const QString &string)
{
    // don't use setValue here!
    element()->m_value = string;
}

void RenderLineEdit::select()
{
    static_cast<LineEditWidget*>(m_widget)->selectAll();
}

// ---------------------------------------------------------------------------

RenderFieldset::RenderFieldset(HTMLGenericFormElementImpl *element)
    : RenderFormElement(element)
{
}

// -------------------------------------------------------------------------

RenderFileButton::RenderFileButton(HTMLInputElementImpl *element)
    : RenderFormElement(element)
{
    QHBox *w = new QHBox(view()->viewport());

    m_edit = new LineEditWidget(w);

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
    KHTMLAssert( !minMaxKnown() );

    const QFontMetrics &fm = style()->fontMetrics();
    QSize s;
    int size = element()->size();

    int h = fm.lineSpacing();
    int w = fm.width( 'x' ) * (size > 0 ? size : 17); // "some"
    w += 6 + fm.width( m_button->text() ) + 2*fm.width( ' ' );
    s = QSize(w + 2 + 2*m_edit->frameWidth(),
              QMAX(h, 14) + 2 + 2*m_edit->frameWidth())
        .expandedTo(QApplication::globalStrut());

    setIntrinsicWidth( s.width() );
    setIntrinsicHeight( s.height() );

    RenderFormElement::calcMinMaxWidth();
}

void RenderFileButton::handleFocusOut()
{
    if ( m_edit && m_edit->edited() ) {
        element()->onChange();
        m_edit->setEdited( false );
    }
}

void RenderFileButton::slotClicked()
{
    QString file_name = KFileDialog::getOpenFileName(QString::null, QString::null, 0, i18n("Browse..."));
    if (!file_name.isNull()) {
        element()->m_value = DOMString(file_name);
        m_edit->setText(file_name);
    }
}

void RenderFileButton::updateFromElement()
{
    m_edit->blockSignals(true);
    m_edit->setText(element()->value().string());
    m_edit->blockSignals(false);
    int ml = element()->maxLength();
    if ( ml < 0 || ml > 1024 )
        ml = 1024;
    m_edit->setMaxLength( ml );
    m_edit->setEdited( false );

    RenderFormElement::updateFromElement();
}

void RenderFileButton::slotReturnPressed()
{
    if (element()->form())
	element()->form()->prepareSubmit();
}

void RenderFileButton::slotTextChanged(const QString &string)
{
   element()->m_value = DOMString(string);
}

void RenderFileButton::select()
{
    m_edit->selectAll();
}


// -------------------------------------------------------------------------

RenderLabel::RenderLabel(HTMLGenericFormElementImpl *element)
    : RenderFormElement(element)
{

}

// -------------------------------------------------------------------------

RenderLegend::RenderLegend(HTMLGenericFormElementImpl *element)
    : RenderFormElement(element)
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

RenderSelect::RenderSelect(HTMLSelectElementImpl *element)
    : RenderFormElement(element)
{
    m_ignoreSelectEvents = false;
    m_multiple = element->multiple();
    m_size = element->size();
    m_useListBox = (m_multiple || m_size > 1);
    m_selectionChanged = true;
    m_optionsChanged = true;

    if(m_useListBox)
        setQWidget(createListBox());
    else
        setQWidget(createComboBox());
}

void RenderSelect::updateFromElement()
{
    m_ignoreSelectEvents = true;

    // change widget type
    bool oldMultiple = m_multiple;
    unsigned oldSize = m_size;
    bool oldListbox = m_useListBox;

    m_multiple = element()->multiple();
    m_size = element()->size();
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

    // update contents listbox/combobox based on options in m_element
    if ( m_optionsChanged ) {
        if (element()->m_recalcListItems)
            element()->recalcListItems();
        QMemArray<HTMLGenericFormElementImpl*> listItems = element()->listItems();
        int listIndex;

        if(m_useListBox) {
            static_cast<KListBox*>(m_widget)->clear();
        }

        else
            static_cast<KComboBox*>(m_widget)->clear();

#ifdef APPLE_CHANGES
        if(!m_useListBox)
            static_cast<KComboBox*>(m_widget)->setSize(listItems.size());
	else
            static_cast<KListBox*>(m_widget)->beginBatchInsert();
#endif
        for (listIndex = 0; listIndex < int(listItems.size()); listIndex++) {
            if (listItems[listIndex]->id() == ID_OPTGROUP) {
                DOMString text = listItems[listIndex]->getAttribute(ATTR_LABEL);
                if (text.isNull())
                    text = "";

                if(m_useListBox) {
                    QListBoxText *item = new QListBoxText(QString(text.implementation()->s, text.implementation()->l));
                    static_cast<KListBox*>(m_widget)
                        ->insertItem(item, listIndex);
                    item->setSelectable(false);
                }
                else
                    static_cast<KComboBox*>(m_widget)
                        ->insertItem(QString(text.implementation()->s, text.implementation()->l), listIndex);
            }
            else if (listItems[listIndex]->id() == ID_OPTION) {
                DOMString text = static_cast<HTMLOptionElementImpl*>(listItems[listIndex])->text();
                if (text.isNull())
                    text = "";
                if (listItems[listIndex]->parentNode()->id() == ID_OPTGROUP)
                    text = DOMString("    ")+text;

                if(m_useListBox)
                    static_cast<KListBox*>(m_widget)
                        ->insertItem(QString(text.implementation()->s, text.implementation()->l), listIndex);
                else
                    static_cast<KComboBox*>(m_widget)
                        ->insertItem(QString(text.implementation()->s, text.implementation()->l), listIndex);
            }
            else
                KHTMLAssert(false);
            m_selectionChanged = true;
        }
#ifdef APPLE_CHANGES
        if(!m_useListBox)
            static_cast<KComboBox*>(m_widget)->doneLoading();
	else
	    static_cast<KListBox*>(m_widget)->endBatchInsert();
#endif
        setMinMaxKnown(false);
        setLayouted(false);
        m_optionsChanged = false;
    }

    // update selection
    if (m_selectionChanged) {
        updateSelection();
    }


    m_ignoreSelectEvents = false;

    RenderFormElement::updateFromElement();
}

#ifdef APPLE_CHANGES
// Override to deal with our widget.
short RenderSelect::baselinePosition( bool f ) const
{
    return RenderWidget::baselinePosition( f ) - 7;
}
#endif

void RenderSelect::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

    if (m_optionsChanged)
        updateFromElement();

    // ### ugly HACK FIXME!!!
    setMinMaxKnown();
    if ( !layouted() )
        layout();
    setLayouted( false );
    setMinMaxKnown( false );
    // ### end FIXME

    RenderFormElement::calcMinMaxWidth();
}

void RenderSelect::layout( )
{
    KHTMLAssert(!layouted());
    KHTMLAssert(minMaxKnown());

    // ### maintain selection properly between type/size changes, and work
    // out how to handle multiselect->singleselect (probably just select
    // first selected one)

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

#ifdef APPLE_CHANGES
        width += w->scrollBarWidth();
        height = size*height;
        // NSBrowser has problems drawing scrollbar correctly when its size is too small.
        if (height < 60)
            height = 60;
#else
        width += 2*w->frameWidth() + w->verticalScrollBar()->sizeHint().width();
        height = size*height + 2*w->frameWidth();
#endif

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
    QMemArray<HTMLGenericFormElementImpl*> listItems = element()->listItems();

    bool foundOption = false;
    for (uint i = 0; i < listItems.size() && !foundOption; i++)
	foundOption = (listItems[i]->id() == ID_OPTION);

    m_widget->setEnabled(foundOption && ! element()->disabled());
}

void RenderSelect::slotSelected(int index)
{
    if ( m_ignoreSelectEvents ) return;

    KHTMLAssert( !m_useListBox );

    QMemArray<HTMLGenericFormElementImpl*> listItems = element()->listItems();
    if(index >= 0 && index < int(listItems.size()))
    {
        bool found = ( listItems[index]->id() == ID_OPTION );

        if ( !found ) {
            // this one is not selectable,  we need to find an option element
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
        }

        if ( found ) {
            if ( index != static_cast<ComboBoxWidget*>( m_widget )->currentItem() )
                static_cast<ComboBoxWidget*>( m_widget )->setCurrentItem( index );

            for ( unsigned int i = 0; i < listItems.size(); ++i )
                if ( listItems[i]->id() == ID_OPTION && i != (unsigned int) index )
                    static_cast<HTMLOptionElementImpl*>( listItems[i] )->m_selected = false;

            static_cast<HTMLOptionElementImpl*>(listItems[index])->m_selected = true;
        }
    }

    element()->onChange();
}


#ifdef APPLE_CHANGES
void RenderSelect::performAction(Actions action)
{
    if (action == ACTION_LISTBOX_CLICKED)
        slotSelectionChanged();
    else if (action == ACTION_COMBOBOX_CLICKED){
        ComboBoxWidget *combo = static_cast<ComboBoxWidget*>(m_widget);

        slotSelected(combo->indexOfCurrentItem());
    }
}
#endif /* APPLE_CHANGES */

void RenderSelect::slotSelectionChanged()
{
    if ( m_ignoreSelectEvents ) return;

    // don't use listItems() here as we have to avoid recalculations - changing the
    // option list will make use update options not in the way the user expects them
    QMemArray<HTMLGenericFormElementImpl*> listItems = element()->m_listItems;
    for ( unsigned i = 0; i < listItems.count(); i++ )
        // don't use setSelected() here because it will cause us to be called
        // again with updateSelection.
        if ( listItems[i]->id() == ID_OPTION )
            static_cast<HTMLOptionElementImpl*>( listItems[i] )
                ->m_selected = static_cast<KListBox*>( m_widget )->isSelected( i );

    element()->onChange();
}


void RenderSelect::setOptionsChanged(bool _optionsChanged)
{
    m_optionsChanged = _optionsChanged;
}

KListBox* RenderSelect::createListBox()
{
    KListBox *lb = new KListBox(view()->viewport());
    lb->setSelectionMode(m_multiple ? QListBox::Extended : QListBox::Single);
    // ### looks broken
    //lb->setAutoMask(true);
    connect( lb, SIGNAL( selectionChanged() ), this, SLOT( slotSelectionChanged() ) );
    connect( lb, SIGNAL( clicked( QListBoxItem * ) ), this, SLOT( slotClicked() ) );
    m_ignoreSelectEvents = false;
    lb->setMouseTracking(true);

    return lb;
}

ComboBoxWidget *RenderSelect::createComboBox()
{
    ComboBoxWidget *cb = new ComboBoxWidget(view()->viewport());
    connect(cb, SIGNAL(activated(int)), this, SLOT(slotSelected(int)));
    return cb;
}

void RenderSelect::updateSelection()
{
    QMemArray<HTMLGenericFormElementImpl*> listItems = element()->listItems();
    int i;
    if (m_useListBox) {
        // if multi-select, we select only the new selected index
        KListBox *listBox = static_cast<KListBox*>(m_widget);
        for (i = 0; i < int(listItems.size()); i++)
            listBox->setSelected(i,listItems[i]->id() == ID_OPTION &&
                                static_cast<HTMLOptionElementImpl*>(listItems[i])->selected());
    }
    else {
        bool found = false;
        unsigned firstOption = listItems.size();
        i = listItems.size();
        while (i--)
            if (listItems[i]->id() == ID_OPTION) {
                if (found)
                    static_cast<HTMLOptionElementImpl*>(listItems[i])->m_selected = false;
                else if (static_cast<HTMLOptionElementImpl*>(listItems[i])->selected()) {
                    static_cast<KComboBox*>( m_widget )->setCurrentItem(i);
                    found = true;
                }
                firstOption = i;
            }

        Q_ASSERT(firstOption == listItems.size() || found);
    }

    m_selectionChanged = false;
}


// -------------------------------------------------------------------------

TextAreaWidget::TextAreaWidget(int wrap, QWidget* parent)
    : KTextEdit(parent)
{
    if(wrap != DOM::HTMLTextAreaElementImpl::ta_NoWrap) {
        setWordWrap(QTextEdit::WidgetWidth);
        setHScrollBarMode( AlwaysOff );
        setVScrollBarMode( AlwaysOn );
    }
    else {
        setWordWrap(QTextEdit::NoWrap);
        setHScrollBarMode( Auto );
        setVScrollBarMode( Auto );
    }
    KCursor::setAutoHideCursor(viewport(), true);
    setTextFormat(QTextEdit::PlainText);
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
    return KTextEdit::event( e );
}

// -------------------------------------------------------------------------

RenderTextArea::RenderTextArea(HTMLTextAreaElementImpl *element)
    : RenderFormElement(element)
{
    TextAreaWidget *edit = new TextAreaWidget(element->wrap(), view());
    setQWidget(edit);

    connect(edit,SIGNAL(textChanged()),this,SLOT(slotTextChanged()));
}

RenderTextArea::~RenderTextArea()
{
    if ( element()->m_dirtyvalue ) {
        element()->m_value = text();
        element()->m_dirtyvalue = false;
    }
}

void RenderTextArea::handleFocusOut()
{
    TextAreaWidget* w = static_cast<TextAreaWidget*>(m_widget);
    if ( w && element()->m_dirtyvalue ) {
        element()->m_value = text();
        element()->m_dirtyvalue = false;
        element()->onChange();
    }
}

void RenderTextArea::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

    TextAreaWidget* w = static_cast<TextAreaWidget*>(m_widget);
    const QFontMetrics &m = style()->fontMetrics();
    w->setTabStopWidth(8 * m.width(" "));
#ifdef APPLE_CHANGES
    QSize size( QMAX(element()->cols(), 1)*m.width('x') + w->frameWidth() +
                w->verticalScrollBarWidth(),
                QMAX(element()->rows(), 1)*m.height() + w->frameWidth()*2 +
                (w->wordWrap() == QTextEdit::NoWrap ?
                 w->horizontalScrollBarHeight() : 0)
        );
#else
    QSize size( QMAX(element()->cols(), 1)*m.width('x') + w->frameWidth() +
                w->verticalScrollBar()->sizeHint().width(),
                QMAX(element()->rows(), 1)*m.height() + w->frameWidth()*2 +
                (w->wordWrap() == QTextEdit::NoWrap ?
                 w->horizontalScrollBar()->sizeHint().height() : 0)
        );
#endif

    setIntrinsicWidth( size.width() );
    setIntrinsicHeight( size.height() );

    RenderFormElement::calcMinMaxWidth();
}

void RenderTextArea::updateFromElement()
{
    TextAreaWidget* w = static_cast<TextAreaWidget*>(m_widget);
    w->setReadOnly(element()->readOnly());
    w->blockSignals(true);
    int line, col;
    w->getCursorPosition( &line, &col );
    w->setText(element()->value().string());
    w->setCursorPosition( line, col );
    w->blockSignals(false);
    element()->m_dirtyvalue = false;

    RenderFormElement::updateFromElement();
}

void RenderTextArea::close( )
{
    element()->setValue( element()->defaultValue() );

    RenderFormElement::close();
}

QString RenderTextArea::text()
{
    QString txt;
    TextAreaWidget* w = static_cast<TextAreaWidget*>(m_widget);

    if(element()->wrap() == DOM::HTMLTextAreaElementImpl::ta_Physical) {
        // yeah, QTextEdit has no accessor for getting the visually wrapped text
        for (int p=0; p < w->paragraphs(); ++p) {
            int pl = w->paragraphLength(p);
            int ll = 0;
            int lindex = w->lineOfChar(p, 0);
            QString paragraphText = w->text(p);
            for (int l = 0; l < pl; ++l) {
                if (lindex != w->lineOfChar(p, l)) {
                    paragraphText.insert(l+ll++, QString::fromLatin1("\n"));
                    lindex = w->lineOfChar(p, l);
                }
            }
            txt += paragraphText;
            if (p < w->paragraphs() - 1)
                txt += QString::fromLatin1("\n");
        }
    }
    else
        txt = w->text();

    return txt;
}

#ifdef APPLE_CHANGES
void RenderTextArea::performAction(Actions action)
{
    if (action == ACTION_TEXT_AREA_END_EDITING)
        slotTextChanged();
}
#endif /* APPLE_CHANGES */

void RenderTextArea::slotTextChanged()
{
    element()->m_dirtyvalue = true;
}

void RenderTextArea::select()
{
    static_cast<TextAreaWidget *>(m_widget)->selectAll();
}

// ---------------------------------------------------------------------------

#include "render_form.moc"
