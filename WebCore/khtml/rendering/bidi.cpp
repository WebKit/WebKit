/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
#include "bidi.h"
#include "break_lines.h"
#include "render_block.h"
#include "render_text.h"
#include "render_arena.h"
#include "xml/dom_docimpl.h"

#include "kdebug.h"
#include "qdatetime.h"
#include "qfontmetrics.h"

#define BIDI_DEBUG 0
//#define DEBUG_LINEBREAKS

namespace khtml {
    
static BidiIterator sor;
static BidiIterator eor;
static BidiIterator last;
static BidiIterator current;
static BidiContext *context;
static BidiStatus status = { QChar::DirON, QChar::DirON, QChar::DirON };

// Used to track a list of chained bidi runs.
static BidiRun* sFirstBidiRun = 0;
static BidiRun* sLastBidiRun = 0;
static int sBidiRunCount = 0;
static BidiRun* sCompactFirstBidiRun = 0;
static BidiRun* sCompactLastBidiRun = 0;
static int sCompactBidiRunCount = 0;
static bool sBuildingCompactRuns = false;

// Midpoint globals.  The goal is not to do any allocation when dealing with
// these midpoints, so we just keep an array around and never clear it.  We track
// the number of items and position using the two other variables.
static QMemArray<BidiIterator> *smidpoints = 0;
static uint sNumMidpoints = 0;
static uint sCurrMidpoint = 0;
static bool betweenMidpoints = false;

static bool isLineEmpty = true;
static bool previousLineBrokeAtBR = true;
static QChar::Direction dir;
static bool adjustEmbeddding = false;
static bool emptyRun = true;
static int numSpaces;

static void embed( QChar::Direction d );
static void appendRun();

static int getBPMWidth(int childValue, Length cssUnit)
{
    if (cssUnit.type != Variable)
        return (cssUnit.type == Fixed ? cssUnit.value : childValue);
    return 0;
}

static int getBorderPaddingMargin(RenderObject* child, bool endOfInline)
{
    RenderStyle* cstyle = child->style();
    int result = 0;
    bool leftSide = (cstyle->direction() == LTR) ? !endOfInline : endOfInline;
    result += getBPMWidth((leftSide ? child->marginLeft() : child->marginRight()),
                          (leftSide ? cstyle->marginLeft() :
                           cstyle->marginRight()));
    result += getBPMWidth((leftSide ? child->paddingLeft() : child->paddingRight()),
                          (leftSide ? cstyle->paddingLeft() :
                           cstyle->paddingRight()));
    result += leftSide ? child->borderLeft() : child->borderRight();
    return result;
}

static int inlineWidth(RenderObject* child, bool start = true, bool end = true)
{
    int extraWidth = 0;
    RenderObject* parent = child->parent();
    while (parent->isInline() && !parent->isInlineBlockOrInlineTable()) {
        if (start && parent->firstChild() == child)
            extraWidth += getBorderPaddingMargin(parent, false);
        if (end && parent->lastChild() == child)
            extraWidth += getBorderPaddingMargin(parent, true);
        child = parent;
        parent = child->parent();
    }
    return extraWidth;
}

#ifndef NDEBUG
static bool inBidiRunDetach;
#endif

void BidiRun::detach(RenderArena* renderArena)
{
#ifndef NDEBUG
    inBidiRunDetach = true;
#endif
    delete this;
#ifndef NDEBUG
    inBidiRunDetach = false;
#endif

    // Recover the size left there for us by operator delete and free the memory.
    renderArena->free(*(size_t *)this, this);
}

void* BidiRun::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}

void BidiRun::operator delete(void* ptr, size_t sz)
{
    assert(inBidiRunDetach);

    // Stash size where detach can find it.
    *(size_t*)ptr = sz;
}

static void deleteBidiRuns(RenderArena* arena)
{
    if (!sFirstBidiRun)
        return;

    BidiRun* curr = sFirstBidiRun;
    while (curr) {
        BidiRun* s = curr->nextRun;
        curr->detach(arena);
        curr = s;
    }
    
    sFirstBidiRun = 0;
    sLastBidiRun = 0;
    sBidiRunCount = 0;
}

// ---------------------------------------------------------------------

/* a small helper class used internally to resolve Bidi embedding levels.
   Each line of text caches the embedding level at the start of the line for faster
   relayouting
*/
BidiContext::BidiContext(unsigned char l, QChar::Direction e, BidiContext *p, bool o)
    : level(l) , override(o), dir(e)
{
    parent = p;
    if(p) {
        p->ref();
        basicDir = p->basicDir;
    } else
        basicDir = e;
    count = 0;
}

BidiContext::~BidiContext()
{
    if(parent) parent->deref();
}

void BidiContext::ref() const
{
    count++;
}

void BidiContext::deref() const
{
    count--;
    if(count <= 0) delete this;
}

// ---------------------------------------------------------------------

inline bool operator==( const BidiIterator &it1, const BidiIterator &it2 )
{
    if(it1.pos != it2.pos) return false;
    if(it1.obj != it2.obj) return false;
    return true;
}

inline bool operator!=( const BidiIterator &it1, const BidiIterator &it2 )
{
    if(it1.pos != it2.pos) return true;
    if(it1.obj != it2.obj) return true;
    return false;
}

static inline RenderObject *Bidinext(RenderObject *par, RenderObject *current,
                                     bool skipInlines = true, bool* endOfInline = 0)
{
    RenderObject *next = 0;
    bool oldEndOfInline = endOfInline ? *endOfInline : false;
    if (endOfInline)
        *endOfInline = false;

    while(current != 0)
    {
        //kdDebug( 6040 ) << "current = " << current << endl;
        if (!oldEndOfInline && !current->isFloating() && !current->isReplaced() && !current->isPositioned()) {
            next = current->firstChild();
            if ( next && adjustEmbeddding ) {
                EUnicodeBidi ub = next->style()->unicodeBidi();
                if ( ub != UBNormal && !emptyRun ) {
                    EDirection dir = next->style()->direction();
QChar::Direction d = ( ub == Embed ? ( dir == RTL ? QChar::DirRLE : QChar::DirLRE )
                                   : ( dir == RTL ? QChar::DirRLO : QChar::DirLRO ) );
                    embed( d );
                }
            }
        }
        if (!next) {
            if (!skipInlines && !oldEndOfInline && current->isInlineFlow())
            {
                next = current;
                if (endOfInline)
                    *endOfInline = true;
                break;
            }

            while (current && current != par) {
                next = current->nextSibling();
                if (next) break;
                if ( adjustEmbeddding && current->style()->unicodeBidi() != UBNormal && !emptyRun ) {
                    embed( QChar::DirPDF );
                }
                current = current->parent();
                if (!skipInlines && current && current != par && current->isInlineFlow()) {
                    next = current;
                    if (endOfInline)
                        *endOfInline = true;
                    break;
                }
            }
        }

        if (!next) break;

        if (next->isText() || next->isBR() || next->isFloating() || next->isReplaced() || next->isPositioned()
            || ((!skipInlines || !next->firstChild()) // Always return EMPTY inlines.
                && next->isInlineFlow()))
            break;
        current = next;
    }
    return next;
}

static RenderObject *first( RenderObject *par, bool skipInlines = true )
{
    if(!par->firstChild()) return 0;
    RenderObject *o = par->firstChild();

    if (o->isInlineFlow()) {
        if (skipInlines && o->firstChild())
            o = Bidinext( par, o, skipInlines );
        else
            return o; // Never skip empty inlines.
    }

    if (o && !o->isText() && !o->isBR() && !o->isReplaced() && !o->isFloating() && !o->isPositioned())
        o = Bidinext( par, o, skipInlines );
    return o;
}

