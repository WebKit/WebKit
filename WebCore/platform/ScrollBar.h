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

#ifndef ScrollBar_h
#define ScrollBar_h

namespace WebCore {

class GraphicsContext;
class IntRect;
class ScrollBar;

// These match the numbers we use over in WebKit (WebFrameView.m).
#define LINE_STEP   40
#define PAGE_KEEP   40

enum ScrollDirection {
    ScrollUp,
    ScrollDown,
    ScrollLeft,
    ScrollRight
};

enum ScrollGranularity {
    ScrollByLine,
    ScrollByPage,
    ScrollByDocument,
    ScrollByWheel
};

enum ScrollBarOrientation { HorizontalScrollBar, VerticalScrollBar };

class ScrollBarClient {
public:
    virtual ~ScrollBarClient() {}
    virtual void valueChanged(ScrollBar*) = 0;
};

class ScrollBar {
protected:
    ScrollBar(ScrollBarClient*, ScrollBarOrientation);

public:
    virtual ~ScrollBar() {}

    virtual bool isWidget() const = 0;

    ScrollBarOrientation orientation() const { return m_orientation; }
    int value() const { return m_currentPos; } 

    void setSteps(int lineStep, int pageStep);
    
    bool setValue(int);
    void setProportion(int visibleSize, int totalSize);

    bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1.0);
    
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual void setRect(const IntRect&) = 0;
    virtual void setEnabled(bool) = 0;
    virtual void paint(GraphicsContext*, const IntRect& damageRect) = 0;

    static bool hasPlatformScrollBars() {
        // To use the platform's built-in scrollbars by default, return true.  We may
        // support styled engine scrollbars someday, and some platforms may wish to not
        // implement a platform scrollbar at all by default.  That's what this method is for.
        return true;
    }

protected:
    virtual void updateThumbPosition() = 0;
    virtual void updateThumbProportion() = 0;

    ScrollBarClient* client() const { return m_client; }

    ScrollBarClient* m_client;
    ScrollBarOrientation m_orientation;
    int m_visibleSize;
    int m_totalSize;
    int m_currentPos;
    int m_lineStep;
    int m_pageStep;
};

}

#endif
