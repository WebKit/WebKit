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

#ifndef WebFileSystemCallbacksImpl_h
#define WebFileSystemCallbacksImpl_h

#include "FileSystemType.h"
#include "WebFileSystemCallbacks.h"
#include "platform/WebVector.h"
#include <public/WebFileSystem.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {
class AsyncFileSystemCallbacks;
class ScriptExecutionContext;
}

namespace WebKit {

struct WebFileInfo;
struct WebFileSystemEntry;
class WebString;
class WebURL;

class WebFileSystemCallbacksImpl : public WebFileSystemCallbacks {
public:
    WebFileSystemCallbacksImpl(PassOwnPtr<WebCore::AsyncFileSystemCallbacks>, WebCore::FileSystemType = WebCore::FileSystemTypeTemporary, WebCore::ScriptExecutionContext* = 0, WebCore::FileSystemSynchronousType = WebCore::AsynchronousFileSystem);
    virtual ~WebFileSystemCallbacksImpl();

    virtual void didSucceed();
    virtual void didReadMetadata(const WebFileInfo& info);
    virtual void didReadDirectory(const WebVector<WebFileSystemEntry>& entries, bool hasMore);
    virtual void didOpenFileSystem(const WebString& name, const WebURL& rootURL);
    virtual void didFail(WebFileError error);

private:
    OwnPtr<WebCore::AsyncFileSystemCallbacks> m_callbacks;

    // Used for openFileSystem callbacks.
    WebCore::FileSystemType m_type;

    // Used for worker's openFileSystem callbacks.
    WebCore::ScriptExecutionContext* m_context;
    WebCore::FileSystemSynchronousType m_synchronousType;
};

} // namespace WebKit

#endif // WebFileSystemCallbacksImpl_h
