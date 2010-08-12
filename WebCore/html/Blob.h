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

#ifndef Blob_h
#define Blob_h

#include "BlobItem.h"
#include "KURL.h"
#include "PlatformString.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class BlobData;
class ScriptExecutionContext;

class Blob : public RefCounted<Blob> {
public:
    // FIXME: To be removed when we switch to using BlobData.
    static PassRefPtr<Blob> create(ScriptExecutionContext* scriptExecutionContext, const String& type, const BlobItemList& items)
    {
        return adoptRef(new Blob(scriptExecutionContext, type, items));
    }

    // For deserialization.
    static PassRefPtr<Blob> create(ScriptExecutionContext* scriptExecutionContext, const KURL& srcURL, const String& type, long long size)
    {
        return adoptRef(new Blob(scriptExecutionContext, srcURL, type, size));
    }

    virtual ~Blob();

    void contextDestroyed();

    const KURL& url() const { return m_url; }
    unsigned long long size() const;
    const String& type() const { return m_type; }
    virtual bool isFile() const { return false; }

    // FIXME: To be removed when we switch to using BlobData.
    const String& path() const;

    // FIXME: To be removed when we switch to using BlobData.
    const BlobItemList& items() const { return m_items; }

#if ENABLE(BLOB)
    PassRefPtr<Blob> slice(ScriptExecutionContext*, long long start, long long length, const String& contentType = String()) const;
#endif

protected:
    // FIXME: To be removed when we switch to using BlobData.
    Blob(ScriptExecutionContext*, const String& type, const BlobItemList&);
    Blob(ScriptExecutionContext*, const PassRefPtr<BlobItem>&);
    Blob(ScriptExecutionContext*, const String& path);

    Blob(ScriptExecutionContext*, PassOwnPtr<BlobData>, long long size);

    // For deserialization.
    Blob(ScriptExecutionContext*, const KURL& srcURL, const String& type, long long size);

    // FIXME: To be removed when we switch to using BlobData.
    BlobItemList m_items;

    // This is an internal URL referring to the blob data associated with this object.
    // It is only used by FileReader to read the blob data via loading from the blob URL resource.
    KURL m_url;

    ScriptExecutionContext* m_scriptExecutionContext;
    String m_type;
    long long m_size;
};

} // namespace WebCore

#endif // Blob_h
