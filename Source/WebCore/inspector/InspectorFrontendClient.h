/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CertificateInfo.h"
#include "UserInterfaceLayoutDirection.h"
#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class InspectorFrontendClient {
public:
    enum class DockSide {
        Undocked = 0,
        Right,
        Left,
        Bottom,
    };

    virtual ~InspectorFrontendClient() = default;

    WEBCORE_EXPORT virtual void windowObjectCleared() = 0;
    virtual void frontendLoaded() = 0;

    virtual void startWindowDrag() = 0;
    virtual void moveWindowBy(float x, float y) = 0;

    virtual String localizedStringsURL() = 0;
    virtual unsigned inspectionLevel() const = 0;
    virtual String backendCommandsURL() { return String(); };
    virtual String debuggableType() { return "web"_s; }

    virtual void bringToFront() = 0;
    virtual void closeWindow() = 0;

    virtual UserInterfaceLayoutDirection userInterfaceLayoutDirection() const = 0;

    WEBCORE_EXPORT virtual void requestSetDockSide(DockSide) = 0;
    WEBCORE_EXPORT virtual void changeAttachedWindowHeight(unsigned) = 0;
    WEBCORE_EXPORT virtual void changeAttachedWindowWidth(unsigned) = 0;

    WEBCORE_EXPORT virtual void openInNewTab(const String& url) = 0;

    virtual bool canSave() = 0;
    virtual void save(const WTF::String& url, const WTF::String& content, bool base64Encoded, bool forceSaveAs) = 0;
    virtual void append(const WTF::String& url, const WTF::String& content) = 0;

    virtual void inspectedURLChanged(const String&) = 0;
    virtual void showCertificate(const CertificateInfo&) = 0;

    virtual void pagePaused() { }
    virtual void pageUnpaused() { }

    WEBCORE_EXPORT virtual void sendMessageToBackend(const String&) = 0;

    WEBCORE_EXPORT virtual bool isUnderTest() = 0;
};

} // namespace WebCore
