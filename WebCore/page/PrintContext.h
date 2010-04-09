/*
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2007 Apple Inc.
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
 */

#ifndef PrintContext_h
#define PrintContext_h

#include <wtf/Vector.h>

namespace WebCore {

class Element;
class Frame;
class FloatRect;
class FloatSize;
class GraphicsContext;
class IntRect;

class PrintContext {
public:
    PrintContext(Frame*);
    ~PrintContext();

    int pageCount() const;
    const IntRect& pageRect(int pageNumber) const;
    const Vector<IntRect>& pageRects() const { return m_pageRects; }

    void computePageRects(const FloatRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, float& outPageHeight);
    void computePageRectsWithPageSize(const FloatSize& pageSizeInPixels, bool allowHorizontalMultiPages);

    // TODO: eliminate width param
    void begin(float width);

    // TODO: eliminate width param
    void spoolPage(GraphicsContext& ctx, int pageNumber, float width);

    void end();

    // Used by layout tests.
    static int pageNumberForElement(Element*, const FloatSize& pageSizeInPixels);
    static int numberOfPages(Frame*, const FloatSize& pageSizeInPixels);

protected:
    Frame* m_frame;
    Vector<IntRect> m_pageRects;

private:
    void computePageRectsWithPageSizeInternal(const FloatSize& pageSizeInPixels, bool allowHorizontalMultiPages);

    // Used to prevent misuses of begin() and end() (e.g., call end without begin).
    bool m_isPrinting;
};

}

#endif
