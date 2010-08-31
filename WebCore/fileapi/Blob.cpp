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
#include "Blob.h"

#include "BlobURL.h"
#include "File.h"
#include "ScriptExecutionContext.h"
#include "ThreadableBlobRegistry.h"

namespace WebCore {

Blob::Blob(ScriptExecutionContext* scriptExecutionContext, PassOwnPtr<BlobData> blobData, long long size)
    : m_scriptExecutionContext(scriptExecutionContext)
    , m_type(blobData->contentType())
    , m_size(size)
{
    ASSERT(blobData);

    m_scriptExecutionContext->addBlob(this);

    // Create a new internal URL and register it with the provided blob data.
    m_url = BlobURL::createURL(scriptExecutionContext);
    ThreadableBlobRegistry::registerBlobURL(scriptExecutionContext, m_url, blobData);
}

Blob::Blob(ScriptExecutionContext* scriptExecutionContext, const KURL& srcURL, const String& type, long long size)
    : m_scriptExecutionContext(scriptExecutionContext)
    , m_type(type)
    , m_size(size)
{
   m_scriptExecutionContext->addBlob(this);

    // Create a new internal URL and register it with the same blob data as the source URL.
    m_url = BlobURL::createURL(scriptExecutionContext);
    ThreadableBlobRegistry::registerBlobURL(scriptExecutionContext, m_url, srcURL);
}

Blob::~Blob()
{
    // The internal URL is only used to refer to the Blob object. So we need to unregister the URL when the object is GC-ed.
    if (m_scriptExecutionContext) {
        m_scriptExecutionContext->removeBlob(this);
        ThreadableBlobRegistry::unregisterBlobURL(m_scriptExecutionContext, m_url);
    }
}

void Blob::contextDestroyed()
{
    ASSERT(m_scriptExecutionContext);

    // Unregister the internal URL before the context is gone.
    ThreadableBlobRegistry::unregisterBlobURL(m_scriptExecutionContext, m_url);
    m_scriptExecutionContext = 0;
}

#if ENABLE(BLOB)
PassRefPtr<Blob> Blob::slice(ScriptExecutionContext* scriptExecutionContext, long long start, long long length, const String& contentType) const
{
    // When we slice a file for the first time, we obtain a snapshot of the file by capturing its current size and modification time.
    // The modification time will be used to verify if the file has been changed or not, when the underlying data are accessed.
    long long size;
    double modificationTime;
    if (isFile())
        // FIXME: This involves synchronous file operation. We need to figure out how to make it asynchronous.
        static_cast<const File*>(this)->captureSnapshot(size, modificationTime);
    else {
        ASSERT(m_size != -1);
        size = m_size;
    }

    // Clamp the range if it exceeds the size limit.
    if (start < 0)
        start = 0;
    if (length < 0)
        length = 0;

    if (start >= size) {
        start = 0;
        length = 0;
    } else if (start + length > size)
        length = size - start;

    OwnPtr<BlobData> blobData = BlobData::create();
    blobData->setContentType(contentType);
    if (isFile())
        blobData->appendFile(static_cast<const File*>(this)->path(), start, length, modificationTime);
    else
        blobData->appendBlob(m_url, start, length);

    return Blob::create(scriptExecutionContext, blobData.release(), length);
}
#endif

} // namespace WebCore
