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

#if APPLE_CHANGES
#include "KWQFileButton.h"
#include "KWQSlider.h"
#endif

using namespace khtml;
using namespace DOM;

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
#if APPLE_CHANGES
    return marginTop() + widget()->baselinePosition(m_height);
#else
    return RenderWidget::baselinePosition( f, isRootLineBox ) - 2 - style()->fontMetrics().descent();
#endif
}

void RenderFormElement::setStyle(RenderStyle* s)
{
#if APPLE_CHANGES
    if (canHaveIntrinsicMargins())
        addIntrinsicMarginsIfAllowed(s);
#endif

    RenderWidget::setStyle(s);

#if APPLE_CHANGES
    // Do not paint a background or border for Aqua form elements
    setShouldPaintBackgroundOrBorder(false);
#endif

    m_widget->setFont(style()->font());
}

void RenderFormElement::updateFromElement()
{
    m_widget->setEnabled(!element()->disabled());

#if APPLE_CHANGES
    m_widget->setPalette(QPalette(style()->backgroundColor(), style()->color()));
#else
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
#endif
}

void RenderFormElement::layout()
{
    KHTMLAssert( needsLayout() );
    KHTMLAssert( minMaxKnown() );

    // minimum height
    m_height = 0;

    calcWidth();
    calcHeight();

#if !APPLE_CHANGES
    if ( m_widget )
        resizeWidget(m_widget,
                     m_width-borderLeft()-borderRight()-paddingLeft()-paddingRight(),
                     m_height-borderLeft()-borderRight()-paddingLeft()-paddingRight());
#endif
    
    setNeedsLayout(false);
}

void RenderFormElement::slotClicked()
{
    // FIXME: Should share code with KHTMLView::dispatchMouseEvent, which does a lot of the same stuff.

    RenderArena *arena = ref();

#if APPLE_CHANGES
    QMouseEvent event(QEvent::MouseButtonRelease); // gets "current event"
    element()->dispatchMouseEvent(&event, EventImpl::CLICK_EVENT, event.clickCount());
#else
    // We also send the KHTML_CLICK or KHTML_DBLCLICK event for
    // CLICK. This is not part of the DOM specs, but is used for
    // compatibility with the traditional onclick="" and ondblclick=""
    // attributes, as there is no way to tell the difference between
    // single & double clicks using DOM (only the click count is
    // stored, which is not necessarily the same)

    QMouseEvent e2(QEvent::MouseButtonRelease, m_mousePos, m_button, m_state);
    element()->dispatchMouseEvent(&e2, EventImpl::CLICK_EVENT, m_clickCount);
    element()->dispatchMouseEvent(&e2, m_isDoubleClick ? EventImpl::KHTML_DBLCLICK_EVENT : EventImpl::KHTML_CLICK_EVENT, m_clickCount);
#endif

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

#if APPLE_CHANGES

void RenderFormElement::addIntrinsicMarginsIfAllowed(RenderStyle* _style)
{
    // Cut out the intrinsic margins completely if we end up using mini controls.
    if (_style->font().pixelSize() < 11)
        return;
    
    int m = intrinsicMargin();
    if (_style->width().isVariable()) {
        if (_style->marginLeft().quirk)
            _style->setMarginLeft(Length(m, Fixed));
        if (_style->marginRight().quirk)
            _style->setMarginRight(Length(m, Fixed));
    }

    if (_style->height().isVariable()) {
        if (_style->marginTop().quirk)
            _style->setMarginTop(Length(m, Fixed));
        if (_style->marginBottom().quirk)
            _style->setMarginBottom(Length(m, Fixed));
    }
}

#endif

// -------------------------------------------------------------------------

RenderButton::RenderButton(HTMLGenericFormElementImpl *element)
    : RenderFormElement(element)
{
}

short RenderButton::baselinePosition( bool f, bool isRootLineBox ) const
{
#if APPLE_CHANGES
    return RenderFormElement::baselinePosition( f, isRootLineBox );
#else
    return RenderWidget::baselinePosition( f, isRootLineBox ) - 2;
#endif
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

#if APPLE_CHANGES
    // Let the widget tell us how big it wants to be.
    QSize s(widget()->sizeHint());
#else
    QCheckBox *cb = static_cast<QCheckBox *>( m_widget );
    QSize s( cb->style().pixelMetric( QStyle::PM_IndicatorWidth ),
             cb->style().pixelMetric( QStyle::PM_IndicatorHeight ) );
#endif
    setIntrinsicWidth( s.width() );
    setIntrinsicHeight( s.height() );

    RenderButton::calcMinMaxWidth();
}

void RenderCheckBox::updateFromElement()
{
    widget()->setChecked(element()->checked());

    RenderButton::updateFromElement();
}

// From the Qt documentation:
// state is 2 if the button is on, 1 if it is in the "no change" state or 0 if the button is off. 
void RenderCheckBox::slotStateChanged(int state)
{
    element()->setChecked(state == 2);
    element()->onChange();
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

#if APPLE_CHANGES
    // Let the widget tell us how big it wants to be.
    QSize s(widget()->sizeHint());
#else
    QRadioButton *rb = static_cast<QRadioButton *>( m_widget );
    QSize s( rb->style().pixelMetric( QStyle::PM_ExclusiveIndicatorWidth ),
             rb->style().pixelMetric( QStyle::PM_ExclusiveIndicatorHeight ) );
#endif
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
}

QString RenderSubmitButton::rawText()
{
    QString value = element()->valueWithDefault().string();
    value = value.stripWhiteSpace();
    value.replace(QChar('\\'), backslashAsCurrencySymbol());
#if APPLE_CHANGES
    return value;
#else
    QString raw;
    for(unsigned int i = 0; i < value.length(); i++) {
        raw += value[i];
        if(value[i] == '&')
            raw += '&';
    }
    return raw;
#endif
}

void RenderSubmitButton::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

#if APPLE_CHANGES
    // Let the widget tell us how big it wants to be.
    QSize s(widget()->sizeHint());
    setIntrinsicWidth(s.width());
    setIntrinsicHeight(s.height());
#else
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
#endif

    RenderButton::calcMinMaxWidth();
}

