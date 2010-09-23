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
#include "AsyncFileWriterChromium.h"

#if ENABLE(FILE_SYSTEM)

#include "AsyncFileWriterClient.h"
#include "Blob.h"
#include "WebFileWriter.h"
#include "WebURL.h"

namespace WebCore {

AsyncFileWriterChromium::AsyncFileWriterChromium(AsyncFileWriterClient* client)
    : m_client(client)
{
}

AsyncFileWriterChromium::~AsyncFileWriterChromium()
{
}

void AsyncFileWriterChromium::setWebFileWriter(PassOwnPtr<WebKit::WebFileWriter> writer)
{
    m_writer = writer;
}

void AsyncFileWriterChromium::write(long long position, Blob* data)
{
    ASSERT(m_writer);
    m_writer->write(position, WebKit::WebURL(data->url()));
}

void AsyncFileWriterChromium::truncate(long long length)
{
    ASSERT(m_writer);
    m_writer->truncate(length);
}

void AsyncFileWriterChromium::abort()
{
    ASSERT(m_writer);
    m_writer->cancel();
}

void AsyncFileWriterChromium::didWrite(long long bytes, bool complete)
{
    ASSERT(m_writer);
    m_client->didWrite(bytes, complete);
}

void AsyncFileWriterChromium::didTruncate(long long length)
{
    m_client->didTruncate(length);
}

void AsyncFileWriterChromium::didFail(WebKit::WebFileError error)
{
    m_client->didFail(error);
}

} // namespace

#endif // ENABLE(FILE_SYSTEM)
