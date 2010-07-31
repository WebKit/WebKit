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
#include "Entry.h"

#if ENABLE(FILE_SYSTEM)

#include "DOMFileSystem.h"
#include "EntryCallback.h"
#include "ErrorCallback.h"
#include "MetadataCallback.h"
#include "VoidCallback.h"

namespace WebCore {

Entry::Entry(PassRefPtr<DOMFileSystem> fileSystem, const String& fullPath, bool isDirectory)
    : m_fileSystem(fileSystem)
    , m_fullPath(fullPath)
    , m_isDirectory(isDirectory)
{
    int index = fullPath.reverseFind("/");
    if (index != -1)
        m_name = fullPath.substring(index);
    else
        m_name = fullPath;
}

void Entry::getMetadata(ScriptExecutionContext*, PassRefPtr<MetadataCallback>, PassRefPtr<ErrorCallback>)
{
    // FIXME: to be implemented.
}

void Entry::moveTo(ScriptExecutionContext*, PassRefPtr<Entry>, const String&, PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>)
{
    // FIXME: to be implemented.
}

void Entry::copyTo(ScriptExecutionContext*, PassRefPtr<Entry>, const String&, PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>)
{
    // FIXME: to be implemented.
}

void Entry::remove(ScriptExecutionContext*, PassRefPtr<VoidCallback>, PassRefPtr<ErrorCallback>)
{
    // FIXME: to be implemented.
}

void Entry::getParent(ScriptExecutionContext*, PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>)
{
    // FIXME: to be implemented.
}

String Entry::toURI(const String&)
{
    // FIXME: to be implemented.
    return String();
}

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)