inline void BidiIterator::operator ++ ()
{
    if(!obj) return;
    if(obj->isText()) {
        pos++;
        if(pos >= static_cast<RenderText *>(obj)->stringLength()) {
            obj = Bidinext( par, obj );
            pos = 0;
        }
    } else {
        obj = Bidinext( par, obj );
        pos = 0;
    }
}

inline bool BidiIterator::atEnd() const
{
    if(!obj) return true;
    return false;
}

const QChar &BidiIterator::current() const
{
    if( !obj || !obj->isText()) {
      static QChar nonBreakingSpace(0xA0);
      return nonBreakingSpace;
    }
    return static_cast<RenderText *>(obj)->text()[pos];
}

inline QChar::Direction BidiIterator::direction() const
{
    if(!obj || !obj->isText() ) return QChar::DirON;
    
    RenderText *renderTxt = static_cast<RenderText *>( obj );
    if ( pos >= renderTxt->stringLength() )
        return QChar::DirON;
    return renderTxt->text()[pos].direction();
}

// -------------------------------------------------------------------------------------------------

static void addRun(BidiRun* bidiRun)
{
    if (!sFirstBidiRun)
        sFirstBidiRun = sLastBidiRun = bidiRun;
    else {
        sLastBidiRun->nextRun = bidiRun;
        sLastBidiRun = bidiRun;
    }
    sBidiRunCount++;
    bidiRun->compact = sBuildingCompactRuns;

    // Compute the number of spaces in this run,
    if (bidiRun->obj && bidiRun->obj->isText()) {
        RenderText* text = static_cast<RenderText*>(bidiRun->obj);
        for (int i = bidiRun->start; i < bidiRun->stop; i++)
            if (text->text()[i].unicode() == ' ')
                numSpaces++;
    }
}

static void reverseRuns(int start, int end)
{
    if (start >= end)
        return;

    assert(start >= 0 && end < sBidiRunCount);
    
    // Get the item before the start of the runs to reverse and put it in
    // |beforeStart|.  |curr| should point to the first run to reverse.
    BidiRun* curr = sFirstBidiRun;
    BidiRun* beforeStart = 0;
    int i = 0;
    while (i < start) {
        i++;
        beforeStart = curr;
        curr = curr->nextRun;
    }

    BidiRun* startRun = curr;
    while (i < end) {
        i++;
        curr = curr->nextRun;
    }
    BidiRun* endRun = curr;
    BidiRun* afterEnd = curr->nextRun;

    i = start;
    curr = startRun;
    BidiRun* newNext = afterEnd;
    while (i <= end) {
        // Do the reversal.
        BidiRun* next = curr->nextRun;
        curr->nextRun = newNext;
        newNext = curr;
        curr = next;
        i++;
    }

    // Now hook up beforeStart and afterEnd to the newStart and newEnd.
    if (beforeStart)
        beforeStart->nextRun = endRun;
    else
        sFirstBidiRun = endRun;

    startRun->nextRun = afterEnd;
    if (!afterEnd)
        sLastBidiRun = startRun;
}

static void checkMidpoints(BidiIterator& lBreak)
{
    // Check to see if our last midpoint is a start point beyond the line break.  If so,
    // shave it off the list, and shave off a trailing space if the previous end point isn't
    // white-space: pre.
    if (lBreak.obj && sNumMidpoints && sNumMidpoints%2 == 0) {
        BidiIterator* midpoints = smidpoints->data();
        BidiIterator& endpoint = midpoints[sNumMidpoints-2];
        const BidiIterator& startpoint = midpoints[sNumMidpoints-1];
        BidiIterator currpoint = endpoint;
        while (!currpoint.atEnd() && currpoint != startpoint && currpoint != lBreak)
            ++currpoint;
        if (currpoint == lBreak) {
            // We hit the line break before the start point.  Shave off the start point.
            sNumMidpoints--;
            if (endpoint.obj->style()->whiteSpace() != PRE)
                endpoint.pos--;
        }
    }    
}

static void addMidpoint(const BidiIterator& midpoint)
{
    if (!smidpoints)
        return;

    if (smidpoints->size() <= sNumMidpoints)
        smidpoints->resize(sNumMidpoints+10);

    BidiIterator* midpoints = smidpoints->data();
    midpoints[sNumMidpoints++] = midpoint;
}

static void appendRunsForObject(int start, int end, RenderObject* obj)
{
    if (start > end || obj->isFloating() ||
        (obj->isPositioned() && !obj->hasStaticX() && !obj->hasStaticY()))
        return;

    bool haveNextMidpoint = (smidpoints && sCurrMidpoint < sNumMidpoints);
    BidiIterator nextMidpoint = { 0, 0, 0 };
    if (haveNextMidpoint)
        nextMidpoint = smidpoints->at(sCurrMidpoint);
    if (betweenMidpoints) {
        if (!(haveNextMidpoint && nextMidpoint.obj == obj))
            return;
        // This is a new start point. Stop ignoring objects and 
        // adjust our start.
        betweenMidpoints = false;
        start = nextMidpoint.pos;
        sCurrMidpoint++;
        if (start < end)
            return appendRunsForObject(start, end, obj);
    }
    else {
        if (!smidpoints || !haveNextMidpoint || (obj != nextMidpoint.obj)) {
            addRun(new (obj->renderArena()) BidiRun(start, end, obj, context, dir));
            return;
        }
        
        // An end midpoint has been encounted within our object.  We
        // need to go ahead and append a run with our endpoint.
        if (int(nextMidpoint.pos+1) <= end) {
            betweenMidpoints = true;
            sCurrMidpoint++;
            if (nextMidpoint.pos != UINT_MAX) { // UINT_MAX means stop at the object and don't include any of it.
                addRun(new (obj->renderArena())
                    BidiRun(start, nextMidpoint.pos+1, obj, context, dir));
                return appendRunsForObject(nextMidpoint.pos+1, end, obj);
            }
        }
        else
           addRun(new (obj->renderArena()) BidiRun(start, end, obj, context, dir));
    }
}

static void appendRun()
{
    if ( emptyRun ) return;
#if BIDI_DEBUG > 1
    kdDebug(6041) << "appendRun: dir="<<(int)dir<<endl;
#endif

    bool b = adjustEmbeddding;
    adjustEmbeddding = false;

    int start = sor.pos;
    RenderObject *obj = sor.obj;
    while( obj && obj != eor.obj ) {
        appendRunsForObject(start, obj->length(), obj);        
        start = 0;
        obj = Bidinext( sor.par, obj );
    }
    if (obj)
        appendRunsForObject(start, eor.pos+1, obj);
    
    ++eor;
    sor = eor;
    dir = QChar::DirON;
    status.eor = QChar::DirON;
    adjustEmbeddding = b;
}

static void embed( QChar::Direction d )
{
#if BIDI_DEBUG > 1
    qDebug("*** embed dir=%d emptyrun=%d", d, emptyRun );
#endif
    bool b = adjustEmbeddding ;
    adjustEmbeddding = false;
    if ( d == QChar::DirPDF ) {
	BidiContext *c = context->parent;
	if (c) {
	    if ( eor != last ) {
		appendRun();
		eor = last;
	    }
	    appendRun();
	    emptyRun = true;
	    status.last = context->dir;
	    context->deref();
	    context = c;
	    if(context->override)
		dir = context->dir;
	    else
		dir = QChar::DirON;
	    status.lastStrong = context->dir;
	}
    } else {
	QChar::Direction runDir;
	if( d == QChar::DirRLE || d == QChar::DirRLO )
	    runDir = QChar::DirR;
	else
	    runDir = QChar::DirL;
	bool override;
	if( d == QChar::DirLRO || d == QChar::DirRLO )
	    override = true;
	else
	    override = false;

	unsigned char level = context->level;
	if ( runDir == QChar::DirR ) {
	    if(level%2) // we have an odd level
		level += 2;
	    else
		level++;
	} else {
	    if(level%2) // we have an odd level
		level++;
	    else
		level += 2;
	}

	if(level < 61) {
	    if ( eor != last ) {
                appendRun();
                eor = last;
            }
            appendRun();
            emptyRun = true;

	    context = new BidiContext(level, runDir, context, override);
	    context->ref();
	    if ( override )
		dir = runDir;
	    status.last = runDir;
	    status.lastStrong = runDir;
	}
    }
    adjustEmbeddding = b;
}

