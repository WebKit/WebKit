/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CSSValueTypes.h>
#include <WebCore/GridArea.h>
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
namespace CSS {

// Parsed representation of the `<string>+` of <'grid-template-areas'>.
struct GridNamedAreaMap {
    using Map = HashMap<String, GridArea>;

    Map map;
    size_t rowCount { 0 };
    size_t columnCount { 0 };

    bool operator==(const GridNamedAreaMap&) const = default;
};

// A single `<string>` of <'grid-template-areas'>.
using GridNamedAreaMapRow = Vector<String, 8>;

// Adds a row to a `GridNamedAreaMap`. Returns `true` on success, `false` on failure.
bool addRow(GridNamedAreaMap&, const GridNamedAreaMapRow&);

// MARK: - Serialization

template<> struct Serialize<GridNamedAreaMap> { void operator()(StringBuilder&, const CSS::SerializationContext&, const GridNamedAreaMap&); };

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const GridNamedAreaMap&);

} // namespace CSS
} // namespace WebCore
