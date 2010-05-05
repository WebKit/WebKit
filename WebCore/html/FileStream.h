/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
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

#ifndef FileStream_h
#define FileStream_h

#if ENABLE(FILE_READER) || ENABLE(FILE_WRITER)

#include "FileStreamClient.h"
#include "FileSystem.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Blob;
class String;

// All methods are synchronous and should be called on File or Worker thread.
class FileStream : public RefCounted<FileStream> {
public:
    static PassRefPtr<FileStream> create(FileStreamClient* client)
    {
        return adoptRef(new FileStream(client));
    }
    virtual ~FileStream();

    void start();
    void stop();

    void openForRead(Blob*);
    void openForWrite(const String& path);
    void close();
    void read(char* buffer, int length);
    void write(Blob* blob, long long position, int length);
    void truncate(long long position);

private:
    FileStream(FileStreamClient*);

    FileStreamClient* m_client;
    PlatformFileHandle m_handle;
    long long m_bytesProcessed;
    long long m_totalBytesToRead;
};

} // namespace WebCore

#endif // ENABLE(FILE_READER) || ENABLE(FILE_WRITER)

#endif // FileStream_h
