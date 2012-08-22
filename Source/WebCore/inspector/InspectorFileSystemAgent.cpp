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
#include "DOMImplementation.h"
#include "DirectoryEntry.h"
#include "DirectoryReader.h"
#include "Document.h"
#include "EntriesCallback.h"
#include "EntryArray.h"
#include "EntryCallback.h"
#include "ErrorCallback.h"
#include "File.h"
#include "FileCallback.h"
#include "FileEntry.h"
#include "FileError.h"
#include "FileReader.h"
#include "FileSystemCallback.h"
#include "FileSystemCallbacks.h"
#include "Frame.h"
#include "InspectorPageAgent.h"
#include "InspectorState.h"
#include "InstrumentingAgents.h"
#include "KURL.h"
#include "LocalFileSystem.h"
#include "MIMETypeRegistry.h"
#include "Metadata.h"
#include "MetadataCallback.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "TextEncoding.h"
#include "TextResourceDecoder.h"
#include "VoidCallback.h"
#include <wtf/text/Base64.h>

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

template<typename BaseCallback, typename Handler, typename Argument>
class CallbackDispatcher : public BaseCallback {
public:
    typedef bool (Handler::*HandlingMethod)(Argument*);

    static PassRefPtr<CallbackDispatcher> create(PassRefPtr<Handler> handler, HandlingMethod handlingMethod)
    {
        return adoptRef(new CallbackDispatcher(handler, handlingMethod));
    }

    virtual bool handleEvent(Argument* argument) OVERRIDE
    {
        return (m_handler.get()->*m_handlingMethod)(argument);
    }

private:
    CallbackDispatcher(PassRefPtr<Handler> handler, HandlingMethod handlingMethod)
        : m_handler(handler)
        , m_handlingMethod(handlingMethod) { }

    RefPtr<Handler> m_handler;
    HandlingMethod m_handlingMethod;
};

template<typename BaseCallback>
class CallbackDispatcherFactory {
public:
    template<typename Handler, typename Argument>
    static PassRefPtr<CallbackDispatcher<BaseCallback, Handler, Argument> > create(Handler* handler, bool (Handler::*handlingMethod)(Argument*))
    {
        return CallbackDispatcher<BaseCallback, Handler, Argument>::create(PassRefPtr<Handler>(handler), handlingMethod);
    }
};

class ReportErrorTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<ReportErrorTask> create(PassRefPtr<ErrorCallback> errorCallback, FileError::ErrorCode errorCode)
    {
        return adoptPtr(new ReportErrorTask(errorCallback, errorCode));
    }

    virtual void performTask(ScriptExecutionContext*) OVERRIDE
    {
        m_errorCallback->handleEvent(FileError::create(m_errorCode).get());
    }

private:
    ReportErrorTask(PassRefPtr<ErrorCallback> errorCallback, FileError::ErrorCode errorCode)
        : m_errorCallback(errorCallback)
        , m_errorCode(errorCode) { }

    RefPtr<ErrorCallback> m_errorCallback;
    FileError::ErrorCode m_errorCode;
};

class FileSystemRootRequest : public RefCounted<FileSystemRootRequest> {
    WTF_MAKE_NONCOPYABLE(FileSystemRootRequest);
public:
    static PassRefPtr<FileSystemRootRequest> create(PassRefPtr<FrontendProvider> frontendProvider, int requestId, const String& type)
    {
        return adoptRef(new FileSystemRootRequest(frontendProvider, requestId, type));
    }

    void start(ScriptExecutionContext*);

private:
    bool didHitError(FileError* error)
    {
        reportResult(error->code());
        return true;
    }

    bool didGetEntry(Entry*);

    void reportResult(FileError::ErrorCode errorCode, PassRefPtr<TypeBuilder::FileSystem::Entry> entry = 0)
    {
        if (!m_frontendProvider || !m_frontendProvider->frontend())
            return;
        m_frontendProvider->frontend()->fileSystemRootReceived(m_requestId, static_cast<int>(errorCode), entry);
        m_frontendProvider = 0;
    }

    FileSystemRootRequest(PassRefPtr<FrontendProvider> frontendProvider, int requestId, const String& type)
        : m_frontendProvider(frontendProvider)
        , m_requestId(requestId)
        , m_type(type) { }

    RefPtr<FrontendProvider> m_frontendProvider;
    int m_requestId;
    String m_type;
};

