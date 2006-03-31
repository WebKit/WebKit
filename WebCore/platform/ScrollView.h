/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef ScrollView_H
#define ScrollView_H

#include "ScrollBarMode.h"
#include "Widget.h"

namespace WebCore {
    class FloatRect;

    class ScrollView : public Widget {
    public:
        int visibleWidth() const;
        int visibleHeight() const;
        FloatRect visibleContentRect() const;
        int contentsWidth() const;
        int contentsHeight() const;
        int contentsX() const;
        int contentsY() const;
        IntSize scrollOffset() const;
        void scrollBy(int dx, int dy);
        void scrollPointRecursively(int dx, int dy);

        void setContentsPos(int x, int y);

        virtual void setVScrollBarMode(ScrollBarMode);
        virtual void setHScrollBarMode(ScrollBarMode);

        // Set the mode for both scroll bars at once.
        virtual void setScrollBarsMode(ScrollBarMode);

        // This gives us a means of blocking painting on our scrollbars until the first layout has occurred.
        void suppressScrollBars(bool suppressed, bool repaintOnUnsuppress = false);
        
        ScrollBarMode vScrollBarMode() const;
        ScrollBarMode hScrollBarMode() const;

        void addChild(Widget*, int x = 0, int y = 0);
        void removeChild(Widget*);

        virtual void resizeContents(int w, int h);
        void updateContents(const IntRect&, bool now = false);

        IntPoint contentsToViewport(const IntPoint&);
        IntPoint viewportToContents(const IntPoint&);

        void setStaticBackground(bool);

        bool inWindow() const;

#if __APPLE__
        NSView* getDocumentView() const;
#endif

#if WIN32
        ScrollView();
        ~ScrollView();
    private:
        void updateScrollBars();
        IntPoint maximumScroll() const;
        int updateScrollInfo(short type, int current, int max, int pageSize);
        class ScrollViewPrivate;
        ScrollViewPrivate* m_data;
#endif
    };

}

#endif
