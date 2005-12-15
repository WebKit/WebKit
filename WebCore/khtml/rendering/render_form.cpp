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

#include "config.h"
#include <kdebug.h>
#include <klocale.h>
#include <kcursor.h>

#include <qstyle.h>

#include "misc/helper.h"
#include "xml/dom2_eventsimpl.h"
#include "html/html_formimpl.h"

#include "rendering/render_form.h"
#include <assert.h>

#include "khtmlview.h"
#include "khtml_ext.h"
#include "xml/dom_docimpl.h"
#include "xml/EventNames.h"

#include <kdebug.h>

#include "KWQFileButton.h"
#include "KWQSlider.h"

using namespace khtml;
using namespace DOM;
using namespace EventNames;
using namespace HTMLNames;

RenderFormElement::RenderFormElement(HTMLGenericFormElementImpl *element)
    : RenderWidget(element)
{
    setInline(true);
}

RenderFormElement::~RenderFormElement()
{
}

short RenderFormElement::baselinePosition( bool f, bool isRootLineBox ) const
{
    return marginTop() + widget()->baselinePosition(m_height);
}

void RenderFormElement::setStyle(RenderStyle* s)
{
    if (canHaveIntrinsicMargins())
        addIntrinsicMarginsIfAllowed(s);

    RenderWidget::setStyle(s);

    // Do not paint a background or border for Aqua form elements
    setShouldPaintBackgroundOrBorder(false);

    m_widget->setFont(style()->font());
}

void RenderFormElement::updateFromElement()
{
    m_widget->setEnabled(!element()->disabled());

    m_widget->setPalette(QPalette(style()->backgroundColor(), style()->color()));
}

void RenderFormElement::layout()
{
    KHTMLAssert( needsLayout() );
    KHTMLAssert( minMaxKnown() );

    // minimum height
    m_height = 0;

    calcWidth();
    calcHeight();

    
    setNeedsLayout(false);
}

void RenderFormElement::slotClicked()
{
    // FIXME: Should share code with KHTMLView::dispatchMouseEvent, which does a lot of the same stuff.

    RenderArena *arena = ref();

    QMouseEvent event(QEvent::MouseButtonRelease); // gets "current event"
    element()->dispatchMouseEvent(&event, clickEvent, event.clickCount());

    deref(arena);
}

Qt::AlignmentFlags RenderFormElement::textAlignment() const
{
    switch (style()->textAlign()) {
        case LEFT:
        case KHTML_LEFT:
            return AlignLeft;
        case RIGHT:
        case KHTML_RIGHT:
            return AlignRight;
        case CENTER:
        case KHTML_CENTER:
            return AlignHCenter;
        case JUSTIFY:
            // Just fall into the auto code for justify.
        case TAAUTO:
            return style()->direction() == RTL ? AlignRight : AlignLeft;
    }
    assert(false); // Should never be reached.
    return AlignLeft;
}


void RenderFormElement::addIntrinsicMarginsIfAllowed(RenderStyle* _style)
{
    // Cut out the intrinsic margins completely if we end up using mini controls.
    if (_style->font().pixelSize() < 11)
        return;
    
    // FIXME: Using width/height alone and not also dealing with min-width/max-width is flawed.
    int m = intrinsicMargin();
    if (_style->width().isIntrinsicOrAuto()) {
        if (_style->marginLeft().quirk)
            _style->setMarginLeft(Length(m, Fixed));
        if (_style->marginRight().quirk)
            _style->setMarginRight(Length(m, Fixed));
    }

    if (_style->height().isAuto()) {
        if (_style->marginTop().quirk)
            _style->setMarginTop(Length(m, Fixed));
        if (_style->marginBottom().quirk)
            _style->setMarginBottom(Length(m, Fixed));
    }
}

void RenderFormElement::slotTextChanged(const QString &)
{
    // do nothing
}

void RenderFormElement::slotSelectionChanged()
{
    // do nothing
}


// -------------------------------------------------------------------------------

RenderImageButton::RenderImageButton(HTMLInputElementImpl *element)
    : RenderImage(element)
{
    // ### support DOMActivate event when clicked
}

// -------------------------------------------------------------------------------


// -----------------------------------------------------------------------------