InlineFlowBox* RenderBlock::createLineBoxes(RenderObject* obj)
{
    // See if we have an unconstructed line box for this object that is also
    // the last item on the line.
    KHTMLAssert(obj->isInlineFlow() || obj == this);
    RenderFlow* flow = static_cast<RenderFlow*>(obj);

    // Get the last box we made for this render object.
    InlineFlowBox* box = flow->lastLineBox();

    // If this box is constructed then it is from a previous line, and we need
    // to make a new box for our line.  If this box is unconstructed but it has
    // something following it on the line, then we know we have to make a new box
    // as well.  In this situation our inline has actually been split in two on
    // the same line (this can happen with very fancy language mixtures).
    if (!box || box->isConstructed() || box->nextOnLine()) {
        // We need to make a new box for this render object.  Once
        // made, we need to place it at the end of the current line.
        InlineBox* newBox = obj->createInlineBox(false, obj == this);
        KHTMLAssert(newBox->isInlineFlowBox());
        box = static_cast<InlineFlowBox*>(newBox);
        box->setFirstLineStyleBit(m_firstLine);
        
        // We have a new box. Append it to the inline box we get by constructing our
        // parent.  If we have hit the block itself, then |box| represents the root
        // inline box for the line, and it doesn't have to be appended to any parent
        // inline.
        if (obj != this) {
            InlineFlowBox* parentBox = createLineBoxes(obj->parent());
            parentBox->addToLine(box);
        }
    }

    return box;
}

InlineFlowBox* RenderBlock::constructLine(const BidiIterator &start, const BidiIterator &end)
{
    if (!sFirstBidiRun)
        return 0; // We had no runs. Don't make a root inline box at all. The line is empty.

    InlineFlowBox* parentBox = 0;
    for (BidiRun* r = sFirstBidiRun; r; r = r->nextRun) {
        // Create a box for our object.
        r->box = r->obj->createInlineBox(r->obj->isPositioned(), false);
        
        // If we have no parent box yet, or if the run is not simply a sibling,
        // then we need to construct inline boxes as necessary to properly enclose the
        // run's inline box.
        if (!parentBox || (parentBox->object() != r->obj->parent()))
            // Create new inline boxes all the way back to the appropriate insertion point.
            parentBox = createLineBoxes(r->obj->parent());

        // Append the inline box to this line.
        parentBox->addToLine(r->box);
    }

    // We should have a root inline box.  It should be unconstructed and
    // be the last continuation of our line list.
    KHTMLAssert(lastLineBox() && !lastLineBox()->isConstructed());

    // Set bits on our inline flow boxes that indicate which sides should
    // paint borders/margins/padding.  This knowledge will ultimately be used when
    // we determine the horizontal positions and widths of all the inline boxes on
    // the line.
    RenderObject* endObject = 0;
    bool lastLine = !end.obj;
    if (end.obj && end.pos == 0)
        endObject = end.obj;
    lastLineBox()->determineSpacingForFlowBoxes(lastLine, endObject);

    // Now mark the line boxes as being constructed.
    lastLineBox()->setConstructed();

    // Return the last line.
    return lastLineBox();
}

void RenderBlock::computeHorizontalPositionsForLine(InlineFlowBox* lineBox, BidiContext* endEmbed)
{
    // First determine our total width.
    int totWidth = lineBox->getFlowSpacingWidth();
    BidiRun* r = 0;
    for (r = sFirstBidiRun; r; r = r->nextRun) {
        if (r->obj->isPositioned())
            continue; // Positioned objects are only participating to figure out their
                      // correct static x position.  They have no effect on the width.
        if (r->obj->isText())
            r->box->setWidth(static_cast<RenderText *>(r->obj)->width(r->start, r->stop-r->start, m_firstLine));
        else if (!r->obj->isInlineFlow()) {
            r->obj->calcWidth();
            r->box->setWidth(r->obj->width());
            if (!r->compact)
                totWidth += r->obj->marginLeft() + r->obj->marginRight();
        }

        // Compacts don't contribute to the width of the line, since they are placed in the margin.
        if (!r->compact)
            totWidth += r->box->width();
    }

    // Armed with the total width of the line (without justification),
    // we now examine our text-align property in order to determine where to position the
    // objects horizontally.  The total width of the line can be increased if we end up
    // justifying text.
    int x = leftOffset(m_height);
    int availableWidth = lineWidth(m_height);
    switch(style()->textAlign()) {
        case LEFT:
        case KHTML_LEFT:
            numSpaces = 0;
            break;
        case JUSTIFY:
            if (numSpaces != 0 && !current.atEnd() && !current.obj->isBR() )
                break;
            // fall through
        case TAAUTO:
            numSpaces = 0;
            // for right to left fall through to right aligned
            if (endEmbed->basicDir == QChar::DirL)
                break;
        case RIGHT:
        case KHTML_RIGHT:
            x += availableWidth - totWidth;
            numSpaces = 0;
            break;
        case CENTER:
        case KHTML_CENTER:
            int xd = (availableWidth - totWidth)/2;
            x += xd >0 ? xd : 0;
            numSpaces = 0;
            break;
    }

    if (numSpaces > 0) {
        for (r = sFirstBidiRun; r; r = r->nextRun) {
            int spaceAdd = 0;
            if (numSpaces > 0 && r->obj->isText() && !r->compact) {
                // get the number of spaces in the run
                int spaces = 0;
                for ( int i = r->start; i < r->stop; i++ )
                    if ( static_cast<RenderText *>(r->obj)->text()[i].unicode() == ' ' )
                        spaces++;

                KHTMLAssert(spaces <= numSpaces);

                // Only justify text with white-space: normal.
                if (r->obj->style()->whiteSpace() != PRE) {
                    spaceAdd = (availableWidth - totWidth)*spaces/numSpaces;
                    static_cast<TextRun*>(r->box)->setSpaceAdd(spaceAdd);
                    totWidth += spaceAdd;
                }
                numSpaces -= spaces;
            }
        }
    }
    
    // The widths of all runs are now known.  We can now place every inline box (and
    // compute accurate widths for the inline flow boxes).
    int rightPos = lineBox->placeBoxesHorizontally(x);
    if (rightPos > m_overflowWidth)
        m_overflowWidth = rightPos; // FIXME: Work for rtl overflow also.
}

void RenderBlock::computeVerticalPositionsForLine(InlineFlowBox* lineBox)
{
    lineBox->verticallyAlignBoxes(m_height);

    // See if the line spilled out.  If so set overflow height accordingly.
    int bottomOfLine = lineBox->bottomOverflow();
    if (bottomOfLine > m_height && bottomOfLine > m_overflowHeight)
        m_overflowHeight = bottomOfLine;
        
    // Now make sure we place replaced render objects correctly.
    for (BidiRun* r = sFirstBidiRun; r; r = r->nextRun) {
        // Align positioned boxes with the top of the line box.  This is
        // a reasonable approximation of an appropriate y position.
        if (r->obj->isPositioned())
            r->box->setYPos(m_height);

        // Position is used to properly position both replaced elements and
        // to update the static normal flow x/y of positioned elements.
        r->obj->position(r->box, r->start, r->stop - r->start, r->level%2);
    }
}

