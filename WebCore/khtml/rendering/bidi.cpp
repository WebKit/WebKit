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
 * $Id$
 */
#include "bidi.h"
#include "break_lines.h"
#include "render_flow.h"
#include "render_text.h"
using namespace khtml;

#include "kdebug.h"
#include "qdatetime.h"
#include "qfontmetrics.h"

#define BIDI_DEBUG 0
//#define DEBUG_LINEBREAKS

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

BidiIterator::BidiIterator()
{
    par = 0;
    obj = 0;
    pos = 0;
}

BidiIterator::BidiIterator(RenderFlow *_par)
{
    par = _par;
    obj = par->first();
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
        if(pos >= obj->length()) {
            obj = par->next(obj);
            pos = 0;
        }
    } else {
        obj = par->next(obj);
        pos = 0;
    }
}

inline bool BidiIterator::atEnd()
{
    if(!obj) return true;
    return false;
}

const QChar &BidiIterator::current()
{
    static const QChar nbsp = QChar(0xA0);
    if( !obj || !obj->isText()) return nbsp; // non breaking space
    return static_cast<RenderText *>(obj)->text()[pos];
}

QChar::Direction BidiIterator::direction()
{
    if(!obj || !obj->isText() || obj->length() <= 0) return QChar::DirON;
    RenderText *renderTxt = static_cast<RenderText *>( obj );
    if ( pos >= renderTxt->length() )
        return QChar::DirON;
    return renderTxt->text()[pos].direction();
}

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

// -------------------------------------------------------------------------------------------------

void RenderFlow::appendRun(QList<BidiRun> &runs, BidiIterator &sor, BidiIterator &eor,
                           BidiContext *context, QChar::Direction dir)
{
#if BIDI_DEBUG > 1
    kdDebug(6041) << "appendRun: dir="<<(int)dir<<endl;
#endif

    int start = sor.pos;
    RenderObject *obj = sor.obj;
    while( obj && obj != eor.obj ) {
        if(!obj->isHidden()) {
            //kdDebug(6041) << "appendRun: "<< start << "/" << obj->length() <<endl;
            runs.append( new BidiRun(start, obj->length(), obj, context, dir) );
        }
        start = 0;
        obj = next(obj);
    }
    if( obj && !obj->isHidden()) {
        //kdDebug(6041) << "appendRun: "<< start << "/" << eor.pos <<endl;
        runs.append( new BidiRun(start, eor.pos + 1, obj, context, dir) );
    }
}


