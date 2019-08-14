/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FormData.h"
#include <wtf/FileSystem.h>
#include <wtf/Vector.h>

namespace WebCore {

class CurlFormDataStream {
public:
    CurlFormDataStream(const FormData*, PAL::SessionID);
    WEBCORE_EXPORT ~CurlFormDataStream();

    void clean();

    size_t elementSize() { return m_formData ? m_formData->elements().size() : 0; }

    const Vector<char>* getPostData();
    bool shouldUseChunkTransfer();
    unsigned long long totalSize();

    Optional<size_t> read(char*, size_t);
    unsigned long long totalReadSize() { return m_totalReadSize; }

private:
    void computeContentLength();

    Optional<size_t> readFromFile(const FormDataElement::EncodedFileData&, char*, size_t);
    Optional<size_t> readFromData(const Vector<char>&, char*, size_t);

    PAL::SessionID m_sessionID;
    RefPtr<FormData> m_formData;

    std::unique_ptr<Vector<char>> m_postData;
    bool m_isContentLengthUpdated { false };
    bool m_shouldUseChunkTransfer { false };
    unsigned long long m_totalSize { 0 };
    unsigned long long m_totalReadSize { 0 };

    size_t m_elementPosition { 0 };

    FileSystem::PlatformFileHandle m_fileHandle { FileSystem::invalidPlatformFileHandle };
    size_t m_dataOffset { 0 };
};

} // namespace WebCore
