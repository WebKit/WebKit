/*
 * Copyright (C) 2013 University of Szeged
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

#pragma once

#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CurlMultipartHandleClient;
class CurlResponse;
class SharedBuffer;

class CurlMultipartHandle {
public:
    static std::unique_ptr<CurlMultipartHandle> createIfNeeded(CurlMultipartHandleClient&, const CurlResponse&);

    CurlMultipartHandle(CurlMultipartHandleClient&, const String&);
    ~CurlMultipartHandle() { }

    void didReceiveData(const SharedBuffer&);
    void didComplete();

private:
    enum class State {
        CheckBoundary,
        InBoundary,
        InHeader,
        InContent,
        EndBoundary,
        End
    };

    static Optional<String> extractBoundary(const CurlResponse&);
    static Optional<String> extractBoundaryFromContentType(const String&);

    bool processContent();
    bool checkForBoundary(size_t& boundaryStartPosition, size_t& lastPartialMatchPosition);
    size_t matchedLength(const char* data);
    bool parseHeadersIfPossible();

    CurlMultipartHandleClient& m_client;

    String m_boundary;
    Vector<char> m_buffer;
    Vector<String> m_headers;

    State m_state { State::CheckBoundary };
};

} // namespace WebCore
