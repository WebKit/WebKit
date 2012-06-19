/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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

#if ENABLE(INSPECTOR) && ENABLE(FILE_SYSTEM)

#include "InspectorFileSystemAgent.h"

#include "DOMFileSystem.h"
#include "DirectoryEntry.h"
#include "DirectoryReader.h"
#include "Document.h"
#include "EntriesCallback.h"
#include "EntryArray.h"
#include "EntryCallback.h"
#include "ErrorCallback.h"
#include "FileError.h"
#include "FileSystemCallback.h"
#include "FileSystemCallbacks.h"
#include "Frame.h"
#include "InspectorPageAgent.h"
#include "InspectorState.h"
#include "InstrumentingAgents.h"
#include "KURL.h"
#include "LocalFileSystem.h"
#include "MIMETypeRegistry.h"
#include "SecurityOrigin.h"

using WebCore::TypeBuilder::Array;

namespace WebCore {

namespace FileSystemAgentState {
static const char fileSystemAgentEnabled[] = "fileSystemAgentEnabled";
}

class InspectorFileSystemAgent::FrontendProvider : public RefCounted<FrontendProvider> {
    WTF_MAKE_NONCOPYABLE(FrontendProvider);
public:
    static PassRefPtr<FrontendProvider> create(InspectorFileSystemAgent* agent, InspectorFrontend::FileSystem* frontend)
    {
        return adoptRef(new FrontendProvider(agent, frontend));
    }

    InspectorFrontend::FileSystem* frontend() const
    {
        if (m_agent && m_agent->m_enabled)
            return m_frontend;
        return 0;
    }

    void clear()
    {
        m_agent = 0;
        m_frontend = 0;
    }

private:
    FrontendProvider(InspectorFileSystemAgent* agent, InspectorFrontend::FileSystem* frontend)
        : m_agent(agent)
        , m_frontend(frontend) { }

    InspectorFileSystemAgent* m_agent;
    InspectorFrontend::FileSystem* m_frontend;
};

typedef InspectorFileSystemAgent::FrontendProvider FrontendProvider;

namespace {

class GetFileSystemRootTask : public RefCounted<GetFileSystemRootTask> {
    WTF_MAKE_NONCOPYABLE(GetFileSystemRootTask);
public:
    static PassRefPtr<GetFileSystemRootTask> create(PassRefPtr<FrontendProvider> frontendProvider, int requestId, const String& type)
    {
        return adoptRef(new GetFileSystemRootTask(frontendProvider, requestId, type));
    }

    void start(ScriptExecutionContext*);

private:
    class ErrorCallback;
    class GetEntryCallback;

    void gotEntry(Entry*);

    void reportResult(FileError::ErrorCode errorCode, PassRefPtr<TypeBuilder::FileSystem::Entry> entry)
    {
        if (!m_frontendProvider || !m_frontendProvider->frontend())
            return;
        m_frontendProvider->frontend()->gotFileSystemRoot(m_requestId, static_cast<int>(errorCode), entry);
        m_frontendProvider = 0;
    }

    GetFileSystemRootTask(PassRefPtr<FrontendProvider> frontendProvider, int requestId, const String& type)
        : m_frontendProvider(frontendProvider)
        , m_requestId(requestId)
        , m_type(type) { }

    RefPtr<FrontendProvider> m_frontendProvider;
    int m_requestId;
    String m_type;
};

class GetFileSystemRootTask::ErrorCallback : public WebCore::ErrorCallback {
    WTF_MAKE_NONCOPYABLE(ErrorCallback);
public:
    static PassRefPtr<GetFileSystemRootTask::ErrorCallback> create(PassRefPtr<GetFileSystemRootTask> getFileSystemRootTask)
    {
        return adoptRef(new GetFileSystemRootTask::ErrorCallback(getFileSystemRootTask));
    }

    virtual bool handleEvent(FileError* error) OVERRIDE
    {
        m_getFileSystemRootTask->reportResult(error->code(), 0);
        return true;
    }

private:
    ErrorCallback(PassRefPtr<GetFileSystemRootTask> getFileSystemRootTask)
        : m_getFileSystemRootTask(getFileSystemRootTask) { }
    RefPtr<GetFileSystemRootTask> m_getFileSystemRootTask;
};

class GetFileSystemRootTask::GetEntryCallback : public WebCore::EntryCallback {
    WTF_MAKE_NONCOPYABLE(GetEntryCallback);
public:
    static PassRefPtr<GetFileSystemRootTask::GetEntryCallback> create(PassRefPtr<GetFileSystemRootTask> getFileSystemRootTask)
    {
        return adoptRef(new GetFileSystemRootTask::GetEntryCallback(getFileSystemRootTask));
    }

