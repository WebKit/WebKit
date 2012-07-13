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
#include "File.h"
#include "FileEntry.h"
#include "FileMetadata.h"
#include "FileSystem.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

// static
PassRefPtr<Entry> DataTransferItemFileSystem::webkitGetAsEntry(ScriptExecutionContext* scriptExecutionContext, DataTransferItem* item)
{
    DataTransferItemPolicyWrapper* itemPolicyWrapper = static_cast<DataTransferItemPolicyWrapper*>(item);

    if (!itemPolicyWrapper->dataObjectItem()->isFilename())
        return adoptRef(static_cast<Entry*>(0));

    // For dragged files getAsFile must be pretty lightweight.
    Blob* file = itemPolicyWrapper->getAsFile().get();
    // The clipboard may not be in a readable state.
    if (!file)
        return adoptRef(static_cast<Entry*>(0));
    ASSERT(file->isFile());

    DraggedIsolatedFileSystem* filesystem = DraggedIsolatedFileSystem::from(itemPolicyWrapper->clipboard()->dataObject().get());
    DOMFileSystem* domFileSystem = filesystem ? filesystem->getDOMFileSystem(scriptExecutionContext) : 0;
    if (!filesystem) {
        // IsolatedFileSystem may not be enabled.
        return adoptRef(static_cast<Entry*>(0));
    }

    ASSERT(domFileSystem);

    // The dropped entries are mapped as top-level entries in the isolated filesystem.
    String virtualPath = DOMFilePath::append("/", toFile(file)->name());

    // FIXME: This involves synchronous file operation. Consider passing file type data when we dispatch drag event.
    FileMetadata metadata;
    if (!getFileMetadata(toFile(file)->path(), metadata))
        return adoptRef(static_cast<Entry*>(0));

    if (metadata.type == FileMetadata::TypeDirectory)
        return static_cast<Entry*>(DirectoryEntry::create(domFileSystem, virtualPath).get());
    return static_cast<Entry*>(FileEntry::create(domFileSystem, virtualPath).get());
}

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)
