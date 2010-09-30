/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010 Apple Inc. All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef InlineIterator_h
#define InlineIterator_h

#include "BidiRun.h"
#include "RenderBlock.h"
#include "RenderText.h"
#include <wtf/AlwaysInline.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

class InlineIterator {
public:
    InlineIterator()
        : block(0)
        , obj(0)
        , pos(0)
        , nextBreakablePosition(-1)
    {
    }

    InlineIterator(RenderBlock* b, RenderObject* o, unsigned p)
        : block(b)
        , obj(o)
        , pos(p)
        , nextBreakablePosition(-1)
    {
    }

    void increment(InlineBidiResolver* resolver = 0);
    bool atEnd() const;

    UChar current() const;
    WTF::Unicode::Direction direction() const;

    RenderBlock* block;
    RenderObject* obj;
    unsigned pos;
    int nextBreakablePosition;
};

inline bool operator==(const InlineIterator& it1, const InlineIterator& it2)
{
    return it1.pos == it2.pos && it1.obj == it2.obj;
}

inline bool operator!=(const InlineIterator& it1, const InlineIterator& it2)
{
    return it1.pos != it2.pos || it1.obj != it2.obj;
}

static inline RenderObject* bidiNext(RenderBlock* block, RenderObject* current, InlineBidiResolver* resolver = 0, bool skipInlines = true, bool* endOfInlinePtr = 0)
{
    RenderObject* next = 0;
    bool oldEndOfInline = endOfInlinePtr ? *endOfInlinePtr : false;
    bool endOfInline = false;

    while (current) {
        next = 0;
        if (!oldEndOfInline && !current->isFloating() && !current->isReplaced() && !current->isPositioned() && !current->isText()) {
            next = current->firstChild();
            if (next && resolver && next->isRenderInline()) {
                EUnicodeBidi ub = next->style()->unicodeBidi();
                if (ub != UBNormal) {
                    TextDirection dir = next->style()->direction();
                    WTF::Unicode::Direction d = (ub == Embed
                        ? (dir == RTL ? WTF::Unicode::RightToLeftEmbedding : WTF::Unicode::LeftToRightEmbedding)
                        : (dir == RTL ? WTF::Unicode::RightToLeftOverride : WTF::Unicode::LeftToRightOverride));
                    resolver->embed(d);
                }
            }
        }

        if (!next) {
            if (!skipInlines && !oldEndOfInline && current->isRenderInline()) {
                next = current;
                endOfInline = true;
                break;
            }

            while (current && current != block) {
                if (resolver && current->isRenderInline() && current->style()->unicodeBidi() != UBNormal)
                    resolver->embed(WTF::Unicode::PopDirectionalFormat);

                next = current->nextSibling();
                if (next) {
                    if (resolver && next->isRenderInline()) {
                        EUnicodeBidi ub = next->style()->unicodeBidi();
                        if (ub != UBNormal) {
                            TextDirection dir = next->style()->direction();
                            WTF::Unicode::Direction d = (ub == Embed
                                ? (dir == RTL ? WTF::Unicode::RightToLeftEmbedding: WTF::Unicode::LeftToRightEmbedding)
                                : (dir == RTL ? WTF::Unicode::RightToLeftOverride : WTF::Unicode::LeftToRightOverride));
                            resolver->embed(d);
                        }
                    }
                    break;
                }
                
                current = current->parent();
                if (!skipInlines && current && current != block && current->isRenderInline()) {
                    next = current;
                    endOfInline = true;
                    break;
                }
            }
        }

        if (!next)
            break;

        if (next->isText() || next->isFloating() || next->isReplaced() || next->isPositioned()
            || ((!skipInlines || !next->firstChild()) // Always return EMPTY inlines.
                && next->isRenderInline()))
            break;
        current = next;
    }

    if (endOfInlinePtr)
        *endOfInlinePtr = endOfInline;

    return next;
}

static inline RenderObject* bidiFirst(RenderBlock* block, InlineBidiResolver* resolver, bool skipInlines = true)
{
    if (!block->firstChild())
        return 0;
    
    RenderObject* o = block->firstChild();
    if (o->isRenderInline()) {
        if (resolver) {
            EUnicodeBidi ub = o->style()->unicodeBidi();
            if (ub != UBNormal) {
                TextDirection dir = o->style()->direction();
                WTF::Unicode::Direction d = (ub == Embed
                    ? (dir == RTL ? WTF::Unicode::RightToLeftEmbedding : WTF::Unicode::LeftToRightEmbedding)
                    : (dir == RTL ? WTF::Unicode::RightToLeftOverride : WTF::Unicode::LeftToRightOverride));
                resolver->embed(d);
            }
        }
        if (skipInlines && o->firstChild())
            o = bidiNext(block, o, resolver, skipInlines);
        else {
            // Never skip empty inlines.
            if (resolver)
                resolver->commitExplicitEmbedding();
            return o; 
        }
    }

    if (o && !o->isText() && !o->isReplaced() && !o->isFloating() && !o->isPositioned())
        o = bidiNext(block, o, resolver, skipInlines);

    if (resolver)
        resolver->commitExplicitEmbedding();
    return o;
}

inline void InlineIterator::increment(InlineBidiResolver* resolver)
{
    if (!obj)
        return;
    if (obj->isText()) {
        pos++;
        if (pos >= toRenderText(obj)->textLength()) {
            obj = bidiNext(block, obj, resolver);
            pos = 0;
            nextBreakablePosition = -1;
        }
    } else {
        obj = bidiNext(block, obj, resolver);
        pos = 0;
        nextBreakablePosition = -1;
    }
}

inline bool InlineIterator::atEnd() const
{
    return !obj;
}

inline UChar InlineIterator::current() const
{
    if (!obj || !obj->isText())
        return 0;

    RenderText* text = toRenderText(obj);
    if (pos >= text->textLength())
        return 0;

    return text->characters()[pos];
}

ALWAYS_INLINE WTF::Unicode::Direction InlineIterator::direction() const
{
    if (UChar c = current())
        return WTF::Unicode::direction(c);

    if (obj && obj->isListMarker())
        return obj->style()->isLeftToRightDirection() ? WTF::Unicode::LeftToRight : WTF::Unicode::RightToLeft;

    return WTF::Unicode::OtherNeutral;
}

template<>
inline void InlineBidiResolver::increment()
{
    current.increment(this);
}

template <>
inline void InlineBidiResolver::appendRun()
{
    if (!emptyRun && !eor.atEnd()) {
        int start = sor.pos;
        RenderObject *obj = sor.obj;
        while (obj && obj != eor.obj && obj != endOfLine.obj) {
            RenderBlock::appendRunsForObject(start, obj->length(), obj, *this);        
            start = 0;
            obj = bidiNext(sor.block, obj);
        }
        if (obj) {
            unsigned pos = obj == eor.obj ? eor.pos : UINT_MAX;
            if (obj == endOfLine.obj && endOfLine.pos <= pos) {
                reachedEndOfLine = true;
                pos = endOfLine.pos;
            }
            // It's OK to add runs for zero-length RenderObjects, just don't make the run larger than it should be
            int end = obj->length() ? pos+1 : 0;
            RenderBlock::appendRunsForObject(start, end, obj, *this);
        }
        
        eor.increment();
        sor = eor;
    }

    m_direction = WTF::Unicode::OtherNeutral;
    m_status.eor = WTF::Unicode::OtherNeutral;
}

}

#endif // InlineIterator_h