    virtual bool handleEvent(Entry* entry) OVERRIDE
    {
        m_getFileSystemRootTask->gotEntry(entry);
        return true;
    }

private:
    GetEntryCallback(PassRefPtr<GetFileSystemRootTask> getFileSystemRootTask)
        : m_getFileSystemRootTask(getFileSystemRootTask) { }
    RefPtr<GetFileSystemRootTask> m_getFileSystemRootTask;
};

void GetFileSystemRootTask::start(ScriptExecutionContext* scriptExecutionContext)
{
    FileSystemType type;
    if (m_type == DOMFileSystemBase::persistentPathPrefix)
        type = FileSystemTypePersistent;
    else if (m_type == DOMFileSystemBase::temporaryPathPrefix)
        type = FileSystemTypeTemporary;
    else {
        reportResult(FileError::SYNTAX_ERR, 0);
        return;
    }

    RefPtr<EntryCallback> successCallback = GetFileSystemRootTask::GetEntryCallback::create(this);
    RefPtr<ErrorCallback> errorCallback = GetFileSystemRootTask::ErrorCallback::create(this);
    OwnPtr<ResolveURICallbacks> fileSystemCallbacks = ResolveURICallbacks::create(successCallback, errorCallback, scriptExecutionContext, type, "/");

    LocalFileSystem::localFileSystem().readFileSystem(scriptExecutionContext, type, fileSystemCallbacks.release());
}

void GetFileSystemRootTask::gotEntry(Entry* entry)
{
    RefPtr<TypeBuilder::FileSystem::Entry> result(TypeBuilder::FileSystem::Entry::create().setUrl(entry->toURL()).setName("/").setIsDirectory(true));
    reportResult(static_cast<FileError::ErrorCode>(0), result);
}

class ReadDirectoryTask : public RefCounted<ReadDirectoryTask> {
    WTF_MAKE_NONCOPYABLE(ReadDirectoryTask);
public:
    static PassRefPtr<ReadDirectoryTask> create(PassRefPtr<FrontendProvider> frontendProvider, int requestId, const String& url)
    {
        return adoptRef(new ReadDirectoryTask(frontendProvider, requestId, url));
    }

    virtual ~ReadDirectoryTask()
    {
        reportResult(FileError::ABORT_ERR, 0);
    }

    void start(ScriptExecutionContext*);

private:
    class ErrorCallback;
    class GetEntryCallback;
    class ReadDirectoryEntriesCallback;

    void gotEntry(Entry*);
    void didReadDirectoryEntries(EntryArray*);

    void reportResult(FileError::ErrorCode errorCode, PassRefPtr<Array<TypeBuilder::FileSystem::Entry> > entries)
    {
        if (!m_frontendProvider || !m_frontendProvider->frontend())
            return;
        m_frontendProvider->frontend()->didReadDirectory(m_requestId, static_cast<int>(errorCode), entries);
        m_frontendProvider = 0;
    }

    ReadDirectoryTask(PassRefPtr<FrontendProvider> frontendProvider, int requestId, const String& url)
        : m_frontendProvider(frontendProvider)
        , m_requestId(requestId)
        , m_url(ParsedURLString, url) { }

    void readDirectoryEntries();

    RefPtr<FrontendProvider> m_frontendProvider;
    int m_requestId;
    KURL m_url;
    RefPtr<Array<TypeBuilder::FileSystem::Entry> > m_entries;
    RefPtr<DirectoryReader> m_directoryReader;
};

class ReadDirectoryTask::ErrorCallback : public WebCore::ErrorCallback {
    WTF_MAKE_NONCOPYABLE(ErrorCallback);
public:
    static PassRefPtr<ReadDirectoryTask::ErrorCallback> create(PassRefPtr<ReadDirectoryTask> readDirectoryTask)
    {
        return adoptRef(new ReadDirectoryTask::ErrorCallback(readDirectoryTask));
    }