void FileSystemRootRequest::start(ScriptExecutionContext* scriptExecutionContext)
{
    ASSERT(scriptExecutionContext);

    RefPtr<ErrorCallback> errorCallback = CallbackDispatcherFactory<ErrorCallback>::create(this, &FileSystemRootRequest::didHitError);
    FileSystemType type;
    if (m_type == DOMFileSystemBase::persistentPathPrefix)
        type = FileSystemTypePersistent;
    else if (m_type == DOMFileSystemBase::temporaryPathPrefix)
        type = FileSystemTypeTemporary;
    else {
        scriptExecutionContext->postTask(ReportErrorTask::create(errorCallback, FileError::SYNTAX_ERR));
        return;
    }

    RefPtr<EntryCallback> successCallback = CallbackDispatcherFactory<EntryCallback>::create(this, &FileSystemRootRequest::didGetEntry);
    OwnPtr<ResolveURICallbacks> fileSystemCallbacks = ResolveURICallbacks::create(successCallback, errorCallback, scriptExecutionContext, type, "/");

    LocalFileSystem::localFileSystem().readFileSystem(scriptExecutionContext, type, fileSystemCallbacks.release());
}

bool FileSystemRootRequest::didGetEntry(Entry* entry)
{
    RefPtr<TypeBuilder::FileSystem::Entry> result = TypeBuilder::FileSystem::Entry::create()
        .setUrl(entry->toURL())
        .setName("/")
        .setIsDirectory(true);
    reportResult(static_cast<FileError::ErrorCode>(0), result);
    return true;
}

class DirectoryContentRequest : public RefCounted<DirectoryContentRequest> {
    WTF_MAKE_NONCOPYABLE(DirectoryContentRequest);
public:
    static PassRefPtr<DirectoryContentRequest> create(PassRefPtr<FrontendProvider> frontendProvider, int requestId, const String& url)
    {
        return adoptRef(new DirectoryContentRequest(frontendProvider, requestId, url));
    }

    virtual ~DirectoryContentRequest()
    {
        reportResult(FileError::ABORT_ERR);
    }

    void start(ScriptExecutionContext*);

private:
    bool didHitError(FileError* error)
    {
        reportResult(error->code());
        return true;
    }

    bool didGetEntry(Entry*);
    bool didReadDirectoryEntries(EntryArray*);

    void reportResult(FileError::ErrorCode errorCode, PassRefPtr<Array<TypeBuilder::FileSystem::Entry> > entries = 0)
    {
        if (!m_frontendProvider || !m_frontendProvider->frontend())
            return;
        m_frontendProvider->frontend()->directoryContentReceived(m_requestId, static_cast<int>(errorCode), entries);
        m_frontendProvider = 0;
    }

