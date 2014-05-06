/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef BlobDataFileReference_h
#define BlobDataFileReference_h

#include "FileSystem.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class BlobDataFileReference : public RefCounted<BlobDataFileReference> {
public:
    static PassRefPtr<BlobDataFileReference> create(const String& path)
    {
        return adoptRef(new BlobDataFileReference(path));
    }

    const String& path() const { return m_path; }
    unsigned long long size() const;
    double expectedModificationTime() const;

private:
    BlobDataFileReference(const String& path)
        : m_path(path)
        , m_fileSystemDataComputed(false)
        , m_size(0)
        , m_expectedModificationTime(invalidFileTime())
    {
        // FIXME: Per the spec, a File object should start watching for changes as soon as it's created, so we should record m_expectedModificationTime right away.
        // FIXME: Some platforms provide better ways to listen for file system object changes, consider using these.
    }

    void computeFileSystemData() const;

    String m_path;
    mutable bool m_fileSystemDataComputed;
    mutable unsigned long long m_size;
    mutable double m_expectedModificationTime;
};

}

#endif // BlobDataFileReference_h