RenderLineEdit::RenderLineEdit(HTMLInputElementImpl *element)
    : RenderFormElement(element), m_updating(false)
{
    QLineEdit::Type type;
    switch (element->inputType()) {
        case HTMLInputElementImpl::PASSWORD:
            type = QLineEdit::Password;
            break;
        case HTMLInputElementImpl::SEARCH:
            type = QLineEdit::Search;
            break;
        default:
            type = QLineEdit::Normal;
    }
    KLineEdit *edit = new KLineEdit(type);
    if (type == QLineEdit::Search)
        edit->setLiveSearch(false);
    connect(edit,SIGNAL(returnPressed()), this, SLOT(slotReturnPressed()));
    connect(edit, SIGNAL(selectionChanged()), this, SLOT(slotSelectionChanged()));
    connect(edit,SIGNAL(textChanged(const QString &)),this,SLOT(slotTextChanged(const QString &)));
    connect(edit,SIGNAL(clicked()),this,SLOT(slotClicked()));

    connect(edit,SIGNAL(performSearch()), this, SLOT(slotPerformSearch()));


    setQWidget(edit);
}

void RenderLineEdit::slotSelectionChanged()
{
    QLineEdit* w = static_cast<QLineEdit*>(m_widget);
    
    // We only want to call onselect if there actually is a selection
    if (!w->hasSelectedText())
        return;
    
    element()->onSelect();
}

void RenderLineEdit::slotReturnPressed()
{

    // Emit onChange if necessary
    // Works but might not be enough, dirk said he had another solution at
    // hand (can't remember which) - David
    handleFocusOut();

    HTMLFormElementImpl* fe = element()->form();
    if ( fe )
        fe->submitClick();
}

void RenderLineEdit::slotPerformSearch()
{
    // Fire the "search" DOM event.
    element()->dispatchHTMLEvent(searchEvent, true, false);
}

void RenderLineEdit::addSearchResult()
{
    if (widget())
        widget()->addSearchResult();
}

void RenderLineEdit::handleFocusOut()
{
    if ( widget() && widget()->edited() && element()) {
        element()->onChange();
        widget()->setEdited( false );
    }
}

void RenderLineEdit::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

    // Let the widget tell us how big it wants to be.
    m_updating = true;
    int size = element()->size();
    QSize s(widget()->sizeForCharacterWidth(size > 0 ? size : 20));
    m_updating = false;

    setIntrinsicWidth( s.width() );
    setIntrinsicHeight( s.height() );

    RenderFormElement::calcMinMaxWidth();
}

void RenderLineEdit::setStyle(RenderStyle *s)
{
    RenderFormElement::setStyle(s);

    KLineEdit *w = widget();
    w->setAlignment(textAlignment());
    w->setWritingDirection(style()->direction() == RTL ? QPainter::RTL : QPainter::LTR);
}

void RenderLineEdit::updateFromElement()
{
    HTMLInputElementImpl *e = element();
    KLineEdit *w = widget();
    
    int ml = e->maxLength();
    if ( ml <= 0 || ml > 1024 )
        ml = 1024;
    if ( w->maxLength() != ml )
        w->setMaxLength( ml );

    if (!e->valueMatchesRenderer()) {
        QString widgetText = w->text();
        QString newText = e->value().qstring();
        newText.replace(QChar('\\'), backslashAsCurrencySymbol());
        if (widgetText != newText) {
            w->blockSignals(true);
            int pos = w->cursorPosition();

            m_updating = true;
            w->setText(newText);
            m_updating = false;
            
            w->setEdited( false );

            w->setCursorPosition(pos);
            w->blockSignals(false);
        }
        e->setValueMatchesRenderer();
    }

    w->setReadOnly(e->readOnly());
    
    // Handle updating the search attributes.
    w->setPlaceholderString(e->getAttribute(placeholderAttr).qstring());
    if (w->type() == QLineEdit::Search) {
        w->setLiveSearch(!e->getAttribute(incrementalAttr).isNull());
        w->setAutoSaveName(e->getAttribute(autosaveAttr).qstring());
        w->setMaxResults(e->maxResults());
    }

    RenderFormElement::updateFromElement();
}

void RenderLineEdit::slotTextChanged(const QString &string)
{
    if (m_updating) // Don't alter the value if we are in the middle of initing the control, since
        return;     // we are getting the value from the DOM and it's not user input.

    // A null string value is used to indicate that the form control has not altered the original
    // default value.  That means that we should never use the null string value when the user
    // empties a textfield, but should always force an empty textfield to use the empty string.
    QString newText = string.isNull() ? "" : string;
    newText.replace(backslashAsCurrencySymbol(), QChar('\\'));
    element()->setValueFromRenderer(newText);
}

