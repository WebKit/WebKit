/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef WebSelectionServiceController_h
#define WebSelectionServiceController_h

#if ENABLE(SERVICE_CONTROLS)

#import "WebSharingServicePickerController.h"
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

OBJC_CLASS NSImage;
OBJC_CLASS NSWindow;
OBJC_CLASS WebView;

namespace WebCore {
class FrameSelection;
class IntPoint;
}

class WebSelectionServiceController : public WebSharingServicePickerClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebSelectionServiceController(WebView*);

    void handleSelectionServiceClick(WebCore::FrameSelection&, const Vector<String>& telephoneNumbers, const WebCore::IntPoint&);
    bool hasRelevantSelectionServices(bool isTextOnly) const;

    // WebSharingServicePickerClient
    void sharingServicePickerWillBeDestroyed(WebSharingServicePickerController &) override;

private:
    RetainPtr<WebSharingServicePickerController> m_sharingServicePickerController;
};

#endif // ENABLE(SERVICE_CONTROLS)

#endif // WebSelectionServiceController_h
