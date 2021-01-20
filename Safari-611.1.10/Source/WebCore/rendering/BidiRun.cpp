/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
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

#include "config.h"
#include "BidiRun.h"
#include "InlineBox.h"
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, bidiRunCounter, ("BidiRun"));

BidiRun::BidiRun(unsigned start, unsigned stop, RenderObject& renderer, BidiContext* context, UCharDirection dir)
    : BidiCharacterRun(start, stop, context, dir)
    , m_renderer(renderer)
    , m_box(nullptr)
{
#ifndef NDEBUG
    bidiRunCounter.increment();
#endif
    ASSERT(!is<RenderText>(m_renderer) || static_cast<unsigned>(stop) <= downcast<RenderText>(m_renderer).text().length());
    // Stored in base class to save space.
    m_hasHyphen = false;
}

BidiRun::~BidiRun()
{
#ifndef NDEBUG
    bidiRunCounter.decrement();
#endif
}

std::unique_ptr<BidiRun> BidiRun::takeNext()
{
    std::unique_ptr<BidiCharacterRun> next = BidiCharacterRun::takeNext();
    BidiCharacterRun* raw = next.release();
    std::unique_ptr<BidiRun> result = std::unique_ptr<BidiRun>(static_cast<BidiRun*>(raw));
    return result;
}

}