// collects one line of the paragraph and transforms it to visual order
BidiContext *RenderFlow::bidiReorderLine(BidiStatus &status, const BidiIterator &start, const BidiIterator &end, BidiContext *startEmbed)
{

    //kdDebug(6041) << "reordering Line from " << start.obj << "/" << start.pos << " to " << end.obj << "/" << end.pos << endl;

    QList<BidiRun> runs;
    runs.setAutoDelete(true);

    BidiContext *context = startEmbed;
    //    context->ref();

    QChar::Direction dir = QChar::DirON;

    BidiIterator sor = start;
    BidiIterator eor = start;

#ifndef QT_NO_UNICODETABLES
    BidiIterator current = start;
    BidiIterator last = current;
    while(current != end) {

        QChar::Direction dirCurrent;
        if(current.atEnd()) {
            //kdDebug(6041) << "atEnd" << endl;
            BidiContext *c = context;
            while ( c->parent )
                c = c->parent;
            dirCurrent = c->dir;
        } else
            dirCurrent = current.direction();


#if BIDI_DEBUG > 1
        kdDebug(6041) << "directions: dir=" << (int)dir << " current=" << (int)dirCurrent << " last=" << status.last << " eor=" << status.eor << " lastStrong=" << status.lastStrong << " embedding=" << (int)context->dir << " level =" << (int)context->level << endl;
#endif

        switch(dirCurrent) {

            // embedding and overrides (X1-X9 in the Bidi specs)
        case QChar::DirRLE:
            {
                unsigned char level = context->level;
                if(level%2) // we have an odd level
                    level += 2;
                else
                    level++;
                if(level < 61) {
                    appendRun(runs, sor, eor, context, dir);
                    ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
                    context = new BidiContext(level, QChar::DirR, context);
                    context->ref();
                    status.last = QChar::DirR;
                    status.lastStrong = QChar::DirR;
                }
                break;
            }
        case QChar::DirLRE:
            {
                unsigned char level = context->level;
                if(level%2) // we have an odd level
                    level++;
                else
                    level += 2;
                if(level < 61) {
                    appendRun(runs, sor, eor, context, dir);
                    ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
                    context = new BidiContext(level, QChar::DirL, context);
                    context->ref();
                    status.last = QChar::DirL;
                    status.lastStrong = QChar::DirL;
                }
                break;
            }
        case QChar::DirRLO:
            {
                unsigned char level = context->level;
                if(level%2) // we have an odd level
                    level += 2;
                else
                    level++;
                if(level < 61) {
                    appendRun(runs, sor, eor, context, dir);
                    ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
                    context = new BidiContext(level, QChar::DirR, context, true);
                    context->ref();
                    dir = QChar::DirR;
                    status.last = QChar::DirR;
                    status.lastStrong = QChar::DirR;
                }
                break;
            }
        case QChar::DirLRO:
            {
                unsigned char level = context->level;
                if(level%2) // we have an odd level
                    level++;
                else
                    level += 2;
                if(level < 61) {
                    appendRun(runs, sor, eor, context, dir);
                    ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
                    context = new BidiContext(level, QChar::DirL, context, true);
                    context->ref();
                    dir = QChar::DirL;
                    status.last = QChar::DirL;
                    status.lastStrong = QChar::DirL;
                }
                break;
            }
        case QChar::DirPDF:
            {
                BidiContext *c = context->parent;
                if(c) {
                    appendRun(runs, sor, eor, context, dir);
                    ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
                    status.last = context->dir;
                    context->deref();
                    context = c;
                    if(context->override)
                        dir = context->dir;
                    else
                        dir = QChar::DirON;
                    status.lastStrong = context->dir;
                }
                break;
            }

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
                    appendRun(runs, sor, eor, context, dir);
                    ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
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
                            if(status.eor != QChar::DirR) {
                                // AN or EN
                                appendRun(runs, sor, eor, context, dir);
                                ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
                                dir = QChar::DirR;
                            }
                            else
                                eor = last;
                            appendRun(runs, sor, eor, context, dir);
                            ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
                        } else {
                            if(status.eor == QChar::DirR) {
                                appendRun(runs, sor, eor, context, dir);
                                ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
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
                    appendRun(runs, sor, eor, context, dir);
                    ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
                    break;
                case QChar::DirES:
                case QChar::DirET:
                case QChar::DirCS:
                case QChar::DirBN:
                case QChar::DirB:
                case QChar::DirS:
                case QChar::DirWS:
                case QChar::DirON:
                    if( status.eor != QChar::DirR && status.eor != QChar::DirAL ) {
                        //last stuff takes embedding dir
                        if(context->dir == QChar::DirR || status.lastStrong == QChar::DirR) {
                            appendRun(runs, sor, eor, context, dir);
                            ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
                            dir = QChar::DirR;
                            eor = current;
                        } else {
                            eor = last;
                            appendRun(runs, sor, eor, context, dir);
                            ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
                            dir = QChar::DirR;
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
            if(status.lastStrong != QChar::DirAL) {
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
			    appendRun(runs, sor, eor, context, dir);
			    ++eor; sor = eor;
			    dir = QChar::DirAN;
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
                        appendRun(runs, sor, eor, context, dir);
                        ++eor; sor = eor; status.eor = QChar::DirEN;
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
                            appendRun(runs, sor, eor, context, dir);
                            ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirEN;
                            dir = QChar::DirAN;
                        }
                        else if( status.eor == QChar::DirL ||
                                 (status.eor == QChar::DirEN && status.lastStrong == QChar::DirL)) {
                            eor = current; status.eor = dirCurrent;
                        } else {
                            // numbers on both sides, neutrals get right to left direction
                            if(dir != QChar::DirL) {
                                appendRun(runs, sor, eor, context, dir);
                                ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
                                eor = last;
                                dir = QChar::DirR;
                                appendRun(runs, sor, eor, context, dir);
                                ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
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
                    appendRun(runs, sor, eor, context, dir);
                    ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
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
                        appendRun(runs, sor, eor, context, dir);
                        ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
                        dir = QChar::DirAN;
                    } else if( status.eor == QChar::DirL ||
                               (status.eor == QChar::DirEN && status.lastStrong == QChar::DirL)) {
                        eor = current; status.eor = dirCurrent;
                    } else {
                        // numbers on both sides, neutrals get right to left direction
                        if(dir != QChar::DirL) {
                            appendRun(runs, sor, eor, context, dir);
                            ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
                            eor = last;
                            dir = QChar::DirR;
                            appendRun(runs, sor, eor, context, dir);
                            ++eor; sor = eor; dir = QChar::DirON; status.eor = QChar::DirON;
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

        last = current;
        ++current;
    }

#if BIDI_DEBUG > 0
    kdDebug(6041) << "reached end of line current=" << current.pos << ", eor=" << eor.pos << endl;
#endif
    eor = last;


#else
    eor = end;
#endif

    appendRun(runs, sor, eor, context, dir);

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
    if(!levelLow%2) levelLow++;

#if BIDI_DEBUG > 0
    kdDebug(6041) << "reorderLine: lineLow = " << (uint)levelLow << ", lineHigh = " << (uint)levelHigh << endl;
    kdDebug(6041) << "logical order is:" << endl;
    QListIterator<BidiRun> it2(runs);
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
    QListIterator<BidiRun> it3(runs);
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
	if ( r->baseline > r->height )
	    r->baseline = r->height;
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
    r = runs.first();
    int totWidth = 0;
    while ( r ) {
        if(r->vertical == PositionTop)
            r->vertical = m_height;
        else if(r->vertical == PositionBottom)
            r->vertical = m_height + maxHeight - r->height;
        else
            r->vertical += m_height + maxAscent - r->baseline;

	//kdDebug(6041) << "object="<< r->obj << " placing at vertical=" << r->vertical <<endl;
        if(r->obj->isText())
            r->width = static_cast<RenderText *>(r->obj)->width(r->start, r->stop-r->start);
        else {
            r->obj->calcWidth();
            r->width = r->obj->width()+r->obj->marginLeft()+r->obj->marginRight();
        }
        totWidth += r->width;
        r = runs.next();
    }
    //kdDebug(6040) << "yPos of line=" << m_height << "  lineHeight=" << maxHeight << endl;

    // now construct the reordered string out of the runs...

    r = runs.first();
    int x = leftOffset(m_height);
    int availableWidth = lineWidth(m_height);
    switch(style()->textAlign()) {
    case LEFT:
        break;
    case JUSTIFY:
        if(endEmbed->basicDir == QChar::DirL)
            break;
        // for right to left fall through to right aligned
    case RIGHT:
        x += availableWidth - totWidth;
        break;
    case CENTER:
    case KONQ_CENTER:
        int xd = (availableWidth - totWidth)/2;
        x += xd>0?xd:0;
        break;
    }
    while ( r ) {
        //kdDebug(6040) << "positioning " << r->obj << " start=" << r->start << " stop=" << r->stop << " yPos=" << r->vertical << endl;
        r->obj->position(x, r->vertical, r->start, r->stop - r->start, r->width, r->level%2, firstLine);
        x += r->width;
        r = runs.next();
    }

    m_height += maxHeight;

    return endEmbed;
}


void RenderFlow::layoutInlineChildren()
{
    invalidateVerticalPositions();
#ifdef DEBUG_LAYOUT
    QTime qt;
    qt.start();
    kdDebug( 6040 ) << renderName() << " layoutInlineChildren( " << this <<" )" << endl;
#endif
    int toAdd = style()->borderBottomWidth();
    m_height = style()->borderTopWidth();

    if(style()->hasPadding())
    {
        m_height += paddingTop();
        toAdd += paddingBottom();
    }
    if(firstChild()) {
        // layout replaced elements
        RenderObject *o = first();
        while ( o ) {
            if(o->isReplaced() || o->isFloating() || o->isPositioned()) {
                //kdDebug(6041) << "layouting replaced or floating child" << endl;
                o->layout();
                if(o->isPositioned())
                    static_cast<RenderFlow*>(o->containingBlock())->insertPositioned(o);
            }
            else if(o->isText())
                static_cast<RenderText *>(o)->deleteSlaves();
            o = next(o);
        }

        BidiContext *startEmbed;
        BidiStatus status;
        if( style()->direction() == LTR ) {
            startEmbed = new BidiContext( 0, QChar::DirL );
            status.eor = QChar::DirL;
        } else {
            startEmbed = new BidiContext( 1, QChar::DirR );
            status.eor = QChar::DirR;
        }
        startEmbed->ref();

        BidiIterator start(this);
        BidiIterator end(this);
        BidiContext *embed = startEmbed;

        firstLine = true;
        while( !end.atEnd() ) {
            start = end;
            if(m_pre && start.current() == QChar('\n') ) {
                ++start;
                if( start.atEnd() )
                    break;
            }
            if(!m_pre) {
                // remove leading spaces
#ifndef QT_NO_UNICODETABLES
#ifdef _KWQ_
                while(!start.atEnd() && start.current() == ' ' )
#else
                while(!start.atEnd() && start.direction() == QChar::DirWS )
#endif
#else
                while(!start.atEnd() && start.current() == ' ' )
#endif
                    ++start;
            }
            if( start.atEnd() ) break;

            end = findNextLineBreak(start);
	    embed = bidiReorderLine(status, start, end, embed);

            if( end == start || (end.obj && end.obj->isBR() ) )
                ++end;

            newLine();
            firstLine = false;
        }
        startEmbed->deref();
        //embed->deref();
    }
    m_height += toAdd;


    //kdDebug() << "RenderFlow::layoutInlineChildren time used " << qt.elapsed() << endl;
    //kdDebug(6040) << "height = " << m_height <<endl;
}

BidiIterator RenderFlow::findNextLineBreak(const BidiIterator &start)
{
    BidiIterator lBreak = start;

    int width = lineWidth(m_height);
    int w = 0;
    int tmpW = 0;
#ifdef DEBUG_LINEBREAKS
    kdDebug(6041) << "findNextLineBreak: line at " << m_height << " line width " << width << endl;
    kdDebug(6041) << "sol: " << start.obj << " " << start.pos << endl;
#endif

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
            specialHandler(o);

            // check if it fits in the current line.
            // If it does, position it now, otherwise, position
            // it after moving to next line (in newLine() func)
            if (o->width()+o->marginLeft()+o->marginRight()+w+tmpW <= width) {
                positionNewFloats();
                width = lineWidth(m_height);
            }
        } else if ( o->isReplaced() ) {
            tmpW += o->width()+o->marginLeft()+o->marginRight();
        } else if ( o->isText() ) {
	    RenderText *t = static_cast<RenderText *>(o);
	    int strlen = t->length();
	    int len = strlen - pos;
	    QChar *str = t->text();
#if 0
	    if ( ( !firstLine || !t->firstLine() ) && !t->hasReturn() && t->maxWidth() + w + tmpW < width ) {
		// the rest of the object fits completely
// 		kdDebug() << "RenderFlow::findNextLineBreak object fits completely, max=" << t->maxWidth()
// 			  << " width =" << w << " avail = " << width <<" tmp=" << tmpW << endl;
		//search backwards for last possible linebreak
		int lastSpace = strlen-1;
		while( lastSpace >= pos && !isBreakable( str, lastSpace, strlen ) )
		    lastSpace--;
		lastSpace++;
		if ( lastSpace > pos ) {
		    w += tmpW + t->width( pos, lastSpace-pos, firstLine);
		    tmpW = 0;
		    lBreak.obj = o;
		    lBreak.pos = lastSpace;
		}
		tmpW += t->width( lastSpace, strlen-lastSpace, firstLine);
// 		kdDebug() << "--> width=" << t->width( pos, strlen-pos, firstLine)
// 			  <<" first=" << t->width( pos, lastSpace-pos, firstLine)
// 			  <<" end=" << t->width( lastSpace, strlen-lastSpace, firstLine)
// 			  << endl;
	    } else {
#endif
	    QFontMetrics fm = t->metrics( firstLine );
	    // proportional font, needs a bit more work.
	    int lastSpace = pos;
	    while(len) {
		if( isBreakable( str, pos, strlen ) ) {
		    tmpW += t->width(lastSpace, pos - lastSpace, &fm);
#ifdef DEBUG_LINEBREAKS
		    kdDebug(6041) << "found space at " << pos << " in string '" << QString( str, strlen ).latin1() << "' adding " << tmpW << " new width = " << w << endl;
#endif
		    if ( w + tmpW > width && w == 0 ) {
			int fb = floatBottom();
			if(!w && m_height < fb && width < lineWidth(fb)) {
			    m_height = fb;
			    width = lineWidth(m_height);
#ifdef DEBUG_LINEBREAKS
			    kdDebug() << "RenderFlow::findNextLineBreak new position at " << m_height << " newWidth " << width << endl;
#endif
			}
		    }
		    if ( w + tmpW > width )
			goto end;

		    lBreak.obj = o;
		    lBreak.pos = pos;

		    if( *(str+pos) == '\n' ) {
#ifdef DEBUG_LINEBREAKS
			kdDebug(6041) << "forced break sol: " << start.obj << " " << start.pos << "   end: " << lBreak.obj << " " << lBreak.pos << "   width=" << w << endl;
#endif
			return lBreak;
		    }
		    w += tmpW;
		    tmpW = 0;
		    lastSpace = pos;
		}
		pos++;
		len--;
	    }
            // IMPORTANT: pos is > length here!
            tmpW += t->width(lastSpace, pos - lastSpace, &fm);
        } else
            assert( false );

        if( w + tmpW > width+1 ) {
            //kdDebug() << " too wide w=" << w << " tmpW = " << tmpW << " width = " << width << endl;
	    //kdDebug() << "start=" << start.obj << " current=" << o << endl;
            // if we have floats, try to get below them.
            int fb = floatBottom();
            if(!w && m_height < fb && width < lineWidth(fb)) {
                m_height = fb;
                width = lineWidth(m_height);
		//kdDebug() << "RenderFlow::findNextLineBreak new position at " << m_height << " newWidth " << width << endl;
            }
	    if( !w && w + tmpW > width+1 && (o != start.obj || (unsigned) pos != start.pos) ) {
		// getting below floats wasn't enough...
		//kdDebug() << "still too wide w=" << w << " tmpW = " << tmpW << " width = " << width << endl;
		lBreak.obj = o;
                if(last != o) {
                    //kdDebug() << " using last " << last << endl;
                    //lBreak.obj = last;
                    lBreak.pos = 0;//last->length() - 1;
                }
                else if ( unsigned ( pos ) >= o->length() ) {
                    lBreak.obj = start.par->next(o);
                    lBreak.pos = 0;
                }
                else
                    lBreak.pos = pos;
            }

            return lBreak;
        }

        last = o;
        o = start.par->next(o);
        pos = 0;
    }

#if 0
    kdDebug( 6041 ) << "end of par, width = " << width << " linewidth = " << w + tmpW << endl;
#endif
    if( w + tmpW <= width ) {
        lBreak.obj = 0;
        lBreak.pos = 0;
    }

 end:

    if( lBreak == start && !lBreak.obj->isBR() ) {
        //kdDebug( 6041 ) << "lBreak == start, adding...." << endl;
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
    return lBreak;
}


RenderObject *RenderFlow::first()
{
    if(!firstChild()) return 0;
    RenderObject *o = firstChild();

    if(!o->isText() && !o->isBR() && !o->isReplaced() && !o->isFloating() && !o->isPositioned())
        o = next(o) ;

    return o;
}

RenderObject *RenderFlow::next(RenderObject *current)
{
    if(!current) return 0;

    while(current != 0)
    {
        //kdDebug( 6040 ) << "current = " << current << endl;
        RenderObject *next = nextObject(current);

        if(!next) return 0;

        if(next->isText() || next->isBR() || next->isFloating() || next->isReplaced() || next->isPositioned())
            return next;
        current = next;
    }
    return 0;
}

RenderObject *RenderFlow::nextObject(RenderObject *current)
{
    RenderObject *next = 0;
    if(!current->isFloating() && !current->isReplaced() && !current->isPositioned())
        next = current->firstChild();
    if(next) return next;

    while(current && current != this)
    {
        next = current->nextSibling();
        if(next) return next;
        current = current->parent();
    }
    return 0;
}

// For --enable-final
#undef BIDI_DEBUG
#undef DEBUG_LINEBREAKS
#undef DEBUG_LAYOUT
