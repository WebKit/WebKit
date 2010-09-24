/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PageClient_h
#define PageClient_h

#include <wtf/Forward.h>

namespace WebCore {
    class Cursor;
}

namespace WebKit {

class WebEditCommandProxy;

class PageClient {
public:
    virtual ~PageClient() { }

    virtual void processDidExit() = 0;
    virtual void processDidRevive() = 0;

    virtual void takeFocus(bool direction) = 0;
    virtual void toolTipChanged(const String&, const String&) = 0;

    virtual void setCursor(const WebCore::Cursor&) = 0;

    enum UndoOrRedo { Undo, Redo };
    virtual void registerEditCommand(PassRefPtr<WebEditCommandProxy>, UndoOrRedo) = 0;
    virtual void clearAllEditCommands() = 0;

#if USE(ACCELERATED_COMPOSITING)
    virtual void pageDidEnterAcceleratedCompositing() = 0;
    virtual void pageDidLeaveAcceleratedCompositing() = 0;
#endif

#if PLATFORM(WIN)
    virtual HWND nativeWindow() = 0;
#endif
};

} // namespace WebKit

#endif // PageClient_h