// collects one line of the paragraph and transforms it to visual order
void RenderBlock::bidiReorderLine(const BidiIterator &start, const BidiIterator &end)
{
    if ( start == end ) {
        if ( start.current() == '\n' ) {
            m_height += lineHeight( m_firstLine, true );
        }
        return;
    }

#if BIDI_DEBUG > 1
    kdDebug(6041) << "reordering Line from " << start.obj << "/" << start.pos << " to " << end.obj << "/" << end.pos << endl;
#endif

    sFirstBidiRun = 0;
    sLastBidiRun = 0;
    sBidiRunCount = 0;
    
    //    context->ref();

    dir = QChar::DirON;
    emptyRun = true;

    numSpaces = 0;

    current = start;
    last = current;
    bool atEnd = false;
    while( 1 ) {

        QChar::Direction dirCurrent;
        if (atEnd) {
            //kdDebug(6041) << "atEnd" << endl;
            BidiContext *c = context;
            if ( current.atEnd())
                while ( c->parent )
                    c = c->parent;
            dirCurrent = c->dir;
        } else {
            dirCurrent = current.direction();
        }

#ifndef QT_NO_UNICODETABLES

#if BIDI_DEBUG > 1
        kdDebug(6041) << "directions: dir=" << (int)dir << " current=" << (int)dirCurrent << " last=" << status.last << " eor=" << status.eor << " lastStrong=" << status.lastStrong << " embedding=" << (int)context->dir << " level =" << (int)context->level << endl;
#endif

        switch(dirCurrent) {

            // embedding and overrides (X1-X9 in the Bidi specs)
        case QChar::DirRLE:
        case QChar::DirLRE:
        case QChar::DirRLO:
        case QChar::DirLRO:
        case QChar::DirPDF:
            eor = last;
            embed( dirCurrent );
            break;

            // strong types
        case QChar::DirL:
            if(dir == QChar::DirON)
                dir = QChar::DirL;
            switch(status.last)
                {
                case QChar::DirL:
                    eor = current; status.eor = QChar::DirL; break;
                case QChar::DirR:
                case QChar::DirAL:
                case QChar::DirEN:
                case QChar::DirAN:
                    appendRun();
                    break;
                case QChar::DirES:
                case QChar::DirET:
                case QChar::DirCS:
                case QChar::DirBN:
                case QChar::DirB:
                case QChar::DirS:
                case QChar::DirWS:
                case QChar::DirON:
                    if(dir != QChar::DirL) {
                        //last stuff takes embedding dir
                        if( context->dir == QChar::DirR ) {
                            if(!(status.eor == QChar::DirR)) {
                                // AN or EN
                                appendRun();
                                dir = QChar::DirR;
                            }
                            else
                                eor = last;
                            appendRun();
                            dir = QChar::DirL;
                            status.eor = QChar::DirL;
                        } else {
                            if(status.eor == QChar::DirR) {
                                appendRun();
                                dir = QChar::DirL;
                            } else {
                                eor = current; status.eor = QChar::DirL; break;
                            }
                        }
                    } else {
                        eor = current; status.eor = QChar::DirL;
                    }
                default:
                    break;
                }
            status.lastStrong = QChar::DirL;
            break;
        case QChar::DirAL:
        case QChar::DirR:
            if(dir == QChar::DirON) dir = QChar::DirR;
            switch(status.last)
                {
                case QChar::DirR:
                case QChar::DirAL:
                    eor = current; status.eor = QChar::DirR; break;
                case QChar::DirL:
                case QChar::DirEN:
                case QChar::DirAN:
                    appendRun();
		    dir = QChar::DirR;
		    eor = current;
		    status.eor = QChar::DirR;
                    break;
                case QChar::DirES:
                case QChar::DirET:
                case QChar::DirCS:
                case QChar::DirBN:
                case QChar::DirB:
                case QChar::DirS:
                case QChar::DirWS:
                case QChar::DirON:
                    if( !(status.eor == QChar::DirR) && !(status.eor == QChar::DirAL) ) {
                        //last stuff takes embedding dir
                        if(context->dir == QChar::DirR || status.lastStrong == QChar::DirR) {
                            appendRun();
                            dir = QChar::DirR;
                            eor = current;
			    status.eor = QChar::DirR;
                        } else {
                            eor = last;
                            appendRun();
                            dir = QChar::DirR;
			    status.eor = QChar::DirR;
                        }
                    } else {
                        eor = current; status.eor = QChar::DirR;
                    }
                default:
                    break;
                }
            status.lastStrong = dirCurrent;
            break;

            // weak types:

        case QChar::DirNSM:
            // ### if @sor, set dir to dirSor
            break;
        case QChar::DirEN:
            if(!(status.lastStrong == QChar::DirAL)) {
                // if last strong was AL change EN to AN
                if(dir == QChar::DirON) {
                    if(status.lastStrong == QChar::DirAL)
                        dir = QChar::DirAN;
                    else
                        dir = QChar::DirL;
                }
                switch(status.last)
                    {
                    case QChar::DirET:
			if ( status.lastStrong == QChar::DirR || status.lastStrong == QChar::DirAL ) {
			    appendRun();
                            dir = QChar::DirEN;
                            status.eor = QChar::DirEN;
			}
			// fall through
                    case QChar::DirEN:
                    case QChar::DirL:
                        eor = current;
                        status.eor = dirCurrent;
                        break;
                    case QChar::DirR:
                    case QChar::DirAL:
                    case QChar::DirAN:
                        appendRun();
			status.eor = QChar::DirEN;
                        dir = QChar::DirEN; break;
                    case QChar::DirES:
                    case QChar::DirCS:
                        if(status.eor == QChar::DirEN) {
                            eor = current; break;
                        }
                    case QChar::DirBN:
                    case QChar::DirB:
                    case QChar::DirS:
                    case QChar::DirWS:
                    case QChar::DirON:
                        if(status.eor == QChar::DirR) {
                            // neutrals go to R
                            eor = last;
                            appendRun();
                            dir = QChar::DirEN;
                            status.eor = QChar::DirEN;
                        }
                        else if( status.eor == QChar::DirL ||
                                 (status.eor == QChar::DirEN && status.lastStrong == QChar::DirL)) {
                            eor = current; status.eor = dirCurrent;
                        } else {
                            // numbers on both sides, neutrals get right to left direction
                            if(dir != QChar::DirL) {
                                appendRun();
                                eor = last;
                                dir = QChar::DirR;
                                appendRun();
                                dir = QChar::DirEN;
                                status.eor = QChar::DirEN;
                            } else {
                                eor = current; status.eor = dirCurrent;
                            }
                        }
                    default:
                        break;
                    }
                break;
            }
        case QChar::DirAN:
            dirCurrent = QChar::DirAN;
            if(dir == QChar::DirON) dir = QChar::DirAN;
            switch(status.last)
                {
                case QChar::DirL:
                case QChar::DirAN:
                    eor = current; status.eor = QChar::DirAN; break;
                case QChar::DirR:
                case QChar::DirAL:
                case QChar::DirEN:
                    appendRun();
                    dir = QChar::DirAN; status.eor = QChar::DirAN;
                    break;
                case QChar::DirCS:
                    if(status.eor == QChar::DirAN) {
                        eor = current; status.eor = QChar::DirR; break;
                    }
                case QChar::DirES:
                case QChar::DirET:
                case QChar::DirBN:
                case QChar::DirB:
                case QChar::DirS:
                case QChar::DirWS:
                case QChar::DirON:
                    if(status.eor == QChar::DirR) {
                        // neutrals go to R
                        eor = last;
                        appendRun();
                        dir = QChar::DirAN;
                        status.eor = QChar::DirAN;
                    } else if( status.eor == QChar::DirL ||
                               (status.eor == QChar::DirEN && status.lastStrong == QChar::DirL)) {
                        eor = current; status.eor = dirCurrent;
                    } else {
                        // numbers on both sides, neutrals get right to left direction
                        if(dir != QChar::DirL) {
                            appendRun();
                            eor = last;
                            dir = QChar::DirR;
                            appendRun();
                            dir = QChar::DirAN;
                            status.eor = QChar::DirAN;
                        } else {
                            eor = current; status.eor = dirCurrent;
                        }
                    }
                default:
                    break;
                }
            break;
        case QChar::DirES:
        case QChar::DirCS:
            break;
        case QChar::DirET:
            if(status.last == QChar::DirEN) {
                dirCurrent = QChar::DirEN;
                eor = current; status.eor = dirCurrent;
                break;
            }
            break;

        // boundary neutrals should be ignored
        case QChar::DirBN:
            break;
            // neutrals
        case QChar::DirB:
            // ### what do we do with newline and paragraph seperators that come to here?
            break;
        case QChar::DirS:
            // ### implement rule L1
            break;
        case QChar::DirWS:
            break;
        case QChar::DirON:
            break;
        default:
            break;
        }

        //cout << "     after: dir=" << //        dir << " current=" << dirCurrent << " last=" << status.last << " eor=" << status.eor << " lastStrong=" << status.lastStrong << " embedding=" << context->dir << endl;

        if(current.atEnd()) break;

        // set status.last as needed.
        switch(dirCurrent)
            {
            case QChar::DirET:
            case QChar::DirES:
            case QChar::DirCS:
            case QChar::DirS:
            case QChar::DirWS:
            case QChar::DirON:
                switch(status.last)
                    {
                    case QChar::DirL:
                    case QChar::DirR:
                    case QChar::DirAL:
                    case QChar::DirEN:
                    case QChar::DirAN:
                        status.last = dirCurrent;
                        break;
                    default:
                        status.last = QChar::DirON;
                    }
                break;
            case QChar::DirNSM:
            case QChar::DirBN:
                // ignore these
                break;
            case QChar::DirEN:
                if ( status.last == QChar::DirL ) {
                    status.last = QChar::DirL;
                    break;
                }
                // fall through
            default:
                status.last = dirCurrent;
            }
#endif

	if ( atEnd ) break;
        last = current;

	if ( emptyRun ) {
	    sor = current;
	    eor = current;
	    emptyRun = false;
	}

	// this causes the operator ++ to open and close embedding levels as needed
	// for the CSS unicode-bidi property
	adjustEmbeddding = true;
        ++current;
	adjustEmbeddding = false;

	if ( current == end ) {
	    if ( emptyRun )
		break;
	    atEnd = true;
	}
    }

#if BIDI_DEBUG > 0
    kdDebug(6041) << "reached end of line current=" << current.obj << "/" << current.pos
		  << ", eor=" << eor.obj << "/" << eor.pos << endl;
#endif
    if ( !emptyRun && sor != current ) {
	    eor = last;
	    appendRun();
    }

    // both commands below together give a noop...
    //endEmbed->ref();
    //context->deref();

    // reorder line according to run structure...

    // first find highest and lowest levels
    uchar levelLow = 128;
    uchar levelHigh = 0;
    BidiRun *r = sFirstBidiRun;
    while ( r ) {
        if ( r->level > levelHigh )
            levelHigh = r->level;
        if ( r->level < levelLow )
            levelLow = r->level;
        r = r->nextRun;
    }

    // implements reordering of the line (L2 according to Bidi spec):
    // L2. From the highest level found in the text to the lowest odd level on each line,
    // reverse any contiguous sequence of characters that are at that level or higher.

    // reversing is only done up to the lowest odd level
    if( !(levelLow%2) ) levelLow++;

#if BIDI_DEBUG > 0
    kdDebug(6041) << "lineLow = " << (uint)levelLow << ", lineHigh = " << (uint)levelHigh << endl;
    kdDebug(6041) << "logical order is:" << endl;
    QPtrListIterator<BidiRun> it2(runs);
    BidiRun *r2;
    for ( ; (r2 = it2.current()); ++it2 )
        kdDebug(6041) << "    " << r2 << "  start=" << r2->start << "  stop=" << r2->stop << "  level=" << (uint)r2->level << endl;
#endif

    int count = sBidiRunCount - 1;

    // do not reverse for visually ordered web sites
    if(!style()->visuallyOrdered()) {
        while(levelHigh >= levelLow) {
            int i = 0;
            BidiRun* currRun = sFirstBidiRun;
            while ( i < count ) {
                while(i < count && currRun && currRun->level < levelHigh) {
                    i++;
                    currRun = currRun->nextRun;
                }
                int start = i;
                while(i <= count && currRun && currRun->level >= levelHigh) {
                    i++;
                    currRun = currRun->nextRun;
                }
                int end = i-1;
                reverseRuns(start, end);
                i++;
                if(i >= count) break;
            }
            levelHigh--;
        }
    }

#if BIDI_DEBUG > 0
    kdDebug(6041) << "visual order is:" << endl;
    for (BidiRun* curr = sFirstRun; curr; curr = curr->nextRun)
        kdDebug(6041) << "    " << curr << endl;
#endif
}