    DirectoryContentRequest(PassRefPtr<FrontendProvider> frontendProvider, int requestId, const String& url)
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

void DirectoryContentRequest::start(ScriptExecutionContext* scriptExecutionContext)
{
    ASSERT(scriptExecutionContext);

    RefPtr<ErrorCallback> errorCallback = CallbackDispatcherFactory<ErrorCallback>::create(this, &DirectoryContentRequest::didHitError);
    FileSystemType type;
    String path;
    if (!DOMFileSystemBase::crackFileSystemURL(m_url, type, path)) {
        scriptExecutionContext->postTask(ReportErrorTask::create(errorCallback, FileError::SYNTAX_ERR));
        return;
    }

    RefPtr<EntryCallback> successCallback = CallbackDispatcherFactory<EntryCallback>::create(this, &DirectoryContentRequest::didGetEntry);
    OwnPtr<ResolveURICallbacks> fileSystemCallbacks = ResolveURICallbacks::create(successCallback, errorCallback, scriptExecutionContext, type, path);

    LocalFileSystem::localFileSystem().readFileSystem(scriptExecutionContext, type, fileSystemCallbacks.release());
}

bool DirectoryContentRequest::didGetEntry(Entry* entry)
{
    if (!entry->isDirectory()) {
        reportResult(FileError::TYPE_MISMATCH_ERR);
        return true;
    }

    m_directoryReader = static_cast<DirectoryEntry*>(entry)->createReader();
    m_entries = Array<TypeBuilder::FileSystem::Entry>::create();
    readDirectoryEntries();
    return true;
}

void DirectoryContentRequest::readDirectoryEntries()
{
    if (!m_directoryReader->filesystem()->scriptExecutionContext()) {
        reportResult(FileError::ABORT_ERR);
        return;
    }

    RefPtr<EntriesCallback> successCallback = CallbackDispatcherFactory<EntriesCallback>::create(this, &DirectoryContentRequest::didReadDirectoryEntries);
    RefPtr<ErrorCallback> errorCallback = CallbackDispatcherFactory<ErrorCallback>::create(this, &DirectoryContentRequest::didHitError);
    m_directoryReader->readEntries(successCallback, errorCallback);
}

bool DirectoryContentRequest::didReadDirectoryEntries(EntryArray* entries)
{
    if (!entries->length()) {
        reportResult(static_cast<FileError::ErrorCode>(0), m_entries);
        return true;
    }

    for (unsigned i = 0; i < entries->length(); ++i) {
        Entry* entry = entries->item(i);
        RefPtr<TypeBuilder::FileSystem::Entry> entryForFrontend = TypeBuilder::FileSystem::Entry::create()
            .setUrl(entry->toURL())
            .setName(entry->name())
            .setIsDirectory(entry->isDirectory());

        using TypeBuilder::Page::ResourceType;
        if (!entry->isDirectory()) {
            String mimeType = MIMETypeRegistry::getMIMETypeForPath(entry->name());
            ResourceType::Enum resourceType;
            if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType)) {
                resourceType = ResourceType::Image;
                entryForFrontend->setIsTextFile(false);
            } else if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType)) {
                resourceType = ResourceType::Script;
                entryForFrontend->setIsTextFile(true);
            } else if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType)) {
                resourceType = ResourceType::Document;
                entryForFrontend->setIsTextFile(true);
            } else {
                resourceType = ResourceType::Other;
                entryForFrontend->setIsTextFile(DOMImplementation::isXMLMIMEType(mimeType) || DOMImplementation::isTextMIMEType(mimeType));
            }

            entryForFrontend->setMimeType(mimeType);
            entryForFrontend->setResourceType(resourceType);
        }

        m_entries->addItem(entryForFrontend);
    }
    readDirectoryEntries();
    return true;
}

class MetadataRequest : public RefCounted<MetadataRequest> {
    WTF_MAKE_NONCOPYABLE(MetadataRequest);
public:
    static PassRefPtr<MetadataRequest> create(PassRefPtr<FrontendProvider> frontendProvider, int requestId, const String& url)
    {
        return adoptRef(new MetadataRequest(frontendProvider, requestId, url));
    }

    virtual ~MetadataRequest()
    {
        reportResult(FileError::ABORT_ERR);
    }

    void start(ScriptExecutionContext*);

private:
    bool didHitError(FileError* error)
    {
        reportResult(error->code());
        return true;
    }

    bool didGetEntry(Entry*);
    bool didGetMetadata(Metadata*);

    void reportResult(FileError::ErrorCode errorCode, PassRefPtr<TypeBuilder::FileSystem::Metadata> metadata = 0)
    {
        if (!m_frontendProvider || !m_frontendProvider->frontend())
            return;
        m_frontendProvider->frontend()->metadataReceived(m_requestId, static_cast<int>(errorCode), metadata);
        m_frontendProvider = 0;
    }

    MetadataRequest(PassRefPtr<FrontendProvider> frontendProvider, int requestId, const String& url)
        : m_frontendProvider(frontendProvider)
        , m_requestId(requestId)
        , m_url(ParsedURLString, url) { }

    RefPtr<FrontendProvider> m_frontendProvider;
    int m_requestId;
    KURL m_url;
    String m_path;
    bool m_isDirectory;
};

void MetadataRequest::start(ScriptExecutionContext* scriptExecutionContext)
{
    ASSERT(scriptExecutionContext);

    RefPtr<ErrorCallback> errorCallback = CallbackDispatcherFactory<ErrorCallback>::create(this, &MetadataRequest::didHitError);

    FileSystemType type;
    if (!DOMFileSystemBase::crackFileSystemURL(m_url, type, m_path)) {
        scriptExecutionContext->postTask(ReportErrorTask::create(errorCallback, FileError::SYNTAX_ERR));
        return;
    }

    RefPtr<EntryCallback> successCallback = CallbackDispatcherFactory<EntryCallback>::create(this, &MetadataRequest::didGetEntry);
    OwnPtr<ResolveURICallbacks> fileSystemCallbacks = ResolveURICallbacks::create(successCallback, errorCallback, scriptExecutionContext, type, m_path);
    LocalFileSystem::localFileSystem().readFileSystem(scriptExecutionContext, type, fileSystemCallbacks.release());
}

