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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef bidi_h
#define bidi_h

#include "BidiResolver.h"

namespace WebCore {

class RenderArena;
class RenderBlock;
class RenderObject;
class InlineBox;

struct BidiRun : BidiCharacterRun {
    BidiRun(int start, int stop, RenderObject* object, BidiContext* context, WTF::Unicode::Direction dir)
        : BidiCharacterRun(start, stop, context, dir)
        , m_object(object)
        , m_box(0)
    {
    }

    void destroy();

    // Overloaded new operator.
    void* operator new(size_t, RenderArena*) throw();

    // Overridden to prevent the normal delete from being called.
    void operator delete(void*, size_t);

    BidiRun* next() { return static_cast<BidiRun*>(m_next); }

private:
    // The normal operator new is disallowed.
    void* operator new(size_t) throw();

public:
    RenderObject* m_object;
    InlineBox* m_box;
};

} // namespace WebCore

#endif // bidi_h
