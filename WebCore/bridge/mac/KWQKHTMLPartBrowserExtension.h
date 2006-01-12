/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#include "KWQKPartsBrowserExtension.h"
#include "KWQKPartsBrowserInterface.h"

class QWidget;
class Frame;

class KHTMLPartBrowserExtension : public KParts::BrowserExtension {
public:
    KHTMLPartBrowserExtension(Frame *);
    
    virtual KParts::BrowserInterface *browserInterface() { return &_browserInterface; }

    virtual void openURLRequest(const KURL &, 
				const KParts::URLArgs &args = KParts::URLArgs());
    virtual void openURLNotify();
     
    virtual void createNewWindow(const KURL &url, 
				 const KParts::URLArgs &urlArgs = KParts::URLArgs());
    virtual void createNewWindow(const KURL &url,
				 const KParts::URLArgs &urlArgs, 
				 const KParts::WindowArgs &winArgs, 
				 ObjectContents *&part);

    virtual void setIconURL(const KURL &url);
    virtual void setTypedIconURL(const KURL &url, const QString &type);

    bool canRunModal();
    bool canRunModalNow();
    void runModal();
    
private:
     void createNewWindow(const KURL &url, 
			  const KParts::URLArgs &urlArgs, 
			  const KParts::WindowArgs &winArgs, 
			  ObjectContents **part);

     MacFrame *m_frame;
     KParts::BrowserInterface _browserInterface;
};