bool MetadataRequest::didGetEntry(Entry* entry)
{
    if (!entry->filesystem()->scriptExecutionContext()) {
        reportResult(FileError::ABORT_ERR);
        return true;
    }

    RefPtr<MetadataCallback> successCallback = CallbackDispatcherFactory<MetadataCallback>::create(this, &MetadataRequest::didGetMetadata);
    RefPtr<ErrorCallback> errorCallback = CallbackDispatcherFactory<ErrorCallback>::create(this, &MetadataRequest::didHitError);
    entry->getMetadata(successCallback, errorCallback);
    m_isDirectory = entry->isDirectory();
    return true;
}

bool MetadataRequest::didGetMetadata(Metadata* metadata)
{
    using TypeBuilder::FileSystem::Metadata;
    RefPtr<Metadata> result = Metadata::create()
        .setModificationTime(metadata->modificationTime())
        .setSize(metadata->size());
    reportResult(static_cast<FileError::ErrorCode>(0), result);
    return true;
}

class FileContentRequest : public EventListener {
    WTF_MAKE_NONCOPYABLE(FileContentRequest);
public:
    static PassRefPtr<FileContentRequest> create(PassRefPtr<FrontendProvider> frontendProvider, int requestId, const String& url, bool readAsText, long long start, long long end, const String& charset)
    {
        return adoptRef(new FileContentRequest(frontendProvider, requestId, url, readAsText, start, end, charset));
    }

    virtual ~FileContentRequest()
    {
        reportResult(FileError::ABORT_ERR);
    }

    void start(ScriptExecutionContext*);

    virtual bool operator==(const EventListener& other) OVERRIDE
    {
        return this == &other;
    }

    virtual void handleEvent(ScriptExecutionContext*, Event* event) OVERRIDE
    {
        if (event->type() == eventNames().loadEvent)
            didRead();
        else if (event->type() == eventNames().errorEvent)
            didHitError(m_reader->error().get());
    }

private:
    bool didHitError(FileError* error)
    {
        reportResult(error->code());
        return true;
    }

    bool didGetEntry(Entry*);
    bool didGetFile(File*);
    void didRead();

    void reportResult(FileError::ErrorCode errorCode, const String* result = 0, const String* charset = 0)
    {
        if (!m_frontendProvider || !m_frontendProvider->frontend())
            return;
        m_frontendProvider->frontend()->fileContentReceived(m_requestId, static_cast<int>(errorCode), result, charset);
        m_frontendProvider = 0;
    }

    FileContentRequest(PassRefPtr<FrontendProvider> frontendProvider, int requestId, const String& url, bool readAsText, long long start, long long end, const String& charset)
        : EventListener(EventListener::CPPEventListenerType)
        , m_frontendProvider(frontendProvider)
        , m_requestId(requestId)
        , m_url(ParsedURLString, url)
        , m_readAsText(readAsText)
        , m_start(start)
        , m_end(end)
        , m_charset(charset) { }

    RefPtr<FrontendProvider> m_frontendProvider;
    int m_requestId;
    KURL m_url;
    bool m_readAsText;
    int m_start;
    long long m_end;
    String m_mimeType;
    String m_charset;

    RefPtr<FileReader> m_reader;
};

void FileContentRequest::start(ScriptExecutionContext* scriptExecutionContext)
{
    ASSERT(scriptExecutionContext);

    RefPtr<ErrorCallback> errorCallback = CallbackDispatcherFactory<ErrorCallback>::create(this, &FileContentRequest::didHitError);

    FileSystemType type;
    String path;
    if (!DOMFileSystemBase::crackFileSystemURL(m_url, type, path)) {
        scriptExecutionContext->postTask(ReportErrorTask::create(errorCallback, FileError::SYNTAX_ERR));
        return;
    }

    RefPtr<EntryCallback> successCallback = CallbackDispatcherFactory<EntryCallback>::create(this, &FileContentRequest::didGetEntry);
    OwnPtr<ResolveURICallbacks> fileSystemCallbacks = ResolveURICallbacks::create(successCallback, errorCallback, scriptExecutionContext, type, path);

    LocalFileSystem::localFileSystem().readFileSystem(scriptExecutionContext, type, fileSystemCallbacks.release());
}

