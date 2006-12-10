/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * All rights reserved.
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

#ifndef FrameQtClient_H
#define FrameQtClient_H

#include <wtf/PassRefPtr.h>
#include "ResourceHandleClient.h"

namespace WebCore {

class String;
class FrameQt;
class FormData;

class FrameQtClient {
public:
    virtual ~FrameQtClient() { }

    virtual void setFrame(const FrameQt*) = 0;

    virtual void openURL(const KURL&) = 0;
    virtual void submitForm(const String& method, const KURL&, PassRefPtr<FormData>) = 0;

    // This is invoked after any ResourceHandle is done,
    // to check wheter all items to be loaded are finished.
    virtual void checkLoaded() = 0;

    // WebKitPart / DumpRenderTree want to handle this differently.
    virtual bool menubarVisible() const = 0;
    virtual bool toolbarVisible() const = 0;
    virtual bool statusbarVisible() const = 0;
    virtual bool personalbarVisible() const = 0;
    virtual bool locationbarVisible() const = 0;

    virtual void loadFinished() const = 0;

    virtual void setTitle(const String &title) = 0;

    virtual void runJavaScriptAlert(String const& message) = 0;
    virtual bool runJavaScriptConfirm(const String& message) = 0;
    virtual bool runJavaScriptPrompt(const String& message, const String& defaultValue, String& result) = 0;
};

class FrameQtClientDefault : public FrameQtClient,
                             public ResourceHandleClient
{
public:
    FrameQtClientDefault();
    virtual ~FrameQtClientDefault();

    // FrameQtClient
    virtual void setFrame(const FrameQt*);

    virtual void openURL(const KURL&);
    virtual void submitForm(const String& method, const KURL&, PassRefPtr<FormData>);

    virtual void checkLoaded();

    virtual void runJavaScriptAlert(String const& message);
    virtual bool runJavaScriptConfirm(const String& message);
    virtual bool runJavaScriptPrompt(const String& message, const String& defaultValue, String& result);

    virtual bool menubarVisible() const;
    virtual bool toolbarVisible() const;
    virtual bool statusbarVisible() const;
    virtual bool personalbarVisible() const;
    virtual bool locationbarVisible() const;

    virtual void loadFinished() const;

    // ResourceHandleClient
    virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&);
    virtual void didReceiveData(ResourceHandle*, const char*, int, int);
    virtual void didFinishLoading(ResourceHandle*);
    virtual void didFail(ResourceHandle*, const ResourceError&);
    virtual void receivedAllData(ResourceHandle*, PlatformData);

    virtual void setTitle(const String &title);

private:
    // Internal helpers
    FrameQt* traverseNextFrameStayWithin(FrameQt*) const;
    int numPendingOrLoadingRequests(bool recurse) const;

    FrameQt* m_frame;
};

}

#endif

// vim: ts=4 sw=4 et