static void buildCompactRuns(RenderObject* compactObj)
{
    sBuildingCompactRuns = true;
    if (!compactObj->isRenderBlock()) {
        // Just append a run for our object.
        isLineEmpty = false;
        addRun(new (compactObj->renderArena()) BidiRun(0, compactObj->length(), compactObj, context, dir));
    }
    else {
        // Format the compact like it is its own single line.  We build up all the runs for
        // the little compact and then reorder them for bidi.
        RenderBlock* compactBlock = static_cast<RenderBlock*>(compactObj);
        adjustEmbeddding = true;
        BidiIterator start = { compactBlock, first(compactBlock), 0 };
        adjustEmbeddding = false;
        BidiIterator end = start;
    
        betweenMidpoints = false;
        isLineEmpty = true;
        previousLineBrokeAtBR = true;
        
        end = compactBlock->findNextLineBreak(start);
        if (!isLineEmpty)
            compactBlock->bidiReorderLine(start, end);
    }
    
    
    sCompactFirstBidiRun = sFirstBidiRun;
    sCompactLastBidiRun = sLastBidiRun;
    sCompactBidiRunCount = sBidiRunCount;
    
    sNumMidpoints = 0;
    sCurrMidpoint = 0;
    betweenMidpoints = false;
    sBuildingCompactRuns = false;
}

