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
#include <qobject.h>

class KURL;
class ObjectContents;

namespace WebCore {

struct URLArgs {

    QString frameName;
    FormData postData;
    bool reload;
    QString serviceType;
    int xOffset;
    int yOffset;

    URLArgs() : reload(false), xOffset(0), yOffset(0), m_doPost(false), m_lockHistory(false) { }
    
    QString contentType() const { return m_contentType; }
    void setContentType(const QString &t) { m_contentType = t; }

    bool doPost() const { return m_doPost; }
    void setDoPost(bool post) { m_doPost = post; }
    
    bool lockHistory() const { return m_lockHistory; }
    void setLockHistory(bool lock) { m_lockHistory = lock; }

    HashMap<DOMString, DOMString>& metaData() { return m_metadata; }
    const HashMap<DOMString, DOMString>& metaData() const { return m_metadata; }

private:
    QString m_contentType;
    bool m_doPost;
    bool m_lockHistory;
    HashMap<DOMString, DOMString> m_metadata;
};

struct WindowArgs {
    int x;
    int y;
    int width;
    int height;
    bool menuBarVisible;
    bool statusBarVisible;
    bool toolBarVisible;
    bool locationBarVisible;
    bool scrollBarsVisible;
    bool resizable;
    bool fullscreen;
    bool xSet;
    bool ySet;
    bool widthSet;
    bool heightSet;
    bool dialog;
};

class BrowserExtension : public QObject {
public:
    virtual void openURLRequest(const KURL &, const URLArgs &args = URLArgs()) = 0;
    virtual void openURLNotify() = 0;
    
    virtual void createNewWindow(const KURL &url, 
                                 const URLArgs &urlArgs = URLArgs()) = 0;
    
    virtual void createNewWindow(const KURL &url, 
                                 const URLArgs &urlArgs, 
                                 const WindowArgs &winArgs, 
                                 ObjectContents *&part) = 0;
    
    virtual void setIconURL(const KURL &url) = 0;
    virtual void setTypedIconURL(const KURL &url, const QString &type) = 0;
    
    virtual int getHistoryLength() = 0;
    virtual void goBackOrForward(int distance) = 0;
    
    void setURLArgs(const URLArgs &args) { m_args = args; }
    URLArgs urlArgs() const { return m_args; }

    virtual bool canRunModal() = 0;
    virtual bool canRunModalNow() = 0;
    virtual void runModal() = 0;

protected:
    BrowserExtension() {};

private:
    URLArgs m_args;
};

}

#endif
