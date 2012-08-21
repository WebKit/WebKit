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

#ifndef WebPermissionClient_h
#define WebPermissionClient_h

namespace WebKit {

class WebDocument;
class WebFrame;
class WebSecurityOrigin;
class WebString;
class WebURL;

class WebPermissionClient {
public:
    // Controls whether access to Web Databases is allowed for this frame.
    virtual bool allowDatabase(WebFrame*, const WebString& name, const WebString& displayName, unsigned long estimatedSize) { return true; }

    // Controls whether access to File System is allowed for this frame.
    virtual bool allowFileSystem(WebFrame*) { return true; }

    // Controls whether images are allowed for this frame.
    virtual bool allowImage(WebFrame* frame, bool enabledPerSettings, const WebURL& imageURL) { return enabledPerSettings; }

    // Controls whether access to Indexed DB are allowed for this frame.
    virtual bool allowIndexedDB(WebFrame*, const WebString& name, const WebSecurityOrigin&) { return true; }

    // Controls whether plugins are allowed for this frame.
    virtual bool allowPlugins(WebFrame*, bool enabledPerSettings) { return enabledPerSettings; }

    // Controls whether scripts are allowed to execute for this frame.
    virtual bool allowScript(WebFrame*, bool enabledPerSettings) { return enabledPerSettings; }

    // Controls whether scripts loaded from the given URL are allowed to execute for this frame.
    virtual bool allowScriptFromSource(WebFrame*, bool enabledPerSettings, const WebURL& scriptURL) { return enabledPerSettings; }

    // Controls whether insecrure content is allowed to display for this frame.
    virtual bool allowDisplayingInsecureContent(WebFrame*, bool enabledPerSettings, const WebSecurityOrigin&, const WebURL&) { return enabledPerSettings; }

    // Controls whether insecrure scripts are allowed to execute for this frame.
    virtual bool allowRunningInsecureContent(WebFrame*, bool enabledPerSettings, const WebSecurityOrigin&, const WebURL&) { return enabledPerSettings; }

    // Controls whether the given script extension should run in a new script
    // context in this frame. If extensionGroup is 0, the script context is the
    // frame's main context. Otherwise, it is a context created by
    // WebFrame::executeScriptInIsolatedWorld with that same extensionGroup
    // value.
    virtual bool allowScriptExtension(WebFrame*, const WebString& extensionName, int extensionGroup) { return true; }

    virtual bool allowScriptExtension(WebFrame* webFrame, const WebString& extensionName, int extensionGroup, int worldId)
    {
        return allowScriptExtension(webFrame, extensionName, extensionGroup);
    }

    // Controls whether HTML5 Web Storage is allowed for this frame.
    // If local is true, then this is for local storage, otherwise it's for session storage.
    virtual bool allowStorage(WebFrame*, bool local) { return true; }

    // Controls whether access to read the clipboard is allowed for this frame.
    virtual bool allowReadFromClipboard(WebFrame*, bool defaultValue) { return defaultValue; }

    // Controls whether access to write the clipboard is allowed for this frame.
    virtual bool allowWriteToClipboard(WebFrame*, bool defaultValue) { return defaultValue; }

    // Controls whether enabling Web Components API for this frame.
    virtual bool allowWebComponents(const WebDocument&, bool defaultValue) { return defaultValue; }

    // Controls whether enabling HTML notifications for this frame.
    virtual bool allowHTMLNotifications(const WebDocument&) { return true; }

    // Controls whether to enable MutationEvents for this document.
    // The common use case of this method is actually to selectively disable MutationEvents,
    // but it's been named for consistency with the rest of the interface.
    virtual bool allowMutationEvents(const WebDocument&, bool defaultValue) { return defaultValue; }

    // Notifies the client that the frame would have instantiated a plug-in if plug-ins were enabled.
    virtual void didNotAllowPlugins(WebFrame*) { }

    // Notifies the client that the frame would have executed script if script were enabled.
    virtual void didNotAllowScript(WebFrame*) { }

protected:
    ~WebPermissionClient() { }
};

} // namespace WebKit

#endif
