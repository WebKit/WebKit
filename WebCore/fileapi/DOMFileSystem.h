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

#ifndef DOMFileSystem_h
#define DOMFileSystem_h

#if ENABLE(FILE_SYSTEM)

#include "ActiveDOMObject.h"
#include "AsyncFileSystem.h"
#include "Flags.h"
#include "PlatformString.h"
#include "ScriptExecutionContext.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class DirectoryEntry;
class DirectoryReader;
class Entry;
class EntryCallback;
class EntriesCallback;
class ErrorCallback;
class FileEntry;
class FileWriterCallback;
class MetadataCallback;
class VoidCallback;

class DOMFileSystem : public RefCounted<DOMFileSystem>, public ActiveDOMObject {
public:
    static PassRefPtr<DOMFileSystem> create(ScriptExecutionContext* context, const String& name, PassOwnPtr<AsyncFileSystem> asyncFileSystem)
    {
        return adoptRef(new DOMFileSystem(context, name, asyncFileSystem));
    }

    virtual ~DOMFileSystem();

    const String& name() const { return m_name; }
    PassRefPtr<DirectoryEntry> root();

    // ActiveDOMObject methods.
    virtual void stop();
    virtual bool hasPendingActivity() const;
    virtual void contextDestroyed();

    // Actual FileSystem API implementations.  All the validity checks on virtual paths are done at DOMFileSystem level.
    void getMetadata(const Entry* entry, PassRefPtr<MetadataCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback);
    void move(const Entry* src, PassRefPtr<Entry> parent, const String& name, PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>);
    void copy(const Entry* src, PassRefPtr<Entry> parent, const String& name, PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>);
    void remove(const Entry* entry, PassRefPtr<VoidCallback>, PassRefPtr<ErrorCallback>);
    void getParent(const Entry* entry, PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>);
    void getFile(const Entry* base, const String& path, PassRefPtr<Flags>, PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>);
    void getDirectory(const Entry* base, const String& path, PassRefPtr<Flags>, PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>);
    void readDirectory(DirectoryReader* reader, const String& path, PassRefPtr<EntriesCallback>, PassRefPtr<ErrorCallback>);
    void createWriter(const FileEntry* file, PassRefPtr<FileWriterCallback>, PassRefPtr<ErrorCallback>);

    // Schedule a callback. This should not cross threads (should be called on the same context thread).
    // FIXME: move this to a more generic place.
    template <typename CB, typename CBArg>
    static void scheduleCallback(ScriptExecutionContext*, PassRefPtr<CB>, PassRefPtr<CBArg>);

    template <typename CB, typename CBArg>
    void scheduleCallback(PassRefPtr<CB> callback, PassRefPtr<CBArg> callbackArg)
    {
        scheduleCallback(scriptExecutionContext(), callback, callbackArg);
    }

private:
    DOMFileSystem(ScriptExecutionContext*, const String& name, PassOwnPtr<AsyncFileSystem>);

    AsyncFileSystem* asyncFileSystem() const { return m_asyncFileSystem.get(); }

    // A helper template to schedule a callback task.
    // FIXME: move this to a more generic place.
    template <typename CB, typename CBArg>
    class DispatchCallbackTask : public ScriptExecutionContext::Task {
    public:
        DispatchCallbackTask(PassRefPtr<CB> callback, PassRefPtr<CBArg> arg)
            : m_callback(callback)
            , m_callbackArg(arg)
        {
        }

        virtual void performTask(ScriptExecutionContext*)
        {
            m_callback->handleEvent(m_callbackArg.get());
        }

    private:
        RefPtr<CB> m_callback;
        RefPtr<CBArg> m_callbackArg;
    };

    String m_name;
    mutable OwnPtr<AsyncFileSystem> m_asyncFileSystem;
};

template <typename CB, typename CBArg>
void DOMFileSystem::scheduleCallback(ScriptExecutionContext* scriptExecutionContext, PassRefPtr<CB> callback, PassRefPtr<CBArg> arg)
{
    ASSERT(scriptExecutionContext->isContextThread());
    scriptExecutionContext->postTask(new DispatchCallbackTask<CB, CBArg>(callback, arg));
}

} // namespace

#endif // ENABLE(FILE_SYSTEM)

#endif // DOMFileSystem_h
