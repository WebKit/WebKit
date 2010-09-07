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

#ifndef WebKit_h
#define WebKit_h

#include "WebCommon.h"

namespace WebKit {

class WebKitClient;

// Must be called on the thread that will be the main WebKit thread before
// using any other WebKit APIs.  The provided WebKitClient must be non-null
// and must remain valid until the current thread calls shutdown.
WEBKIT_API void initialize(WebKitClient*);

// Once shutdown, the WebKitClient passed to initialize will no longer be
// accessed.  No other WebKit objects should be in use when this function
// is called.  Any background threads created by WebKit are promised to be
// terminated by the time this function returns.
WEBKIT_API void shutdown();

// Returns the WebKitClient instance passed to initialize.
WEBKIT_API WebKitClient* webKitClient();

// Alters the rendering of content to conform to a fixed set of rules.
WEBKIT_API void setLayoutTestMode(bool);
WEBKIT_API bool layoutTestMode();

// This is a temporary flag while we try to get Linux to match Windows'
// checksum computation.  It specifies whether or not the baseline images
// should be opaque or not.
WEBKIT_API bool areLayoutTestImagesOpaque();

// Enables the named log channel.  See WebCore/platform/Logging.h for details.
WEBKIT_API void enableLogChannel(const char*);

// Purge the plugin list cache. If |reloadPages| is true, any pages
// containing plugins will be reloaded after refreshing the plugin list.
WEBKIT_API void resetPluginCache(bool reloadPages);

} // namespace WebKit

#endif
