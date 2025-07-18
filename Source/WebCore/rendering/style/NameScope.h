/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#pragma once

#include "StyleScopeOrdinal.h"
#include <wtf/ListHashSet.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

struct NameScope {
    enum class Type : uint8_t { None, All, Ident };

    Type type { Type::None };
    ListHashSet<AtomString> names;
    Style::ScopeOrdinal scopeOrdinal { Style::ScopeOrdinal::Element };

    friend bool operator==(const NameScope&, const NameScope&);
};

inline bool operator==(const NameScope& lhs, const NameScope& rhs)
{
    return lhs.type == rhs.type && lhs.scopeOrdinal == rhs.scopeOrdinal
        // Two name lists are equal if they contain the same values in the same order.
        && (lhs.names.isEmpty() || std::equal(lhs.names.begin(), lhs.names.end(), rhs.names.begin(), rhs.names.end()));
}

inline TextStream& operator<<(TextStream& ts, const NameScope& scope)
{
    switch (scope.type) {
    case NameScope::Type::None:
        ts << "none"_s;
        break;
    case NameScope::Type::All:
        ts << "all"_s;
        break;
    case NameScope::Type::Ident:
        ts << "ident: "_s << scope.names;
        break;
    }
    ts << " (style scope: " << scope.scopeOrdinal << ")";
    return ts;
}

} // namespace WebCore
