/*
 * Copyright (C) 2010 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef ColumnInfo_h
#define ColumnInfo_h

#include <wtf/Vector.h>
#include "IntRect.h"

namespace WebCore {

class ColumnInfo : public Noncopyable {
public:
    ColumnInfo()
        : m_desiredColumnWidth(0)
        , m_desiredColumnCount(1)
        { }
    
    int desiredColumnWidth() const { return m_desiredColumnWidth; }
    void setDesiredColumnWidth(int width) { m_desiredColumnWidth = width; }
    
    unsigned desiredColumnCount() const { return m_desiredColumnCount; }
    void setDesiredColumnCount(unsigned count) { m_desiredColumnCount = count; }

    // Encapsulated for the future where we can avoid storing the rects and just compute them dynamically.
    size_t columnCount() const { return m_columnRects.size(); }
    const IntRect& columnRectAt(size_t i) const { return m_columnRects[i]; }

    void clearColumns() { m_columnRects.clear(); }

    // FIXME: Will go away once we don't use the Vector.
    void addColumnRect(const IntRect& rect) { m_columnRects.append(rect); }
    
private:
    int m_desiredColumnWidth;
    unsigned m_desiredColumnCount;
    Vector<IntRect> m_columnRects;
};

}

#endif
