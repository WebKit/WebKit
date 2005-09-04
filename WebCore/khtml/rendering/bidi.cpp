/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
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
#include "bidi.h"
#include "break_lines.h"
#include "render_block.h"
#include "render_text.h"
#include "render_arena.h"
#include "render_canvas.h"
#include "khtmlview.h"
#include "xml/dom_docimpl.h"

#include "kdebug.h"
#include "qdatetime.h"
#include "qfontmetrics.h"

#define BIDI_DEBUG 0
//#define DEBUG_LINEBREAKS

using DOM::AtomicString;

namespace khtml {


// an iterator which goes through a BidiParagraph
struct BidiIterator
{
    BidiIterator() : par(0), obj(0), pos(0) {}
    BidiIterator(RenderBlock *_par, RenderObject *_obj, unsigned int _pos) : par(_par), obj(_obj), pos(_pos) {}
    
    void increment( BidiState &bidi );
    
    bool atEnd() const;
    
    const QChar &current() const;
    QChar::Direction direction() const;
    
    RenderBlock *par;
    RenderObject *obj;
    unsigned int pos;
};

struct BidiStatus {
    BidiStatus() : eor(QChar::DirON), lastStrong(QChar::DirON), last(QChar::DirON) {}
    
    QChar::Direction eor;
    QChar::Direction lastStrong;
    QChar::Direction last;
};
    
struct BidiState {
    BidiState() : context(0) {}
    
