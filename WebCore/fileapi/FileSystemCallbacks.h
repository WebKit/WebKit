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

#ifndef FileSystemCallbacks_h
#define FileSystemCallbacks_h

#if ENABLE(FILE_SYSTEM)

#include "AsyncFileSystemCallbacks.h"
#include "PlatformString.h"
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class AsyncFileWriter;
class DOMFileSystem;
class ErrorCallback;
class EntriesCallback;
class EntryArray;
class EntryCallback;
class FileSystemCallback;
class FileWriter;
class FileWriterCallback;
class MetadataCallback;
class ScriptExecutionContext;
class VoidCallback;

class FileSystemCallbacksBase : public AsyncFileSystemCallbacks {
public:
    virtual ~FileSystemCallbacksBase();

    // For EntryCallbacks and VoidCallbacks.
    virtual void didSucceed();

    // For FileSystemCallbacks.
    virtual void didOpenFileSystem(const String& name, PassOwnPtr<AsyncFileSystem>);

    // For MetadataCallbacks.
    virtual void didReadMetadata(double modificationTime);

    // For EntriesCallbacks. didReadDirectoryEntry is called each time the API reads an entry, and didReadDirectoryDone is called when a chunk of entries have been read (i.e. good time to call back to the application).  If hasMore is true there can be more chunks.
    virtual void didReadDirectoryEntry(const String& name, bool isDirectory);
    virtual void didReadDirectoryEntries(bool hasMore);

    // For createFileWriter.
    virtual void didCreateFileWriter(PassOwnPtr<AsyncFileWriter>, long long length);

    // For ErrorCallback.
    virtual void didFail(int code);

protected:
    FileSystemCallbacksBase(PassRefPtr<ErrorCallback> errorCallback);
    RefPtr<ErrorCallback> m_errorCallback;
};

// Subclasses ----------------------------------------------------------------

class EntryCallbacks : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<EntryCallbacks> create(PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>, DOMFileSystem*, const String& expectedPath, bool isDirectory);
    virtual void didSucceed();

private:
    EntryCallbacks(PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>, DOMFileSystem*, const String& expectedPath, bool isDirectory);
    RefPtr<EntryCallback> m_successCallback;
    DOMFileSystem* m_fileSystem;
    String m_expectedPath;
    bool m_isDirectory;
};

class EntriesCallbacks : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<EntriesCallbacks> create(PassRefPtr<EntriesCallback>, PassRefPtr<ErrorCallback>, DOMFileSystem*, const String& basePath);
    virtual void didReadDirectoryEntry(const String& name, bool isDirectory);
    virtual void didReadDirectoryEntries(bool hasMore);

private:
    EntriesCallbacks(PassRefPtr<EntriesCallback>, PassRefPtr<ErrorCallback>, DOMFileSystem*, const String& basePath);
    RefPtr<EntriesCallback> m_successCallback;
    DOMFileSystem* m_fileSystem;
    String m_basePath;
    RefPtr<EntryArray> m_entries;
};

class FileSystemCallbacks : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<FileSystemCallbacks> create(PassRefPtr<FileSystemCallback>, PassRefPtr<ErrorCallback>, ScriptExecutionContext*);
    virtual void didOpenFileSystem(const String& name, PassOwnPtr<AsyncFileSystem>);

private:
    FileSystemCallbacks(PassRefPtr<FileSystemCallback>, PassRefPtr<ErrorCallback>, ScriptExecutionContext*);
    RefPtr<FileSystemCallback> m_successCallback;
    RefPtr<ScriptExecutionContext> m_scriptExecutionContext;
};

class MetadataCallbacks : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<MetadataCallbacks> create(PassRefPtr<MetadataCallback>, PassRefPtr<ErrorCallback>);
    virtual void didReadMetadata(double modificationTime);

private:
    MetadataCallbacks(PassRefPtr<MetadataCallback>, PassRefPtr<ErrorCallback>);
    RefPtr<MetadataCallback> m_successCallback;
};

class FileWriterCallbacks : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<FileWriterCallbacks> create(PassRefPtr<FileWriter>, PassRefPtr<FileWriterCallback>, PassRefPtr<ErrorCallback>);
    virtual void didCreateFileWriter(PassOwnPtr<AsyncFileWriter>, long long length);

private:
    FileWriterCallbacks(PassRefPtr<FileWriter>, PassRefPtr<FileWriterCallback>, PassRefPtr<ErrorCallback>);
    RefPtr<FileWriter> m_fileWriter;
    RefPtr<FileWriterCallback> m_successCallback;
};

class VoidCallbacks : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<VoidCallbacks> create(PassRefPtr<VoidCallback>, PassRefPtr<ErrorCallback>);
    virtual void didSucceed();

private:
    VoidCallbacks(PassRefPtr<VoidCallback>, PassRefPtr<ErrorCallback>);
    RefPtr<VoidCallback> m_successCallback;
};

} // namespace

#endif // ENABLE(FILE_SYSTEM)

#endif // FileSystemCallbacks_h
