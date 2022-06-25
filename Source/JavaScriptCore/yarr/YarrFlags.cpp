/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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

#include "config.h"
#include "YarrFlags.h"

#include <wtf/OptionSet.h>
#include <wtf/text/StringView.h>

namespace JSC { namespace Yarr {

std::optional<OptionSet<Flags>> parseFlags(StringView string)
{
    OptionSet<Flags> flags;
    for (auto character : string.codeUnits()) {
        switch (character) {
#define JSC_HANDLE_REGEXP_FLAG(key, name, lowerCaseName, _) \
        case key: \
            if (flags.contains(Flags::name)) \
                return std::nullopt; \
            flags.add(Flags::name); \
            break;

        JSC_REGEXP_FLAGS(JSC_HANDLE_REGEXP_FLAG)

#undef JSC_HANDLE_REGEXP_FLAG

        default:
            return std::nullopt;
        }
    }

    return std::make_optional(flags);
}

FlagsString flagsString(OptionSet<Flags> flags)
{
    FlagsString string;
    unsigned index = 0;

#define JSC_WRITE_REGEXP_FLAG(key, name, lowerCaseName, _) \
    do { \
        if (flags.contains(Flags::name)) \
            string[index++] = key; \
    } while (0);

    JSC_REGEXP_FLAGS(JSC_WRITE_REGEXP_FLAG)

#undef JSC_WRITE_REGEXP_FLAG

    ASSERT(index < string.size());
    string[index] = 0;
    return string;
}

} } // namespace JSC::Yarr
