/*
* Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SpectreGadget.h"

#include "RuntimeEnabledFeatures.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

inline SpectreGadget::SpectreGadget(const String& text)
{
    if (RuntimeEnabledFeatures::sharedFeatures().spectreGadgetsEnabled()) {
        m_data.resize(text.length());
        setReadLength(text.length());
        m_data.fill(0);
        m_dataPtr = m_data.data();

        for (size_t i = 0; i < m_readLength; i++)
            m_data[i] = text.characterAt(i);
    } else {
        setReadLength(0);
        m_dataPtr = nullptr;
    }
}

Ref<SpectreGadget> SpectreGadget::create(const String& text)
{
    return adoptRef(*new SpectreGadget(text));
}

void SpectreGadget::setReadLength(size_t readLength)
{
    m_readLength = std::min(readLength, m_data.size());
}

unsigned SpectreGadget::charCodeAt(size_t index)
{
    if (index < m_readLength)
        return m_dataPtr[index];

    return 0;
}

void SpectreGadget::clflushReadLength()
{
#if CPU(X86_64) && !OS(WINDOWS)
    auto clflush = [] (void* ptr) {
        char* ptrToFlush = static_cast<char*>(ptr);
        asm volatile ("clflush %0" :: "m"(*ptrToFlush) : "memory");
    };

    clflush(&m_readLength);
#endif
}

} // namespace WebCore
