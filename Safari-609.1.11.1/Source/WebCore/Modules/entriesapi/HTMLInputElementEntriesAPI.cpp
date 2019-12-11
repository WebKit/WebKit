/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTMLInputElementEntriesAPI.h"

#include "DOMFileSystem.h"
#include "FileList.h"
#include "HTMLInputElement.h"

namespace WebCore {

using namespace HTMLNames;

Vector<Ref<FileSystemEntry>> HTMLInputElementEntriesAPI::webkitEntries(ScriptExecutionContext& context, HTMLInputElement& input)
{
    // As of Sept 2017, Chrome and Firefox both only populate webkitEntries when the webkitdirectory flag is unset.
    // We do the same for consistency.
    if (input.hasAttributeWithoutSynchronization(webkitdirectoryAttr))
        return { };

    auto* fileList = input.files();
    if (!fileList)
        return { };

    Vector<Ref<FileSystemEntry>> entries;
    entries.reserveInitialCapacity(fileList->files().size());
    for (auto& file : fileList->files())
        entries.uncheckedAppend(DOMFileSystem::createEntryForFile(context, file.copyRef()));
    return entries;
}

}
