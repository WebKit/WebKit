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

#include "config.h"
#include "LocalFileSystem.h"

#if PLATFORM(CHROMIUM)
#error "Chromium should not compile this file and instead define its own version of these factories."
#endif

#if ENABLE(FILE_SYSTEM)

#include "CrossThreadTask.h"
#include "DOMFileSystem.h"
#include "ErrorCallback.h"
#include "ExceptionCode.h"
#include "FileError.h"
#include "FileSystemCallback.h"
#include "FileSystemCallbacks.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include <wtf/PassRefPtr.h>

namespace WebCore {

PassRefPtr<LocalFileSystem> LocalFileSystem::create(const String& basePath)
{
    return adoptRef(new LocalFileSystem(basePath));
}

static void openFileSystem(ScriptExecutionContext*, const String& basePath, const String& identifier, AsyncFileSystem::Type type, PassOwnPtr<FileSystemCallbacks> callbacks)
{
    AsyncFileSystem::openFileSystem(basePath, identifier, type, callbacks);
}

void LocalFileSystem::requestFileSystem(ScriptExecutionContext* context, AsyncFileSystem::Type type, long long, PassRefPtr<FileSystemCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    if (type != AsyncFileSystem::Temporary && type != AsyncFileSystem::Persistent) {
        DOMFileSystem::scheduleCallback(context, errorCallback, FileError::create(INVALID_MODIFICATION_ERR));
        return;
    }

    // AsyncFileSystem::openFileSystem calls callbacks synchronously, so the method needs to be called asynchronously.
    context->postTask(createCallbackTask(&openFileSystem, m_basePath, context->securityOrigin()->databaseIdentifier(), type, new FileSystemCallbacks(successCallback, errorCallback, context)));
}

} // namespace

#endif // ENABLE(FILE_SYSTEM)