int RenderLineEdit::selectionStart()
{
    KLineEdit *lineEdit = static_cast<KLineEdit *>(m_widget);
    int start = lineEdit->selectionStart();
    if (start == -1)
        start = lineEdit->cursorPosition();
    return start;
}

int RenderLineEdit::selectionEnd()
{
    KLineEdit *lineEdit = static_cast<KLineEdit *>(m_widget);
    int start = lineEdit->selectionStart();
    if (start == -1)
        return lineEdit->cursorPosition();
    return start + (int)lineEdit->selectedText().length();
}

void RenderLineEdit::setSelectionStart(int start)
{
    int realStart = kMax(start, 0);
    int length = kMax(selectionEnd() - realStart, 0);
    static_cast<KLineEdit *>(m_widget)->setSelection(realStart, length);
}

void RenderLineEdit::setSelectionEnd(int end)
{
    int start = selectionStart();
    int realEnd = kMax(end, 0);
    int length = realEnd - start;
    if (length < 0) {
        start = realEnd;
        length = 0;
    }
    static_cast<KLineEdit *>(m_widget)->setSelection(start, length);
}

void RenderLineEdit::select()
{
    static_cast<KLineEdit*>(m_widget)->selectAll();
}

void RenderLineEdit::setSelectionRange(int start, int end)
{
    int realStart = kMax(start, 0);
    int length = kMax(end - realStart, 0);
    static_cast<KLineEdit *>(m_widget)->setSelection(realStart, length);
}

// ---------------------------------------------------------------------------

RenderFieldset::RenderFieldset(HTMLGenericFormElementImpl *element)
: RenderBlock(element)
{
}

RenderObject* RenderFieldset::layoutLegend(bool relayoutChildren)
{
    RenderObject* legend = findLegend();
    if (legend) {
        if (relayoutChildren)
            legend->setNeedsLayout(true);
        legend->layoutIfNeeded();

        int xPos = borderLeft() + paddingLeft() + legend->marginLeft();
        if (style()->direction() == RTL)
            xPos = m_width - paddingRight() - borderRight() - legend->width() - legend->marginRight();
        int b = borderTop();
        int h = legend->height();
        legend->setPos(xPos, kMax((b-h)/2, 0));
        m_height = kMax(b,h) + paddingTop();
    }
    return legend;
}

RenderObject* RenderFieldset::findLegend()
{
    for (RenderObject* legend = firstChild(); legend; legend = legend->nextSibling()) {
      if (!legend->isFloatingOrPositioned() && legend->element() &&
          legend->element()->hasTagName(legendTag))
        return legend;
    }
    return 0;
}

void RenderFieldset::paintBoxDecorations(PaintInfo& i, int _tx, int _ty)
{
    //kdDebug( 6040 ) << renderName() << "::paintDecorations()" << endl;

    int w = width();
    int h = height() + borderTopExtra() + borderBottomExtra();
    RenderObject* legend = findLegend();
    if (!legend)
        return RenderBlock::paintBoxDecorations(i, _tx, _ty);

    int yOff = (legend->yPos() > 0) ? 0 : (legend->height()-borderTop())/2;
    h -= yOff;
    _ty += yOff - borderTopExtra();

    int my = kMax(_ty, i.r.y());
    int end = kMin(i.r.y() + i.r.height(),  _ty + h);
    int mh = end - my;

    paintBackground(i.p, style()->backgroundColor(), style()->backgroundLayers(), my, mh, _tx, _ty, w, h);

    if (style()->hasBorder())
        paintBorderMinusLegend(i.p, _tx, _ty, w, h, style(), legend->xPos(), legend->width());
}