#if APPLE_CHANGES

void RenderSubmitButton::setStyle(RenderStyle *s)
{
    RenderButton::setStyle(s);
    
    QPushButton *w = static_cast<QPushButton*>(m_widget);
    w->setWritingDirection(style()->direction() == RTL ? QPainter::RTL : QPainter::LTR);
}

#endif

void RenderSubmitButton::updateFromElement()
{
    QPushButton *w = static_cast<QPushButton*>(m_widget);

    QString oldText = w->text();
    QString newText = rawText();
    w->setText(newText);
    if ( oldText != newText )
        setNeedsLayoutAndMinMaxRecalc();
    RenderFormElement::updateFromElement();
}

short RenderSubmitButton::baselinePosition( bool f, bool isRootLineBox ) const
{
    return RenderFormElement::baselinePosition( f, isRootLineBox );
}

// -------------------------------------------------------------------------------

RenderImageButton::RenderImageButton(HTMLInputElementImpl *element)
    : RenderImage(element)
{
    // ### support DOMActivate event when clicked
}

// -------------------------------------------------------------------------------

// FIXME: No real reason to need separate classes for RenderResetButton and
// RenderSubmitButton now that the default label is handled on the DOM side.
RenderResetButton::RenderResetButton(HTMLInputElementImpl *element)
    : RenderSubmitButton(element)
{
}

// -------------------------------------------------------------------------------

// FIXME: No real reason to need separate classes for RenderPushButton and
// RenderSubmitButton now that the default label is handled on the DOM side.
RenderPushButton::RenderPushButton(HTMLInputElementImpl *element)
    : RenderSubmitButton(element)
{
}

// -------------------------------------------------------------------------------

#if !APPLE_CHANGES

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

#endif

// -----------------------------------------------------------------------------

