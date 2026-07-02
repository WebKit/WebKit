/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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

#include <wtf/StdLibExtras.h>
#include <wtf/spi/darwin/XPCSPI.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/WTFString.h>

#if OS(DARWIN)

namespace WTF {

inline std::span<const uint8_t> xpcDictionaryGetData(xpc_object_t xdict, ASCIILiteral key)
{
    size_t dataSize { 0 };
    auto* data = static_cast<const uint8_t*>(xpc_dictionary_get_data(xdict, key.characters(), &dataSize)); // NOLINT
    return unsafeMakeSpan(data, dataSize);
}

// ASCIILiteral version of XPC_ERROR_KEY_DESCRIPTION.
static constexpr auto xpcErrorDescriptionKey = "XPCErrorDescription"_s;

inline String xpcDictionaryGetString(xpc_object_t xdict, ASCIILiteral key)
{
    auto* cstring = xpc_dictionary_get_string(xdict, key.characters()); // NOLINT
    if (!cstring)
        return { };
    return String::fromUTF8(cstring);
}

inline String xpcStringGetString(xpc_object_t xvalue)
{
    return String::fromUTF8(unsafeMakeSpan(xpc_string_get_string_ptr(xvalue), xpc_string_get_length(xvalue))); // NOLINT
}

} // namespace WTF

using WTF::xpcDictionaryGetData;
using WTF::xpcDictionaryGetString;
using WTF::xpcErrorDescriptionKey;
using WTF::xpcStringGetString;

#endif // OS(DARWIN)
