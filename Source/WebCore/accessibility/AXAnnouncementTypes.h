/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/AXID.h>
#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class NotifyPriority : uint8_t { Normal, High };

enum class InterruptBehavior : uint8_t { None, All, Pending };

enum class LiveRegionStatus : uint8_t { Off, Polite, Assertive };

enum class LiveRegionRelevant : uint8_t {
    Additions = 1 << 0,
    Removals  = 1 << 1,
    Text      = 1 << 2,
    All       = 1 << 3
};

enum class AnnouncementContents : bool { All, Changes };

struct LiveRegionObject {
    AXID objectID;
    String text;
    String language;
    HashSet<AXID> descendants; // For atomic regions only, to track additions/removals of descendants.
    bool isTextContent { false };
};

struct LiveRegionSnapshot {
    Vector<LiveRegionObject> objects;
    LiveRegionStatus liveRegionStatus { LiveRegionStatus::Off };
    OptionSet<LiveRegionRelevant> liveRegionRelevant { { LiveRegionRelevant::Additions, LiveRegionRelevant::Text } };
    // Every object's text concatenated with runs of whitespace collapsed. Cached because change
    // detection compares it on every update, and the previous snapshot's copy would otherwise be
    // rebuilt from scratch each time.
    String aggregatedText;
    // For each object, the offset just past the last character it contributed to aggregatedText, so
    // object i covers [end(i - 1), end(i)). Cached alongside the text because deriving it later would
    // mean rebuilding the whole aggregate, and it is only meaningful when isTruncated is false.
    Vector<size_t> objectEndOffsets;
    // Whether any object carries text.
    bool hasAnyText { false };
    // Whether the region contains an atomic region, which must be announced in its entirety. Not
    // derivable from an object's descendants, which are empty when its text comes from a label.
    bool hasAtomicRegion { false };
    // Whether this baseline was kept after the region went empty and that removal was already announced.
    // The baseline is deliberately not replaced by the empty state, so without this the same removal
    // would be announced again on every subsequent update that leaves the region empty.
    bool announcedEmptyRemoval { false };
    // Whether the region stayed empty long enough to count as genuinely cleared rather than mid-render.
    bool settledEmpty { false };
    // True when a limit stopped the walk, so this is an incomplete view of the region.
    bool isTruncated { false };
};

} // namespace WebCore
