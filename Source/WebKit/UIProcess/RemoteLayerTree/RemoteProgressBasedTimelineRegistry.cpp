/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RemoteProgressBasedTimelineRegistry.h"

#if ENABLE(THREADED_ANIMATIONS)

#import "RemoteScrollingTree.h"
#import <WebCore/ScrollingTreeScrollingNode.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteProgressBasedTimelineRegistry);

void RemoteProgressBasedTimelineRegistry::update(const RemoteScrollingTree& scrollingTree, WebCore::ProcessIdentifier processIdentifier, const HashSet<Ref<WebCore::AcceleratedTimeline>>& timelineRepresentations)
{
    // If there are no active timelines for this process identifier, simply remove that entry.
    if (timelineRepresentations.isEmpty()) {
        m_timelines.remove(processIdentifier);
        return;
    }

    auto& processTimelines = m_timelines.ensure(processIdentifier, [] {
        return UncheckedKeyHashMap<WebCore::ScrollingNodeID, HashSet<Ref<RemoteProgressBasedTimeline>>>();
    }).iterator->value;

    for (auto& timelineRepresentation : timelineRepresentations) {
        if (!timelineRepresentation->isProgressBased())
            continue;

        TimelineID timelineID { timelineRepresentation->identifier(), processIdentifier };
        ASSERT(timelineRepresentation->progressResolutionData());
        auto resolutionData = *timelineRepresentation->progressResolutionData();

        // FIXME: we should make it so by the time this function is called we're
        // guaranteed to have a matching source node and turn this into a Ref.
        RefPtr sourceNode = dynamicDowncast<WebCore::ScrollingTreeScrollingNode>(Ref { scrollingTree }->nodeForID(resolutionData.source));

        auto done = false;

        auto sourceIterator = processTimelines.find(resolutionData.source);
        if (sourceIterator != processTimelines.end()) {
            auto& existingTimelines = sourceIterator->value;
            for (auto& existingTimeline : existingTimelines) {
                if (existingTimeline->identifier() == timelineID) {
                    existingTimeline->setResolutionData(sourceNode.get(), resolutionData);
                    done = true;
                    break;
                }
            }
        }

        if (done)
            continue;

        // If we didn't find the timeline within the timelines for this source, it may have been
        // associated with another source.
        for (auto& [source, existingTimelines] : processTimelines) {
            // We've already looked at the exisiting source.
            if (source == resolutionData.source)
                continue;

            auto matchingTimelines = existingTimelines.takeIf([&](const auto& existingTimeline) {
                return existingTimeline->identifier() == timelineID;
            });

            if (matchingTimelines.isEmpty())
                continue;

            ASSERT(matchingTimelines.size() == 1);
            auto& existingTimeline = matchingTimelines[0];
            existingTimeline->setResolutionData(sourceNode.get(), resolutionData);
            // Move it to the right source.
            processTimelines.ensure(existingTimeline->source(), [] {
                return HashSet<Ref<RemoteProgressBasedTimeline>> { };
            }).iterator->value.add(existingTimeline.releaseNonNull());
            done = true;
            break;
        }

        if (done)
            continue;

        // If we get to this point, we're dealing with a new timeline.
        auto timeline = RemoteProgressBasedTimeline::create(timelineID, resolutionData);
        processTimelines.ensure(timeline->source(), [] {
            return HashSet<Ref<RemoteProgressBasedTimeline>> { };
        }).iterator->value.add(WTFMove(timeline));
    }
}

RemoteProgressBasedTimeline* RemoteProgressBasedTimelineRegistry::get(const TimelineID& timelineID) const
{
    auto it = m_timelines.find(timelineID.processIdentifier());
    if (it == m_timelines.end())
        return nullptr;

    for (auto& sourceTimelines : it->value.values()) {
        for (auto& timeline : sourceTimelines) {
            if (timeline->identifier() == timelineID)
                return timeline.ptr();
        }
    }

    return nullptr;
}

bool RemoteProgressBasedTimelineRegistry::hasTimelineForNode(const WebCore::ScrollingTreeScrollingNode& node) const
{
    auto scrollingNodeID = node.scrollingNodeID();
    auto it = m_timelines.find(scrollingNodeID.processIdentifier());
    return it != m_timelines.end() && it->value.contains(scrollingNodeID);
}

void RemoteProgressBasedTimelineRegistry::updateTimelinesForNode(const WebCore::ScrollingTreeScrollingNode& node)
{
    auto processIterator = m_timelines.find(node.scrollingNodeID().processIdentifier());
    if (processIterator == m_timelines.end())
        return;

    auto& processTimelines = processIterator->value;
    auto sourceIterator = processTimelines.find(node.scrollingNodeID());
    if (sourceIterator != processTimelines.end()) {
        for (auto& timeline : sourceIterator->value)
            timeline->updateCurrentTime(node);
    }
}

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATIONS)