    BidiIterator sor;
    BidiIterator eor;
    BidiIterator last;
    BidiIterator current;
    BidiContext *context;
    BidiStatus status;
};

// Used to track a list of chained bidi runs.
static BidiRun* sFirstBidiRun;
static BidiRun* sLastBidiRun;
static int sBidiRunCount;
static BidiRun* sCompactFirstBidiRun;
static BidiRun* sCompactLastBidiRun;
static int sCompactBidiRunCount;
static bool sBuildingCompactRuns;

// Midpoint globals.  The goal is not to do any allocation when dealing with
// these midpoints, so we just keep an array around and never clear it.  We track
// the number of items and position using the two other variables.
static QMemArray<BidiIterator> *smidpoints;
static uint sNumMidpoints;
static uint sCurrMidpoint;
static bool betweenMidpoints;

static bool isLineEmpty = true;
static bool previousLineBrokeCleanly = true;
static QChar::Direction dir;
static bool adjustEmbedding;
static bool emptyRun = true;
static int numSpaces;

static void embed( QChar::Direction d, BidiState &bidi );
static void appendRun( BidiState &bidi );

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

static inline RenderObject *Bidinext(RenderObject *par, RenderObject *current, BidiState &bidi,
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
            if ( next && adjustEmbedding ) {
                EUnicodeBidi ub = next->style()->unicodeBidi();
                if ( ub != UBNormal && !emptyRun ) {
                    EDirection dir = next->style()->direction();
QChar::Direction d = ( ub == Embed ? ( dir == RTL ? QChar::DirRLE : QChar::DirLRE )
                                   : ( dir == RTL ? QChar::DirRLO : QChar::DirLRO ) );
                    embed( d, bidi );
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
                if ( adjustEmbedding && current->style()->unicodeBidi() != UBNormal && !emptyRun ) {
                    embed( QChar::DirPDF, bidi );
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

static RenderObject *first( RenderObject *par, BidiState &bidi, bool skipInlines = true )
{
    if(!par->firstChild()) return 0;
    RenderObject *o = par->firstChild();

    if (o->isInlineFlow()) {
        if (skipInlines && o->firstChild())
            o = Bidinext( par, o, bidi, skipInlines );
        else
            return o; // Never skip empty inlines.
    }

    if (o && !o->isText() && !o->isBR() && !o->isReplaced() && !o->isFloating() && !o->isPositioned())
        o = Bidinext( par, o, bidi, skipInlines );
    return o;
}

inline void BidiIterator::increment (BidiState &bidi)
{
    if(!obj) return;
    if(obj->isText()) {
        pos++;
        if(pos >= static_cast<RenderText *>(obj)->stringLength()) {
            obj = Bidinext( par, obj, bidi );
            pos = 0;
        }
    } else {
        obj = Bidinext( par, obj, bidi );
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
    static QChar nullCharacter;
    
    if (!obj || !obj->isText())
      return nullCharacter;
    
    RenderText* text = static_cast<RenderText*>(obj);
    if (!text->text())
        return nullCharacter;
    
    return text->text()[pos];
}

inline QChar::Direction BidiIterator::direction() const
{
    if (!obj)
        return QChar::DirON;
    if (obj->isListMarker())
        return obj->style()->direction() == LTR ? QChar::DirL : QChar::DirR;
    if (!obj->isText())
        return QChar::DirON;

    RenderText *renderTxt = static_cast<RenderText *>(obj);
    if (pos >= renderTxt->stringLength())
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
        if (text->text()) {
            for (int i = bidiRun->start; i < bidiRun->stop; i++) {
                const QChar c = text->text()[i];
                if (c == ' ' || c == '\n' || c == '\t')
                    numSpaces++;
            }
        }
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

static void chopMidpointsAt(RenderObject* obj, uint pos)
{
    if (!sNumMidpoints) return;
    BidiIterator* midpoints = smidpoints->data();
    for (uint i = 0; i < sNumMidpoints; i++) {
        const BidiIterator& point = midpoints[i];
        if (point.obj == obj && point.pos == pos) {
            sNumMidpoints = i;
            break;
        }
    }
}

static void checkMidpoints(BidiIterator& lBreak, BidiState &bidi)
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
            currpoint.increment( bidi );
        if (currpoint == lBreak) {
            // We hit the line break before the start point.  Shave off the start point.
            sNumMidpoints--;
            if (endpoint.obj->style()->whiteSpace() != PRE) {
                if (endpoint.obj->isText()) {
                    // Don't shave a character off the endpoint if it was from a soft hyphen.
                    RenderText* textObj = static_cast<RenderText*>(endpoint.obj);
                    if (endpoint.pos+1 < textObj->length() &&
                        textObj->text()[endpoint.pos+1].unicode() == SOFT_HYPHEN)
                        return;
                }
                endpoint.pos--;
            }
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

static void appendRunsForObject(int start, int end, RenderObject* obj, BidiState &bidi)
{
    if (start > end || obj->isFloating() ||
        (obj->isPositioned() && !obj->hasStaticX() && !obj->hasStaticY() && !obj->container()->isInlineFlow()))
        return;

    bool haveNextMidpoint = (smidpoints && sCurrMidpoint < sNumMidpoints);
    BidiIterator nextMidpoint;
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
            return appendRunsForObject(start, end, obj, bidi);
    }
    else {
        if (!smidpoints || !haveNextMidpoint || (obj != nextMidpoint.obj)) {
            addRun(new (obj->renderArena()) BidiRun(start, end, obj, bidi.context, dir));
            return;
        }
        
        // An end midpoint has been encountered within our object.  We
        // need to go ahead and append a run with our endpoint.
        if (int(nextMidpoint.pos+1) <= end) {
            betweenMidpoints = true;
            sCurrMidpoint++;
            if (nextMidpoint.pos != UINT_MAX) { // UINT_MAX means stop at the object and don't include any of it.
                addRun(new (obj->renderArena())
                    BidiRun(start, nextMidpoint.pos+1, obj, bidi.context, dir));
                return appendRunsForObject(nextMidpoint.pos+1, end, obj, bidi);
            }
        }
        else
           addRun(new (obj->renderArena()) BidiRun(start, end, obj, bidi.context, dir));
    }
}

static void appendRun( BidiState &bidi )
{
    if (emptyRun || !bidi.eor.obj)
        return;
#if BIDI_DEBUG > 1
    kdDebug(6041) << "appendRun: dir="<<(int)dir<<endl;
#endif

    bool b = adjustEmbedding;
    adjustEmbedding = false;

    int start = bidi.sor.pos;
    RenderObject *obj = bidi.sor.obj;
    while( obj && obj != bidi.eor.obj ) {
        appendRunsForObject(start, obj->length(), obj, bidi);        
        start = 0;
        obj = Bidinext( bidi.sor.par, obj, bidi );
    }
    if (obj) {
        // It's OK to add runs for zero-length RenderObjects, just don't make the run larger than it should be
        int end = obj->length() ? bidi.eor.pos+1 : 0;
        appendRunsForObject(start, end, obj, bidi);
    }
    
    bidi.eor.increment( bidi );
    bidi.sor = bidi.eor;
    dir = QChar::DirON;
    bidi.status.eor = QChar::DirON;
    adjustEmbedding = b;
}

static void embed( QChar::Direction d, BidiState &bidi )
{
#if BIDI_DEBUG > 1
    qDebug("*** embed dir=%d emptyrun=%d", d, emptyRun );
#endif
    bool b = adjustEmbedding ;
    adjustEmbedding = false;
    if ( d == QChar::DirPDF ) {
	BidiContext *c = bidi.context->parent;
	if (c) {
	    if ( bidi.eor != bidi.last ) {
		appendRun( bidi );
		bidi.eor = bidi.last;
	    }
	    appendRun( bidi );
	    emptyRun = true;
	    bidi.status.last = bidi.context->dir;
	    bidi.context->deref();
	    bidi.context = c;
	    if(bidi.context->override)
		dir = bidi.context->dir;
	    else
		dir = QChar::DirON;
	    bidi.status.lastStrong = bidi.context->dir;
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

	unsigned char level = bidi.context->level;
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
	    if ( bidi.eor != bidi.last ) {
                appendRun( bidi );
                bidi.eor = bidi.last;
            }
            appendRun( bidi );
            emptyRun = true;

	    bidi.context = new BidiContext(level, runDir, bidi.context, override);
	    bidi.context->ref();
            dir = runDir;
	    bidi.status.last = runDir;
	    bidi.status.lastStrong = runDir;
	    bidi.status.eor = runDir;
	}
    }
    adjustEmbedding = b;
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

RootInlineBox* RenderBlock::constructLine(const BidiIterator &start, const BidiIterator &end)
{
    if (!sFirstBidiRun)
        return 0; // We had no runs. Don't make a root inline box at all. The line is empty.

    InlineFlowBox* parentBox = 0;
    for (BidiRun* r = sFirstBidiRun; r; r = r->nextRun) {
        // Create a box for our object.
        r->box = r->obj->createInlineBox(r->obj->isPositioned(), false, sBidiRunCount == 1);
        if (r->box) {
            // If we have no parent box yet, or if the run is not simply a sibling,
            // then we need to construct inline boxes as necessary to properly enclose the
            // run's inline box.
            if (!parentBox || (parentBox->object() != r->obj->parent()))
                // Create new inline boxes all the way back to the appropriate insertion point.
                parentBox = createLineBoxes(r->obj->parent());

            // Append the inline box to this line.
            parentBox->addToLine(r->box);
            
            if (r->box->isInlineTextBox()) {
                InlineTextBox *text = static_cast<InlineTextBox*>(r->box);
                text->setStart(r->start);
                text->setLen(r->stop-r->start);
            }
              
        }
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
    return lastRootBox();
}

// usage: tw - (xpos % tw);
int RenderBlock::tabWidth(bool isWhitespacePre)
{
    if (!isWhitespacePre)
        return 0;

    if (!m_tabWidth) {
        QChar   spaceChar(' ');
        const Font& font = style()->htmlFont();
        int spaceWidth = font.width(&spaceChar, 1, 0, 0);
        m_tabWidth = spaceWidth * 8;
        assert(m_tabWidth != 0);
    }

    return m_tabWidth;
}

void RenderBlock::computeHorizontalPositionsForLine(RootInlineBox* lineBox, BidiState &bidi)
{
    // First determine our total width.
    int availableWidth = lineWidth(m_height);
    int totWidth = lineBox->getFlowSpacingWidth();
    BidiRun* r = 0;
    bool needsWordSpacing = false;
    for (r = sFirstBidiRun; r; r = r->nextRun) {
        if (!r->box || r->obj->isPositioned())
            continue; // Positioned objects are only participating to figure out their
                      // correct static x position.  They have no effect on the width.
        if (r->obj->isText()) {
            RenderText *rt = static_cast<RenderText *>(r->obj);
            int textWidth = rt->width(r->start, r->stop-r->start, totWidth, m_firstLine);
            int effectiveWidth = textWidth;
            int rtLength = rt->length();
            if (rtLength != 0) {
                if (r->start == 0 && needsWordSpacing && rt->text()[r->start].isSpace())
                    effectiveWidth += rt->htmlFont(m_firstLine)->getWordSpacing();
                needsWordSpacing = !rt->text()[r->stop-1].isSpace() && r->stop == rtLength;          
            }
            if (!r->compact) {
                RenderStyle *style = r->obj->style();
                if (style->whiteSpace() == NORMAL && style->khtmlLineBreak() == AFTER_WHITE_SPACE) {
                    // shrink the box as needed to keep the line from overflowing the available width
                    textWidth = kMin(effectiveWidth, availableWidth - totWidth + style->borderLeftWidth());
                }
            }
            r->box->setWidth(textWidth);
        } else if (!r->obj->isInlineFlow()) {
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
    switch(style()->textAlign()) {
        case LEFT:
        case KHTML_LEFT:
            // The direction of the block should determine what happens with wide lines.  In
            // particular with RTL blocks, wide lines should still spill out to the left.
            if (style()->direction() == RTL && totWidth > availableWidth)
                x -= (totWidth - availableWidth);
            numSpaces = 0;
            break;
        case JUSTIFY:
            if (numSpaces != 0 && !bidi.current.atEnd() && !lineBox->endsWithBreak())
                break;
            // fall through
        case TAAUTO:
            numSpaces = 0;
            // for right to left fall through to right aligned
            if (bidi.context->basicDir == QChar::DirL)
                break;
        case RIGHT:
        case KHTML_RIGHT:
            // Wide lines spill out of the block based off direction.
            // So even if text-align is right, if direction is LTR, wide lines should overflow out of the right
            // side of the block.
            if (style()->direction() == RTL || totWidth < availableWidth)
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
            if (!r->box) continue;

            int spaceAdd = 0;
            if (numSpaces > 0 && r->obj->isText() && !r->compact) {
                // get the number of spaces in the run
                int spaces = 0;
                for ( int i = r->start; i < r->stop; i++ ) {
                    const QChar c = static_cast<RenderText *>(r->obj)->text()[i];
                    if (c == ' ' || c == '\n' || c == '\t')
                        spaces++;
                }

                KHTMLAssert(spaces <= numSpaces);

                // Only justify text with white-space: normal.
                if (r->obj->style()->whiteSpace() != PRE) {
                    spaceAdd = (availableWidth - totWidth)*spaces/numSpaces;
                    static_cast<InlineTextBox*>(r->box)->setSpaceAdd(spaceAdd);
                    totWidth += spaceAdd;
                }
                numSpaces -= spaces;
            }
        }
    }
    
    // The widths of all runs are now known.  We can now place every inline box (and
    // compute accurate widths for the inline flow boxes).
    int leftPosition = x;
    int rightPosition = x;
    needsWordSpacing = false;
    lineBox->placeBoxesHorizontally(x, leftPosition, rightPosition, needsWordSpacing);
    lineBox->setHorizontalOverflowPositions(leftPosition, rightPosition);
}

void RenderBlock::computeVerticalPositionsForLine(RootInlineBox* lineBox)
{
    lineBox->verticallyAlignBoxes(m_height);
    lineBox->setBlockHeight(m_height);

    // See if the line spilled out.  If so set overflow height accordingly.
    int bottomOfLine = lineBox->bottomOverflow();
    if (bottomOfLine > m_height && bottomOfLine > m_overflowHeight)
        m_overflowHeight = bottomOfLine;
        
    // Now make sure we place replaced render objects correctly.
    for (BidiRun* r = sFirstBidiRun; r; r = r->nextRun) {
        if (!r->box) continue; // Skip runs with no line boxes.

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
void RenderBlock::bidiReorderLine(const BidiIterator &start, const BidiIterator &end, BidiState &bidi)
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

    dir = bidi.context->dir;

    emptyRun = true;
    bidi.eor.obj = 0;

    numSpaces = 0;

    bidi.current = start;
    bidi.last = bidi.current;
    bool atEnd = false;

    while( 1 ) {

        QChar::Direction dirCurrent;
        if (atEnd) {
            //kdDebug(6041) << "atEnd" << endl;
            BidiContext *c = bidi.context;
            if ( bidi.current.atEnd())
                while ( c->parent )
                    c = c->parent;
            dirCurrent = c->dir;
        } else {
            dirCurrent = bidi.current.direction();
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
            embed( dirCurrent, bidi );
            break;

            // strong types
        case QChar::DirL:
            if(dir == QChar::DirON)
                dir = QChar::DirL;
            switch(bidi.status.last)
                {
                case QChar::DirR:
                case QChar::DirAL:
                case QChar::DirEN:
                case QChar::DirAN:
                    if (bidi.status.last != QChar::DirEN || bidi.status.lastStrong != QChar::DirL)
                        appendRun( bidi );
                    dir = QChar::DirL;
                    // fall through
                case QChar::DirL:
                    bidi.eor = bidi.current;
                    bidi.status.eor = QChar::DirL;
                    break;
                case QChar::DirES:
                case QChar::DirET:
                case QChar::DirCS:
                case QChar::DirBN:
                case QChar::DirB:
                case QChar::DirS:
                case QChar::DirWS:
                case QChar::DirON:
                    if (bidi.status.eor == QChar::DirEN) {
                        if (bidi.status.lastStrong != QChar::DirL) {
                            // the numbers need to be on a higher embedding level, so let's close that run
                            dir = QChar::DirEN;
                            appendRun(bidi);
                            if (bidi.context->dir != QChar::DirL) {
                                // the neutrals take the embedding direction, which is R
                                bidi.eor = bidi.last;
                                dir = QChar::DirR;
                                appendRun(bidi);
                            }
                        }
                    } else if (bidi.status.eor == QChar::DirAN) {
                        // Arabic numbers are always on a higher embedding level, so let's close that run
                        dir = QChar::DirAN;
                        appendRun(bidi);
                        if (bidi.context->dir != QChar::DirL) {
                            // the neutrals take the embedding direction, which is R
                            bidi.eor = bidi.last;
                            dir = QChar::DirR;
                            appendRun(bidi);
                        }
                    } else if(bidi.status.eor != QChar::DirL) {
                        //last stuff takes embedding dir
                        if(bidi.context->dir == QChar::DirL || bidi.status.lastStrong == QChar::DirL) { 
                            if (bidi.status.eor != QChar::DirON) 
                            appendRun( bidi );
                        } else {
                            dir = QChar::DirR; 
                            bidi.eor = bidi.last; 
                            appendRun( bidi ); 
                        }
                    }
                    bidi.eor = bidi.current;
                    bidi.status.eor = QChar::DirL;
                    dir = QChar::DirL;
                default:
                    break;
                }
            bidi.status.lastStrong = QChar::DirL;
            break;
        case QChar::DirAL:
        case QChar::DirR:
            if(dir == QChar::DirON) dir = QChar::DirR;
            switch(bidi.status.last)
                {
                case QChar::DirR:
                case QChar::DirAL:
                    bidi.eor = bidi.current; bidi.status.eor = QChar::DirR; break;
                case QChar::DirL:
                case QChar::DirEN:
                case QChar::DirAN:
                    appendRun( bidi );
		    dir = QChar::DirR;
		    bidi.eor = bidi.current;
		    bidi.status.eor = QChar::DirR;
                    break;
                case QChar::DirES:
                case QChar::DirET:
                case QChar::DirCS:
                case QChar::DirBN:
                case QChar::DirB:
                case QChar::DirS:
                case QChar::DirWS:
                case QChar::DirON:
                    if( !(bidi.status.eor == QChar::DirR) && !(bidi.status.eor == QChar::DirAL) ) {
                        //last stuff takes embedding dir
                        if(bidi.context->dir == QChar::DirR || bidi.status.lastStrong == QChar::DirR 
                            || bidi.status.lastStrong == QChar::DirAL) { 
                            appendRun( bidi );
                            dir = QChar::DirR;
                            bidi.eor = bidi.current;
			    bidi.status.eor = QChar::DirR;
                        } else {
                            dir = QChar::DirL; 
                            bidi.eor = bidi.last;
                            appendRun( bidi );
                            dir = QChar::DirR;
			    bidi.status.eor = QChar::DirR;
                        }
                    } else {
                        bidi.eor = bidi.current; bidi.status.eor = QChar::DirR;
                    }
                default:
                    break;
                }
            bidi.status.lastStrong = dirCurrent;
            break;

            // weak types:

        case QChar::DirNSM:
            // ### if @sor, set dir to dirSor
            break;
        case QChar::DirEN:
            if(!(bidi.status.lastStrong == QChar::DirAL)) {
                // if last strong was AL change EN to AN
                if(dir == QChar::DirON) {
                        dir = QChar::DirL;
                }
                switch(bidi.status.last)
                    {
                    case QChar::DirET:
			if ( bidi.status.lastStrong == QChar::DirR || bidi.status.lastStrong == QChar::DirAL ) {
			    appendRun( bidi );
                            dir = QChar::DirEN;
                            bidi.status.eor = QChar::DirEN;
			}
			// fall through
                    case QChar::DirEN:
                    case QChar::DirL:
                        bidi.eor = bidi.current;
                        bidi.status.eor = dirCurrent;
                        break;
                    case QChar::DirR:
                    case QChar::DirAL:
                    case QChar::DirAN:
                        appendRun( bidi );
			bidi.status.eor = QChar::DirEN;
                        dir = QChar::DirEN; break;
                    case QChar::DirES:
                    case QChar::DirCS:
                        if(bidi.status.eor == QChar::DirEN) {
                            bidi.eor = bidi.current; break;
                        }
                    case QChar::DirBN:
                    case QChar::DirB:
                    case QChar::DirS:
                    case QChar::DirWS:
                    case QChar::DirON:
                        if(bidi.status.eor == QChar::DirR) {
                            // neutrals go to R
                            bidi.eor = bidi.last;
                            appendRun( bidi );
                            dir = QChar::DirEN;
                            bidi.status.eor = QChar::DirEN;
                        }
                        else if( bidi.status.eor == QChar::DirL ||
                                 (bidi.status.eor == QChar::DirEN && bidi.status.lastStrong == QChar::DirL)) {
                            bidi.eor = bidi.current; bidi.status.eor = dirCurrent;
                        } else {
                            // numbers on both sides, neutrals get right to left direction
                            if(dir != QChar::DirL) {
                                appendRun( bidi );
                                bidi.eor = bidi.last;
                                dir = QChar::DirR;
                                appendRun( bidi );
                                dir = QChar::DirEN;
                                bidi.status.eor = QChar::DirEN;
                            } else {
                                bidi.eor = bidi.current; bidi.status.eor = dirCurrent;
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
            switch(bidi.status.last)
                {
                case QChar::DirL:
                case QChar::DirAN:
                    bidi.eor = bidi.current; bidi.status.eor = QChar::DirAN; break;
                case QChar::DirR:
                case QChar::DirAL:
                case QChar::DirEN:
                    appendRun( bidi );
                    dir = QChar::DirAN; bidi.status.eor = QChar::DirAN;
                    break;
                case QChar::DirCS:
                    if(bidi.status.eor == QChar::DirAN) {
                        bidi.eor = bidi.current; break;
                    }
                case QChar::DirES:
                case QChar::DirET:
                case QChar::DirBN:
                case QChar::DirB:
                case QChar::DirS:
                case QChar::DirWS:
                case QChar::DirON:
                    if(bidi.status.eor != QChar::DirR && bidi.status.eor != QChar::DirAL) {
                        // run of L before neutrals, neutrals take embedding dir (N2)
                        if(bidi.context->dir == QChar::DirR || bidi.status.lastStrong == QChar::DirR 
                            || bidi.status.lastStrong == QChar::DirAL) { 
                            // the embedding direction is R
                            // close the L run
                            appendRun( bidi );
                            // neutrals become an R run
                            bidi.eor = bidi.last;
                            dir = QChar::DirR;
                            appendRun( bidi );
                            bidi.eor = bidi.current;
                        } else {
                            // the embedding direction is L
                            // append neutrals to the L run and close it
                            dir = QChar::DirL; 
                            bidi.eor = bidi.last;
                            appendRun(bidi);
                        }
                    } else {
                        bidi.eor = bidi.last;
                        appendRun(bidi);
                        bidi.eor = bidi.current;
                    }
                    dir = QChar::DirAN;
                    bidi.status.eor = QChar::DirAN;
                default:
                    break;
                }
            break;
        case QChar::DirES:
        case QChar::DirCS:
            break;
        case QChar::DirET:
            if(bidi.status.last == QChar::DirEN) {
                dirCurrent = QChar::DirEN;
                bidi.eor = bidi.current; bidi.status.eor = dirCurrent;
                break;
            } else if ((bidi.status.eor == QChar::DirR || bidi.status.eor == QChar::DirAL || bidi.status.eor == QChar::DirAN || (bidi.status.eor == QChar::DirEN && bidi.status.lastStrong == QChar::DirR)) && bidi.last!=bidi.current) {
                // most of the time this is unnecessary, but we need to secure the R run in case
                // the ET ends up being neutral and followed by L
                if (bidi.status.last!=QChar::DirET) {
                    dir = bidi.status.eor;
                    appendRun(bidi);
                    bidi.eor = bidi.last;
                }
                bidi.status.eor = QChar::DirR;
                dir = QChar::DirR;
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

        if(bidi.current.atEnd()) break;

        // set status.last as needed.
        switch(dirCurrent)
            {
            case QChar::DirET:
                if (bidi.status.last != QChar::DirEN)
                    bidi.status.last = QChar::DirET;
                break;
            case QChar::DirES:
            case QChar::DirCS:
            case QChar::DirS:
            case QChar::DirWS:
            case QChar::DirON:
                switch(bidi.status.last)
                    {
                    case QChar::DirL:
                    case QChar::DirR:
                    case QChar::DirAL:
                    case QChar::DirEN:
                    case QChar::DirAN:
                        bidi.status.last = dirCurrent;
                        break;
                    default:
                        bidi.status.last = QChar::DirON;
                    }
                break;
            case QChar::DirNSM:
            case QChar::DirBN:
                // ignore these
                break;
            case QChar::DirEN:
                // fall through
            default:
                bidi.status.last = dirCurrent;
            }
#endif

	if ( atEnd ) break;
        bidi.last = bidi.current;

	if ( emptyRun ) {
	    bidi.sor = bidi.current;
	    emptyRun = false;
	}

	// this causes the operator ++ to open and close embedding levels as needed
	// for the CSS unicode-bidi property
	adjustEmbedding = true;
        bidi.current.increment( bidi );
	adjustEmbedding = false;

	if ( bidi.current == end ) {
	    if ( emptyRun )
		break;
	    atEnd = true;
	}
    }

#if BIDI_DEBUG > 0
    kdDebug(6041) << "reached end of line current=" << current.obj << "/" << current.pos
		  << ", eor=" << eor.obj << "/" << eor.pos << endl;
#endif
    if ( !emptyRun && bidi.sor != bidi.current ) {
	    bidi.eor = bidi.last;
	    appendRun( bidi );
    }

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

static void buildCompactRuns(RenderObject* compactObj, BidiState &bidi)
{
    sBuildingCompactRuns = true;
    if (!compactObj->isRenderBlock()) {
        // Just append a run for our object.
        isLineEmpty = false;
        addRun(new (compactObj->renderArena()) BidiRun(0, compactObj->length(), compactObj, bidi.context, dir));
    }
    else {
        // Format the compact like it is its own single line.  We build up all the runs for
        // the little compact and then reorder them for bidi.
        RenderBlock* compactBlock = static_cast<RenderBlock*>(compactObj);
        adjustEmbedding = true;
        BidiIterator start(compactBlock, first(compactBlock, bidi), 0);
        adjustEmbedding = false;
        BidiIterator end = start;
    
        betweenMidpoints = false;
        isLineEmpty = true;
        previousLineBrokeCleanly = true;
        
        end = compactBlock->findNextLineBreak(start, bidi);
        if (!isLineEmpty)
            compactBlock->bidiReorderLine(start, end, bidi);
    }
    
    
    sCompactFirstBidiRun = sFirstBidiRun;
    sCompactLastBidiRun = sLastBidiRun;
    sCompactBidiRunCount = sBidiRunCount;
    
    sNumMidpoints = 0;
    sCurrMidpoint = 0;
    betweenMidpoints = false;
    sBuildingCompactRuns = false;
}

QRect RenderBlock::layoutInlineChildren(bool relayoutChildren)
{
    BidiState bidi;

    bool useRepaintRect = false;
    QRect repaintRect(0,0,0,0);

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
    
    m_height = borderTop() + paddingTop();
    int toAdd = borderBottom() + paddingBottom();
    if (includeScrollbarSize())
        toAdd += m_layer->horizontalScrollbarHeight();
    
    // Figure out if we should clear out our line boxes.
    // FIXME: Handle resize eventually!
    // FIXME: Do something better when floats are present.
    bool fullLayout = !firstLineBox() || !firstChild() || selfNeedsLayout() || relayoutChildren || containsFloats();
    if (fullLayout)
        deleteLineBoxes();
        
    // Text truncation only kicks in if your overflow isn't visible and your text-overflow-mode isn't
    // clip.
    // FIXME: CSS3 says that descendants that are clipped must also know how to truncate.  This is insanely
    // difficult to figure out (especially in the middle of doing layout), and is really an esoteric pile of nonsense
    // anyway, so we won't worry about following the draft here.
    bool hasTextOverflow = style()->textOverflow() && hasOverflowClip();
    
    // Walk all the lines and delete our ellipsis line boxes if they exist.
    if (hasTextOverflow)
         deleteEllipsisLineBoxes();

    int oldLineBottom = lastRootBox() ? lastRootBox()->bottomOverflow() : m_height;
    int startLineBottom = 0;

    if (firstChild()) {
        // layout replaced elements
        bool endOfInline = false;
        RenderObject *o = first(this, bidi, false);
        bool hasFloat = false;
        while (o) {
            if (o->isReplaced() || o->isFloating() || o->isPositioned()) {
                if (relayoutChildren || o->style()->width().isPercent() || o->style()->height().isPercent())
                    o->setChildNeedsLayout(true, false);
                if (o->isPositioned())
                    o->containingBlock()->insertPositionedObject(o);
                else {
                    if (o->isFloating())
                        hasFloat = true;
                    else if (fullLayout || o->needsLayout()) // Replaced elements
                        o->dirtyLineBoxes(fullLayout);
                    o->layoutIfNeeded();
                }
            }
            else if (o->isText() || (o->isInlineFlow() && !endOfInline)) {
                if (fullLayout || o->selfNeedsLayout())
                    o->dirtyLineBoxes(fullLayout);
                o->setNeedsLayout(false);
            }
            o = Bidinext( this, o, bidi, false, &endOfInline);
        }

        if (hasFloat)
            fullLayout = true; // FIXME: Will need to find a way to optimize floats some day.
        
        if (fullLayout && !selfNeedsLayout()) {
            setNeedsLayout(true, false);  // Mark ourselves as needing a full layout. This way we'll repaint like
                                          // we're supposed to.
            if (!document()->view()->needsFullRepaint() && m_layer) {
                // Because we waited until we were already inside layout to discover
                // that the block really needed a full layout, we missed our chance to repaint the layer
                // before layout started.  Luckily the layer has cached the repaint rect for its original
                // position and size, and so we can use that to make a repaint happen now.
                RenderCanvas* c = canvas();
                if (c && !c->printingMode())
                    c->repaintViewRectangle(m_layer->repaintRect());
            }
        }

        BidiContext *startEmbed;
        if( style()->direction() == LTR ) {
            startEmbed = new BidiContext( 0, QChar::DirL );
            bidi.status.eor = QChar::DirL;
        } else {
            startEmbed = new BidiContext( 1, QChar::DirR );
            bidi.status.eor = QChar::DirR;
        }
        startEmbed->ref();

        bidi.status.lastStrong = startEmbed->dir;
        bidi.status.last = startEmbed->dir;
        bidi.context = startEmbed;
        
        if (!smidpoints)
            smidpoints = new QMemArray<BidiIterator>;
        
        sNumMidpoints = 0;
        sCurrMidpoint = 0;
        sCompactFirstBidiRun = sCompactLastBidiRun = 0;
        sCompactBidiRunCount = 0;
        
        // We want to skip ahead to the first dirty line
        BidiIterator start;
        RootInlineBox* startLine = determineStartPosition(fullLayout, start, bidi);
        
        // We also find the first clean line and extract these lines.  We will add them back
        // if we determine that we're able to synchronize after handling all our dirty lines.
        BidiIterator cleanLineStart;
        int endLineYPos;
        RootInlineBox* endLine = (fullLayout || !startLine) ? 
                                 0 : determineEndPosition(startLine, cleanLineStart, endLineYPos);
        if (startLine) {
            useRepaintRect = true;
            startLineBottom = startLine->bottomOverflow();
            repaintRect.setY(kMin(m_height, startLine->topOverflow()));
            RenderArena* arena = renderArena();
            RootInlineBox* box = startLine;
            while (box) {
                RootInlineBox* next = box->nextRootBox();
                box->deleteLine(arena);
                box = next;
            }
            startLine = 0;
        }
        
        BidiIterator end = start;

        bool endLineMatched = false;
        while (!end.atEnd()) {
            start = end;
            if (endLine && (endLineMatched = matchedEndLine(start, cleanLineStart, endLine, endLineYPos)))
                break;

            betweenMidpoints = false;
            isLineEmpty = true;
            if (m_firstLine && firstChild() && firstChild()->isCompact() && firstChild()->isRenderBlock()) {
                buildCompactRuns(firstChild(), bidi);
                start.obj = firstChild()->nextSibling();
                end = start;
            }
            end = findNextLineBreak(start, bidi);
            if( start.atEnd() ) break;
            if (!isLineEmpty) {
                bidiReorderLine(start, end, bidi);

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

                RootInlineBox* lineBox = 0;
                if (sBidiRunCount) {
                    lineBox = constructLine(start, end);
                    if (lineBox) {
                        lineBox->setEndsWithBreak(previousLineBrokeCleanly);
                        
                        // Now we position all of our text runs horizontally.
                        computeHorizontalPositionsForLine(lineBox, bidi);
        
                        // Now position our text runs vertically.
                        computeVerticalPositionsForLine(lineBox);
        
                        deleteBidiRuns(renderArena());
                    }
                }
                
                if (end == start || (!previousLineBrokeCleanly && end.obj && end.obj->style()->whiteSpace() == PRE && end.current() == QChar('\n'))) {
                    adjustEmbedding = true;
                    end.increment(bidi);
                    adjustEmbedding = false;
                }

                if (lineBox)
                    lineBox->setLineBreakInfo(end.obj, end.pos);
                
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
        
        if (endLine) {
            if (endLineMatched) {
                // Note our current y-position for correct repainting when no lines move.  If no lines move, we still have to
                // repaint up to the maximum of the bottom overflow of the old start line or the bottom overflow of the new last line.
                int currYPos = kMax(startLineBottom, m_height);
                if (lastRootBox())
                    currYPos = kMax(currYPos, lastRootBox()->bottomOverflow());
                
                // Attach all the remaining lines, and then adjust their y-positions as needed.
                for (RootInlineBox* line = endLine; line; line = line->nextRootBox())
                    line->attachLine();
                
                // Now apply the offset to each line if needed.
                int delta = m_height - endLineYPos;
                if (delta) {
                    for (RootInlineBox* line = endLine; line; line = line->nextRootBox())
                        line->adjustPosition(0, delta);
                }
                m_height = lastRootBox()->blockHeight();
                m_overflowHeight = kMax(m_height, m_overflowHeight);
                int bottomOfLine = lastRootBox()->bottomOverflow();
                if (bottomOfLine > m_height && bottomOfLine > m_overflowHeight)
                    m_overflowHeight = bottomOfLine;
                if (delta)
                    repaintRect.setHeight(kMax(m_overflowHeight-delta, m_overflowHeight) - repaintRect.y());
                else
                    repaintRect.setHeight(currYPos - repaintRect.y());
            }
            else {
                // Delete all the remaining lines.
                m_overflowHeight = kMax(m_height, m_overflowHeight);
                InlineRunBox* line = endLine;
                RenderArena* arena = renderArena();
                while (line) {
                    InlineRunBox* next = line->nextLineBox();
                    if (!next)
                        repaintRect.setHeight(kMax(m_overflowHeight, line->bottomOverflow()) - repaintRect.y());
                    line->deleteLine(arena);
                    line = next;
                }
            }
        }
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
    m_overflowHeight = kMax(m_height, m_overflowHeight);
    
    // See if any lines spill out of the block.  If so, we need to update our overflow width.
    checkLinesForOverflow();

    if (useRepaintRect) {
        repaintRect.setWidth(kMax((int)m_width, m_overflowWidth));
        if (repaintRect.height() == 0)
            repaintRect.setHeight(kMax(oldLineBottom, m_overflowHeight) - repaintRect.y());
    }

    if (!firstLineBox() && element() && element()->isContentEditable() && element()->rootEditableElement() == element())
        m_height += lineHeight(true);

    // See if we have any lines that spill out of our block.  If we do, then we will possibly need to
    // truncate text.
    if (hasTextOverflow)
        checkLinesForTextOverflow();

    return repaintRect;

#if BIDI_DEBUG > 1
    kdDebug(6041) << " ------- bidi end " << this << " -------" << endl;
#endif
    //kdDebug() << "RenderBlock::layoutInlineChildren time used " << qt.elapsed() << endl;
    //kdDebug(6040) << "height = " << m_height <<endl;
}

RootInlineBox* RenderBlock::determineStartPosition(bool fullLayout, BidiIterator& start, BidiState& bidi)
{
    RootInlineBox* curr = 0;
    RootInlineBox* last = 0;
    RenderObject* startObj = 0;
    int pos = 0;
    
    if (fullLayout) {
        // Nuke all our lines.
        if (firstRootBox()) {
            RenderArena* arena = renderArena();
            curr = firstRootBox(); 
            while (curr) {
                RootInlineBox* next = curr->nextRootBox();
                curr->deleteLine(arena);
                curr = next;
            }
            KHTMLAssert(!m_firstLineBox && !m_lastLineBox);
        }
    }
    else {
        for (curr = firstRootBox(); curr && !curr->isDirty(); curr = curr->nextRootBox());
        if (curr) {
            // We have a dirty line.
            if (curr->prevRootBox()) {
                // We have a previous line.
                if (!curr->prevRootBox()->endsWithBreak())
                    curr = curr->prevRootBox();  // The previous line didn't break cleanly, so treat it as dirty also.
            }
        }
        else {
            // No dirty lines were found.
            // If the last line didn't break cleanly, treat it as dirty.
            if (lastRootBox() && !lastRootBox()->endsWithBreak())
                curr = lastRootBox();
        }
        
        // If we have no dirty lines, then last is just the last root box.
        last = curr ? curr->prevRootBox() : lastRootBox();
    }
    
    m_firstLine = !last;
    previousLineBrokeCleanly = !last || last->endsWithBreak();
    if (last) {
        m_height = last->blockHeight();
        int bottomOfLine = last->bottomOverflow();
        if (bottomOfLine > m_height && bottomOfLine > m_overflowHeight)
            m_overflowHeight = bottomOfLine;
        startObj = last->lineBreakObj();
        pos = last->lineBreakPos();
    }
    else
        startObj = first(this, bidi, 0);
        
    adjustEmbedding = true;
    start = BidiIterator(this, startObj, pos);
    
    adjustEmbedding = false;
    
    return curr;
}

RootInlineBox* RenderBlock::determineEndPosition(RootInlineBox* startLine, BidiIterator& cleanLineStart,
                                                 int& yPos)
{
    RootInlineBox* last = 0;
    if (!startLine)
        last = 0;
    else {
        for (RootInlineBox* curr = startLine->nextRootBox(); curr; curr = curr->nextRootBox()) {
            if (curr->isDirty())
                last = 0;
            else if (!last)
                last = curr;
        }
    }
    
    if (!last)
        return 0;
    
    cleanLineStart = BidiIterator(this, last->prevRootBox()->lineBreakObj(), last->prevRootBox()->lineBreakPos());
    yPos = last->prevRootBox()->blockHeight();
    
    for (RootInlineBox* line = last; line; line = line->nextRootBox())
        line->extractLine(); // Disconnect all line boxes from their render objects while preserving
                             // their connections to one another.
    
    return last;
}

bool RenderBlock::matchedEndLine(const BidiIterator& start, const BidiIterator& endLineStart, 
                                 RootInlineBox*& endLine, int& endYPos)
{
    if (start == endLineStart)
        return true; // The common case. All the data we already have is correct.
    else {
        // The first clean line doesn't match, but we can check a handful of following lines to try
        // to match back up.
        static int numLines = 8; // The # of lines we're willing to match against.
        RootInlineBox* line = endLine;
        for (int i = 0; i < numLines && line; i++, line = line->nextRootBox()) {
            if (line->lineBreakObj() == start.obj && line->lineBreakPos() == start.pos) {
                // We have a match.
                RootInlineBox* result = line->nextRootBox();
                                
                // Set our yPos to be the block height of endLine.
                if (result)
                    endYPos = line->blockHeight();
                
                // Now delete the lines that we failed to sync.
                RootInlineBox* boxToDelete = endLine;
                RenderArena* arena = renderArena();
                while (boxToDelete && boxToDelete != result) {
                    RootInlineBox* next = boxToDelete->nextRootBox();
                    boxToDelete->deleteLine(arena);
                    boxToDelete = next;
                }

                endLine = result;
                return result;
            }
        }
    }
    return false;
}

static const ushort nonBreakingSpace = 0xa0;

inline bool RenderBlock::skipNonBreakingSpace(BidiIterator &it)
{
    if (it.obj->style()->nbspMode() != SPACE || it.current().unicode() != nonBreakingSpace)
        return false;
 
    // Do not skip a non-breaking space if it is the first character
    // on the first line of a block.
    if (m_firstLine && isLineEmpty)
        return false;
        
    // Do not skip a non-breaking space if it is the first character
    // on a line after a clean line break.
    if (!m_firstLine && isLineEmpty && previousLineBrokeCleanly)
        return false;
    
    return true;
}

int RenderBlock::skipWhitespace(BidiIterator &it, BidiState &bidi)
{
    // FIXME: The entire concept of the skipWhitespace function is flawed, since we really need to be building
    // line boxes even for containers that may ultimately collapse away.  Otherwise we'll never get positioned
    // elements quite right.  In other words, we need to build this function's work into the normal line
    // object iteration process.
    int w = lineWidth(m_height);
    while (!it.atEnd() && (it.obj->isInlineFlow() || (it.obj->style()->whiteSpace() != PRE && !it.obj->isBR() &&
          (it.current() == ' ' || it.current() == '\t' || it.current() == '\n' || 
           skipNonBreakingSpace(it) || it.obj->isFloatingOrPositioned())))) {
        if (it.obj->isFloatingOrPositioned()) {
            RenderObject *o = it.obj;
            // add to special objects...
            if (o->isFloating()) {
                insertFloatingObject(o);
                positionNewFloats();
                w = lineWidth(m_height);
            }
            else if (o->isPositioned()) {
                // FIXME: The math here is actually not really right.  It's a best-guess approximation that
                // will work for the common cases
                RenderObject* c = o->container();
                if (c->isInlineFlow()) {
                    // A relative positioned inline encloses us.  In this case, we also have to determine our
                    // position as though we were an inline.  Set |staticX| and |staticY| on the relative positioned
                    // inline so that we can obtain the value later.
                    c->setStaticX(style()->direction() == LTR ?
                                  leftOffset(m_height) : rightOffset(m_height));
                    c->setStaticY(m_height);
                }
                
                if (o->hasStaticX()) {
                    bool wasInline = o->style()->isOriginalDisplayInlineType();
                    if (wasInline)
                        o->setStaticX(style()->direction() == LTR ?
                                      leftOffset(m_height) :
                                      width() - rightOffset(m_height));
                    else
                        o->setStaticX(style()->direction() == LTR ?
                                      borderLeft() + paddingLeft() :
                                      borderRight() + paddingRight());
                }
                if (o->hasStaticY())
                    o->setStaticY(m_height);
            }
        }
        
        adjustEmbedding = true;
        it.increment(bidi);
        adjustEmbedding = false;
    }
    return w;
}

BidiIterator RenderBlock::findNextLineBreak(BidiIterator &start, BidiState &bidi)
{
    int width = lineWidth(m_height);
    int w = 0;
    int tmpW = 0;
#ifdef DEBUG_LINEBREAKS
    kdDebug(6041) << "findNextLineBreak: line at " << m_height << " line width " << width << endl;
    kdDebug(6041) << "sol: " << start.obj << " " << start.pos << endl;
#endif

    // eliminate spaces at beginning of line
    width = skipWhitespace(start, bidi);
    if (start.atEnd())
        return start;

    // This variable is used only if whitespace isn't set to PRE, and it tells us whether
    // or not we are currently ignoring whitespace.
    bool ignoringSpaces = false;
    BidiIterator ignoreStart;
    
    // This variable tracks whether the very last character we saw was a space.  We use
    // this to detect when we encounter a second space so we know we have to terminate
    // a run.
    bool currentCharacterIsSpace = false;
    bool currentCharacterIsWS = false;
    RenderObject* trailingSpaceObject = 0;

    BidiIterator lBreak = start;

    RenderObject *o = start.obj;
    RenderObject *last = o;
    int pos = start.pos;

    bool prevLineBrokeCleanly = previousLineBrokeCleanly;
    previousLineBrokeCleanly = false;
    
    while( o ) {
#ifdef DEBUG_LINEBREAKS
        kdDebug(6041) << "new object "<< o <<" width = " << w <<" tmpw = " << tmpW << endl;
#endif
        if(o->isBR()) {
            if (w + tmpW <= width) {
                lBreak.obj = o;
                lBreak.pos = 0;
                lBreak.increment(bidi);

                // A <br> always breaks a line, so don't let the line be collapsed
                // away. Also, the space at the end of a line with a <br> does not
                // get collapsed away.  It only does this if the previous line broke
                // cleanly.  Otherwise the <br> has no effect on whether the line is
                // empty or not.
                if (prevLineBrokeCleanly)
                    isLineEmpty = false;
                trailingSpaceObject = 0;
                previousLineBrokeCleanly = true;

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
                
                bool needToCreateLineBox = needToSetStaticX || needToSetStaticY;
                RenderObject* c = o->container();
                if (c->isInlineFlow() && (!needToSetStaticX || !needToSetStaticY))
                    needToCreateLineBox = true;

                // If we're ignoring spaces, we have to stop and include this object and
                // then start ignoring spaces again.
                if (needToCreateLineBox) {
                    trailingSpaceObject = 0;
                    ignoreStart.obj = o;
                    ignoreStart.pos = 0;
                    if (ignoringSpaces) {
                        addMidpoint(ignoreStart); // Stop ignoring spaces.
                        addMidpoint(ignoreStart); // Start ignoring again.
                    }
                    
                }
            }
        } else if (o->isInlineFlow()) {
            // Only empty inlines matter.  We treat those similarly to replaced elements.
            KHTMLAssert(!o->firstChild());
            tmpW += o->marginLeft()+o->borderLeft()+o->paddingLeft()+
                    o->marginRight()+o->borderRight()+o->paddingRight();
        } else if ( o->isReplaced() ) {
            EWhiteSpace currWS = o->style()->whiteSpace();
            EWhiteSpace lastWS = last->style()->whiteSpace();
            
            // WinIE marquees have different whitespace characteristics by default when viewed from
            // the outside vs. the inside.  Text inside is NOWRAP, and so we altered the marquee's
            // style to reflect this, but we now have to get back to the original whitespace value
            // for the marquee when checking for line breaking.
            if (o->isHTMLMarquee() && o->layer() && o->layer()->marquee())
                currWS = o->layer()->marquee()->whiteSpace();
            if (last->isHTMLMarquee() && last->layer() && last->layer()->marquee())
                lastWS = last->layer()->marquee()->whiteSpace();
            
            // Break on replaced elements if either has normal white-space.
            // FIXME: This does not match WinIE, Opera, and Mozilla.  They treat replaced elements
            // like characters in a word, and require spaces between the replaced elements in order
            // to break.
            if (currWS == NORMAL || lastWS == NORMAL) {
                w += tmpW;
                tmpW = 0;
                lBreak.obj = o;
                lBreak.pos = 0;
            }

            tmpW += o->width()+o->marginLeft()+o->marginRight()+inlineWidth(o);
            if (ignoringSpaces) {
                BidiIterator startMid( 0, o, 0 );
                addMidpoint(startMid);
            }
            isLineEmpty = false;
            ignoringSpaces = false;
            currentCharacterIsSpace = false;
            currentCharacterIsWS = false;
            trailingSpaceObject = 0;
            
            if (o->isListMarker() && o->style()->listStylePosition() == OUTSIDE) {
                // The marker must not have an effect on whitespace at the start
                // of the line.  We start ignoring spaces to make sure that any additional
                // spaces we see will be discarded. 
                //
                // Optimize for a common case. If we can't find whitespace after the list
                // item, then this is all moot. -dwh
                RenderObject* next = Bidinext( start.par, o, bidi );
                if (!m_pre && next && next->isText() && static_cast<RenderText*>(next)->stringLength() > 0) {
                    RenderText *nextText = static_cast<RenderText*>(next);
                    QChar nextChar = nextText->text()[0];

                    if (nextText->style()->whiteSpace() != PRE && 
                        (nextChar == ' ' || nextChar == '\n' || nextChar == '\t' ||
                        nextChar.unicode() == nonBreakingSpace && next->style()->nbspMode() == SPACE)) {
                        currentCharacterIsSpace = true;
                        currentCharacterIsWS = true;
                        ignoringSpaces = true;
                        BidiIterator endMid( 0, o, 0 );
                        addMidpoint(endMid);
                    }
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

            int wrapW = tmpW;
            int nextBreakable = -1;

            while(len) {
                bool previousCharacterIsSpace = currentCharacterIsSpace;
                bool previousCharacterIsWS = currentCharacterIsWS;
                const QChar c = str[pos];
                currentCharacterIsSpace = c == ' ' || c == '\t' || (!isPre && (c == '\n'));

                if (isPre || !currentCharacterIsSpace)
                    isLineEmpty = false;
                
                // Check for soft hyphens.  Go ahead and ignore them.
                if (c.unicode() == SOFT_HYPHEN && pos > 0) {
                    if (!ignoringSpaces) {
                        // Ignore soft hyphens
                        BidiIterator endMid(0, o, pos-1);
                        addMidpoint(endMid);
                        
                        // Add the width up to but not including the hyphen.
                        tmpW += t->width(lastSpace, pos - lastSpace, f, w+tmpW);
                        
                        // For whitespace normal only, include the hyphen.  We need to ensure it will fit
                        // on the line if it shows when we break.
                        if (o->style()->whiteSpace() == NORMAL)
                            tmpW += t->width(pos, 1, f, w+tmpW);
                        
                        BidiIterator startMid(0, o, pos+1);
                        addMidpoint(startMid);
                    }
                    
                    pos++;
                    len--;
                    lastSpace = pos; // Cheesy hack to prevent adding in widths of the run twice.
                    continue;
                }
                
                bool applyWordSpacing = false;
                bool isNormal = o->style()->whiteSpace() == NORMAL;
                bool breakNBSP = isNormal && o->style()->nbspMode() == SPACE;
                bool breakWords = o->style()->wordWrap() == BREAK_WORD && ((isNormal && w == 0) || o->style()->whiteSpace() == PRE);

                currentCharacterIsWS = currentCharacterIsSpace || (breakNBSP && c.unicode() == nonBreakingSpace);

                if (breakWords)
                    wrapW += t->width(pos, 1, f, w+wrapW);

                if (c == '\n' || (!isPre && isBreakable(str, pos, strlen, nextBreakable, breakNBSP)) || (breakWords && (w + wrapW > width))) {
                    if (ignoringSpaces) {
                        if (!currentCharacterIsSpace) {
                            // Stop ignoring spaces and begin at this
                            // new point.
                            ignoringSpaces = false;
                            lastSpace = pos; // e.g., "Foo    goo", don't add in any of the ignored spaces.
                            BidiIterator startMid ( 0, o, pos );
                            addMidpoint(startMid);
                        } else {
                            // Just keep ignoring these spaces.
                            pos++;
                            len--;
                            continue;
                        }
                    }

                    tmpW += t->width(lastSpace, pos - lastSpace, f, w+tmpW);
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
        
                    if (o->style()->whiteSpace() == NORMAL || breakWords) {
                        // In AFTER_WHITE_SPACE mode, consider the current character
                        // as candidate width for this line.
                        int charWidth = o->style()->khtmlLineBreak() == AFTER_WHITE_SPACE ? t->width(pos, 1, f, w + tmpW) : 0;
                        if (w + tmpW + charWidth > width) {
                            if (o->style()->khtmlLineBreak() == AFTER_WHITE_SPACE) {
                                // Check if line is too big even without the extra space
                                // at the end of the line. If it is not, do nothing. 
                                // If the line needs the extra whitespace to be too long, 
                                // then move the line break to the space and skip all 
                                // additional whitespace.
                                if (w + tmpW <= width) {
                                    lBreak.obj = o;
                                    lBreak.pos = pos;
                                    skipWhitespace(lBreak, bidi);
                                }
                            }
                            goto end; // Didn't fit. Jump to the end.
                        }
                        else if (pos > 1 && str[pos-1].unicode() == SOFT_HYPHEN)
                            // Subtract the width of the soft hyphen out since we fit on a line.
                            tmpW -= t->width(pos-1, 1, f, w+tmpW);
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
                        if (currentCharacterIsSpace && previousCharacterIsSpace) {
                            ignoringSpaces = true;
                            
                            // We just entered a mode where we are ignoring
                            // spaces. Create a midpoint to terminate the run
                            // before the second space. 
                            addMidpoint(ignoreStart);
                            lastSpace = pos;
                        }
                    }
                }
                else if (ignoringSpaces) {
                    // Stop ignoring spaces and begin at this
                    // new point.
                    ignoringSpaces = false;
                    lastSpace = pos; // e.g., "Foo    goo", don't add in any of the ignored spaces.
                    BidiIterator startMid ( 0, o, pos );
                    addMidpoint(startMid);
                }

                if (currentCharacterIsSpace && !previousCharacterIsSpace) {
                    ignoreStart.obj = o;
                    ignoreStart.pos = pos;
                }

                if (!currentCharacterIsWS && previousCharacterIsWS) {
                    if (o->style()->khtmlLineBreak() == AFTER_WHITE_SPACE && o->style()->whiteSpace() == NORMAL) {
                        lBreak.obj = o;
                        lBreak.pos = pos;
                    }
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
                tmpW += t->width(lastSpace, pos - lastSpace, f, w+tmpW);
            if (!appliedStartWidth)
                tmpW += inlineWidth(o, true, false);
            if (!appliedEndWidth)
                tmpW += inlineWidth(o, false, true);
        } else
            KHTMLAssert( false );

        RenderObject* next = Bidinext(start.par, o, bidi);
        bool isNormal = o->style()->whiteSpace() == NORMAL;
        bool checkForBreak = isNormal;
        if (w && w + tmpW > width+1 && lBreak.obj && o->style()->whiteSpace() == NOWRAP)
            checkForBreak = true;
        else if (next && o->isText() && next->isText() && !next->isBR()) {
            if (isNormal || (next->style()->whiteSpace() == NORMAL)) {
                if (currentCharacterIsSpace)
                    checkForBreak = true;
                else {
                    checkForBreak = false;
                    RenderText* nextText = static_cast<RenderText*>(next);
                    if (nextText->stringLength() != 0) {
	                    QChar c = nextText->text()[0];
                        if (c == ' ' || c == '\t' || (c == '\n' && next->style()->whiteSpace() != PRE)) {
                        	// If the next item on the line is text, and if we did not end with
                        	// a space, then the next text run continues our word (and so it needs to
                       	 	// keep adding to |tmpW|.  Just update and continue.
 							checkForBreak = true;
                        	tmpW += nextText->htmlFont(m_firstLine)->getWordSpacing();
                        }
                    }
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
        lBreak.increment(bidi);
    
#ifdef DEBUG_LINEBREAKS
    kdDebug(6041) << "regular break sol: " << start.obj << " " << start.pos << "   end: " << lBreak.obj << " " << lBreak.pos << "   width=" << w << endl;
#endif

    // Sanity check our midpoints.
    checkMidpoints(lBreak, bidi);
        
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
            BidiIterator endMid ( 0, trailingSpaceObject, pos );
            addMidpoint(endMid);
        }
    }

    // We might have made lBreak an iterator that points past the end
    // of the object. Do this adjustment to make it point to the start
    // of the next object instead to avoid confusing the rest of the
    // code.
    if (lBreak.pos > 0) {
	lBreak.pos--;
	lBreak.increment(bidi);
    }

    if (lBreak.obj && lBreak.pos >= 2 && lBreak.obj->isText()) {
        // For soft hyphens on line breaks, we have to chop out the midpoints that made us
        // ignore the hyphen so that it will render at the end of the line.
        QChar c = static_cast<RenderText*>(lBreak.obj)->text()[lBreak.pos-1];
        if (c.unicode() == SOFT_HYPHEN)
            chopMidpointsAt(lBreak.obj, lBreak.pos-2);
    }
    
    return lBreak;
}

void RenderBlock::checkLinesForOverflow()
{
    // FIXME: Inline blocks can have overflow.  Need to understand when those objects are present on a line
    // and factor that in somehow.
    m_overflowWidth = m_width;
    for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
        m_overflowLeft = kMin(curr->leftOverflow(), m_overflowLeft);
        m_overflowTop = kMin(curr->topOverflow(), m_overflowTop);
        m_overflowWidth = kMax(curr->rightOverflow(), m_overflowWidth);
        m_overflowHeight = kMax(curr->bottomOverflow(), m_overflowHeight);
    }
}

void RenderBlock::deleteEllipsisLineBoxes()
{
    for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox())
        curr->clearTruncation();
}

void RenderBlock::checkLinesForTextOverflow()
{
    // Determine the width of the ellipsis using the current font.
    QChar ellipsis = 0x2026; // FIXME: CSS3 says this is configurable, also need to use 0x002E (FULL STOP) if 0x2026 not renderable
    static AtomicString ellipsisStr(ellipsis);
    const Font& firstLineFont = style(true)->htmlFont();
    const Font& font = style()->htmlFont();
    int firstLineEllipsisWidth = firstLineFont.width(&ellipsis, 1, 0, 0);
    int ellipsisWidth = (font == firstLineFont) ? firstLineEllipsisWidth : font.width(&ellipsis, 1, 0, 0);

    // For LTR text truncation, we want to get the right edge of our padding box, and then we want to see
    // if the right edge of a line box exceeds that.  For RTL, we use the left edge of the padding box and
    // check the left edge of the line box to see if it is less
    // Include the scrollbar for overflow blocks, which means we want to use "contentWidth()"
    bool ltr = style()->direction() == LTR;
    for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
        int blockEdge = ltr ? rightOffset(curr->yPos()) : leftOffset(curr->yPos());
        int lineBoxEdge = ltr ? curr->xPos() + curr->width() : curr->xPos();
        if ((ltr && lineBoxEdge > blockEdge) || (!ltr && lineBoxEdge < blockEdge)) {
            // This line spills out of our box in the appropriate direction.  Now we need to see if the line
            // can be truncated.  In order for truncation to be possible, the line must have sufficient space to
            // accommodate our truncation string, and no replaced elements (images, tables) can overlap the ellipsis
            // space.
            int width = curr == firstRootBox() ? firstLineEllipsisWidth : ellipsisWidth;
            if (curr->canAccommodateEllipsis(ltr, blockEdge, lineBoxEdge, width))
                curr->placeEllipsis(ellipsisStr, ltr, blockEdge, width);
        }
    }
}

// For --enable-final
#undef BIDI_DEBUG
#undef DEBUG_LINEBREAKS
#undef DEBUG_LAYOUT

}