RenderLineEdit::RenderLineEdit(HTMLInputElementImpl *element)
    : RenderFormElement(element), m_updating(false)
{
#if APPLE_CHANGES
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
#else
    LineEditWidget *edit = new LineEditWidget(view()->viewport());
#endif
    connect(edit,SIGNAL(returnPressed()), this, SLOT(slotReturnPressed()));
    connect(edit,SIGNAL(textChanged(const QString &)),this,SLOT(slotTextChanged(const QString &)));
    connect(edit,SIGNAL(clicked()),this,SLOT(slotClicked()));

#if APPLE_CHANGES
    connect(edit,SIGNAL(performSearch()), this, SLOT(slotPerformSearch()));
#endif

#if !APPLE_CHANGES
    if(element->inputType() == HTMLInputElementImpl::PASSWORD)
        edit->setEchoMode( QLineEdit::Password );

    if ( element->autoComplete() ) {
        QStringList completions = view()->formCompletionItems(element->name().string());
        if (completions.count()) {
            edit->completionObject()->setItems(completions);
            edit->setContextMenuEnabled(true);
        }
    }
#endif

    setQWidget(edit);
}

void RenderLineEdit::slotReturnPressed()
{
#if !APPLE_CHANGES
    // don't submit the form when return was pressed in a completion-popup
    KCompletionBox *box = widget()->completionBox(false);
    if ( box && box->isVisible() && box->currentItem() != -1 )
	return;
#endif

    // Emit onChange if necessary
    // Works but might not be enough, dirk said he had another solution at
    // hand (can't remember which) - David
    handleFocusOut();

    HTMLFormElementImpl* fe = element()->form();
    if ( fe )
        fe->submitClick();
}

#if APPLE_CHANGES
void RenderLineEdit::slotPerformSearch()
{
    // Fire the "search" DOM event.
    element()->dispatchHTMLEvent(EventImpl::SEARCH_EVENT, true, false);
}

void RenderLineEdit::addSearchResult()
{
    if (widget())
        widget()->addSearchResult();
}
#endif

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

#if APPLE_CHANGES
    // Let the widget tell us how big it wants to be.
    m_updating = true;
    int size = element()->size();
    QSize s(widget()->sizeForCharacterWidth(size > 0 ? size : 20));
    m_updating = false;
#else
    const QFontMetrics &fm = style()->fontMetrics();
    QSize s;

    int size = element()->size();

    int h = fm.lineSpacing();
    int w = fm.width( 'x' ) * (size > 0 ? size : 17); // "some"
    s = QSize(w + 2 + 2*widget()->frameWidth(),
              kMax(h, 14) + 2 + 2*widget()->frameWidth())
        .expandedTo(QApplication::globalStrut());
#endif

    setIntrinsicWidth( s.width() );
    setIntrinsicHeight( s.height() );

    RenderFormElement::calcMinMaxWidth();
}

void RenderLineEdit::setStyle(RenderStyle *s)
{
    RenderFormElement::setStyle(s);

    KLineEdit *w = widget();
    w->setAlignment(textAlignment());
#if APPLE_CHANGES
    w->setWritingDirection(style()->direction() == RTL ? QPainter::RTL : QPainter::LTR);
#endif
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
        QString newText = e->value().string();
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
    
#if APPLE_CHANGES
    // Handle updating the search attributes.
    w->setPlaceholderString(e->getAttribute(ATTR_PLACEHOLDER).string());
    if (w->type() == QLineEdit::Search) {
        w->setLiveSearch(!e->getAttribute(ATTR_INCREMENTAL).isNull());
        w->setAutoSaveName(e->getAttribute(ATTR_AUTOSAVE).string());
        w->setMaxResults(e->maxResults());
    }
#endif

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

void RenderLineEdit::select()
{
    static_cast<KLineEdit*>(m_widget)->selectAll();
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
          legend->element()->id() == ID_LEGEND)
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
#if APPLE_CHANGES
    KWQFileButton *w = new KWQFileButton(view()->part());
    connect(w, SIGNAL(textChanged(const QString &)),this,SLOT(slotTextChanged(const QString &)));
    connect(w, SIGNAL(clicked()), this, SLOT(slotClicked()));
    setQWidget(w);
#else
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
#endif
}

