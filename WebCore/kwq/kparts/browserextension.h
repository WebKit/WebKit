/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#include <qevent.h>
#include <qmap.h>
#include <qpoint.h>

#include <kurl.h>

#include "part.h"
#include "browserinterface.h"

class KXMLGUIClient { };

namespace KParts {

struct URLArgs {

    QString frameName;
    QByteArray postData;
    bool reload;
    QString serviceType;
    int xOffset;
    int yOffset;

    URLArgs() : xOffset(0), yOffset(0) { }
    
    QString contentType() const { return QString::null; }
    bool doPost() const { return false; }
    
    QMap<QString, QString> &metaData() { return m_metadata; }
    void setLockHistory(bool) { }

private:
    QMap<QString, QString> m_metadata;

};

struct WindowArgs {

    int x;
    int y;
    int width;
    int height;
    bool menuBarVisible;
    bool statusBarVisible;
    bool toolBarsVisible;
    bool scrollbarsVisible;
    bool resizable;
    bool fullscreen;
    bool xSet;
    bool ySet;
    bool widthSet;
    bool heightSet;

    WindowArgs() : x(0), y(0), width(0), height(0), menuBarVisible(false), statusBarVisible(true), toolBarsVisible(true), scrollbarsVisible(true), resizable(true), fullscreen(true), xSet(false), ySet(false), widthSet(false), heightSet(false) { }

};

class BrowserExtension : public QObject {
public:
     BrowserExtension() { }
     BrowserInterface *browserInterface() const { return 0; }
     
     virtual void openURLRequest(const KURL &, const KParts::URLArgs &args = KParts::URLArgs()) = 0;
     
     virtual void createNewWindow(const KURL &url, 
				  const KParts::URLArgs &urlArgs = KParts::URLArgs()) = 0;

     virtual void createNewWindow(const KURL &url, 
				  const KParts::URLArgs &urlArgs, 
				  const KParts::WindowArgs &winArgs, 
				  KParts::ReadOnlyPart *&part) = 0;

     virtual void setIconURL(const KURL &url) = 0;
     virtual void setTypedIconURL(const KURL &url, const QString &type) = 0;
     
     void setURLArgs(const KParts::URLArgs &args) { m_args = args; }
     KParts::URLArgs urlArgs() const { return m_args; }

private:
    KParts::URLArgs m_args;
};

class BrowserHostExtension : public QObject { };

} // namespace KParts

#endif