bool FileContentRequest::didGetEntry(Entry* entry)
{
    if (entry->isDirectory()) {
        reportResult(FileError::TYPE_MISMATCH_ERR);
        return true;
    }

    if (!entry->filesystem()->scriptExecutionContext()) {
        reportResult(FileError::ABORT_ERR);
        return true;
    }

    RefPtr<FileCallback> successCallback = CallbackDispatcherFactory<FileCallback>::create(this, &FileContentRequest::didGetFile);
    RefPtr<ErrorCallback> errorCallback = CallbackDispatcherFactory<ErrorCallback>::create(this, &FileContentRequest::didHitError);
    static_cast<FileEntry*>(entry)->file(successCallback, errorCallback);

    m_reader = FileReader::create(entry->filesystem()->scriptExecutionContext());
    m_mimeType = MIMETypeRegistry::getMIMETypeForPath(entry->name());

    return true;
}

bool FileContentRequest::didGetFile(File* file)
{
    RefPtr<Blob> blob = file->slice(m_start, m_end);
    m_reader->setOnload(this);
    m_reader->setOnerror(this);

    ExceptionCode ec = 0;
    m_reader->readAsArrayBuffer(blob.get(), ec);
    return true;
}

void FileContentRequest::didRead()
{
    RefPtr<ArrayBuffer> buffer = m_reader->arrayBufferResult();

    if (!m_readAsText) {
        String result = base64Encode(static_cast<char*>(buffer->data()), buffer->byteLength());
        reportResult(static_cast<FileError::ErrorCode>(0), &result, 0);
        return;
    }

    RefPtr<TextResourceDecoder> decoder = TextResourceDecoder::create(m_mimeType, m_charset, true);
    String result = decoder->decode(static_cast<char*>(buffer->data()), buffer->byteLength());
    result += decoder->flush();
    m_charset = decoder->encoding().domName();
    reportResult(static_cast<FileError::ErrorCode>(0), &result, &m_charset);
}

class DeleteEntryRequest : public VoidCallback {
public:
    static PassRefPtr<DeleteEntryRequest> create(PassRefPtr<FrontendProvider> frontendProvider, int requestId, const KURL& url)
    {
        return adoptRef(new DeleteEntryRequest(frontendProvider, requestId, url));
    }

    virtual ~DeleteEntryRequest()
    {
        reportResult(FileError::ABORT_ERR);
    }

    virtual bool handleEvent() OVERRIDE
    {
        return didDeleteEntry();
    }

    void start(ScriptExecutionContext*);

private:
    bool didHitError(FileError* error)
    {
        reportResult(error->code());
        return true;
    }

    bool didGetEntry(Entry*);
    bool didDeleteEntry();

    void reportResult(FileError::ErrorCode errorCode)
    {
        if (!m_frontendProvider || !m_frontendProvider->frontend())
            return;
        m_frontendProvider->frontend()->deletionCompleted(m_requestId, static_cast<int>(errorCode));
        m_frontendProvider = 0;
    }

    DeleteEntryRequest(PassRefPtr<FrontendProvider> frontendProvider, int requestId, const KURL& url)
        : m_frontendProvider(frontendProvider)
        , m_requestId(requestId)
        , m_url(url) { }

    RefPtr<FrontendProvider> m_frontendProvider;
    int m_requestId;
    FileSystemType m_type;
    KURL m_url;
};

void DeleteEntryRequest::start(ScriptExecutionContext* scriptExecutionContext)
{
    ASSERT(scriptExecutionContext);

    RefPtr<ErrorCallback> errorCallback = CallbackDispatcherFactory<ErrorCallback>::create(this, &DeleteEntryRequest::didHitError);

    FileSystemType type;
    String path;
    if (!DOMFileSystemBase::crackFileSystemURL(m_url, type, path)) {
        scriptExecutionContext->postTask(ReportErrorTask::create(errorCallback, FileError::SYNTAX_ERR));
        return;
    }

    if (path == "/") {
        OwnPtr<AsyncFileSystemCallbacks> fileSystemCallbacks = VoidCallbacks::create(this, errorCallback);
        LocalFileSystem::localFileSystem().deleteFileSystem(scriptExecutionContext, type, fileSystemCallbacks.release());
    } else {
        RefPtr<EntryCallback> successCallback = CallbackDispatcherFactory<EntryCallback>::create(this, &DeleteEntryRequest::didGetEntry);
        OwnPtr<ResolveURICallbacks> fileSystemCallbacks = ResolveURICallbacks::create(successCallback, errorCallback, scriptExecutionContext, type, path);
        LocalFileSystem::localFileSystem().readFileSystem(scriptExecutionContext, type, fileSystemCallbacks.release());
    }
}