void RenderFieldset::paintBorderMinusLegend(QPainter *p, int _tx, int _ty, int w, int h,
                                            const RenderStyle* style, int lx, int lw)
{

    const QColor& tc = style->borderTopColor();
    const QColor& bc = style->borderBottomColor();

    EBorderStyle ts = style->borderTopStyle();
    EBorderStyle bs = style->borderBottomStyle();
    EBorderStyle ls = style->borderLeftStyle();
    EBorderStyle rs = style->borderRightStyle();

    bool render_t = ts > BHIDDEN;
    bool render_l = ls > BHIDDEN;
    bool render_r = rs > BHIDDEN;
    bool render_b = bs > BHIDDEN;

    if(render_t) {
        drawBorder(p, _tx, _ty, _tx + lx, _ty +  style->borderTopWidth(), BSTop, tc, style->color(), ts,
                   (render_l && (ls == DOTTED || ls == DASHED || ls == DOUBLE)?style->borderLeftWidth():0), 0);
        drawBorder(p, _tx+lx+lw, _ty, _tx + w, _ty +  style->borderTopWidth(), BSTop, tc, style->color(), ts,
                   0, (render_r && (rs == DOTTED || rs == DASHED || rs == DOUBLE)?style->borderRightWidth():0));
    }

    if(render_b)
        drawBorder(p, _tx, _ty + h - style->borderBottomWidth(), _tx + w, _ty + h, BSBottom, bc, style->color(), bs,
                   (render_l && (ls == DOTTED || ls == DASHED || ls == DOUBLE)?style->borderLeftWidth():0),
                   (render_r && (rs == DOTTED || rs == DASHED || rs == DOUBLE)?style->borderRightWidth():0));

    if(render_l)
    {
        const QColor& lc = style->borderLeftColor();

        bool ignore_top =
            (tc == lc) &&
            (ls >= OUTSET) &&
            (ts == DOTTED || ts == DASHED || ts == SOLID || ts == OUTSET);

        bool ignore_bottom =
            (bc == lc) &&
            (ls >= OUTSET) &&
            (bs == DOTTED || bs == DASHED || bs == SOLID || bs == INSET);

        drawBorder(p, _tx, _ty, _tx + style->borderLeftWidth(), _ty + h, BSLeft, lc, style->color(), ls,
                   ignore_top?0:style->borderTopWidth(),
                   ignore_bottom?0:style->borderBottomWidth());
    }

    if(render_r)
    {
        const QColor& rc = style->borderRightColor();

        bool ignore_top =
            (tc == rc) &&
            (rs >= DOTTED || rs == INSET) &&
            (ts == DOTTED || ts == DASHED || ts == SOLID || ts == OUTSET);

        bool ignore_bottom =
            (bc == rc) &&
            (rs >= DOTTED || rs == INSET) &&
            (bs == DOTTED || bs == DASHED || bs == SOLID || bs == INSET);

        drawBorder(p, _tx + w - style->borderRightWidth(), _ty, _tx + w, _ty + h, BSRight, rc, style->color(), rs,
                   ignore_top?0:style->borderTopWidth(),
                   ignore_bottom?0:style->borderBottomWidth());
    }
}

void RenderFieldset::setStyle(RenderStyle* _style)
{
    RenderBlock::setStyle(_style);

    // WinIE renders fieldsets with display:inline like they're inline-blocks.  For us,
    // an inline-block is just a block element with replaced set to true and inline set
    // to true.  Ensure that if we ended up being inline that we set our replaced flag
    // so that we're treated like an inline-block.
    if (isInline())
        setReplaced(true);
}

// -------------------------------------------------------------------------

RenderFileButton::RenderFileButton(HTMLInputElementImpl *element)
    : RenderFormElement(element)
{
    KWQFileButton *w = new KWQFileButton(view()->part());
    connect(w, SIGNAL(textChanged(const QString &)),this,SLOT(slotTextChanged(const QString &)));
    connect(w, SIGNAL(clicked()), this, SLOT(slotClicked()));
    setQWidget(w);
}

void RenderFileButton::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

    // Let the widget tell us how big it wants to be.
    int size = element()->size();
    QSize s(static_cast<KWQFileButton *>(widget())->sizeForCharacterWidth(size > 0 ? size : 20));

    setIntrinsicWidth( s.width() );
    setIntrinsicHeight( s.height() );

    RenderFormElement::calcMinMaxWidth();
}


void RenderFileButton::slotClicked()
{
    RenderFormElement::slotClicked();
}

void RenderFileButton::updateFromElement()
{
    static_cast<KWQFileButton *>(widget())->setFilename(element()->value().qstring());

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
    element()->onChange();
}

void RenderFileButton::select()
{
}


void RenderFileButton::click(bool sendMouseEvents)
{
    static_cast<KWQFileButton *>(widget())->click(sendMouseEvents);
}


// -------------------------------------------------------------------------

RenderLabel::RenderLabel(HTMLGenericFormElementImpl *element)
    : RenderFormElement(element)
{

}

// -------------------------------------------------------------------------

