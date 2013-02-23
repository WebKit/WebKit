/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebTestDelegate_h
#define WebTestDelegate_h

#include "Platform/chromium/public/WebString.h"
#include "Platform/chromium/public/WebURL.h"
#include "Platform/chromium/public/WebVector.h"
#include <string>

#define WEBTESTRUNNER_NEW_HISTORY_CAPTURE

namespace WebKit {
class WebGamepads;
class WebHistoryItem;
struct WebRect;
struct WebURLError;
}

namespace WebTestRunner {

struct WebPreferences;
class WebTask;
class WebTestProxyBase;

class WebTestDelegate {
public:
    // Set and clear the edit command to execute on the next call to
    // WebViewClient::handleCurrentKeyboardEvent().
    virtual void clearEditCommand() = 0;
    virtual void setEditCommand(const std::string& name, const std::string& value) = 0;

    // Set the gamepads to return from Platform::sampleGamepads().
    virtual void setGamepadData(const WebKit::WebGamepads&) = 0;

    // Add a message to the text dump for the layout test.
    virtual void printMessage(const std::string& message) = 0;

    // The delegate takes ownership of the WebTask objects and is responsible
    // for deleting them.
    virtual void postTask(WebTask*) = 0;
    virtual void postDelayedTask(WebTask*, long long ms) = 0;

    // Register a new isolated filesystem with the given files, and return the
    // new filesystem id.
    virtual WebKit::WebString registerIsolatedFileSystem(const WebKit::WebVector<WebKit::WebString>& absoluteFilenames) = 0;

    // Gets the current time in milliseconds since the UNIX epoch.
    virtual long long getCurrentTimeInMillisecond() = 0;

    // Convert the provided relative path into an absolute path.
    virtual WebKit::WebString getAbsoluteWebStringFromUTF8Path(const std::string& path) = 0;

    // Reads in the given file and returns its contents as data URL.
    virtual WebKit::WebURL localFileToDataURL(const WebKit::WebURL&) = 0;

    // Replaces file:///tmp/LayoutTests/ with the actual path to the
    // LayoutTests directory.
    virtual WebKit::WebURL rewriteLayoutTestsURL(const std::string& utf8URL) = 0;

    // Manages the settings to used for layout tests.
    virtual WebPreferences* preferences() = 0;
    virtual void applyPreferences() = 0;

    // Returns a textual description of given error.
    virtual std::string makeURLErrorDescription(const WebKit::WebURLError&) = 0;

    // Resizes the WebView to the given size.
    virtual void setClientWindowRect(const WebKit::WebRect&) = 0;

    // Opens and closes the inspector.
    virtual void showDevTools() = 0;
    virtual void closeDevTools() = 0;

    // Evaluate the given script in the DevTools agent.
    virtual void evaluateInWebInspector(long callID, const std::string& script) = 0;

    // Controls WebSQL databases.
    virtual void clearAllDatabases() = 0;
    virtual void setDatabaseQuota(int) = 0;

    // Controls the device scale factor of the main WebView for hidpi tests.
    virtual void setDeviceScaleFactor(float) = 0;

    // Controls whether or not the main WebView should be focused.
    virtual void setFocus(bool) = 0;

    // Controls whether all cookies should be accepted or writing cookies in a
    // third-party context is blocked.
    virtual void setAcceptAllCookies(bool) = 0;

    // The same as rewriteLayoutTestsURL unless the resource is a path starting
    // with /tmp/, then return a file URL to a temporary file.
    virtual std::string pathToLocalResource(const std::string& resource) = 0;

    // Sets the POSIX locale of the current process.
    virtual void setLocale(const std::string&) = 0;

    // Invoked when the test finished.
    virtual void testFinished() = 0;

    // Invoked if the test timed out.
    virtual void testTimedOut() = 0;

    // If true, never abort a test because of a timeout.
    virtual bool isBeingDebugged() = 0;

    // The time in milliseconds after which a test is considered to have timed
    // out.
    virtual int layoutTestTimeout() = 0;

    // Invoked when the embedder should close all but the main WebView.
    virtual void closeRemainingWindows() = 0;

    // Returns the length of the back/forward history of the main WebView.
    virtual int navigationEntryCount() = 0;

    // The following trigger navigations on the main WebViwe.
    virtual void goToOffset(int offset) = 0;
    virtual void reload() = 0;
    virtual void loadURLForFrame(const WebKit::WebURL&, const std::string& frameName) = 0;

    // Returns true if resource requests to external URLs should be permitted.
    virtual bool allowExternalPages() = 0;

    // Returns the back/forward history for the WebView associated with the
    // given WebTestProxyBase as well as the index of the current entry.
    virtual void captureHistoryForWindow(WebTestProxyBase*, WebKit::WebVector<WebKit::WebHistoryItem>*, size_t* currentEntryIndex) = 0;
};

}

#endif // WebTestDelegate_h
