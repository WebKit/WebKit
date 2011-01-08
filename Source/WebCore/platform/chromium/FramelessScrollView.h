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

#ifndef FramelessScrollView_h
#define FramelessScrollView_h

#include "ScrollView.h"

namespace WebCore {

    class FramelessScrollViewClient;
    class PlatformKeyboardEvent;
    class PlatformMouseEvent;
    class PlatformWheelEvent;

    // A FramelessScrollView is a ScrollView that can be used to render custom
    // content, which does not have an associated Frame.
    //
    // NOTE: It may be better to just develop a custom subclass of Widget that
    // can have scroll bars for this instead of trying to reuse ScrollView.
    //
    class FramelessScrollView : public ScrollView {
    public:
        FramelessScrollView() : m_client(0) {}
        ~FramelessScrollView();

        FramelessScrollViewClient* client() const { return m_client; }
        void setClient(FramelessScrollViewClient* client) { m_client = client; }

        // Event handlers that subclasses must implement.
        virtual bool handleMouseDownEvent(const PlatformMouseEvent&) = 0;
        virtual bool handleMouseMoveEvent(const PlatformMouseEvent&) = 0;
        virtual bool handleMouseReleaseEvent(const PlatformMouseEvent&) = 0;
        virtual bool handleWheelEvent(const PlatformWheelEvent&) = 0;
        virtual bool handleKeyEvent(const PlatformKeyboardEvent&) = 0;

        // ScrollbarClient public methods:
        virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&);
        virtual bool isActive() const;

        // Widget public methods:
        virtual void invalidateRect(const IntRect&);

        // ScrollView public methods:
        virtual HostWindow* hostWindow() const;
        virtual IntRect windowClipRect(bool clipToContents = true) const;

    protected:
        // ScrollView protected methods:
        virtual void paintContents(GraphicsContext*, const IntRect& damageRect);
        virtual void contentsResized();
        virtual void visibleContentsResized();

    private:
        FramelessScrollViewClient* m_client;
    };

} // namespace WebCore

#endif
