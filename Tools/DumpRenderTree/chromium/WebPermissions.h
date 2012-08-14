/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef WebPermissions_h
#define WebPermissions_h

#include "WebPermissionClient.h"

class DRTTestRunner;
class TestShell;

class WebPermissions : public WebKit::WebPermissionClient {
public:
    WebPermissions(TestShell*);
    virtual ~WebPermissions();

    // Override WebPermissionClient methods.
    virtual bool allowImage(WebKit::WebFrame*, bool enabledPerSettings, const WebKit::WebURL& imageURL);
    virtual bool allowScriptFromSource(WebKit::WebFrame*, bool enabledPerSettings, const WebKit::WebURL& scriptURL);
    virtual bool allowStorage(WebKit::WebFrame*, bool local);
    virtual bool allowPlugins(WebKit::WebFrame*, bool enabledPerSettings);
    virtual bool allowDisplayingInsecureContent(WebKit::WebFrame*, bool enabledPerSettings,
                                                const WebKit::WebSecurityOrigin&, const WebKit::WebURL&);
    virtual bool allowRunningInsecureContent(WebKit::WebFrame*, bool enabledPerSettings,
                                             const WebKit::WebSecurityOrigin&, const WebKit::WebURL&);

    // Hooks to set the different policies.
    void setImagesAllowed(bool);
    void setScriptsAllowed(bool);
    void setStorageAllowed(bool);
    void setPluginsAllowed(bool);
    void setDisplayingInsecureContentAllowed(bool);
    void setRunningInsecureContentAllowed(bool);

    // Resets the policy to allow everything, except for running insecure content.
    void reset();

private:
    DRTTestRunner* testRunner() const;

    // Non-owning pointer. The WebPermissions instance is owned by this TestShell instance.
    TestShell* m_shell;

    bool m_imagesAllowed;
    bool m_scriptsAllowed;
    bool m_storageAllowed;
    bool m_pluginsAllowed;
    bool m_displayingInsecureContentAllowed;
    bool m_runningInsecureContentAllowed;
};

#endif
