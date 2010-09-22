/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebScriptController_h
#define WebScriptController_h

#include "WebCommon.h"

namespace v8 {
class Extension;
}

namespace WebKit {

class WebString;

class WebScriptController {
public:
    // Registers a v8 extension to be available on webpages. The three forms
    // offer various restrictions on what types of contexts the extension is
    // loaded into. If a scheme is provided, only pages whose URL has the given
    // scheme will match. If extensionGroup is provided, the extension will only
    // be loaded into scripts run via WebFrame::ExecuteInNewWorld with the
    // matching group.
    // Will only affect v8 contexts initialized after this call. Takes ownership
    // of the v8::Extension object passed.
    // FIXME: remove the latter 2 versions in phase 3 of multipart checkin:
    // https://bugs.webkit.org/show_bug.cgi?id=45721
    WEBKIT_API static void registerExtension(v8::Extension*);
    WEBKIT_API static void registerExtension(v8::Extension*,
                                             const WebString& schemeRestriction);
    WEBKIT_API static void registerExtension(v8::Extension*, int extensionGroup);

    // Enables special settings which are only applicable if V8 is executed
    // in the single thread which must be the main thread.
    // FIXME: make a try to dynamically detect when this condition is broken
    // and automatically switch off single thread mode.
    WEBKIT_API static void enableV8SingleThreadMode();

    // Process any pending JavaScript console messages.
    WEBKIT_API static void flushConsoleMessages();

private:
    WebScriptController();
};

} // namespace WebKit

#endif
