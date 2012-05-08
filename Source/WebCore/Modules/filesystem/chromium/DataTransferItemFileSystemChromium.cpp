/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "DataTransferItemFileSystem.h"

#if ENABLE(FILE_SYSTEM)

#include "AsyncFileSystem.h"
#include "AsyncFileSystemCallbacks.h"
#include "ChromiumDataObject.h"
#include "ClipboardChromium.h"
#include "DOMFilePath.h"
#include "DOMFileSystem.h"
#include "DirectoryEntry.h"
#include "DraggedIsolatedFileSystem.h"
#include "Entry.h"
#include "EntryCallback.h"
#include "File.h"
#include "FileEntry.h"
#include "FileMetadata.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

namespace {

class GetAsEntryCallbacks : public AsyncFileSystemCallbacks {
    WTF_MAKE_NONCOPYABLE(GetAsEntryCallbacks);
public:
    static PassOwnPtr<GetAsEntryCallbacks> create(PassRefPtr<DOMFileSystem> filesystem, const String& virtualPath, PassRefPtr<EntryCallback> callback)
    {
        return adoptPtr(new GetAsEntryCallbacks(filesystem, virtualPath, callback));
    }

    virtual ~GetAsEntryCallbacks()
    {
    }

    virtual void didReadMetadata(const FileMetadata& metadata)
    {
        if (metadata.type == FileMetadata::TypeDirectory)
            m_callback->handleEvent(DirectoryEntry::create(m_filesystem, m_virtualPath).get());
        else
            m_callback->handleEvent(FileEntry::create(m_filesystem, m_virtualPath).get());
    }

    virtual void didFail(int error)
    {
        // FIXME: Return a special FileEntry which will let any succeeding operations fail with the error code.
        m_callback->handleEvent(0);
    }

private:
    GetAsEntryCallbacks(PassRefPtr<DOMFileSystem> filesystem, const String& virtualPath, PassRefPtr<EntryCallback> callback)
        : m_filesystem(filesystem)
        , m_virtualPath(virtualPath)
        , m_callback(callback)
    {
    }

    RefPtr<DOMFileSystem> m_filesystem;
    String m_virtualPath;
    RefPtr<EntryCallback> m_callback;
};

} // namespace

// static
void DataTransferItemFileSystem::webkitGetAsEntry(DataTransferItem* item, ScriptExecutionContext* scriptExecutionContext, PassRefPtr<EntryCallback> callback)
{
    DataTransferItemPolicyWrapper* itemPolicyWrapper = static_cast<DataTransferItemPolicyWrapper*>(item);

    if (!callback || !itemPolicyWrapper->dataObjectItem()->isFilename())
        return;

    // For dragged files getAsFile must be pretty lightweight.
    Blob* file = itemPolicyWrapper->getAsFile().get();
    // The clipboard may not be in a readable state.
    if (!file)
        return;
    ASSERT(file->isFile());

    DraggedIsolatedFileSystem* filesystem = DraggedIsolatedFileSystem::from(itemPolicyWrapper->clipboard()->dataObject().get());
    DOMFileSystem* domFileSystem = filesystem ? filesystem->getDOMFileSystem(scriptExecutionContext) : 0;
    if (!filesystem) {
        // IsolatedFileSystem may not be enabled.
        DOMFileSystem::scheduleCallback(scriptExecutionContext, callback, adoptRef(static_cast<Entry*>(0)));
        return;
    }

    ASSERT(domFileSystem);

    // The dropped entries are mapped as top-level entries in the isolated filesystem.
    String virtualPath = DOMFilePath::append("/", static_cast<File*>(file)->name());
    domFileSystem->asyncFileSystem()->readMetadata(domFileSystem->createFileSystemURL(virtualPath), GetAsEntryCallbacks::create(domFileSystem, virtualPath, callback));
}

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)
