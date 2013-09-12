/*
 * Copyright (C) 2013 University of Szeged
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef MultipartHandle_h
#define MultipartHandle_h

#include "HTTPHeaderMap.h"
#include "ResourceHandle.h"

#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class MultipartHandle {

public:
    static PassOwnPtr<MultipartHandle> create(ResourceHandle* handle, const String& boundary);
    static bool extractBoundary(const String& contentType, String& boundary);

    MultipartHandle(ResourceHandle* handle, const String& boundary)
        : m_resourceHandle(handle)
        , m_boundary("--" + boundary)
        , m_boundaryLength(m_boundary.length())
        , m_state(CheckBoundary)
    {
    }

    ~MultipartHandle() { }

    void contentReceived(const char* data, size_t length);
    void contentEnded();

private:
    enum MultipartHandleState {
        CheckBoundary,
        InBoundary,
        InHeader,
        InContent,
        EndBoundary,
        End
    };

    void didReceiveData(size_t length);
    void didReceiveResponse();

    bool matchForBoundary(const char* data, size_t position, size_t& matchedLength);
    inline bool checkForBoundary(size_t& boundaryStartPosition, size_t& lastPartialMatchPosition);
    bool parseHeadersIfPossible();
    bool processContent();

    ResourceHandle* m_resourceHandle;
    String m_boundary;
    size_t m_boundaryLength;

    Vector<char> m_buffer;
    HTTPHeaderMap m_headers;

    MultipartHandleState m_state;
};

} // namespace WebCore
#endif // MultipartHandle_h
