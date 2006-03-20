/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#ifndef BROWSEREXTENSION_H_
#define BROWSEREXTENSION_H_

#include "PlatformString.h"
#include "formdata.h"
#include <kxmlcore/HashMap.h>
#include "ResourceRequest.h"

class KURL;

namespace WebCore {

class Frame;

struct WindowArgs {
    int x;
    bool xSet;
    int y;
    bool ySet;
    int width;
    bool widthSet;
    int height;
    bool heightSet;

    bool menuBarVisible;
    bool statusBarVisible;
    bool toolBarVisible;
    bool locationBarVisible;
    bool scrollBarsVisible;
    bool resizable;

    bool fullscreen;
    bool dialog;
};

class BrowserExtension {
public:
    virtual ~BrowserExtension() { }

    virtual void createNewWindow(const ResourceRequest&) = 0;
    
    virtual void createNewWindow(const ResourceRequest&, 
                                 const WindowArgs&, 
                                 Frame*& part) = 0;
    
    virtual void setIconURL(const KURL&) = 0;
    virtual void setTypedIconURL(const KURL&, const DeprecatedString& type) = 0;
    
    virtual int getHistoryLength() = 0;
    virtual void goBackOrForward(int distance) = 0;
    
    virtual bool canRunModal() = 0;
    virtual bool canRunModalNow() = 0;
    virtual void runModal() = 0;

protected:
    BrowserExtension() {};

};

}

#endif
