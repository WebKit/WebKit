/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "TextExtractionCache.h"

#include <optional>
#include <wtf/HashSet.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

void TextExtractionCache::add(const String& url, Vector<TextExtractionLineContent>&& lineContents)
{
    if (lineContents.isEmpty())
        return;

    Entry entry;
    entry.url = url;
    entry.lines = WTF::move(lineContents);
    for (unsigned index = 0; index < entry.lines.size(); ++index) {
        if (auto nodeIdentifier = entry.lines[index].nodeIdentifier)
            entry.lineIndexForUID.add(WTF::move(*nodeIdentifier), index);
    }

    if (entry.lineIndexForUID.isEmpty())
        return;

    m_entries.append(WTF::move(entry));
    if (m_entries.size() > maxEntries)
        m_entries.removeAt(0, m_entries.size() - maxEntries);
}

Vector<String> TextExtractionCache::contextWindowBefore(const Vector<TextExtractionLineContent>& lines, unsigned targetIndex)
{
    Vector<String> window;
    window.reserveInitialCapacity(contextLineCount);
    for (unsigned offset = contextLineCount; offset >= 1; --offset) {
        if (offset > targetIndex)
            window.append(emptyString());
        else
            window.append(lines[targetIndex - offset].contentWithoutIdentifier);
    }
    return window;
}

Vector<String> TextExtractionCache::contextWindowAfter(const Vector<TextExtractionLineContent>& lines, unsigned targetIndex)
{
    unsigned lineCount = lines.size();
    Vector<String> window;
    window.reserveInitialCapacity(contextLineCount);
    for (unsigned offset = 1; offset <= contextLineCount; ++offset) {
        unsigned index = targetIndex + offset;
        if (index >= lineCount)
            window.append(emptyString());
        else
            window.append(lines[index].contentWithoutIdentifier);
    }
    return window;
}

auto TextExtractionCache::resolve(const String& identifier) const -> ResolvedNode
{
    if (m_entries.isEmpty())
        return { identifier, NodeResolution::Unknown };

    size_t newestIndex = m_entries.size() - 1;
    if (m_entries[newestIndex].lineIndexForUID.contains(identifier))
        return { identifier, NodeResolution::Current };

    Vector<size_t, maxEntries> sourceEntryIndices;
    for (size_t index = 0; index < newestIndex; ++index) {
        if (m_entries[index].lineIndexForUID.contains(identifier))
            sourceEntryIndices.append(index);
    }

    if (sourceEntryIndices.isEmpty())
        return { identifier, NodeResolution::Unknown };

    auto& newestURL = m_entries[newestIndex].url;
    for (size_t targetIndex = newestIndex; targetIndex > sourceEntryIndices.first(); --targetIndex) {
        auto& target = m_entries[targetIndex];
        if (target.url != newestURL)
            continue;

        HashSet<String> remappedIdentifiers;
        for (auto sourceIndex : sourceEntryIndices) {
            if (sourceIndex >= targetIndex)
                break;

            auto& source = m_entries[sourceIndex];
            auto sourceLineIndex = source.lineIndexForUID.get(identifier);
            auto sourceLineContent = source.lines[sourceLineIndex].contentWithoutIdentifier;
            auto sourceContextBefore = contextWindowBefore(source.lines, sourceLineIndex);
            auto sourceContextAfter = contextWindowAfter(source.lines, sourceLineIndex);

            for (unsigned candidate = 0; candidate < target.lines.size(); ++candidate) {
                if (target.lines[candidate].contentWithoutIdentifier != sourceLineContent)
                    continue;

                bool contextBeforeMatches = contextWindowBefore(target.lines, candidate) == sourceContextBefore;
                bool contextAfterMatches = contextWindowAfter(target.lines, candidate) == sourceContextAfter;
                if (!contextBeforeMatches && !contextAfterMatches)
                    continue;

                if (auto& remapped = target.lines[candidate].nodeIdentifier)
                    remappedIdentifiers.add(*remapped);
            }
        }

        if (remappedIdentifiers.isEmpty())
            continue;

        if (remappedIdentifiers.size() > 1)
            return { { }, NodeResolution::Ambiguous };

        return { *remappedIdentifiers.begin(), NodeResolution::Remapped };
    }

    return { { }, NodeResolution::Stale };
}

} // namespace WebKit
