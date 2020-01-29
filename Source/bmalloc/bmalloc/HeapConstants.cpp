/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#include "HeapConstants.h"

namespace bmalloc {

DEFINE_STATIC_PER_PROCESS_STORAGE(HeapConstants);

HeapConstants::HeapConstants(std::lock_guard<Mutex>&)
    : m_vmPageSizePhysical { vmPageSizePhysical() }
{
    RELEASE_BASSERT(m_vmPageSizePhysical >= smallPageSize);
    RELEASE_BASSERT(vmPageSize() >= m_vmPageSizePhysical);

    initializeLineMetadata();
    initializePageMetadata();
}

void HeapConstants::initializeLineMetadata()
{
    size_t sizeClassCount = bmalloc::sizeClass(smallLineSize);
    m_smallLineMetadata.grow(sizeClassCount * smallLineCount());

    for (size_t sizeClass = 0; sizeClass < sizeClassCount; ++sizeClass) {
        size_t size = objectSize(sizeClass);
        LineMetadata* pageMetadata = &m_smallLineMetadata[sizeClass * smallLineCount()];

        size_t object = 0;
        size_t line = 0;
        while (object < m_vmPageSizePhysical) {
            line = object / smallLineSize;
            size_t leftover = object % smallLineSize;

            size_t objectCount;
            size_t remainder;
            divideRoundingUp(smallLineSize - leftover, size, objectCount, remainder);

            pageMetadata[line] = { static_cast<unsigned char>(leftover), static_cast<unsigned char>(objectCount) };

            object += objectCount * size;
        }

        // Don't allow the last object in a page to escape the page.
        if (object > m_vmPageSizePhysical) {
            BASSERT(pageMetadata[line].objectCount);
            --pageMetadata[line].objectCount;
        }
    }
}

void HeapConstants::initializePageMetadata()
{
    auto computePageSize = [&](size_t sizeClass) {
        size_t size = objectSize(sizeClass);
        if (sizeClass < bmalloc::sizeClass(smallLineSize))
            return m_vmPageSizePhysical;

        for (size_t pageSize = m_vmPageSizePhysical; pageSize < pageSizeMax; pageSize += m_vmPageSizePhysical) {
            RELEASE_BASSERT(pageSize <= chunkSize / 2);
            size_t waste = pageSize % size;
            if (waste <= pageSize / pageSizeWasteFactor)
                return pageSize;
        }

        return pageSizeMax;
    };

    for (size_t i = 0; i < sizeClassCount; ++i)
        m_pageClasses[i] = (computePageSize(i) - 1) / smallPageSize;
}

} // namespace bmalloc
