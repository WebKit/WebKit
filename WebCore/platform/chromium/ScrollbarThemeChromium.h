/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScrollbarThemeChromium_h
#define ScrollbarThemeChromium_h

#include "ScrollbarThemeComposite.h"

namespace WebCore {

    class PlatformMouseEvent;

    // This class contains the Chromium scrollbar implementations for Windows
    // and Linux. All of the symbols here in must be defined somewhere in the
    // code and we manage the platform specific parts by linking in different,
    // platform specific, files. Methods that we shared across platforms are
    // implemented in ScrollbarThemeChromium.cpp
    class ScrollbarThemeChromium : public ScrollbarThemeComposite {
    public:
        ScrollbarThemeChromium();
        virtual ~ScrollbarThemeChromium();

        virtual int scrollbarThickness(ScrollbarControlSize = RegularScrollbar);

        virtual void themeChanged();
        
        virtual bool invalidateOnMouseEnterExit();

    protected:
        virtual bool hasButtons(Scrollbar*) { return true; }
        virtual bool hasThumb(Scrollbar*);

        virtual IntRect backButtonRect(Scrollbar*, ScrollbarPart, bool painting = false);
        virtual IntRect forwardButtonRect(Scrollbar*, ScrollbarPart, bool painting = false);
        virtual IntRect trackRect(Scrollbar*, bool painting = false);

        virtual void paintScrollCorner(ScrollView*, GraphicsContext*, const IntRect&);
        virtual bool shouldCenterOnThumb(Scrollbar*, const PlatformMouseEvent&);
        virtual bool shouldSnapBackToDragOrigin(Scrollbar*, const PlatformMouseEvent&);

        virtual void paintTrackBackground(GraphicsContext*, Scrollbar*, const IntRect&);
        virtual void paintTrackPiece(GraphicsContext*, Scrollbar*, const IntRect&, ScrollbarPart);
        virtual void paintButton(GraphicsContext*, Scrollbar*, const IntRect&, ScrollbarPart);
        virtual void paintThumb(GraphicsContext*, Scrollbar*, const IntRect&);
        virtual void paintTickmarks(GraphicsContext*, Scrollbar*, const IntRect&);

    private:
        IntSize buttonSize(Scrollbar*);

        int getThemeState(Scrollbar*, ScrollbarPart) const;
        int getThemeArrowState(Scrollbar*, ScrollbarPart) const;
        int getClassicThemeState(Scrollbar*, ScrollbarPart) const;
    };

} // namespace WebCore

#endif