void RenderFileButton::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

#if APPLE_CHANGES
    // Let the widget tell us how big it wants to be.
    int size = element()->size();
    QSize s(static_cast<KWQFileButton *>(widget())->sizeForCharacterWidth(size > 0 ? size : 20));
#else
    const QFontMetrics &fm = style()->fontMetrics();
    QSize s;
    int size = element()->size();

    int h = fm.lineSpacing();
    int w = fm.width( 'x' ) * (size > 0 ? size : 17); // "some"
    w += 6 + fm.width( m_button->text() ) + 2*fm.width( ' ' );
    s = QSize(w + 2 + 2*m_edit->frameWidth(),
              kMax(h, 14) + 2 + 2*m_edit->frameWidth())
        .expandedTo(QApplication::globalStrut());
#endif

    setIntrinsicWidth( s.width() );
    setIntrinsicHeight( s.height() );

    RenderFormElement::calcMinMaxWidth();
}

#if !APPLE_CHANGES

void RenderFileButton::handleFocusOut()
{
    if ( m_edit && m_edit->edited() ) {
        element()->onChange();
        m_edit->setEdited( false );
    }
}

#endif

void RenderFileButton::slotClicked()
{
#if APPLE_CHANGES
    RenderFormElement::slotClicked();
#else
    QString file_name = KFileDialog::getOpenFileName(QString::null, QString::null, 0, i18n("Browse..."));
    if (!file_name.isNull()) {
        element()->m_value = DOMString(file_name);
        m_edit->setText(file_name);
    }
#endif
}

void RenderFileButton::updateFromElement()
{
#if APPLE_CHANGES
    static_cast<KWQFileButton *>(widget())->setFilename(element()->value().string());
#else
    m_edit->blockSignals(true);
    m_edit->setText(element()->value().string());
    m_edit->blockSignals(false);
    int ml = element()->maxLength();
    if ( ml <= 0 || ml > 1024 )
        ml = 1024;
    m_edit->setMaxLength( ml );
    m_edit->setEdited( false );
#endif

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
#if !APPLE_CHANGES
    m_edit->selectAll();
#endif
}

#if APPLE_CHANGES

void RenderFileButton::click(bool sendMouseEvents)
{
    static_cast<KWQFileButton *>(widget())->click(sendMouseEvents);
}

#endif

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
    setAutoMask(true);
    if (listBox()) listBox()->installEventFilter(this);
    setMouseTracking(true);
}

bool ComboBoxWidget::event(QEvent *e)
{
#if !APPLE_CHANGES
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
#endif
    return KComboBox::event(e);
}