void RenderBlock::layoutInlineChildren(bool relayoutChildren)
{
    m_overflowHeight = 0;
    
    invalidateVerticalPositions();
#ifdef DEBUG_LAYOUT
    QTime qt;
    qt.start();
    kdDebug( 6040 ) << renderName() << " layoutInlineChildren( " << this <<" )" << endl;
#endif
#if BIDI_DEBUG > 1 || defined( DEBUG_LINEBREAKS )
    kdDebug(6041) << " ------- bidi start " << this << " -------" << endl;
#endif
    int toAdd = style()->borderBottomWidth();
    m_height = style()->borderTopWidth();

    m_height += paddingTop();
    toAdd += paddingBottom();

    // Clear out our line boxes.
    deleteLineBoxes();
    
    if (firstChild()) {
        // layout replaced elements
        bool endOfInline = false;
        RenderObject *o = first( this, false );
        while ( o ) {
            if(o->isReplaced() || o->isFloating() || o->isPositioned()) {
                //kdDebug(6041) << "layouting replaced or floating child" << endl;
                if (relayoutChildren || o->style()->width().isPercent() || o->style()->height().isPercent())
                    o->setNeedsLayout(true);
                if (o->isPositioned())
                    o->containingBlock()->insertPositionedObject(o);
                else
                    o->layoutIfNeeded();
            }
            else if(o->isText()) // FIXME: Should be able to combine deleteLineBoxes/Runs
                static_cast<RenderText *>(o)->deleteRuns();
            else if (o->isInlineFlow() && !endOfInline)
                static_cast<RenderFlow*>(o)->deleteLineBoxes();
            o = Bidinext( this, o, false, &endOfInline);
        }

        BidiContext *startEmbed;
        if( style()->direction() == LTR ) {
            startEmbed = new BidiContext( 0, QChar::DirL );
            status.eor = QChar::DirL;
        } else {
            startEmbed = new BidiContext( 1, QChar::DirR );
            status.eor = QChar::DirR;
        }
        startEmbed->ref();

	status.lastStrong = QChar::DirON;
	status.last = QChar::DirON;

        context = startEmbed;
        adjustEmbeddding = true;
        BidiIterator start = { this, first(this), 0 };
        adjustEmbeddding = false;
        BidiIterator end = start;

        m_firstLine = true;
        
        if (!smidpoints)
            smidpoints = new QMemArray<BidiIterator>;

        sNumMidpoints = 0;
        sCurrMidpoint = 0;
        sCompactFirstBidiRun = sCompactLastBidiRun = 0;
        sCompactBidiRunCount = 0;

        previousLineBrokeAtBR = true;
        
        while( !end.atEnd() ) {
            start = end;
            betweenMidpoints = false;
            isLineEmpty = true;
            if (m_firstLine && firstChild() && firstChild()->isCompact()) {
                buildCompactRuns(firstChild());
                start.obj = firstChild()->nextSibling();
                end = start;
            }
            end = findNextLineBreak(start);
            if( start.atEnd() ) break;
            if (!isLineEmpty) {
                bidiReorderLine(start, end);

                // Now that the runs have been ordered, we create the line boxes.
                // At the same time we figure out where border/padding/margin should be applied for
                // inline flow boxes.
                if (sCompactFirstBidiRun) {
                    // We have a compact line sharing this line.  Link the compact runs
                    // to our runs to create a single line of runs.
                    sCompactLastBidiRun->nextRun = sFirstBidiRun;
                    sFirstBidiRun = sCompactFirstBidiRun;
                    sBidiRunCount += sCompactBidiRunCount;
                }

                if (sBidiRunCount) {
                    InlineFlowBox* lineBox = constructLine(start, end);
                    if (lineBox) {
                        // Now we position all of our text runs horizontally.
                        computeHorizontalPositionsForLine(lineBox, context);
        
                        // Now position our text runs vertically.
                        computeVerticalPositionsForLine(lineBox);
        
                        deleteBidiRuns(renderArena());
                    }
                }
                
                if( end == start || (end.obj && end.obj->isBR() && !start.obj->isBR() ) ) {
                    adjustEmbeddding = true;
                    ++end;
                    adjustEmbeddding = false;
                } else if(m_pre && end.current() == QChar('\n') ) {
                    adjustEmbeddding = true;
                    ++end;
                    adjustEmbeddding = false;
                }

                m_firstLine = false;
                newLine();
            }
             
            sNumMidpoints = 0;
            sCurrMidpoint = 0;
            sCompactFirstBidiRun = sCompactLastBidiRun = 0;
            sCompactBidiRunCount = 0;
        }
        startEmbed->deref();
        //embed->deref();
    }

    sNumMidpoints = 0;
    sCurrMidpoint = 0;

    // in case we have a float on the last line, it might not be positioned up to now.
    // This has to be done before adding in the bottom border/padding, or the float will
    // include the padding incorrectly. -dwh
    positionNewFloats();
    
    // Now add in the bottom border/padding.
    m_height += toAdd;

    // Always make sure this is at least our height.
    if (m_overflowHeight < m_height)
        m_overflowHeight = m_height;
    
#if BIDI_DEBUG > 1
    kdDebug(6041) << " ------- bidi end " << this << " -------" << endl;
#endif
    //kdDebug() << "RenderBlock::layoutInlineChildren time used " << qt.elapsed() << endl;
    //kdDebug(6040) << "height = " << m_height <<endl;
}

