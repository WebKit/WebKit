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

#ifndef Entry_h
#define Entry_h

#if ENABLE(FILE_SYSTEM)

#include "DOMFileSystem.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class EntryCallback;
class ErrorCallback;
class MetadataCallback;
class VoidCallback;

class Entry : public RefCounted<Entry> {
public:
    virtual ~Entry() { }

    virtual bool isFile() const { return false; }
    virtual bool isDirectory() const { return false; }

    const String& fullPath() const { return m_fullPath; }
    const String& name() const { return m_name; }

    DOMFileSystem* filesystem() const { return m_fileSystem; }

    virtual void getMetadata(PassRefPtr<MetadataCallback> successCallback = 0, PassRefPtr<ErrorCallback> errorCallback = 0);
    virtual void moveTo(PassRefPtr<Entry> parent, const String& name = String(), PassRefPtr<EntryCallback> successCallback = 0, PassRefPtr<ErrorCallback> errorCallback = 0) const;
    virtual void copyTo(PassRefPtr<Entry> parent, const String& name = String(), PassRefPtr<EntryCallback> successCallback = 0, PassRefPtr<ErrorCallback> errorCallback = 0) const;
    virtual void remove(PassRefPtr<VoidCallback> successCallback = 0, PassRefPtr<ErrorCallback> errorCallback = 0) const;
    virtual void getParent(PassRefPtr<EntryCallback> successCallback = 0, PassRefPtr<ErrorCallback> errorCallback = 0) const;

    virtual String toURI(const String& mimeType = String());

protected:
    Entry(DOMFileSystem* fileSystem, const String& fullPath);

    DOMFileSystem* m_fileSystem;
    String m_fullPath;
    String m_name;
};

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)

#endif // Entry_h
