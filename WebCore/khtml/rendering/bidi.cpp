/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
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
#include "render_flow.h"
#include "render_text.h"
#include "render_arena.h"
#include "xml/dom_docimpl.h"

using namespace khtml;

#include "kdebug.h"
#include "qdatetime.h"
#include "qfontmetrics.h"

#define BIDI_DEBUG 0
//#define DEBUG_LINEBREAKS


static BidiIterator sor;
static BidiIterator eor;
static BidiIterator last;
static BidiIterator current;
static BidiContext *context;
static BidiStatus status;
static QPtrList<BidiRun> *sruns = 0;
static QPtrList<BidiIterator> *smidpoints = 0;
static bool betweenMidpoints = false;
static bool isLineEmpty = true;
static QChar::Direction dir;
static bool adjustEmbeddding = false;
static bool emptyRun = true;
static int numSpaces;

static void embed( QChar::Direction d );
static void appendRun();

void BidiIterator::detach(RenderArena* renderArena)
{
    delete this;
    
    // Now perform the destroy.
    size_t* sz = (size_t*)this;
    renderArena->free(*sz, (void*)this);
}

void* BidiIterator::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}

void BidiIterator::operator delete(void* ptr, size_t sz) {
    size_t* szPtr = (size_t*)ptr;
    *szPtr = sz;
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

static inline RenderObject *Bidinext(RenderObject *par, RenderObject *current)
{
    RenderObject *next = 0;
    while(current != 0)
    {
        //kdDebug( 6040 ) << "current = " << current << endl;
	if(!current->isFloating() && !current->isReplaced() && !current->isPositioned()) {
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
	if(!next) {
	    while(current && current != par) {
		next = current->nextSibling();
		if(next) break;
		if ( adjustEmbeddding && current->style()->unicodeBidi() != UBNormal && !emptyRun ) {
		    embed( QChar::DirPDF );
		}
		current = current->parent();
	    }
	}

        if(!next) break;

        if(next->isText() || next->isBR() || next->isFloating() || next->isReplaced() || next->isPositioned())
            break;
        current = next;
    }
    return next;
}

static RenderObject *first( RenderObject *par )
{
    if(!par->firstChild()) return 0;
    RenderObject *o = par->firstChild();

    if(!o->isText() && !o->isBR() && !o->isReplaced() && !o->isFloating() && !o->isPositioned())
        o = Bidinext( par, o );

    return o;
}

BidiIterator::BidiIterator()
{
    par = 0;
    obj = 0;
    pos = 0;
}

BidiIterator::BidiIterator(RenderFlow *_par)
{
    par = _par;
    obj = first( par );
    pos = 0;
}

BidiIterator::BidiIterator(const BidiIterator &it)
{
    par = it.par;
    obj = it.obj;
    pos = it.pos;
}

BidiIterator::BidiIterator(RenderFlow *_par, RenderObject *_obj, int _pos)
{
    par = _par;
    obj = _obj;
    pos = _pos;
}

BidiIterator &BidiIterator::operator = (const BidiIterator &it)
{
    obj = it.obj;
    pos = it.pos;
    par = it.par;
    return *this;
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

static const QChar nbsp = QChar(0xA0);

inline const QChar &BidiIterator::current() const
{
    if( !obj || !obj->isText()) return nbsp; // non breaking space
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

static void appendRunsForObject(int start, int end, RenderObject* obj)
{
    if (start > end)
        return;
        
    BidiIterator* nextMidpoint = (smidpoints && smidpoints->count()) ? smidpoints->at(0) : 0;
    if (betweenMidpoints) {
        if (!(nextMidpoint && nextMidpoint->obj == obj))
            return;
        // This is a new start point. Stop ignoring objects and 
        // adjust our start.
        betweenMidpoints = false;
        start = nextMidpoint->pos;
        smidpoints->removeFirst(); // Delete the midpoint.
        if (start < end)
            return appendRunsForObject(start, end, obj);
    }
    else {
        if (!smidpoints || !nextMidpoint || (obj != nextMidpoint->obj)) {
            sruns->append( new BidiRun(start, end, obj, context, dir) );
            return;
        }
        
        // An end midpoint has been encounted within our object.  We
        // need to go ahead and append a run with our endpoint.
        if (int(nextMidpoint->pos+1) <= end) {
            sruns->append( new BidiRun(start, nextMidpoint->pos+1, obj, context, dir) );
            betweenMidpoints = true;
            int nextPos = nextMidpoint->pos+1;
            smidpoints->removeFirst();
            return appendRunsForObject(nextPos, end, obj);
        }
        else
            sruns->append( new BidiRun(start, end, obj, context, dir) );
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
	if(c && sruns) {
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
	    if ( sruns ) {
		if ( eor != last ) {
		    appendRun();
		    eor = last;
		}
		appendRun();
		emptyRun = true;

	    }
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


// collects one line of the paragraph and transforms it to visual order
void RenderFlow::bidiReorderLine(const BidiIterator &start, const BidiIterator &end)
{
    if ( start == end ) {
        if ( start.current() == '\n' ) {
            m_height += lineHeight( firstLine );
        }
        return;
    }
#if BIDI_DEBUG > 1
    kdDebug(6041) << "reordering Line from " << start.obj << "/" << start.pos << " to " << end.obj << "/" << end.pos << endl;
#endif

    QPtrList<BidiRun> runs;
    runs.setAutoDelete(true);
    sruns = &runs;

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
			    dir = QChar::DirAN;
			    status.eor = QChar::DirAN;
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
                        dir = QChar::DirAN; break;
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
                            dir = QChar::DirAN;
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
                                dir = QChar::DirAN;
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
	    numSpaces++;
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

    BidiContext *endEmbed = context;
    // both commands below together give a noop...
    //endEmbed->ref();
    //context->deref();

    // reorder line according to run structure...

    // first find highest and lowest levels
    uchar levelLow = 128;
    uchar levelHigh = 0;
    BidiRun *r = runs.first();

    while ( r ) {
        //printf("level = %d\n", r->level);
        if ( r->level > levelHigh )
            levelHigh = r->level;
        if ( r->level < levelLow )
            levelLow = r->level;
        r = runs.next();
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

    int count = runs.count() - 1;

    // do not reverse for visually ordered web sites
    if(!style()->visuallyOrdered()) {
        while(levelHigh >= levelLow) {
            int i = 0;
            while ( i < count ) {
                while(i < count && runs.at(i)->level < levelHigh)
                    i++;
                int start = i;
                while(i <= count && runs.at(i)->level >= levelHigh)
                    i++;
                int end = i-1;

                if(start != end) {
                    //kdDebug(6041) << "reversing from " << start << " to " << end << endl;
                    for(int j = 0; j < (end-start+1)/2; j++)
                        {
                            BidiRun *first = runs.take(start+j);
                            BidiRun *last = runs.take(end-j-1);
                            runs.insert(start+j, last);
                            runs.insert(end-j, first);
                        }
                }
                i++;
                if(i >= count) break;
            }
            levelHigh--;
        }
    }

#if BIDI_DEBUG > 0
    kdDebug(6041) << "visual order is:" << endl;
    QPtrListIterator<BidiRun> it3(runs);
    BidiRun *r3;
    for ( ; (r3 = it3.current()); ++it3 )
    {
        kdDebug(6041) << "    " << r3 << endl;
    }
#endif

    int maxPositionTop = 0;
    int maxPositionBottom = 0;
    int maxAscent = 0;
    int maxDescent = 0;
    r = runs.first();
    while ( r ) {
        r->height = r->obj->lineHeight( firstLine );
	r->baseline = r->obj->baselinePosition( firstLine );
// 	if ( r->baseline > r->height )
// 	    r->baseline = r->height;
        r->vertical = r->obj->verticalPositionHint( firstLine );
        //kdDebug(6041) << "object="<< r->obj << " height="<<r->height<<" baseline="<< r->baseline << " vertical=" << r->vertical <<endl;
        //int ascent;
        if ( r->vertical == PositionTop ) {
            if ( maxPositionTop < r->height ) maxPositionTop = r->height;
        }
        else if ( r->vertical == PositionBottom ) {
            if ( maxPositionBottom < r->height ) maxPositionBottom = r->height;
        }
        else {
            int ascent = r->baseline - r->vertical;
            int descent = r->height - ascent;
            if(maxAscent < ascent) maxAscent = ascent;
            if(maxDescent < descent) maxDescent = descent;
        }
        r = runs.next();
    }
    if ( maxAscent+maxDescent < QMAX( maxPositionTop, maxPositionBottom ) ) {
        // now the computed lineheight needs to be extended for the
        // positioned elements
        // see khtmltests/rendering/html_align.html
        // ### only iterate over the positioned ones!
        for ( r = runs.first(); r; r = runs.next() ) {
            if ( r->vertical == PositionTop ) {
                if ( maxAscent + maxDescent < r->height )
                    maxDescent = r->height - maxAscent;
            }
            else if ( r->vertical == PositionBottom ) {
                if ( maxAscent + maxDescent < r->height )
                    maxAscent = r->height - maxDescent;
            }
            else
                continue;

            if ( maxAscent + maxDescent >= QMAX( maxPositionTop, maxPositionBottom ) )
                break;

        }
    }
    int maxHeight = maxAscent + maxDescent;
    // CSS2: 10.8.1: line-height on the block level element specifies the *minimum*
    // height of the generated line box
    r = runs.first();
    // ### we have no reliable way of detecting empty lineboxes - which
    // are not allowed to have any height. sigh.(Dirk)
//     if ( r ) {
//         int blockHeight = lineHeight( firstLine );
//         if ( blockHeight > maxHeight )
//             maxHeight = blockHeight;
//     }
    int totWidth = 0;
#if BIDI_DEBUG > 0
    kdDebug( 6040 ) << "starting run.." << endl;
#endif
    while ( r ) {
        if(r->vertical == PositionTop)
            r->vertical = m_height;
        else if(r->vertical == PositionBottom)
            r->vertical = m_height + maxHeight - r->height;
        else
            r->vertical += m_height + maxAscent - r->baseline;

#if BIDI_DEBUG > 0
	kdDebug(6040) << "object="<< r->obj << " placing at vertical=" << r->vertical <<endl;
#endif
        if(r->obj->isText())
            r->width = static_cast<RenderText *>(r->obj)->width(r->start, r->stop-r->start, firstLine);
        else {
            r->obj->calcWidth();
            r->width = r->obj->width()+r->obj->marginLeft()+r->obj->marginRight();
        }
        totWidth += r->width;
        r = runs.next();
    }
    //kdDebug(6040) << "yPos of line=" << m_height << "  lineBoxHeight=" << maxHeight << endl;

    // now construct the reordered string out of the runs...

    r = runs.first();
    int x = leftOffset(m_height);
    int availableWidth = lineWidth(m_height);
    switch(style()->textAlign()) {
    case LEFT:
	numSpaces = 0;
        break;
    case JUSTIFY:
        if(numSpaces != 0 && !current.atEnd() && !current.obj->isBR() )
            break;
	// fall through
    case TAAUTO:
	numSpaces = 0;
        // for right to left fall through to right aligned
	if ( endEmbed->basicDir == QChar::DirL )
	    break;
    case RIGHT:
        x += availableWidth - totWidth;
	numSpaces = 0;
        break;
    case CENTER:
    case KONQ_CENTER:
        int xd = (availableWidth - totWidth)/2;
        x += xd>0?xd:0;
	numSpaces = 0;
        break;
    }
    while ( r ) {
#if BIDI_DEBUG > 1
        kdDebug(6040) << "positioning " << r->obj << " start=" << r->start << " stop=" << r->stop << " yPos=" << r->vertical << endl;
#endif
	int spaceAdd = 0;
	if ( numSpaces > 0 ) {
	    if ( r->obj->isText() ) {
		// get number of spaces in run
		int spaces = 0;
		for ( int i = r->start; i < r->stop; i++ )
		    if ( static_cast<RenderText *>(r->obj)->text()[i].direction() == QChar::DirWS )
			spaces++;
		if ( spaces > numSpaces ) // should never happen...
		    spaces = numSpaces;
		spaceAdd = (availableWidth - totWidth)*spaces/numSpaces;
		numSpaces -= spaces;
		totWidth += spaceAdd;
	    }
	}
        r->obj->position(x, r->vertical, r->start, r->stop - r->start, r->width, r->level%2, firstLine, spaceAdd);
        x += r->width + spaceAdd;
        r = runs.next();
    }

    m_height += maxHeight;

    sruns = 0;
}


static void deleteMidpoints(RenderArena* arena, QPtrList<BidiIterator>* midpoints)
{
    if (!midpoints)
        return;
        
    unsigned int len = midpoints->count();
    for(unsigned int i=0; i < len; i++) {
        BidiIterator* s = midpoints->at(i);
        if (s)
            s->detach(arena);
        midpoints->remove(i);
    }
}

void RenderFlow::layoutInlineChildren()
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

    if(hasPadding())
    {
        m_height += paddingTop();
        toAdd += paddingBottom();
    }
    
    if(firstChild()) {
        // layout replaced elements
        RenderObject *o = first( this );
        while ( o ) {
            if(o->isReplaced() || o->isFloating() || o->isPositioned()) {
                //kdDebug(6041) << "layouting replaced or floating child" << endl;
                if (o->isReplaced() && (o->style()->width().isPercent() || o->style()->height().isPercent()))
                    o->setLayouted(false);
                if( !o->layouted() )
                    o->layout();
                if(o->isPositioned())
                    static_cast<RenderFlow*>(o->containingBlock())->insertSpecialObject(o);
            }
            else if(o->isText())
                static_cast<RenderText *>(o)->deleteSlaves();
            o = Bidinext( this, o );
        }

        BidiContext *startEmbed;
        status = BidiStatus();
        if( style()->direction() == LTR ) {
            startEmbed = new BidiContext( 0, QChar::DirL );
            status.eor = QChar::DirL;
        } else {
            startEmbed = new BidiContext( 1, QChar::DirR );
            status.eor = QChar::DirR;
        }
        startEmbed->ref();

        context = startEmbed;
        adjustEmbeddding = true;
        BidiIterator start(this);
        adjustEmbeddding = false;
        BidiIterator end(this);

        firstLine = true;
        
        if (!smidpoints) {
            smidpoints = new QPtrList<BidiIterator>;
            smidpoints->setAutoDelete(false);
        }
                
        while( !end.atEnd() ) {
            start = end;
            betweenMidpoints = false;
            isLineEmpty = true;
            end = findNextLineBreak(start, *smidpoints);
            if( start.atEnd() ) break;
            if (!isLineEmpty) {
                bidiReorderLine(start, end);
    
                if( end == start || (end.obj && end.obj->isBR() && !start.obj->isBR() ) ) {
                    adjustEmbeddding = true;
                    ++end;
                    adjustEmbeddding = false;
                } else if(m_pre && end.current() == QChar('\n') ) {
                    adjustEmbeddding = true;
                    ++end;
                    adjustEmbeddding = false;
                }
    
                newLine();
            }
            firstLine = false;
            deleteMidpoints(renderArena(), smidpoints);
        }
        startEmbed->deref();
        //embed->deref();
    }
    
    deleteMidpoints(renderArena(), smidpoints);

    // Now add in the bottom border/padding.
    m_height += toAdd;

    // in case we have a float on the last line, it might not be positioned up to now.
    positionNewFloats();

    // Always make sure this is at least our height.
    m_overflowHeight = m_height;
    
#if BIDI_DEBUG > 1
    kdDebug(6041) << " ------- bidi end " << this << " -------" << endl;
#endif
    //kdDebug() << "RenderFlow::layoutInlineChildren time used " << qt.elapsed() << endl;
    //kdDebug(6040) << "height = " << m_height <<endl;
}

BidiIterator RenderFlow::findNextLineBreak(BidiIterator &start, QPtrList<BidiIterator>& midpoints)
{
    int width = lineWidth(m_height);
    int w = 0;
    int tmpW = 0;
#ifdef DEBUG_LINEBREAKS
    kdDebug(6041) << "findNextLineBreak: line at " << m_height << " line width " << width << endl;
    kdDebug(6041) << "sol: " << start.obj << " " << start.pos << endl;
#endif

    // eliminate spaces at beginning of line
    if(!m_pre) {
        // remove leading spaces
        while(!start.atEnd() &&
#ifndef QT_NO_UNICODETABLES
            ( start.direction() == QChar::DirWS || start.obj->isSpecial() )
#else
            ( start.current() == ' ' || start.obj->isSpecial() )
#endif
            ) {
            if( start.obj->isSpecial() ) {
                RenderObject *o = start.obj;
                // add to special objects...
                if(o->isFloating()) {
                    insertSpecialObject(o);
                    // check if it fits in the current line.
                    // If it does, position it now, otherwise, position
                    // it after moving to next line (in newLine() func)
                    if (o->width()+o->marginLeft()+o->marginRight()+w+tmpW <= width) {
                        positionNewFloats();
                        width = lineWidth(m_height);
                    }
                } else if(o->isPositioned()) {
                    static_cast<RenderFlow*>(o->containingBlock())->insertSpecialObject(o);
                }
            }
    
            adjustEmbeddding = true;
            ++start;
            adjustEmbeddding = false;
        }
    }
    if ( start.atEnd() )
        return start;

    // This variable is used only if whitespace isn't set to PRE, and it tells us whether
    // or not we are currently ignoring whitespace.
    bool ignoringSpaces = false;
    
    // This variable tracks whether the very last character we saw was a space.  We use
    // this to detect when we encounter a second space so we know we have to terminate
    // a run.
    bool sawSpace = false;
    RenderObject* trailingSpaceObject = 0;
    
    // The pos of the last whitespace char we saw, not to be confused with the lastSpace
    // variable below, which is really the last breakable char.
    int lastSpacePos = 0;
    
    BidiIterator lBreak = start;

    RenderObject *o = start.obj;
    RenderObject *last = o;
    int pos = start.pos;

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
                // get collapsed away. -dwh
                isLineEmpty = false;
                trailingSpaceObject = 0;
                
                //check the clear status
                EClear clear = o->style()->clear();
                if(clear != CNONE) {
                    m_clearStatus = (EClear) (m_clearStatus | clear);
                }
            }
            goto end;
        }
        if( o->isSpecial() ) {
            // add to special objects...
            if(o->isFloating()) {
                insertSpecialObject(o);
                // check if it fits in the current line.
                // If it does, position it now, otherwise, position
                // it after moving to next line (in newLine() func)
                if (o->width()+o->marginLeft()+o->marginRight()+w+tmpW <= width) {
                    positionNewFloats();
                    width = lineWidth(m_height);
                }
            } else if(o->isPositioned()) {
                static_cast<RenderFlow*>(o->containingBlock())->insertSpecialObject(o);
            }
        } else if ( o->isReplaced() ) {
            if (o->style()->whiteSpace() != NOWRAP || last->style()->whiteSpace() != NOWRAP) {
                w += tmpW;
                tmpW = 0;
                lBreak.obj = o;
                lBreak.pos = 0;
            }

            tmpW += o->width()+o->marginLeft()+o->marginRight();
            if (ignoringSpaces) {
                BidiIterator* startMid = new (o->renderArena()) BidiIterator();
                startMid->obj = o;
                startMid->pos = 0;
                midpoints.append(startMid);
            }
            isLineEmpty = false;
            ignoringSpaces = false;
            sawSpace = false;
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
                      (static_cast<RenderText*>(next)->text()[0].direction() == QChar::DirWS ||
                      static_cast<RenderText*>(next)->text()[0] == '\n')) {
                    sawSpace = true;
                    ignoringSpaces = true;
                    BidiIterator* endMid = new (o->renderArena()) BidiIterator();
                    endMid->obj = o;
                    endMid->pos = 0;
                    midpoints.append(endMid);
                }
            }
        } else if ( o->isText() ) {
            RenderText *t = static_cast<RenderText *>(o);
            int strlen = t->stringLength();
            int len = strlen - pos;
            QChar *str = t->text();

            const Font *f = t->htmlFont( firstLine );
            // proportional font, needs a bit more work.
            int lastSpace = pos;
            bool isPre = o->style()->whiteSpace() == PRE;
            //QChar space[1]; space[0] = ' ';
            //int spaceWidth = f->width(space, 1, 0);
            while(len) {
                //XXXdwh This is wrong. Still mutating the DOM
                // string for newlines... will fix in second stage.
                if (!isPre && str[pos] == '\n')
                    str[pos] = ' ';
                    
                bool oldSawSpace = sawSpace;
                sawSpace = (str[pos].direction() == QChar::DirWS);
                    
                if (isPre || !sawSpace)
                    isLineEmpty = false;
                    
                if( (isPre && str[pos] == '\n') ||
                    (!isPre && isBreakable( str, pos, strlen ) ) ) {
                    
                    if (ignoringSpaces) {
                        if (!sawSpace) {
                            // Stop ignoring spaces and begin at this
                            // new point.
                            BidiIterator* startMid = new (o->renderArena()) BidiIterator();
                            startMid->obj = o;
                            startMid->pos = pos;
                            midpoints.append(startMid);
                        }
                        else {
                            // Just keep ignoring these spaces.
                            pos++;
                            len--;
                            continue;
                        }
                    }
                    else {
                        if (sawSpace && !oldSawSpace)
                            lastSpacePos = pos;
                        tmpW += t->width(lastSpace, pos - lastSpace, f);
                    }
                    
#ifdef DEBUG_LINEBREAKS
                    kdDebug(6041) << "found space at " << pos << " in string '" << QString( str, strlen ).latin1() << "' adding " << tmpW << " new width = " << w << endl;
#endif
                    if ( !isPre && w + tmpW > width && w == 0 ) {
                        int fb = floatBottom();
                        int newLineWidth = lineWidth(fb);
                        if(!w && m_height < fb && width < newLineWidth) {
                            m_height = fb;
                            width = newLineWidth;
#ifdef DEBUG_LINEBREAKS
                            kdDebug() << "RenderFlow::findNextLineBreak new position at " << m_height << " newWidth " << width << endl;
#endif
                        }
                    }
        
                    if ( !isPre && w + tmpW > width )
                        goto end;

                    lBreak.obj = o;
                    lBreak.pos = pos;
                    
                    if( *(str+pos) == '\n' && isPre) {
#ifdef DEBUG_LINEBREAKS
                        kdDebug(6041) << "forced break sol: " << start.obj << " " << start.pos << "   end: " << lBreak.obj << " " << lBreak.pos << "   width=" << w << endl;
#endif
                        return lBreak;
                    }
                    
                    w += tmpW;
                    tmpW = 0;
                    lastSpace = pos;
                    
                    if (!ignoringSpaces && !isPre) {
                        // If we encounter a newline, or if we encounter a
                        // second space, we need to go ahead and break up this
                        // run and enter a mode where we start collapsing spaces.
                        if (sawSpace && oldSawSpace)
                            ignoringSpaces = true;
                        
                        if (ignoringSpaces) {
                            // We just entered a mode where we are ignoring
                            // spaces. Create a midpoint to terminate the run
                            // before the second space. 
                            BidiIterator* endMid = new (o->renderArena()) BidiIterator();
                            if (trailingSpaceObject) {
                                endMid->obj = trailingSpaceObject;
                            }
                            else
                                endMid->obj = o;
                            endMid->pos = lastSpacePos;
                            midpoints.append(endMid);
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
                    BidiIterator* startMid = new (o->renderArena()) BidiIterator();
                    startMid->obj = o;
                    startMid->pos = pos;
                    midpoints.append(startMid);
                }
                
                if (!isPre && sawSpace && !ignoringSpaces)
                    trailingSpaceObject = o;
                else if (isPre || !sawSpace)
                    trailingSpaceObject = 0;
                    
                pos++;
                len--;
            }
            
            // IMPORTANT: pos is > length here!
            if (!ignoringSpaces)
                tmpW += t->width(lastSpace, pos - lastSpace, f);
        } else
            KHTMLAssert( false );

        if( w + tmpW > width+1 && o->style()->whiteSpace() != NOWRAP ) {
            //kdDebug() << " too wide w=" << w << " tmpW = " << tmpW << " width = " << width << endl;
            //kdDebug() << "start=" << start.obj << " current=" << o << endl;
            // if we have floats, try to get below them.
            if (sawSpace && !ignoringSpaces && o->style()->whiteSpace() != PRE)
                trailingSpaceObject = 0;
                
            int fb = floatBottom();
            int newLineWidth = lineWidth(fb);
            if( !w && m_height < fb && width < newLineWidth ) {
                m_height = fb;
                width = newLineWidth;
#ifdef DEBUG_LINEBREAKS
                kdDebug() << "RenderFlow::findNextLineBreak new position at " << m_height << " newWidth " << width << endl;
#endif
            }

            if( !w && w + tmpW > width+1 && (o != start.obj || (unsigned) pos != start.pos) ) {
                // getting below floats wasn't enough...
                //kdDebug() << "still too wide w=" << w << " tmpW = " << tmpW << " width = " << width << endl;
                lBreak.obj = o;
                if(last != o) {
                    //kdDebug() << " using last " << last << endl;
                    lBreak.pos = 0;
                }
                else if ( unsigned ( pos ) >= o->length() ) {
                    lBreak.obj = Bidinext( start.par, o );
                    lBreak.pos = 0;
                }
                else {
                    lBreak.pos = pos;
                }
            }
            goto end;
        }
        
        last = o;
        o = Bidinext( start.par, o );

        if (last->isReplaced() && last->style()->whiteSpace() != NOWRAP) {
            // Go ahead and add in tmpW.
            w += tmpW;
            tmpW = 0;
            lBreak.obj = o;
            lBreak.pos = 0;
        }

        pos = 0;
    }

#ifdef DEBUG_LINEBREAKS
    kdDebug( 6041 ) << "end of par, width = " << width << " linewidth = " << w + tmpW << endl;
#endif
    if( w + tmpW <= width ) {
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
                lBreak.pos = last->length();
            }
        } else if( lBreak.obj ) {
            if( last != o ) {
                // better break between object boundaries than in the middle of a word
                lBreak.obj = o;
                lBreak.pos = 0;
            } else {
                int w = 0;
                if( lBreak.obj->isText() )
                    w += static_cast<RenderText *>(lBreak.obj)->width(lBreak.pos, 1);
                else
                    w += lBreak.obj->width();
                while( lBreak.obj && w < width ) {
                    ++lBreak;
                    if( !lBreak.obj ) break;
                    if( lBreak.obj->isText() )
                    w += static_cast<RenderText *>(lBreak.obj)->width(lBreak.pos, 1);
                    else
                    w += lBreak.obj->width();
                }
            }
        }
    }

    // make sure we consume at least one char/object.
    if( lBreak == start )
        ++lBreak;
    
#ifdef DEBUG_LINEBREAKS
    kdDebug(6041) << "regular break sol: " << start.obj << " " << start.pos << "   end: " << lBreak.obj << " " << lBreak.pos << "   width=" << w << endl;
#endif

    if (trailingSpaceObject) {
        // This object is either going to be part of the last midpoint, or it is going
        // to be the actual endpoint.  In both cases we just decrease our pos by 1 level to
        // exclude the space, allowing it to - in effect - collapse into the newline.
        int count = midpoints.count();
        if (count%2==1) {
            BidiIterator* lastEndPoint = midpoints.at(count-1);
            lastEndPoint->pos--;
        }
        //else if (lBreak.pos > 0)
        //    lBreak.pos--;
        else if (lBreak.obj == 0 && trailingSpaceObject->isText()) {
            // Add a new end midpoint that stops right at the very end.
            BidiIterator* endMid = new (trailingSpaceObject->renderArena()) BidiIterator();
            endMid->obj = trailingSpaceObject;
            RenderText* text = static_cast<RenderText *>(trailingSpaceObject);
            endMid->pos = text->length() >=2 ? text->length() - 2 : 0;
            midpoints.append(endMid);
        }
    }
    
    return lBreak;
}

// For --enable-final
#undef BIDI_DEBUG
#undef DEBUG_LINEBREAKS
#undef DEBUG_LAYOUT
