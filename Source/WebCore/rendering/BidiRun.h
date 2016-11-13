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

#pragma once

#include <wtf/StdLibExtras.h>
#include "BidiResolver.h"

namespace WebCore {

class BidiContext;
class InlineBox;
class RenderObject;

struct BidiRun : BidiCharacterRun {
    BidiRun(unsigned start, unsigned stop, RenderObject&, BidiContext*, UCharDirection);
    ~BidiRun();

    BidiRun* next() { return static_cast<BidiRun*>(BidiCharacterRun::next()); }
    std::unique_ptr<BidiRun> takeNext();
    RenderObject& renderer() { return m_renderer; }
    InlineBox* box() { return m_box; }
    void setBox(InlineBox* box) { m_box = box; }

private:
    RenderObject& m_renderer;
    InlineBox* m_box;
};

} // namespace WebCore
