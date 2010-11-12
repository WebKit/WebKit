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
#include "InspectorFileSystemAgent.h"

#if ENABLE(INSPECTOR) && ENABLE(FILE_SYSTEM)

#include "AsyncFileWriter.h"
#include "Document.h"
#include "FileSystem.h"
#include "FileSystemCallbacks.h"
#include "Frame.h"
#include "FrameTree.h"
#include "InspectorController.h"
#include "InspectorFrontend.h"
#include "LocalFileSystem.h"
#include "NotImplemented.h"
#include "Page.h"
#include "RuntimeEnabledFeatures.h"

namespace WebCore {

class InspectorFileSystemAgentCallbacks : public AsyncFileSystemCallbacks {
public:
    InspectorFileSystemAgentCallbacks(InspectorFileSystemAgent* agent, AsyncFileSystem::Type type, const String& origin)
        : m_agent(agent)
        , m_type(type)
        , m_origin(origin)
    { 
    }

    ~InspectorFileSystemAgentCallbacks()
    {
    }

    // FileSystemCallbacks is only used for getting filesystem. All other methods are irrelevant and will not be called.
    void didSucceed()
    {
        ASSERT_NOT_REACHED();
    }

    void didOpenFileSystem(const String&, PassOwnPtr<AsyncFileSystem> fileSystem)
    {
        // Agent will be alive even if InspectorController is destroyed until callback is run.
        m_agent->didGetFileSystemPath(fileSystem->root(), m_type, m_origin);
    }

    void didReadMetadata(const FileMetadata&)
    {
        ASSERT_NOT_REACHED();
    }

    void didReadDirectoryEntry(const String&, bool)
    {
        ASSERT_NOT_REACHED();
    }

    void didReadDirectoryEntries(bool)
    {
        ASSERT_NOT_REACHED();
    }

    void didCreateFileWriter(PassOwnPtr<AsyncFileWriter>, long long)
    {
        ASSERT_NOT_REACHED();
    }
    
    void didFail(int)
    {
        // FIXME: Is it useful to give back the code to Inspector UI?
        m_agent->didGetFileSystemError(m_type, m_origin);
    }

private:
    RefPtr<InspectorFileSystemAgent> m_agent;
    AsyncFileSystem::Type m_type;
    String m_origin;
};

InspectorFileSystemAgent::InspectorFileSystemAgent(InspectorController* inspectorController, InspectorFrontend* frontend)
    : m_inspectorController(inspectorController)
    , m_frontend(frontend)
{
}

InspectorFileSystemAgent::~InspectorFileSystemAgent() { }

void InspectorFileSystemAgent::stop()
{
    m_inspectorController = 0;
}

#if PLATFORM(CHROMIUM)
void InspectorFileSystemAgent::revealFolderInOS(const String& path)
{
    // FIXME: Remove guard when revealFolderInOS is implemented for non-chromium platforms.
    WebCore::revealFolderInOS(path);
}
#else
void InspectorFileSystemAgent::revealFolderInOS(const String&)
{
    notImplemented();
}
#endif

void InspectorFileSystemAgent::getFileSystemPathAsync(unsigned int type, const String& origin)
{
    if (!RuntimeEnabledFeatures::fileSystemEnabled()) {
        m_frontend->didGetFileSystemDisabled();
        return;
    }

    AsyncFileSystem::Type asyncFileSystemType = static_cast<AsyncFileSystem::Type>(type);
    Frame* mainFrame = m_inspectorController->inspectedPage()->mainFrame();
    for (Frame* frame = mainFrame; frame; frame = frame->tree()->traverseNext()) {
        Document* document = frame->document();
        if (document && document->securityOrigin()->toString() == origin) {
            LocalFileSystem::localFileSystem().readFileSystem(document, asyncFileSystemType, 0, new InspectorFileSystemAgentCallbacks(this, asyncFileSystemType, origin));
            return;
        }
    }
}

void InspectorFileSystemAgent::didGetFileSystemPath(const String& root, AsyncFileSystem::Type type, const String& origin)
{
    // When controller is being destroyed, this is set to 0. Agent can live even after m_inspectorController is destroyed.
    if (!m_inspectorController)
        return;

    m_frontend->didGetFileSystemPath(root, static_cast<unsigned int>(type), origin);
}

void InspectorFileSystemAgent::didGetFileSystemError(AsyncFileSystem::Type type, const String& origin)
{
    // When controller is being destroyed, this is set to 0. Agent can live even after m_inspectorController is destroyed.
    if (!m_inspectorController)
        return;
    m_frontend->didGetFileSystemError(static_cast<unsigned int>(type), origin);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(FILE_SYSTEM)