bool DeleteEntryRequest::didGetEntry(Entry* entry)
{
    RefPtr<ErrorCallback> errorCallback = CallbackDispatcherFactory<ErrorCallback>::create(this, &DeleteEntryRequest::didHitError);
    if (entry->isDirectory()) {
        DirectoryEntry* directoryEntry = static_cast<DirectoryEntry*>(entry);
        directoryEntry->removeRecursively(this, errorCallback);
    } else
        entry->remove(this, errorCallback);
    return true;
}

bool DeleteEntryRequest::didDeleteEntry()
{
    reportResult(static_cast<FileError::ErrorCode>(0));
    return true;
}

} // anonymous namespace

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

void InspectorFileSystemAgent::requestFileSystemRoot(ErrorString* error, const String& origin, const String& type, int* requestId)
{
    if (!assertFrontend(error))
        return;

    ScriptExecutionContext* scriptExecutionContext = assertScriptExecutionContextForOrigin(error, SecurityOrigin::createFromString(origin).get());
    if (!scriptExecutionContext)
        return;

    *requestId = m_nextRequestId++;
    FileSystemRootRequest::create(m_frontendProvider, *requestId, type)->start(scriptExecutionContext);
}

void InspectorFileSystemAgent::requestDirectoryContent(ErrorString* error, const String& url, int* requestId)
{
    if (!assertFrontend(error))
        return;

    ScriptExecutionContext* scriptExecutionContext = assertScriptExecutionContextForOrigin(error, SecurityOrigin::createFromString(url).get());
    if (!scriptExecutionContext)
        return;

    *requestId = m_nextRequestId++;
    DirectoryContentRequest::create(m_frontendProvider, *requestId, url)->start(scriptExecutionContext);
}

void InspectorFileSystemAgent::requestMetadata(ErrorString* error, const String& url, int* requestId)
{
    if (!assertFrontend(error))
        return;

    ScriptExecutionContext* scriptExecutionContext = assertScriptExecutionContextForOrigin(error, SecurityOrigin::createFromString(url).get());
    if (!scriptExecutionContext)
        return;

    *requestId = m_nextRequestId++;
    MetadataRequest::create(m_frontendProvider, *requestId, url)->start(scriptExecutionContext);
}

void InspectorFileSystemAgent::requestFileContent(ErrorString* error, const String& url, bool readAsText, const int* start, const int* end, const String* charset, int* requestId)
{
    if (!assertFrontend(error))
        return;

    ScriptExecutionContext* scriptExecutionContext = assertScriptExecutionContextForOrigin(error, SecurityOrigin::createFromString(url).get());
    if (!scriptExecutionContext)
        return;

    *requestId = m_nextRequestId++;

    long long startPosition = start ? *start : 0;
    long long endPosition = end ? *end : std::numeric_limits<long long>::max();
    FileContentRequest::create(m_frontendProvider, *requestId, url, readAsText, startPosition, endPosition, charset ? *charset : "")->start(scriptExecutionContext);
}

void InspectorFileSystemAgent::deleteEntry(ErrorString* error, const String& urlString, int* requestId)
{
    if (!assertFrontend(error))
        return;

    KURL url(ParsedURLString, urlString);

    ScriptExecutionContext* scriptExecutionContext = assertScriptExecutionContextForOrigin(error, SecurityOrigin::create(url).get());
    if (!scriptExecutionContext)
        return;

    *requestId = m_nextRequestId++;
    DeleteEntryRequest::create(m_frontendProvider, *requestId, url)->start(scriptExecutionContext);
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
    , m_nextRequestId(1)
{
    ASSERT(instrumentingAgents);
    ASSERT(state);
    ASSERT(m_pageAgent);
    m_instrumentingAgents->setInspectorFileSystemAgent(this);
}

bool InspectorFileSystemAgent::assertFrontend(ErrorString* error)
{
    if (!m_enabled || !m_frontendProvider) {
        *error = "FileSystem agent is not enabled.";
        return false;
    }
    ASSERT(m_frontendProvider->frontend());
    return true;
}

ScriptExecutionContext* InspectorFileSystemAgent::assertScriptExecutionContextForOrigin(ErrorString* error, SecurityOrigin* origin)
{
    for (Frame* frame = m_pageAgent->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->document() && frame->document()->securityOrigin()->isSameSchemeHostPort(origin))
            return frame->document();
    }

    *error = "No frame is available for the request";
    return 0;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(FILE_SYSTEM)