bool ComboBoxWidget::eventFilter(QObject *dest, QEvent *e)
{
#if !APPLE_CHANGES
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
#endif
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

#if APPLE_CHANGES

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

#endif

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
#if APPLE_CHANGES
            setWidgetWritingDirection();
#endif
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
        QMemArray<HTMLGenericFormElementImpl*> listItems = element()->listItems();
        int listIndex;

        if (m_useListBox)
            static_cast<KListBox*>(m_widget)->clear();
        else
            static_cast<KComboBox*>(m_widget)->clear();

        for (listIndex = 0; listIndex < int(listItems.size()); listIndex++) {
            if (listItems[listIndex]->id() == ID_OPTGROUP) {
                QString label = listItems[listIndex]->getAttribute(ATTR_LABEL).string();
                label.replace(QChar('\\'), backslashAsCurrencySymbol());

                // In WinIE, an optgroup can't start or end with whitespace (other than the indent
                // we give it).  We match this behavior.
                label = label.stripWhiteSpace();
                
#if APPLE_CHANGES
                if (m_useListBox)
                    static_cast<KListBox*>(m_widget)->appendGroupLabel(label);
                else
                    static_cast<KComboBox*>(m_widget)->appendGroupLabel(label);
#else
                if(m_useListBox) {
                    QListBoxText *item = new QListBoxText(label);
                    static_cast<KListBox*>(m_widget)
                        ->insertItem(item, listIndex);
                    item->setSelectable(false);
                }
                else
                    static_cast<KComboBox*>(m_widget)->insertItem(label, listIndex);
#endif
            }
            else if (listItems[listIndex]->id() == ID_OPTION) {
                QString itemText = static_cast<HTMLOptionElementImpl*>(listItems[listIndex])->text().string();
                itemText.replace(QChar('\\'), backslashAsCurrencySymbol());

                // In WinIE, leading and trailing whitespace is ignored in options. We match this behavior.
                itemText = itemText.stripWhiteSpace();
                
                if (listItems[listIndex]->parentNode()->id() == ID_OPTGROUP)
                    itemText.prepend("    ");

#if APPLE_CHANGES
                if (m_useListBox)
                    static_cast<KListBox*>(m_widget)->appendItem(itemText);
                else
                    static_cast<KComboBox*>(m_widget)->appendItem(itemText);
#else
                if(m_useListBox)
                    static_cast<KListBox*>(m_widget)->insertItem(itemText, listIndex);
                else
                    static_cast<KComboBox*>(m_widget)->insertItem(itemText, listIndex);
#endif
            }
            else
                KHTMLAssert(false);
            m_selectionChanged = true;
        }
#if APPLE_CHANGES
        if (m_useListBox)
	    static_cast<KListBox*>(m_widget)->doneAppendingItems();
#endif
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

#if APPLE_CHANGES

short RenderSelect::baselinePosition( bool f, bool isRootLineBox ) const
{
    if (m_useListBox) {
        // FIXME: Should get the hardcoded constant of 7 by calling a QListBox function,
        // as we do for other widget classes.
        return RenderWidget::baselinePosition( f, isRootLineBox ) - 7;
    }
    return RenderFormElement::baselinePosition( f, isRootLineBox );
}

#endif

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

#if !APPLE_CHANGES
        QListBoxItem* p = w->firstItem();
        int width = 0;
        int height = 0;
        while(p) {
            width = kMax(width, p->width(p->listBox()));
            height = kMax(height, p->height(p->listBox()));
            p = p->next();
        }
#endif

        int size = m_size;
        // check if multiple and size was not given or invalid
        // Internet Exploder sets size to kMin(number of elements, 4)
        // Netscape seems to simply set it to "number of elements"
        // the average of that is IMHO kMin(number of elements, 10)
        // so I did that ;-)
        if(size < 1)
            size = kMin(static_cast<KListBox*>(m_widget)->count(), 10U);

#if APPLE_CHANGES
        // Let the widget tell us how big it wants to be.
        QSize s(w->sizeForNumberOfLines(size));
        setIntrinsicWidth( s.width() );
        setIntrinsicHeight( s.height() );
#else
        width += 2*w->frameWidth() + w->verticalScrollBar()->sizeHint().width();
        height = size*height + 2*w->frameWidth();

        setIntrinsicWidth( width );
        setIntrinsicHeight( height );
#endif
    }
    else {
        QSize s(m_widget->sizeHint());
        setIntrinsicWidth( s.width() );
        setIntrinsicHeight( s.height() );
    }

    /// uuh, ignore the following line..
    setNeedsLayout(true);
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

#if !APPLE_CHANGES

