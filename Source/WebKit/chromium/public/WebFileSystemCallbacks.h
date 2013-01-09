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

#ifndef WebFileSystemCallbacks_h
#define WebFileSystemCallbacks_h

#include "WebFileError.h"
#include "WebFileSystemEntry.h"
#include <public/WebVector.h>

namespace WebKit {

class WebString;
class WebURL;
struct WebFileInfo;

class WebFileSystemCallbacks {
public:
    // Callback for WebFileSystem's various operations that don't require
    // return values.
    virtual void didSucceed() = 0;

    // Callback for WebFileSystem::readMetadata. Called with the file metadata
    // for the requested path.
    virtual void didReadMetadata(const WebFileInfo&) = 0;

    // Callback for WebFileSystem::readDirectory.  Called with a vector of
    // file entries in the requested directory. This callback might be called
    // multiple times if the directory has many entries. |hasMore| must be
    // true when there are more entries.
    virtual void didReadDirectory(const WebVector<WebFileSystemEntry>&, bool hasMore) = 0;

    // Callback for WebFrameClient::openFileSystem. Called with a name and
    // root URL for the FileSystem when the request is accepted.
    virtual void didOpenFileSystem(const WebString& name, const WebURL& rootURL) = 0;

    // Called with an error code when a requested operation hasn't been
    // completed.
    virtual void didFail(WebFileError) = 0;

protected:
    virtual ~WebFileSystemCallbacks() {}
};

} // namespace WebKit

#endif
