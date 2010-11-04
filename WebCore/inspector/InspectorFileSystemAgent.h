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

#ifndef InspectorFileSystemAgent_h
#define InspectorFileSystemAgent_h

#if ENABLE(INSPECTOR) && ENABLE(FILE_SYSTEM)

#include "AsyncFileSystem.h"
#include "AsyncFileSystemCallbacks.h"
#include <wtf/PassRefPtr.h>

namespace WebCore {

class Document;
class InspectorController;
class InspectorFrontend;
class LocalFileSystem;

class InspectorFileSystemAgent : public RefCounted<InspectorFileSystemAgent> {
public:
    static PassRefPtr<InspectorFileSystemAgent> create(InspectorController* inspectorController, InspectorFrontend* frontend)
    {
        return adoptRef(new InspectorFileSystemAgent(inspectorController, frontend));
    }

    ~InspectorFileSystemAgent();
    void stop();

    // From Frontend
    void getFileSystemPathAsync(unsigned int type, const String& origin);
    void revealFolderInOS(const String& path);

    // Backend to Frontend
    void didGetFileSystemPath(const String&, AsyncFileSystem::Type, const String& origin);
    void didGetFileSystemError(AsyncFileSystem::Type, const String& origin);
    void didGetFileSystemDisabled();

private:
    InspectorFileSystemAgent(InspectorController*, InspectorFrontend*);
    void getFileSystemRoot(AsyncFileSystem::Type);

    InspectorController* m_inspectorController;
    InspectorFrontend* m_frontend;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(FILE_SYSTEM)
#endif // InspectorFileSystemAgent_h