    virtual bool handleEvent(FileError* error) OVERRIDE
    {
        if (m_readDirectoryTask)
            m_readDirectoryTask->reportResult(error->code(), 0);
        return true;
    }

private:
    ErrorCallback(PassRefPtr<ReadDirectoryTask> readDirectoryTask)
        : m_readDirectoryTask(readDirectoryTask) { }
    RefPtr<ReadDirectoryTask> m_readDirectoryTask;
};

class ReadDirectoryTask::GetEntryCallback : public EntryCallback {
    WTF_MAKE_NONCOPYABLE(GetEntryCallback);
public:
    static PassRefPtr<ReadDirectoryTask::GetEntryCallback> create(PassRefPtr<ReadDirectoryTask> readDirectoryTask)
    {
        return adoptRef(new ReadDirectoryTask::GetEntryCallback(readDirectoryTask));
    }

    virtual bool handleEvent(Entry* fileSystem) OVERRIDE
    {
        m_readDirectoryTask->gotEntry(fileSystem);
        return true;
    }

private:
    GetEntryCallback(PassRefPtr<ReadDirectoryTask> readDirectoryTask)
        : m_readDirectoryTask(readDirectoryTask) { }
    RefPtr<ReadDirectoryTask> m_readDirectoryTask;
};

class ReadDirectoryTask::ReadDirectoryEntriesCallback : public EntriesCallback {
    WTF_MAKE_NONCOPYABLE(ReadDirectoryEntriesCallback);
public:
    static PassRefPtr<ReadDirectoryTask::ReadDirectoryEntriesCallback> create(PassRefPtr<ReadDirectoryTask> readDirectoryTask)
    {
        return adoptRef(new ReadDirectoryTask::ReadDirectoryEntriesCallback(readDirectoryTask));
    }

    virtual bool handleEvent(EntryArray* entries) OVERRIDE
    {
        ASSERT(entries);
        m_readDirectoryTask->didReadDirectoryEntries(entries);
        return true;
    }

private:
    ReadDirectoryEntriesCallback(PassRefPtr<ReadDirectoryTask> readDirectoryTask)
        : m_readDirectoryTask(readDirectoryTask) { }
    RefPtr<ReadDirectoryTask> m_readDirectoryTask;
};

void ReadDirectoryTask::start(ScriptExecutionContext* scriptExecutionContext)
{
    ASSERT(scriptExecutionContext);

    FileSystemType type;
    String path;
    if (!DOMFileSystemBase::crackFileSystemURL(m_url, type, path)) {
        reportResult(FileError::SYNTAX_ERR, 0);
        return;
    }

    RefPtr<EntryCallback> successCallback = ReadDirectoryTask::GetEntryCallback::create(this);
    RefPtr<ErrorCallback> errorCallback = ReadDirectoryTask::ErrorCallback::create(this);
    OwnPtr<ResolveURICallbacks> fileSystemCallbacks = ResolveURICallbacks::create(successCallback, errorCallback, scriptExecutionContext, type, path);

    LocalFileSystem::localFileSystem().readFileSystem(scriptExecutionContext, type, fileSystemCallbacks.release());
}

void ReadDirectoryTask::gotEntry(Entry* entry)
{
    if (!entry->isDirectory()) {
        reportResult(FileError::TYPE_MISMATCH_ERR, 0);
        return;
    }

    m_directoryReader = static_cast<DirectoryEntry*>(entry)->createReader();
    m_entries = Array<TypeBuilder::FileSystem::Entry>::create();
    readDirectoryEntries();
}

void ReadDirectoryTask::readDirectoryEntries()
{
    if (!m_directoryReader->filesystem()->scriptExecutionContext()) {
        reportResult(FileError::ABORT_ERR, 0);
        return;
    }

    RefPtr<EntriesCallback> successCallback = ReadDirectoryTask::ReadDirectoryEntriesCallback::create(this);
    RefPtr<ErrorCallback> errorCallback = ReadDirectoryTask::ErrorCallback::create(this);
    m_directoryReader->readEntries(successCallback, errorCallback);
}

void ReadDirectoryTask::didReadDirectoryEntries(EntryArray* entries)
{
    if (!entries->length()) {
        reportResult(static_cast<FileError::ErrorCode>(0), m_entries);
        return;
    }

    for (unsigned i = 0; i < entries->length(); ++i) {
        Entry* entry = entries->item(i);
        RefPtr<TypeBuilder::FileSystem::Entry> entryForFrontend = TypeBuilder::FileSystem::Entry::create().setUrl(entry->toURL()).setName(entry->name()).setIsDirectory(entry->isDirectory());

        using TypeBuilder::Page::ResourceType;
        if (!entry->isDirectory()) {
            String mimeType = MIMETypeRegistry::getMIMETypeForPath(entry->name());
            ResourceType::Enum resourceType;
            if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
                resourceType = ResourceType::Image;
            else if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType))
                resourceType = ResourceType::Script;
            else if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType))
                resourceType = ResourceType::Document;
            else
                resourceType = ResourceType::Other;

