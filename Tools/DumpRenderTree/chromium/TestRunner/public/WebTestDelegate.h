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

namespace WebKit {
class WebGamepads;
class WebIntentRequest;
struct WebContextMenuData;
struct WebRect;
struct WebURLError;
}

namespace WebTestRunner {

struct WebPreferences;
class WebTask;

class WebTestDelegate {
public:
    virtual void clearContextMenuData() = 0;
    virtual void clearEditCommand() = 0;
    virtual void fillSpellingSuggestionList(const WebKit::WebString& word, WebKit::WebVector<WebKit::WebString>* suggestions) = 0;
    virtual void setEditCommand(const std::string& name, const std::string& value) = 0;
    virtual WebKit::WebContextMenuData* lastContextMenuData() const = 0;
    virtual void setGamepadData(const WebKit::WebGamepads&) = 0;
    virtual void printMessage(const std::string& message) = 0;

    // The delegate takes ownership of the WebTask objects and is responsible
    // for deleting them.
    virtual void postTask(WebTask*) = 0;
    virtual void postDelayedTask(WebTask*, long long ms) = 0;

    virtual WebKit::WebString registerIsolatedFileSystem(const WebKit::WebVector<WebKit::WebString>& absoluteFilenames) = 0;
    virtual long long getCurrentTimeInMillisecond() = 0;
    virtual WebKit::WebString getAbsoluteWebStringFromUTF8Path(const std::string& path) = 0;

    // Methods used by TestRunner.
    // FIXME: Make these methods pure virtual once the TestRunner is part of
    // the public TestRunner API.
    virtual WebKit::WebURL localFileToDataURL(const WebKit::WebURL&) { return WebKit::WebURL(); }
    virtual WebKit::WebURL rewriteLayoutTestsURL(const std::string&) { return WebKit::WebURL(); }
    virtual WebPreferences* preferences() { return 0; }
    virtual void applyPreferences() { };
    virtual void setCurrentWebIntentRequest(const WebKit::WebIntentRequest&) { };
    virtual WebKit::WebIntentRequest* currentWebIntentRequest() { return 0; }
    virtual std::string makeURLErrorDescription(const WebKit::WebURLError&) { return std::string(); }
    virtual std::string normalizeLayoutTestURL(const std::string&) { return std::string(); }
    virtual void setClientWindowRect(const WebKit::WebRect&) { }
    virtual void setSelectTrailingWhitespaceEnabled(bool) { }
    virtual void setSmartInsertDeleteEnabled(bool) { }
    virtual void showDevTools() { }
    virtual void closeDevTools() { }
    virtual void evaluateInWebInspector(long, const std::string&) { }
    virtual void clearAllDatabases() { }
    virtual void setDatabaseQuota(int) { }
};

}

#endif // WebTestDelegate_h
