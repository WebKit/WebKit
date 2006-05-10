/*
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

#ifndef BIDI_H
#define BIDI_H

#include <unicode/uchar.h>

class RenderArena;

namespace WebCore {

    class RenderBlock;
    class RenderObject;
    class InlineBox;

    struct BidiStatus {
        BidiStatus() : eor(U_OTHER_NEUTRAL), lastStrong(U_OTHER_NEUTRAL), last(U_OTHER_NEUTRAL) {}
        
        UCharDirection eor;
        UCharDirection lastStrong;
        UCharDirection last;
    };
        
    class BidiContext {
    public:
        BidiContext(unsigned char level, UCharDirection embedding, BidiContext* parent = 0, bool override = false);
        ~BidiContext();

        void ref() const;
        void deref() const;

        UCharDirection dir() const { return static_cast<UCharDirection>(m_dir); }
        UCharDirection basicDir() const { return static_cast<UCharDirection>(m_basicDir); }

        unsigned char level;
        bool override : 1;
        unsigned m_dir : 5; // UCharDirection
        unsigned m_basicDir : 5; // UCharDirection

        BidiContext* parent;

        // refcounting....
        mutable int count;
    };

    struct BidiRun {
        BidiRun(int start_, int stop_, RenderObject* o, BidiContext* context, UCharDirection dir)
            :  start(start_), stop(stop_), obj(o), box(0), override(context->override), nextRun(0)
        {
            if (dir == U_OTHER_NEUTRAL) 
                dir = context->dir();

            level = context->level;

            // add level of run (cases I1 & I2)
            if (level % 2) {
                if(dir == U_LEFT_TO_RIGHT || dir == U_ARABIC_NUMBER || dir == U_EUROPEAN_NUMBER)
                    level++;
            } else {
                if (dir == U_RIGHT_TO_LEFT)
                    level++;
                else if (dir == U_ARABIC_NUMBER || dir == U_EUROPEAN_NUMBER)
                    level += 2;
            }
        }

        void destroy(RenderArena*);

        // Overloaded new operator.
        void* operator new(size_t, RenderArena*) throw();

        // Overridden to prevent the normal delete from being called.
        void operator delete(void*, size_t);

private:
        // The normal operator new is disallowed.
        void* operator new(size_t) throw();

public:
        int start;
        int stop;

        RenderObject *obj;
        InlineBox* box;
        
        // explicit + implicit levels here
        unsigned char level;
        bool override : 1;
        bool compact : 1;
        
        BidiRun* nextRun;
    };

    struct BidiIterator;
    struct BidiState;

};

#endif