            entryForFrontend->setMimeType(mimeType);
            entryForFrontend->setResourceType(resourceType);
        }

        m_entries->addItem(entryForFrontend);
    }
    readDirectoryEntries();
}

}

// static
PassOwnPtr<InspectorFileSystemAgent> InspectorFileSystemAgent::create(InstrumentingAgents* instrumentingAgents, InspectorPageAgent* pageAgent, InspectorState* state)
{
    return adoptPtr(new InspectorFileSystemAgent(instrumentingAgents, pageAgent, state));
}

InspectorFileSystemAgent::~InspectorFileSystemAgent()
{
    if (m_frontendProvider)
        m_frontendProvider->clear();
    m_instrumentingAgents->setInspectorFileSystemAgent(0);
}

void InspectorFileSystemAgent::enable(ErrorString*)
{
    if (m_enabled)
        return;
    m_enabled = true;
    m_state->setBoolean(FileSystemAgentState::fileSystemAgentEnabled, m_enabled);
}

void InspectorFileSystemAgent::disable(ErrorString*)
{
    if (!m_enabled)
        return;
    m_enabled = false;
    m_state->setBoolean(FileSystemAgentState::fileSystemAgentEnabled, m_enabled);
}

void InspectorFileSystemAgent::getFileSystemRoot(ErrorString*, int requestId, const String& origin, const String& type)
{
    if (!m_enabled || !m_frontendProvider)
        return;
    ASSERT(m_frontendProvider->frontend());

    if (ScriptExecutionContext* scriptExecutionContext = scriptExecutionContextForOrigin(SecurityOrigin::createFromString(origin).get()))
        GetFileSystemRootTask::create(m_frontendProvider, requestId, type)->start(scriptExecutionContext);
    else
        m_frontendProvider->frontend()->gotFileSystemRoot(requestId, static_cast<int>(FileError::ABORT_ERR), 0);
}

void InspectorFileSystemAgent::readDirectory(ErrorString*, int requestId, const String& url)
{
    if (!m_enabled || !m_frontendProvider)
        return;
    ASSERT(m_frontendProvider->frontend());

    if (ScriptExecutionContext* scriptExecutionContext = scriptExecutionContextForOrigin(SecurityOrigin::createFromString(url).get()))
        ReadDirectoryTask::create(m_frontendProvider, requestId, url)->start(scriptExecutionContext);
    else
        m_frontendProvider->frontend()->didReadDirectory(requestId, static_cast<int>(FileError::ABORT_ERR), 0);
}

void InspectorFileSystemAgent::setFrontend(InspectorFrontend* frontend)
{
    ASSERT(frontend);
    m_frontendProvider = FrontendProvider::create(this, frontend->filesystem());
}

void InspectorFileSystemAgent::clearFrontend()
{
    if (m_frontendProvider) {
        m_frontendProvider->clear();
        m_frontendProvider = 0;
    }
    m_enabled = false;
    m_state->setBoolean(FileSystemAgentState::fileSystemAgentEnabled, m_enabled);
}

void InspectorFileSystemAgent::restore()
{
    m_enabled = m_state->getBoolean(FileSystemAgentState::fileSystemAgentEnabled);
}

InspectorFileSystemAgent::InspectorFileSystemAgent(InstrumentingAgents* instrumentingAgents, InspectorPageAgent* pageAgent, InspectorState* state)
    : InspectorBaseAgent<InspectorFileSystemAgent>("FileSystem", instrumentingAgents, state)
    , m_pageAgent(pageAgent)
    , m_enabled(false)
{
    ASSERT(instrumentingAgents);
    ASSERT(state);
    ASSERT(m_pageAgent);
    m_instrumentingAgents->setInspectorFileSystemAgent(this);
}

ScriptExecutionContext* InspectorFileSystemAgent::scriptExecutionContextForOrigin(SecurityOrigin* origin)
{
    for (Frame* frame = m_pageAgent->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->document() && frame->document()->securityOrigin()->isSameSchemeHostPort(origin))
            return frame->document();
    }
    return 0;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(FILE_SYSTEM)
