/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#include "TransferJobClient.h"
#include "FrameWin.h"

class KURL;

namespace WebCore {
    class FormData;
    class Frame;
    class FrameView;
    class String;
    class TransferJob;
}

namespace WebKit {

    class WebView;

    class WebFrame : public WebCore::TransferJobClient, public WebCore::FrameWinClient {
    public:
    WebFrame(char* name, WebView*);

    void loadFilePath(char*);
    void loadHTMLString(char* html, char* baseURL = 0);

    virtual void openURL(const DeprecatedString&);
    virtual void submitForm(const WebCore::String& method, const KURL&, const WebCore::FormData*);

    void loadURL(const char*);
    
    virtual void receivedData(WebCore::TransferJob*, const char*, int);
    virtual void receivedAllData(WebCore::TransferJob*, WebCore::PlatformData);

    void paint();

    WebCore::Frame* impl();

    private:
        class WebFramePrivate;
        WebFramePrivate *d;
    };
}
