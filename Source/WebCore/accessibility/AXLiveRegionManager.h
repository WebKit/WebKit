/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Platform.h>
#if PLATFORM(COCOA)

#include "AXCoreObject.h"
#include "AttributedString.h"
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class AccessibilityObject;
class AXObjectCache;
enum class LiveRegionStatus : uint8_t;

enum class LiveRegionRelevant : uint8_t {
    Additions = 1 << 0,
    Removals  = 1 << 1,
    Text      = 1 << 2,
    All       = 1 << 3
};

struct LiveRegionObject {
    AXID objectID;
    String text;
    String language;
};

struct LiveRegionSnapshot {
    Vector<LiveRegionObject> objects;
    LiveRegionStatus liveRegionStatus { LiveRegionStatus::Off };
    OptionSet<LiveRegionRelevant> liveRegionRelevant { { LiveRegionRelevant::Additions, LiveRegionRelevant::Text } };
};

enum class AnnouncementContents : bool { All, Changes };

class AXLiveRegionManager {
    WTF_MAKE_NONCOPYABLE(AXLiveRegionManager);
    WTF_MAKE_TZONE_ALLOCATED(AXLiveRegionManager);
public:
    explicit AXLiveRegionManager(AXObjectCache&);
    ~AXLiveRegionManager() = default;

    void registerLiveRegion(AccessibilityObject&, bool = false);
    void unregisterLiveRegion(AXID axID) { m_liveRegions.remove(axID); }

    void handleLiveRegionChange(AccessibilityObject&, AnnouncementContents = AnnouncementContents::Changes);
private:
    struct LiveRegionDiff {
        Vector<LiveRegionObject> added;
        Vector<LiveRegionObject> removed;
        Vector<LiveRegionObject> changed;
    };

    LiveRegionSnapshot buildLiveRegionSnapshot(AccessibilityObject&) const;
    bool shouldIncludeInSnapshot(AccessibilityObject&) const;
    String textForObject(AccessibilityObject&) const;
    void postAnnouncementForChange(AccessibilityObject&, const LiveRegionSnapshot&, const LiveRegionSnapshot&);
    LiveRegionDiff computeChanges(const Vector<LiveRegionObject>&, const Vector<LiveRegionObject>&) const;
    AttributedString computeAnnouncement(const LiveRegionSnapshot&, const LiveRegionDiff&) const;

    CheckedRef<AXObjectCache> m_cache;
    HashMap<AXID, LiveRegionSnapshot> m_liveRegions;
};

} // namespace WebCore

#endif // PLATFORM(COCOA)
