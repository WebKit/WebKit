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

#ifndef PageClientImpl_h
#define PageClientImpl_h

#include "PageClient.h"
#include <wtf/RetainPtr.h>

@class WKView;
@class WebEditorUndoTargetObjC;

namespace WebKit {

// NOTE: This does not use String::operator NSString*() since that function
// expects to be called on the thread running WebCore.
NSString* nsStringFromWebCoreString(const WTF::String&);

class PageClientImpl : public PageClient {
public:
    static PassOwnPtr<PageClientImpl> create(WKView*);
    virtual ~PageClientImpl();

private:
    PageClientImpl(WKView*);

    virtual void processDidExit();
    virtual void processDidRevive();
    virtual void takeFocus(bool direction);
    virtual void toolTipChanged(const WTF::String& oldToolTip, const WTF::String& newToolTip);
    virtual void setCursor(const WebCore::Cursor&);

    void registerEditCommand(PassRefPtr<WebEditCommandProxy>, UndoOrRedo);
    void clearAllEditCommands();

#if USE(ACCELERATED_COMPOSITING)
    void pageDidEnterAcceleratedCompositing();
    void pageDidLeaveAcceleratedCompositing();
#endif

    WKView* m_wkView;
    RetainPtr<WebEditorUndoTargetObjC> m_undoTarget;
};

} // namespace WebKit

#endif // PageClientImpl_h
