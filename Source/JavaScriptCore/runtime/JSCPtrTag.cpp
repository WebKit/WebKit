/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include "JSCPtrTag.h"

namespace JSC {

#if CPU(ARM64E)

static const char* tagForPtr(const void* ptr)
{
#define RETURN_NAME_IF_TAG_MATCHES(tag) \
    if (WTF::untagCodePtrImpl<WTF::PtrTagAction::NoAssert>(ptr, JSC::tag) == removeCodePtrTag(ptr)) \
        return #tag;
    FOR_EACH_JSC_PTRTAG(RETURN_NAME_IF_TAG_MATCHES)
#undef RETURN_NAME_IF_TAG_MATCHES
    return nullptr; // Matching tag not found.
}

static const char* ptrTagName(PtrTag tag)
{
#define RETURN_PTRTAG_NAME(_tagName) case _tagName: return #_tagName;
    switch (static_cast<unsigned>(tag)) {
        FOR_EACH_JSC_PTRTAG(RETURN_PTRTAG_NAME)
    }
#undef RETURN_PTRTAG_NAME
    return nullptr; // Matching tag not found.
}

void initializePtrTagLookup()
{
    static WTF::PtrTagLookup lookup = { tagForPtr, ptrTagName };
    WTF::registerPtrTagLookup(&lookup);
}

#endif // CPU(ARM64E)

} // namespace JSC