BidiIterator RenderBlock::findNextLineBreak(BidiIterator &start)
{
    int width = lineWidth(m_height);
    int w = 0;
    int tmpW = 0;
#ifdef DEBUG_LINEBREAKS
    kdDebug(6041) << "findNextLineBreak: line at " << m_height << " line width " << width << endl;
    kdDebug(6041) << "sol: " << start.obj << " " << start.pos << endl;
#endif

    // eliminate spaces at beginning of line
    // remove leading spaces.  Any inline flows we encounter will be empty and should also
    // be skipped.
    while (!start.atEnd() && (start.obj->isInlineFlow() || (start.obj->style()->whiteSpace() != PRE &&
          (start.current() == ' ' || start.obj->isFloatingOrPositioned())))) {
        if( start.obj->isFloatingOrPositioned() ) {
            RenderObject *o = start.obj;
            // add to special objects...
            if (o->isFloating()) {
                insertFloatingObject(o);
                positionNewFloats();
                width = lineWidth(m_height);
            }
            else if (o->isPositioned()) {
                if (o->hasStaticX())
                    o->setStaticX(style()->direction() == LTR ?
                                  borderLeft()+paddingLeft() :
                                  borderRight()+paddingRight());
                if (o->hasStaticY())
                    o->setStaticY(m_height);
            }
        }
        
        adjustEmbeddding = true;
        ++start;
        adjustEmbeddding = false;
    }
    
    if ( start.atEnd() ){
        return start;
    }

    // This variable is used only if whitespace isn't set to PRE, and it tells us whether
    // or not we are currently ignoring whitespace.
    bool ignoringSpaces = false;
    
    // This variable tracks whether the very last character we saw was a space.  We use
    // this to detect when we encounter a second space so we know we have to terminate
    // a run.
    bool currentCharacterIsSpace = false;
    RenderObject* trailingSpaceObject = 0;
    
    // The pos of the last whitespace char we saw, not to be confused with the lastSpace
    // variable below, which is really the last breakable char.
    int lastSpacePos = 0;
    
    BidiIterator lBreak = start;

    RenderObject *o = start.obj;
    RenderObject *last = o;
    int pos = start.pos;

    bool prevLineBrokeCleanly = previousLineBrokeAtBR;
    previousLineBrokeAtBR = false;
    
    while( o ) {
#ifdef DEBUG_LINEBREAKS
        kdDebug(6041) << "new object "<< o <<" width = " << w <<" tmpw = " << tmpW << endl;
#endif
        if(o->isBR()) {
            if( w + tmpW <= width ) {
                lBreak.obj = o;
                lBreak.pos = 0;
             
                // A <br> always breaks a line, so don't let the line be collapsed
                // away. Also, the space at the end of a line with a <br> does not
                // get collapsed away.  It only does this if the previous line broke
                // cleanly.  Otherwise the <br> has no effect on whether the line is
                // empty or not.
                if (prevLineBrokeCleanly)
                    isLineEmpty = false;
                trailingSpaceObject = 0;
                previousLineBrokeAtBR = true;

                if (!isLineEmpty) {
                    // only check the clear status for non-empty lines.
                    EClear clear = o->style()->clear();
                    if(clear != CNONE)
                        m_clearStatus = (EClear) (m_clearStatus | clear);
                }
            }
            goto end;
        }
        if( o->isFloatingOrPositioned() ) {
            // add to special objects...
            if(o->isFloating()) {
                insertFloatingObject(o);
                // check if it fits in the current line.
                // If it does, position it now, otherwise, position
                // it after moving to next line (in newLine() func)
                if (o->width()+o->marginLeft()+o->marginRight()+w+tmpW <= width) {
                    positionNewFloats();
                    width = lineWidth(m_height);
                }
            }
            else if (o->isPositioned()) {
                // If our original display wasn't an inline type, then we can
                // go ahead and determine our static x position now.
                bool isInlineType = o->style()->isOriginalDisplayInlineType();
                bool needToSetStaticX = o->hasStaticX();
                if (o->hasStaticX() && !isInlineType) {
                    o->setStaticX(o->parent()->style()->direction() == LTR ?
                                  borderLeft()+paddingLeft() :
                                  borderRight()+paddingRight());
                    needToSetStaticX = false;
                }

                // If our original display was an INLINE type, then we can go ahead
                // and determine our static y position now.
                bool needToSetStaticY = o->hasStaticY();
                if (o->hasStaticY() && isInlineType) {
                    o->setStaticY(m_height);
                    needToSetStaticY = false;
                }
                
                // If we're ignoring spaces, we have to stop and include this object and
                // then start ignoring spaces again.
                if (needToSetStaticX || needToSetStaticY) {
                    trailingSpaceObject = 0;
                    if (ignoringSpaces) {
                        BidiIterator startMid = { 0, o, 0 };
                        BidiIterator stopMid = { 0, o, 1 };
                        addMidpoint(startMid); // Stop ignoring spaces.
                        addMidpoint(stopMid); // Start ignoring again.
                    }
                }
            }
        } else if (o->isInlineFlow()) {
            // Only empty inlines matter.  We treat those similarly to replaced elements.
            KHTMLAssert(!o->firstChild());
            tmpW += o->marginLeft()+o->borderLeft()+o->paddingLeft()+
                    o->marginRight()+o->borderRight()+o->paddingRight();
        } else if ( o->isReplaced() ) {
            if (o->style()->whiteSpace() == NORMAL || last->style()->whiteSpace() == NORMAL) {
                w += tmpW;
                tmpW = 0;
                lBreak.obj = o;
                lBreak.pos = 0;
            }

            tmpW += o->width()+o->marginLeft()+o->marginRight()+inlineWidth(o);
            if (ignoringSpaces) {
                BidiIterator startMid = { 0, o, 0 };
                addMidpoint(startMid);
            }
            isLineEmpty = false;
            ignoringSpaces = false;
            currentCharacterIsSpace = false;
            lastSpacePos = 0;
            trailingSpaceObject = 0;
            
            if (o->isListMarker() && o->style()->listStylePosition() == OUTSIDE) {
                // The marker must not have an effect on whitespace at the start
                // of the line.  We start ignoring spaces to make sure that any additional
                // spaces we see will be discarded. 
                //
                // Optimize for a common case. If we can't find whitespace after the list
                // item, then this is all moot. -dwh
                RenderObject* next = Bidinext( start.par, o );
                if (!m_pre && next && next->isText() && static_cast<RenderText*>(next)->stringLength() > 0 &&
                      (static_cast<RenderText*>(next)->text()[0].unicode() == ' ' ||
                      static_cast<RenderText*>(next)->text()[0] == '\n')) {
                    currentCharacterIsSpace = true;
                    ignoringSpaces = true;
                    BidiIterator endMid = { 0, o, 0 };
                    addMidpoint(endMid);
                }
            }
        } else if ( o->isText() ) {
            RenderText *t = static_cast<RenderText *>(o);
            int strlen = t->stringLength();
            int len = strlen - pos;
            QChar *str = t->text();

            const Font *f = t->htmlFont( m_firstLine );
            // proportional font, needs a bit more work.
            int lastSpace = pos;
            bool isPre = o->style()->whiteSpace() == PRE;
            int wordSpacing = o->style()->wordSpacing();

            bool appliedStartWidth = pos > 0; // If the span originated on a previous line,
                                              // then assume the start width has been applied.
            bool appliedEndWidth = false;

            while(len) {
                //XXXdwh This is wrong. Still mutating the DOM
                // string for newlines... will fix in second stage.
                if (!isPre && str[pos] == '\n'){
                    str[pos] = ' ';
                }
                    
                bool previousCharacterIsSpace = currentCharacterIsSpace;
                currentCharacterIsSpace = (str[pos].unicode() == ' ');
                    
                if (isPre || !currentCharacterIsSpace)
                    isLineEmpty = false;
                
                bool applyWordSpacing = false;
                if( (isPre && str[pos] == '\n') || (!isPre && isBreakable( str, pos, strlen )) ) {
                    if (ignoringSpaces) {
                        if (!currentCharacterIsSpace) {
                            // Stop ignoring spaces and begin at this
                            // new point.
                            ignoringSpaces = false;
                            lastSpacePos = 0;
                            lastSpace = pos; // e.g., "Foo    goo", don't add in any of the ignored spaces.
                            BidiIterator startMid = { 0, o, pos };
                            addMidpoint(startMid);
                        }
                        else {
                            // Just keep ignoring these spaces.
                            pos++;
                            len--;
                            continue;
                        }
                    }

                    if (currentCharacterIsSpace && !previousCharacterIsSpace)
                        lastSpacePos = pos;
                    tmpW += t->width(lastSpace, pos - lastSpace, f);
                    if (!appliedStartWidth) {
                        tmpW += inlineWidth(o, true, false);
                        appliedStartWidth = true;
                    }
                    
                    applyWordSpacing = (wordSpacing && currentCharacterIsSpace && !previousCharacterIsSpace &&
                        !t->containsOnlyWhitespace(pos+1, strlen-(pos+1)));

#ifdef DEBUG_LINEBREAKS
                    kdDebug(6041) << "found space at " << pos << " in string '" << QString( str, strlen ).latin1() << "' adding " << tmpW << " new width = " << w << endl;
#endif
                    if ( !isPre && w + tmpW > width && w == 0 ) {
                        int fb = nearestFloatBottom(m_height);
                        int newLineWidth = lineWidth(fb);
                        // See if |tmpW| will fit on the new line.  As long as it does not,
                        // keep adjusting our float bottom until we find some room.
                        int lastFloatBottom = m_height;
                        while (lastFloatBottom < fb && tmpW > newLineWidth) {
                            lastFloatBottom = fb;
                            fb = nearestFloatBottom(fb);
                            newLineWidth = lineWidth(fb);
                        }
                        
                        if(!w && m_height < fb && width < newLineWidth) {
                            m_height = fb;
                            width = newLineWidth;
#ifdef DEBUG_LINEBREAKS
                            kdDebug() << "RenderBlock::findNextLineBreak new position at " << m_height << " newWidth " << width << endl;
#endif
                        }
                    }
        
                    if (w + tmpW > width && o->style()->whiteSpace() == NORMAL){
                        goto end;
                    }

                    if( *(str+pos) == '\n' && isPre) {
                        lBreak.obj = o;
                        lBreak.pos = pos;
                        
#ifdef DEBUG_LINEBREAKS
                        kdDebug(6041) << "forced break sol: " << start.obj << " " << start.pos << "   end: " << lBreak.obj << " " << lBreak.pos << "   width=" << w << endl;
#endif
                        return lBreak;
                    }

                    if (o->style()->whiteSpace() == NORMAL) {
                        w += tmpW;
                        tmpW = 0;
                        lBreak.obj = o;
                        lBreak.pos = pos;
                    }
                    
                    lastSpace = pos;
                    
                    if (applyWordSpacing)
                        w += wordSpacing;
                        
                    if (!ignoringSpaces && !isPre) {
                        // If we encounter a newline, or if we encounter a
                        // second space, we need to go ahead and break up this
                        // run and enter a mode where we start collapsing spaces.
                        if (currentCharacterIsSpace && previousCharacterIsSpace){
                            ignoringSpaces = true;
                        }
                        
                        if (ignoringSpaces) {
                            // We just entered a mode where we are ignoring
                            // spaces. Create a midpoint to terminate the run
                            // before the second space. 
                            BidiIterator endMid = { 0, trailingSpaceObject ? trailingSpaceObject : o, lastSpacePos };
                            addMidpoint(endMid);
                            lastSpace = pos;
                        }
                    }
                }
                else if (ignoringSpaces) {
                    // Stop ignoring spaces and begin at this
                    // new point.
                    ignoringSpaces = false;
                    lastSpacePos = 0;
                    lastSpace = pos; // e.g., "Foo    goo", don't add in any of the ignored spaces.
                    BidiIterator startMid = { 0, o, pos };
                    addMidpoint(startMid);
                }
                
                if (!isPre && currentCharacterIsSpace && !ignoringSpaces)
                    trailingSpaceObject = o;
                else if (isPre || !currentCharacterIsSpace)
                    trailingSpaceObject = 0;
                    
                pos++;
                len--;
            }
            
            // IMPORTANT: pos is > length here!
            if (!ignoringSpaces)
                tmpW += t->width(lastSpace, pos - lastSpace, f);
            if (!appliedStartWidth)
                tmpW += inlineWidth(o, true, false);
            if (!appliedEndWidth)
                tmpW += inlineWidth(o, false, true);
        } else
            KHTMLAssert( false );

        RenderObject* next = Bidinext(start.par, o);
        bool isNormal = o->style()->whiteSpace() == NORMAL;
        bool checkForBreak = isNormal;
        if (w && w + tmpW > width+1 && lBreak.obj && o->style()->whiteSpace() == NOWRAP)
            checkForBreak = true;
        else if (next && o->isText() && next->isText() && !next->isBR()) {
            if (isNormal || (next->style()->whiteSpace() == NORMAL)) {
                if (currentCharacterIsSpace)
                    checkForBreak = true;
                else {
                    RenderText* nextText = static_cast<RenderText*>(next);
                    int strlen = nextText->stringLength();
                    QChar *str = nextText->text();
                    if (strlen &&
                        ((str[0].unicode() == ' ') ||
                            (next->style()->whiteSpace() != PRE && str[0] == '\n')))
                        // If the next item on the line is text, and if we did not end with
                        // a space, then the next text run continues our word (and so it needs to
                        // keep adding to |tmpW|.  Just update and continue.
                        checkForBreak = true;
                    else
                        checkForBreak = false;

                    bool canPlaceOnLine = (w + tmpW <= width+1) || !isNormal;
                    if (canPlaceOnLine && checkForBreak) {
                        w += tmpW;
                        tmpW = 0;
                        lBreak.obj = next;
                        lBreak.pos = 0;
                    }
                }
            }
        }

        if (checkForBreak && (w + tmpW > width+1)) {
            //kdDebug() << " too wide w=" << w << " tmpW = " << tmpW << " width = " << width << endl;
            //kdDebug() << "start=" << start.obj << " current=" << o << endl;
            // if we have floats, try to get below them.
            if (currentCharacterIsSpace && !ignoringSpaces && o->style()->whiteSpace() != PRE)
                trailingSpaceObject = 0;
            
            int fb = nearestFloatBottom(m_height);
            int newLineWidth = lineWidth(fb);
            // See if |tmpW| will fit on the new line.  As long as it does not,
            // keep adjusting our float bottom until we find some room.
            int lastFloatBottom = m_height;
            while (lastFloatBottom < fb && tmpW > newLineWidth) {
                lastFloatBottom = fb;
                fb = nearestFloatBottom(fb);
                newLineWidth = lineWidth(fb);
            }            
            if( !w && m_height < fb && width < newLineWidth ) {
                m_height = fb;
                width = newLineWidth;
#ifdef DEBUG_LINEBREAKS
                kdDebug() << "RenderBlock::findNextLineBreak new position at " << m_height << " newWidth " << width << endl;
#endif
            }

            // |width| may have been adjusted because we got shoved down past a float (thus
            // giving us more room), so we need to retest, and only jump to
            // the end label if we still don't fit on the line. -dwh
            if (w + tmpW > width+1)
                goto end;
        }

        last = o;
        o = next;

        if (!last->isFloatingOrPositioned() && last->isReplaced() && last->style()->whiteSpace() == NORMAL) {
            // Go ahead and add in tmpW.
            w += tmpW;
            tmpW = 0;
            lBreak.obj = o;
            lBreak.pos = 0;
        }

        // Clear out our character space bool, since inline <pre>s don't collapse whitespace
        // with adjacent inline normal/nowrap spans.
        if (last->style()->whiteSpace() == PRE)
            currentCharacterIsSpace = false;
        
        pos = 0;
    }

#ifdef DEBUG_LINEBREAKS
    kdDebug( 6041 ) << "end of par, width = " << width << " linewidth = " << w + tmpW << endl;
#endif
    if( w + tmpW <= width || (last && last->style()->whiteSpace() == NOWRAP)) {
        lBreak.obj = 0;
        lBreak.pos = 0;
    }

 end:

    if( lBreak == start && !lBreak.obj->isBR() ) {
        // we just add as much as possible
        if ( m_pre ) {
            if(pos != 0) {
                lBreak.obj = o;
                lBreak.pos = pos - 1;
            } else {
                lBreak.obj = last;
                lBreak.pos = last->isText() ? last->length() : 0;
            }
        } else if( lBreak.obj ) {
            if( last != o ) {
                // better to break between object boundaries than in the middle of a word
                lBreak.obj = o;
                lBreak.pos = 0;
            } else {
                // Don't ever break in the middle of a word if we can help it.
                // There's no room at all. We just have to be on this line,
                // even though we'll spill out.
                lBreak.obj = o;
                lBreak.pos = pos;
            }
        }
    }

    // make sure we consume at least one char/object.
    if( lBreak == start )
        ++lBreak;
    
#ifdef DEBUG_LINEBREAKS
    kdDebug(6041) << "regular break sol: " << start.obj << " " << start.pos << "   end: " << lBreak.obj << " " << lBreak.pos << "   width=" << w << endl;
#endif

    // Sanity check our midpoints.
    checkMidpoints(lBreak);
        
    if (trailingSpaceObject) {
        // This object is either going to be part of the last midpoint, or it is going
        // to be the actual endpoint.  In both cases we just decrease our pos by 1 level to
        // exclude the space, allowing it to - in effect - collapse into the newline.
        if (sNumMidpoints%2==1) {
            BidiIterator* midpoints = smidpoints->data();
            midpoints[sNumMidpoints-1].pos--;
        }
        //else if (lBreak.pos > 0)
        //    lBreak.pos--;
        else if (lBreak.obj == 0 && trailingSpaceObject->isText()) {
            // Add a new end midpoint that stops right at the very end.
            RenderText* text = static_cast<RenderText *>(trailingSpaceObject);
            unsigned pos = text->length() >=2 ? text->length() - 2 : UINT_MAX;
            BidiIterator endMid = { 0, trailingSpaceObject, pos };
            addMidpoint(endMid);
        }
    }

    // We might have made lBreak an iterator that points past the end
    // of the object. Do this adjustment to make it point to the start
    // of the next object instead to avoid confusing the rest of the
    // code.
    if (lBreak.pos > 0) {
	lBreak.pos--;
	++lBreak;
    }

    return lBreak;
}

// For --enable-final
#undef BIDI_DEBUG
#undef DEBUG_LINEBREAKS
#undef DEBUG_LAYOUT

}
