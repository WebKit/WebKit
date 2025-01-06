/*
 * Copyright (C) 2024 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "RefTrackerMixin.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

#if ENABLE(REFTRACKER)

void RefTracker::reportLive(void* id)
{
    RELEASE_ASSERT(id);
    Locker locker(lock);

    std::unique_ptr<StackShot> stack = nullptr;
    if (!loggingDisabledDepth.load())
        stack = makeUnique<StackShot>(16);
    RELEASE_ASSERT(map.add(id, WTFMove(stack)).isNewEntry);
}

void RefTracker::reportDead(void* id)
{
    RELEASE_ASSERT(id);
    Locker locker(lock);
    if (!map.contains(id)) {
        WTFReportBacktrace();
        WTFLogAlways("******************************************** Dead RefTracker was never live: %p", id);
    }
    RELEASE_ASSERT(map.contains(id));
    RELEASE_ASSERT(map.remove(id));
}

void RefTracker::logAllLiveReferences()
{
    static constexpr int framesToSkip = 3;
    Locker locker(lock);
    auto keysIterator = map.keys();
    auto keys = std::vector<void*> { keysIterator.begin(), keysIterator.end() };
    std::sort(keys.begin(), keys.end());
    for (auto& k : keys) {
        auto* v = map.get(k);
        if (!v)
            continue;
        dataLogLn(StackTracePrinter { { v->array() + framesToSkip, v->size() - framesToSkip } });
        dataLogLn("\n---\n");
    }
}

#endif // ENABLE(REFTRACKER)

} // namespace WTF

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
