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
#include <wtf/PtrTag.h>

namespace WTF {

#if CPU(ARM64E)

static PtrTagLookup* s_ptrTagLookup = nullptr;

static const char* tagForPtr(const void* ptr)
{
    PtrTagLookup* lookup = s_ptrTagLookup;
    while (lookup) {
        const char* tagName = lookup->tagForPtr(ptr);
        if (tagName)
            return tagName;
        lookup = lookup->next;
    }

    if (ptr == removeCodePtrTag(ptr))
        return "NoPtrTag";

#define RETURN_NAME_IF_TAG_MATCHES(tag) \
    if (untagCodePtrImpl<PtrTagAction::NoAssert>(ptr, tag) == removeCodePtrTag(ptr)) \
        return #tag;
    FOR_EACH_WTF_PTRTAG(RETURN_NAME_IF_TAG_MATCHES)
#undef RETURN_NAME_IF_TAG_MATCHES

    return "<unknown PtrTag>";
}

static const char* ptrTagName(PtrTag tag)
{
    PtrTagLookup* lookup = s_ptrTagLookup;
    while (lookup) {
        const char* tagName = lookup->ptrTagName(tag);
        if (tagName)
            return tagName;
        lookup = lookup->next;
    }

#define RETURN_WTF_PTRTAG_NAME(_tagName) case _tagName: return #_tagName;
    switch (tag) {
        FOR_EACH_WTF_PTRTAG(RETURN_WTF_PTRTAG_NAME)
    default: return "<unknown>";
    }
#undef RETURN_WTF_PTRTAG_NAME
}

void registerPtrTagLookup(PtrTagLookup* lookup)
{
    lookup->next = s_ptrTagLookup;
    s_ptrTagLookup = lookup;
}

void reportBadTag(const void* ptr, PtrTag expectedTag)
{
    dataLog("PtrTag ASSERTION FAILED on pointer ", RawPointer(ptr), ", actual tag = ", tagForPtr(ptr));
    if (expectedTag == AnyPtrTag)
        dataLogLn(", expected any tag but NoPtrTag");
    else
        dataLogLn(", expected tag = ", ptrTagName(expectedTag));
}

#endif // CPU(ARM64E)

} // namespace WTF
