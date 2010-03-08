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

#include "ExceptionCode.h"
#include "PlatformString.h"
#include <time.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Blob : public RefCounted<Blob> {
public:
#if ENABLE(BLOB_SLICE)
    static const int toEndOfFile;
    static const double doNotCheckFileChange;
#endif

    static PassRefPtr<Blob> create(const String& path)
    {
        return adoptRef(new Blob(path));
    }

    virtual ~Blob() { }

#if ENABLE(BLOB_SLICE)
    PassRefPtr<Blob> slice(long long start, long long length) const;
#endif

    const String& path() const { return m_path; }
    unsigned long long size() const;
#if ENABLE(BLOB_SLICE)
    long long start() const { return m_start; }
    long long length() const { return m_length; }
    double modificationTime() const { return m_snapshotModificationTime; }
#endif

protected:
    Blob(const String& path);

private:
#if ENABLE(BLOB_SLICE)
    Blob(const String& path, long long start, long long length, long long snapshotSize, double snapshotModificationTime);
#endif

    // The underlying path of the file-based blob. 
    String m_path;

#if ENABLE(BLOB_SLICE)
    // The starting position of the file-based blob.
    long long m_start;

    // The length of the file-based blob. The value of -1 means to the end of the file.
    long long m_length;

    // A flag to tell if a snapshot has been captured.
    bool m_snapshotCaptured;

    // The size of the file when a snapshot is captured. It can be 0 if the file is empty.
    long long m_snapshotSize;

    // The last modification time of the file when a snapshot is captured. The value of 0 also means that the snapshot is not captured.
    double m_snapshotModificationTime;
#endif
};

} // namespace WebCore

#endif // Blob_h
