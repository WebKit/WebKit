/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WASMSections.h"

#if ENABLE(WEBASSEMBLY)

#include <wtf/DataLog.h>
#include <wtf/text/WTFString.h>

namespace JSC {

namespace WASM {

struct SectionData {
    unsigned length;
    const char* name;
};

static const bool verbose = false;

static const unsigned sectionDataLength = static_cast<unsigned>(Sections::Unknown);
static const SectionData sectionData[sectionDataLength] {
#define CREATE_SECTION_DATA(name, str) { sizeof(str) - 1, str },
    FOR_EACH_WASM_SECTION_TYPE(CREATE_SECTION_DATA)
#undef CREATE_SECTION_DATA
};

Sections::Section Sections::lookup(const uint8_t* name, unsigned length)
{
    if (verbose)
        dataLogLn("Decoding section with name: ", String(name, length));
    for (unsigned i = 0; i < sectionDataLength; ++i) {
        if (sectionData[i].length != length)
            continue;
        if (!memcmp(name, sectionData[i].name, length))
            return static_cast<Sections::Section>(i);
    }
    return Sections::Unknown;
}

} // namespace WASM

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