RenderLegend::RenderLegend(HTMLGenericFormElementImpl *element)
: RenderBlock(element)
{
}

// -------------------------------------------------------------------------------

ComboBoxWidget::ComboBoxWidget(QWidget *parent)
    : KComboBox(false, parent)
{
}

bool ComboBoxWidget::event(QEvent *e)
{
    return KComboBox::event(e);
}

bool ComboBoxWidget::eventFilter(QObject *dest, QEvent *e)
{
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


void RenderSelect::setWidgetWritingDirection()
{
    QPainter::TextDirection d = style()->direction() == RTL ? QPainter::RTL : QPainter::LTR;
    if (m_useListBox)
        static_cast<KListBox *>(m_widget)->setWritingDirection(d);
    else
        static_cast<ComboBoxWidget *>(m_widget)->setWritingDirection(d);
}

void RenderSelect::setStyle(RenderStyle *s)
{
    RenderFormElement::setStyle(s);
    setWidgetWritingDirection();
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
            setWidgetWritingDirection();
        }

        if (m_useListBox && oldMultiple != m_multiple) {
            static_cast<KListBox*>(m_widget)->setSelectionMode(m_multiple ? QListBox::Extended : QListBox::Single);
        }
        m_selectionChanged = true;
        m_optionsChanged = true;
    }

    // update contents listbox/combobox based on options in m_element
    if ( m_optionsChanged ) {
        if (element()->m_recalcListItems)
            element()->recalcListItems();
        QMemArray<HTMLElementImpl*> listItems = element()->listItems();
        int listIndex;

        if (m_useListBox)
            static_cast<KListBox*>(m_widget)->clear();
        else
            static_cast<KComboBox*>(m_widget)->clear();

        bool groupEnabled = true;
        for (listIndex = 0; listIndex < int(listItems.size()); listIndex++) {
            if (listItems[listIndex]->hasTagName(optgroupTag)) {
                HTMLOptGroupElementImpl *optgroupElement = static_cast<HTMLOptGroupElementImpl*>(listItems[listIndex]);
                QString label = optgroupElement->getAttribute(labelAttr).qstring();
                label.replace(QChar('\\'), backslashAsCurrencySymbol());
                
                // In WinIE, an optgroup can't start or end with whitespace (other than the indent
                // we give it).  We match this behavior.
                label = label.stripWhiteSpace();
                // We want to collapse our whitespace too.  This will match other browsers.
                label = label.simplifyWhiteSpace();

                groupEnabled = optgroupElement->isEnabled();
                
                if (m_useListBox)
                    static_cast<KListBox*>(m_widget)->appendGroupLabel(label, groupEnabled);
                else
                    static_cast<KComboBox*>(m_widget)->appendGroupLabel(label);
            }
            else if (listItems[listIndex]->hasTagName(optionTag)) {
                HTMLOptionElementImpl *optionElement = static_cast<HTMLOptionElementImpl*>(listItems[listIndex]);
                QString itemText = optionElement->text().qstring();
                itemText.replace(QChar('\\'), backslashAsCurrencySymbol());

                // In WinIE, leading and trailing whitespace is ignored in options. We match this behavior.
                itemText = itemText.stripWhiteSpace();
                // We want to collapse our whitespace too.  This will match other browsers.
                itemText = itemText.simplifyWhiteSpace();
                
                if (listItems[listIndex]->parentNode()->hasTagName(optgroupTag))
                    itemText.prepend("    ");

                if (m_useListBox)
                    static_cast<KListBox*>(m_widget)->appendItem(itemText, groupEnabled && optionElement->isEnabled());
                else
                    static_cast<KComboBox*>(m_widget)->appendItem(itemText, groupEnabled && optionElement->isEnabled());
            }
            else if (listItems[listIndex]->hasTagName(hrTag)) {
                if (!m_useListBox) {
                    static_cast<KComboBox*>(m_widget)->appendSeparator();
                }
            }
            else
                KHTMLAssert(false);
            m_selectionChanged = true;
        }
        if (m_useListBox)
	    static_cast<KListBox*>(m_widget)->doneAppendingItems();
        setNeedsLayoutAndMinMaxRecalc();
        m_optionsChanged = false;
    }

    // update selection
    if (m_selectionChanged) {
        updateSelection();
    }

    m_ignoreSelectEvents = false;

    RenderFormElement::updateFromElement();
}


