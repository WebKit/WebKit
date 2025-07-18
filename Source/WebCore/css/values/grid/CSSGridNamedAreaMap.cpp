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

#include "config.h"
#include "CSSGridNamedAreaMap.h"

#include "CSSSerializationContext.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace CSS {

bool addRow(GridNamedAreaMap& gridAreaMap, const GridNamedAreaMapRow& row)
{
    if (!gridAreaMap.rowCount) {
        gridAreaMap.columnCount = row.size();
        if (!gridAreaMap.columnCount)
            return false;
    } else if (gridAreaMap.columnCount != row.size()) {
        // The declaration is invalid if all the rows don't have the number of columns.
        return false;
    }

    for (size_t currentColumn = 0; currentColumn < gridAreaMap.columnCount; ++currentColumn) {
        auto& gridAreaName = row[currentColumn];

        // Unnamed areas are always valid (we consider them to be 1x1).
        if (gridAreaName == "."_s)
            continue;

        auto lookAheadColumn = currentColumn + 1;
        while (lookAheadColumn < gridAreaMap.columnCount && row[lookAheadColumn] == gridAreaName)
            lookAheadColumn++;

        auto result = gridAreaMap.map.ensure(gridAreaName, [&] {
            return GridArea {
                GridSpan::translatedDefiniteGridSpan(gridAreaMap.rowCount, gridAreaMap.rowCount + 1),
                GridSpan::translatedDefiniteGridSpan(currentColumn, lookAheadColumn)
            };
        });
        if (!result.isNewEntry) {
            auto& gridArea = result.iterator->value;

            // The following checks test that the grid area is a single filled-in rectangle.
            // 1. The new row is adjacent to the previously parsed row.
            if (gridAreaMap.rowCount != gridArea.rows.endLine())
                return false;

            // 2. The new area starts at the same position as the previously parsed area.
            if (currentColumn != gridArea.columns.startLine())
                return false;

            // 3. The new area ends at the same position as the previously parsed area.
            if (lookAheadColumn != gridArea.columns.endLine())
                return false;

            gridArea.rows = GridSpan::translatedDefiniteGridSpan(gridArea.rows.startLine(), gridArea.rows.endLine() + 1);
        }
        currentColumn = lookAheadColumn - 1;
    }

    ++gridAreaMap.rowCount;
    return true;
}

// MARK: - Serialization

static void serializeGridNamedAreaMapPosition(StringBuilder& builder, const GridNamedAreaMap::Map& map, size_t row, size_t column)
{
    HashSet<String> candidates;
    for (auto& [name, area] : map) {
        if (row >= area.rows.startLine() && row < area.rows.endLine())
            candidates.add(name);
    }
    for (auto& [name, area] : map) {
        if (column >= area.columns.startLine() && column < area.columns.endLine() && candidates.contains(name)) {
            builder.append(name);
            return;
        }
    }
    builder.append('.');
}

void Serialize<GridNamedAreaMap>::operator()(StringBuilder& builder, const CSS::SerializationContext&, const GridNamedAreaMap& value)
{
    for (size_t row = 0; row < value.rowCount; ++row) {
        builder.append('"');
        for (size_t column = 0; column < value.columnCount; ++column) {
            serializeGridNamedAreaMapPosition(builder, value.map, row, column);
            if (column != value.columnCount - 1)
                builder.append(' ');
        }
        builder.append('"');
        if (row != value.rowCount - 1)
            builder.append(' ');
    }
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const GridNamedAreaMap& value)
{
    return ts << serializationForCSS(defaultSerializationContext(), value);
}

} // namespace CSS
} // namespace WebCore