TextAreaWidget::TextAreaWidget(QWidget* parent)
    : KTextEdit(parent)
{
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

#endif

// -------------------------------------------------------------------------

RenderTextArea::RenderTextArea(HTMLTextAreaElementImpl *element)
    : RenderFormElement(element), m_dirty(false)
{
#if APPLE_CHANGES
    QTextEdit *edit = new KTextEdit(view());
#else
    QTextEdit *edit = new TextAreaWidget(view());
#endif

    if (element->wrap() != HTMLTextAreaElementImpl::ta_NoWrap)
        edit->setWordWrap(QTextEdit::WidgetWidth);
    else
        edit->setWordWrap(QTextEdit::NoWrap);

#if !APPLE_CHANGES
    KCursor::setAutoHideCursor(edit->viewport(), true);
    edit->setTextFormat(QTextEdit::PlainText);
    edit->setAutoMask(true);
    edit->setMouseTracking(true);
#endif

    setQWidget(edit);

    connect(edit,SIGNAL(textChanged()),this,SLOT(slotTextChanged()));
    connect(edit,SIGNAL(clicked()),this,SLOT(slotClicked()));
}

void RenderTextArea::detach()
{
    element()->updateValue();
    RenderFormElement::detach();
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
#if APPLE_CHANGES
    QSize size(w->sizeWithColumnsAndRows(kMax(element()->cols(), 1L), kMax(element()->rows(), 1L)));
#else
    const QFontMetrics &m = style()->fontMetrics();
    w->setTabStopWidth(8 * m.width(" "));
    QSize size( kMax(element()->cols(), 1)*m.width('x') + w->frameWidth() +
                w->verticalScrollBar()->sizeHint().width(),
                kMax(element()->rows(), 1)*m.height() + w->frameWidth()*2 +
                (w->wordWrap() == QTextEdit::NoWrap ?
                 w->horizontalScrollBar()->sizeHint().height() : 0)
        );
#endif

    setIntrinsicWidth( size.width() );
    setIntrinsicHeight( size.height() );

    RenderFormElement::calcMinMaxWidth();
}

void RenderTextArea::setStyle(RenderStyle *s)
{
    RenderFormElement::setStyle(s);

    QTextEdit* w = static_cast<QTextEdit*>(m_widget);
    w->setAlignment(textAlignment());
#if APPLE_CHANGES
    w->setWritingDirection(style()->direction() == RTL ? QPainter::RTL : QPainter::LTR);
#endif

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

#if APPLE_CHANGES
    w->setScrollBarModes(horizontalScrollMode, scrollMode);
#else
    w->setHScrollBarMode(horizontalScrollMode);
    w->setVScrollBarMode(scrollMode);
#endif
}

void RenderTextArea::updateFromElement()
{
    HTMLTextAreaElementImpl *e = element();
    QTextEdit* w = static_cast<QTextEdit*>(m_widget);

    w->setReadOnly(e->readOnly());
#if APPLE_CHANGES
    w->setDisabled(e->disabled());
#endif

    e->updateValue();
    if (!e->valueMatchesRenderer()) {
        QString widgetText = text();
        QString text = e->value().string();
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
#if APPLE_CHANGES
        txt = w->textWithHardLineBreaks();
#else
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
#endif
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

void RenderTextArea::select()
{
    static_cast<QTextEdit *>(m_widget)->selectAll();
}

// ---------------------------------------------------------------------------

#if APPLE_CHANGES
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
    bool widthSet = !style()->width().isVariable();
    bool heightSet = !style()->height().isVariable();
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
    const DOMString& min = element()->getAttribute(ATTR_MIN);
    const DOMString& max = element()->getAttribute(ATTR_MAX);
    const DOMString& precision = element()->getAttribute(ATTR_PRECISION);
    
    double minVal = min.isNull() ? 0.0 : min.string().toDouble();
    double maxVal = max.isNull() ? 100.0 : max.string().toDouble();
    minVal = kMin(minVal, maxVal); // Make sure the range is sane.
    
    double val = value.isNull() ? (maxVal + minVal)/2.0 : value.string().toDouble();
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
    const DOMString& precision = element()->getAttribute(ATTR_PRECISION);

    // Force integer value if not float (strcasecmp returns confusingly backward boolean).
    if (strcasecmp(precision, "float"))
        val = (int)(val + 0.5);

    element()->setValue(QString::number(val));
    
    // Fire the "input" DOM event.
    element()->dispatchHTMLEvent(EventImpl::INPUT_EVENT, true, false);
}

void RenderSlider::slotClicked()
{
    // emit mouseClick event etc
    RenderFormElement::slotClicked();
}

#endif

#include "render_form.moc"