short RenderSelect::baselinePosition( bool f, bool isRootLineBox ) const
{
    if (m_useListBox) {
        // FIXME: Should get the hardcoded constant of 7 by calling a QListBox function,
        // as we do for other widget classes.
        return RenderWidget::baselinePosition( f, isRootLineBox ) - 7;
    }
    return RenderFormElement::baselinePosition( f, isRootLineBox );
}


void RenderSelect::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

    if (m_optionsChanged)
        updateFromElement();

    // ### ugly HACK FIXME!!!
    setMinMaxKnown();
    layoutIfNeeded();
    setNeedsLayoutAndMinMaxRecalc();
    // ### end FIXME

    RenderFormElement::calcMinMaxWidth();
}

void RenderSelect::layout( )
{
    KHTMLAssert(needsLayout());
    KHTMLAssert(minMaxKnown());

    // ### maintain selection properly between type/size changes, and work
    // out how to handle multiselect->singleselect (probably just select
    // first selected one)

    // calculate size
    if(m_useListBox) {
        KListBox* w = static_cast<KListBox*>(m_widget);


        int size = m_size;
        // check if multiple and size was not given or invalid
        // Internet Exploder sets size to kMin(number of elements, 4)
        // Netscape seems to simply set it to "number of elements"
        // the average of that is IMHO kMin(number of elements, 10)
        // so I did that ;-)
        if(size < 1)
            size = kMin(static_cast<KListBox*>(m_widget)->count(), 10U);

        // Let the widget tell us how big it wants to be.
        QSize s(w->sizeForNumberOfLines(size));
        setIntrinsicWidth( s.width() );
        setIntrinsicHeight( s.height() );
    }
    else {
        QSize s(m_widget->sizeHint());
        setIntrinsicWidth( s.width() );
        setIntrinsicHeight( s.height() );
    }

    RenderFormElement::layout();

    // and now disable the widget in case there is no <option> given
    QMemArray<HTMLElementImpl*> listItems = element()->listItems();

    bool foundOption = false;
    for (uint i = 0; i < listItems.size() && !foundOption; i++)
	foundOption = (listItems[i]->hasTagName(optionTag));

    m_widget->setEnabled(foundOption && ! element()->disabled());
}

