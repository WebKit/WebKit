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
#include <wtf/Optional.h>
#include <wtf/text/StringView.h>

namespace JSC { namespace Yarr {

std::optional<OptionSet<Flags>> parseFlags(StringView string)
{
    OptionSet<Flags> flags;
    for (auto character : string.codeUnits()) {
        switch (character) {
        case 'd':
            if (flags.contains(Flags::HasIndices))
                return std::nullopt;
            flags.add(Flags::HasIndices);
            break;

        case 'g':
            if (flags.contains(Flags::Global))
                return std::nullopt;
            flags.add(Flags::Global);
            break;

        case 'i':
            if (flags.contains(Flags::IgnoreCase))
                return std::nullopt;
            flags.add(Flags::IgnoreCase);
            break;

        case 'm':
            if (flags.contains(Flags::Multiline))
                return std::nullopt;
            flags.add(Flags::Multiline);
            break;

        case 's':
            if (flags.contains(Flags::DotAll))
                return std::nullopt;
            flags.add(Flags::DotAll);
            break;
            
        case 'u':
            if (flags.contains(Flags::Unicode))
                return std::nullopt;
            flags.add(Flags::Unicode);
            break;
                
        case 'y':
            if (flags.contains(Flags::Sticky))
                return std::nullopt;
            flags.add(Flags::Sticky);
            break;

        default:
            return std::nullopt;
        }
    }

    return std::make_optional(flags);
}

} } // namespace JSC::Yarr
