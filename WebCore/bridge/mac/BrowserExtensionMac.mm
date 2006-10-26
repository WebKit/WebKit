/*
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "BrowserExtensionMac.h"

#import "BlockExceptions.h"
#import "FloatRect.h"
#import "FrameLoadRequest.h"
#import "FrameMac.h"
#import "FrameTree.h"
#import "KURL.h"
#import "Page.h"
#import "Screen.h"
#import "WebCoreFrameBridge.h"
#import "WebCorePageBridge.h"
#import "WindowFeatures.h"

namespace WebCore {

BrowserExtensionMac::BrowserExtensionMac(Frame *frame)
    : m_frame(Mac(frame))
{
}

void BrowserExtensionMac::createNewWindow(const FrameLoadRequest& request, 
                                          const WindowFeatures& windowFeatures, 
                                          Frame*& newFrame)
{ 
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    ASSERT(!windowFeatures.dialog || request.m_frameName.isEmpty());

    newFrame = NULL;
    
    const KURL& url = request.m_request.url();

    NSString *frameName = request.m_frameName.isEmpty() ? nil : (NSString*)request.m_frameName;
    if (frameName) {
        if (Frame* frame = m_frame->tree()->find(frameName)) {
            if (!url.isEmpty())
                Mac(frame)->loadRequest(request, true, nil, nil, nil);
            [Mac(frame)->bridge() activateWindow];
            newFrame = frame;
            return;
        }
    }
    
    WebCorePageBridge *pageBridge;
    if (windowFeatures.dialog)
        pageBridge = [m_frame->page()->bridge() createModalDialogWithURL:url.getNSURL() referrer:m_frame->referrer()];
    else
        pageBridge = [m_frame->bridge() createWindowWithURL:url.getNSURL()];
    if (!pageBridge)
        return;
    
    WebCoreFrameBridge *frameBridge = [pageBridge mainFrame];
    if ([frameBridge _frame])
        [frameBridge _frame]->tree()->setName(AtomicString(request.m_frameName));
    
    newFrame = [frameBridge _frame];
    
    [frameBridge setToolbarsVisible:windowFeatures.toolBarVisible || windowFeatures.locationBarVisible];
    [frameBridge setStatusbarVisible:windowFeatures.statusBarVisible];
    [frameBridge setScrollbarsVisible:windowFeatures.scrollbarsVisible];
    [frameBridge setWindowIsResizable:windowFeatures.resizable];
    
    NSRect windowRect = [pageBridge impl]->windowRect();
    if (windowFeatures.xSet)
      windowRect.origin.x = windowFeatures.x;
    if (windowFeatures.ySet)
      windowRect.origin.y = windowFeatures.y;
    
    // 'width' and 'height' specify the dimensions of the WebView, but we can only resize the window, 
    // so we compute a WebView delta and apply it to the window.
    NSRect webViewRect = [[pageBridge outerView] frame];
    if (windowFeatures.widthSet)
      windowRect.size.width += windowFeatures.width - webViewRect.size.width;
    if (windowFeatures.heightSet)
      windowRect.size.height += windowFeatures.height - webViewRect.size.height;
    
    [pageBridge impl]->setWindowRect(windowRect);
    
    [frameBridge showWindow];
    
    END_BLOCK_OBJC_EXCEPTIONS;
}

}