void RenderSelect::slotSelected(int index)
{
    if ( m_ignoreSelectEvents ) return;

    KHTMLAssert( !m_useListBox );

    QMemArray<HTMLElementImpl*> listItems = element()->listItems();
    if(index >= 0 && index < int(listItems.size()))
    {
        bool found = (listItems[index]->hasTagName(optionTag));

        if ( !found ) {
            // this one is not selectable,  we need to find an option element
            while ( ( unsigned ) index < listItems.size() ) {
                if (listItems[index]->hasTagName(optionTag)) {
                    found = true;
                    break;
                }
                ++index;
            }

            if ( !found ) {
                while ( index >= 0 ) {
                    if (listItems[index]->hasTagName(optionTag)) {
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
                if (listItems[i]->hasTagName(optionTag) && i != (unsigned int) index)
                    static_cast<HTMLOptionElementImpl*>( listItems[i] )->m_selected = false;

            static_cast<HTMLOptionElementImpl*>(listItems[index])->m_selected = true;
        }
    }

    element()->onChange();
}


void RenderSelect::slotSelectionChanged()
{
    if ( m_ignoreSelectEvents ) return;

    // don't use listItems() here as we have to avoid recalculations - changing the
    // option list will make use update options not in the way the user expects them
    QMemArray<HTMLElementImpl*> listItems = element()->m_listItems;
    int j = 0;
    for ( unsigned i = 0; i < listItems.count(); i++ ) {
        // don't use setSelected() here because it will cause us to be called
        // again with updateSelection.
        if (listItems[i]->hasTagName(optionTag))
            static_cast<HTMLOptionElementImpl*>( listItems[i] )
                ->m_selected = static_cast<KListBox*>( m_widget )->isSelected( j );
        if (listItems[i]->hasTagName(optionTag) || listItems[i]->hasTagName(optgroupTag))
            ++j;
    }
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
    connect( lb, SIGNAL( selectionChanged() ), this, SLOT( slotSelectionChanged() ) );
    connect( lb, SIGNAL( clicked( QListBoxItem * ) ), this, SLOT( slotClicked() ) );
    m_ignoreSelectEvents = false;

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
    QMemArray<HTMLElementImpl*> listItems = element()->listItems();
    int i;
    if (m_useListBox) {
        // if multi-select, we select only the new selected index
        KListBox *listBox = static_cast<KListBox*>(m_widget);
        int j = 0;
        for (i = 0; i < int(listItems.size()); i++) {
            listBox->setSelected(j, listItems[i]->hasTagName(optionTag) &&
                                static_cast<HTMLOptionElementImpl*>(listItems[i])->selected());
            if (listItems[i]->hasTagName(optionTag) || listItems[i]->hasTagName(optgroupTag))
                ++j;
            
        }
    }
    else {
        bool found = false;
        unsigned firstOption = listItems.size();
        i = listItems.size();
        while (i--)
            if (listItems[i]->hasTagName(optionTag)) {
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


// -------------------------------------------------------------------------

RenderTextArea::RenderTextArea(HTMLTextAreaElementImpl *element)
    : RenderFormElement(element), m_dirty(false)
{
    QTextEdit *edit = new KTextEdit(view());

    if (element->wrap() != HTMLTextAreaElementImpl::ta_NoWrap)
        edit->setWordWrap(QTextEdit::WidgetWidth);
    else
        edit->setWordWrap(QTextEdit::NoWrap);


    setQWidget(edit);

    connect(edit,SIGNAL(textChanged()),this,SLOT(slotTextChanged()));
    connect(edit,SIGNAL(clicked()),this,SLOT(slotClicked()));
    connect(edit,SIGNAL(selectionChanged()),this,SLOT(slotSelectionChanged()));
}

void RenderTextArea::destroy()
{
    element()->updateValue();
    RenderFormElement::destroy();
}

void RenderTextArea::handleFocusOut()
{
    if ( m_dirty && element() ) {
        element()->updateValue();
        element()->onChange();
    }
    m_dirty = false;
}

void RenderTextArea::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

    QTextEdit* w = static_cast<QTextEdit*>(m_widget);
    QSize size(w->sizeWithColumnsAndRows(kMax(element()->cols(), 1), kMax(element()->rows(), 1)));

    setIntrinsicWidth( size.width() );
    setIntrinsicHeight( size.height() );

    RenderFormElement::calcMinMaxWidth();
}

void RenderTextArea::setStyle(RenderStyle *s)
{
    RenderFormElement::setStyle(s);

    QTextEdit* w = static_cast<QTextEdit*>(m_widget);
    w->setAlignment(textAlignment());
    w->setLineHeight(RenderObject::lineHeight(true));

    w->setWritingDirection(style()->direction() == RTL ? QPainter::RTL : QPainter::LTR);

    QScrollView::ScrollBarMode scrollMode = QScrollView::Auto;
    switch (style()->overflow()) {
        case OAUTO:
        case OMARQUEE: // makes no sense, map to auto
        case OOVERLAY: // not implemented for text, map to auto
        case OVISIBLE:
            break;
        case OHIDDEN:
            scrollMode = QScrollView::AlwaysOff;
            break;
        case OSCROLL:
            scrollMode = QScrollView::AlwaysOn;
            break;
    }
    QScrollView::ScrollBarMode horizontalScrollMode = scrollMode;
    if (element()->wrap() != HTMLTextAreaElementImpl::ta_NoWrap)
        horizontalScrollMode = QScrollView::AlwaysOff;

    w->setScrollBarModes(horizontalScrollMode, scrollMode);
}

void RenderTextArea::updateFromElement()
{
    HTMLTextAreaElementImpl *e = element();
    QTextEdit* w = static_cast<QTextEdit*>(m_widget);

    w->setReadOnly(e->readOnly());
    w->setDisabled(e->disabled());

    e->updateValue();
    if (!e->valueMatchesRenderer()) {
        QString widgetText = text();
        QString text = e->value().qstring();
        text.replace(QChar('\\'), backslashAsCurrencySymbol());
        if (widgetText != text) {
            w->blockSignals(true);
            int line, col;
            w->getCursorPosition( &line, &col );
            w->setText(text);
            w->setCursorPosition( line, col );
            w->blockSignals(false);
        }
        e->setValueMatchesRenderer();
        m_dirty = false;
    }

    RenderFormElement::updateFromElement();
}

QString RenderTextArea::text()
{
    QString txt;
    QTextEdit* w = static_cast<QTextEdit*>(m_widget);

    if (element()->wrap() == HTMLTextAreaElementImpl::ta_Physical) {
        txt = w->textWithHardLineBreaks();
    }
    else
        txt = w->text();

    txt.replace(backslashAsCurrencySymbol(), QChar('\\'));
    return txt;
}

void RenderTextArea::slotTextChanged()
{
    element()->invalidateValue();
    m_dirty = true;
}

int RenderTextArea::selectionStart()
{
    QTextEdit *textEdit = static_cast<QTextEdit *>(m_widget);
    return textEdit->selectionStart();
}

int RenderTextArea::selectionEnd()
{
    QTextEdit *textEdit = static_cast<QTextEdit *>(m_widget);
    return textEdit->selectionEnd();
}

void RenderTextArea::setSelectionStart(int start)
{
    QTextEdit *textEdit = static_cast<QTextEdit *>(m_widget);
    textEdit->setSelectionStart(start);
}

void RenderTextArea::setSelectionEnd(int end)
{
    QTextEdit *textEdit = static_cast<QTextEdit *>(m_widget);
    textEdit->setSelectionEnd(end);
}

void RenderTextArea::select()
{
    static_cast<QTextEdit *>(m_widget)->selectAll();
}

void RenderTextArea::setSelectionRange(int start, int end)
{
    QTextEdit *textEdit = static_cast<QTextEdit *>(m_widget);
    textEdit->setSelectionRange(start, end-start);
}

void RenderTextArea::slotSelectionChanged()
{
    QTextEdit* w = static_cast<QTextEdit*>(m_widget);

    // We only want to call onselect if there actually is a selection
    if (!w->hasSelectedText())
        return;
    
    element()->onSelect();
}

// ---------------------------------------------------------------------------

RenderSlider::RenderSlider(HTMLInputElementImpl* element)
:RenderFormElement(element)
{
    QSlider* slider = new QSlider();
    setQWidget(slider);
    connect(slider, SIGNAL(sliderValueChanged()), this, SLOT(slotSliderValueChanged()));
    connect(slider, SIGNAL(clicked()), this, SLOT(slotClicked()));
}

void RenderSlider::calcMinMaxWidth()
{
    KHTMLAssert(!minMaxKnown());
    
    // Let the widget tell us how big it wants to be.
    QSize s(widget()->sizeHint());
    bool widthSet = !style()->width().isAuto();
    bool heightSet = !style()->height().isAuto();
    if (heightSet && !widthSet) {
        // Flip the intrinsic dimensions.
        int barLength = s.width();
        s = QSize(s.height(), barLength);
    }
    setIntrinsicWidth(s.width());
    setIntrinsicHeight(s.height());
    
    RenderFormElement::calcMinMaxWidth();
}

void RenderSlider::updateFromElement()
{
    const DOMString& value = element()->value();
    const DOMString& min = element()->getAttribute(minAttr);
    const DOMString& max = element()->getAttribute(maxAttr);
    const DOMString& precision = element()->getAttribute(precisionAttr);
    
    double minVal = min.isNull() ? 0.0 : min.qstring().toDouble();
    double maxVal = max.isNull() ? 100.0 : max.qstring().toDouble();
    minVal = kMin(minVal, maxVal); // Make sure the range is sane.
    
    double val = value.isNull() ? (maxVal + minVal)/2.0 : value.qstring().toDouble();
    val = kMax(minVal, kMin(val, maxVal)); // Make sure val is within min/max.
    
    // Force integer value if not float (strcasecmp returns confusingly backward boolean).
    if (strcasecmp(precision, "float"))
        val = (int)(val + 0.5);

    element()->setValue(QString::number(val));

    QSlider* slider = (QSlider*)widget();
     
    slider->setMinValue(minVal);
    slider->setMaxValue(maxVal);
    slider->setValue(val);

    RenderFormElement::updateFromElement();
}

void RenderSlider::slotSliderValueChanged()
{
    QSlider* slider = (QSlider*)widget();

    double val = slider->value();
    const DOMString& precision = element()->getAttribute(precisionAttr);

    // Force integer value if not float (strcasecmp returns confusingly backward boolean).
    if (strcasecmp(precision, "float"))
        val = (int)(val + 0.5);

    element()->setValue(QString::number(val));
    
    // Fire the "input" DOM event.
    element()->dispatchHTMLEvent(inputEvent, true, false);
}

void RenderSlider::slotClicked()
{
    // emit mouseClick event etc
    RenderFormElement::slotClicked();
}
