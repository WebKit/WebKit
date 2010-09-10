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

#include "DOMFilePath.h"
#include "DOMFileSystem.h"
#include "EntryCallback.h"
#include "ErrorCallback.h"
#include "FileError.h"
#include "MetadataCallback.h"
#include "VoidCallback.h"

namespace WebCore {

Entry::Entry(DOMFileSystem* fileSystem, const String& fullPath)
    : m_fileSystem(fileSystem)
    , m_fullPath(fullPath)
    , m_name(DOMFilePath::getName(fullPath))
{
}

void Entry::getMetadata(PassRefPtr<MetadataCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    m_fileSystem->getMetadata(this, successCallback, errorCallback);
}

void Entry::moveTo(PassRefPtr<Entry> parent, const String& name, PassRefPtr<EntryCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback) const
{
    m_fileSystem->move(this, parent, name, successCallback, errorCallback);
}

void Entry::copyTo(PassRefPtr<Entry> parent, const String& name, PassRefPtr<EntryCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback) const
{
    m_fileSystem->copy(this, parent, name, successCallback, errorCallback);
}

void Entry::remove(PassRefPtr<VoidCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback) const
{
    m_fileSystem->remove(this, successCallback, errorCallback);
}

void Entry::getParent(PassRefPtr<EntryCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback) const
{
    m_fileSystem->getParent(this, successCallback, errorCallback);
}

String Entry::toURI(const String&)
{
    // FIXME: to be implemented.
    ASSERT_NOT_REACHED();
    return String();
}

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)
