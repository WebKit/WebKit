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

void RemoteProgressBasedTimelineRegistry::update(const RemoteScrollingTree& scrollingTree, WebCore::ProcessIdentifier processIdentifier, const WebCore::AcceleratedTimelinesUpdate& timelinesUpdate)
{
    auto& processTimelines = m_timelines.ensure(processIdentifier, [] {
        return UncheckedKeyHashMap<WebCore::ScrollingNodeID, HashSet<Ref<RemoteProgressBasedTimeline>>>();
    }).iterator->value;

    using IterationCallback = const Function<void(const TimelineID&, WebCore::ProgressResolutionData)>;
    auto forEachProgressBasedTimelineRepresentation = [&](const HashSet<Ref<WebCore::AcceleratedTimeline>>& timelineRepresentations, NOESCAPE IterationCallback& function) {
        if (timelineRepresentations.isEmpty())
            return;
        for (auto& timelineRepresentation : timelineRepresentations) {
            if (!timelineRepresentation->isProgressBased())
                continue;
            ASSERT(timelineRepresentation->progressResolutionData());
            TimelineID timelineID { timelineRepresentation->identifier(), processIdentifier };
            function(timelineID, *timelineRepresentation->progressResolutionData());
        }
    };

    forEachProgressBasedTimelineRepresentation(timelinesUpdate.created, [&](auto timelineID, auto resolutionData) {
        auto& sourceTimelines = processTimelines.ensure(resolutionData.source, [] {
            return HashSet<Ref<RemoteProgressBasedTimeline>>();
        }).iterator->value;
        // There should not be a pre-existing timeline for this identifier since we're creating it.
        ASSERT([&] {
            for (auto& existingTimeline : sourceTimelines) {
                if (existingTimeline->identifier() == timelineID)
                    return false;
            }
            return true;
        }());
        sourceTimelines.add(RemoteProgressBasedTimeline::create(timelineID, resolutionData));
    });

    forEachProgressBasedTimelineRepresentation(timelinesUpdate.modified, [&](auto timelineID, auto resolutionData) {
        // FIXME: we should make it so that when we call this function we're
        // guaranteed to have a matching source node and turn this into a Ref.
        RefPtr sourceNode = dynamicDowncast<WebCore::ScrollingTreeScrollingNode>(Ref { scrollingTree }->nodeForID(resolutionData.source));

        auto sourceIterator = processTimelines.find(resolutionData.source);
        if (sourceIterator != processTimelines.end()) {
            auto& existingTimelines = sourceIterator->value;
            for (auto& existingTimeline : existingTimelines) {
                if (existingTimeline->identifier() == timelineID) {
                    existingTimeline->setResolutionData(sourceNode.get(), resolutionData);
                    return;
                }
            }
        }

        // If we didn't find the timeline within the existing timelines for this source,
        // it may previously have been associated with another source.
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
                return HashSet<Ref<RemoteProgressBasedTimeline>>();
            }).iterator->value.add(existingTimeline.releaseNonNull());
            return;
        }

        // If we reach this point, we've been told to update a timeline that did not exist.
        ASSERT_NOT_REACHED();
    });

    HashSet<WebCore::ScrollingNodeID> emptySources;
    for (auto& destroyedTimelineIdentifier : timelinesUpdate.destroyed) {
        TimelineID destroyedTimelineID { destroyedTimelineIdentifier, processIdentifier };
        for (auto& [source, existingTimelines] : processTimelines) {
            existingTimelines.removeIf([destroyedTimelineID](auto& existingTimeline) {
                return existingTimeline->identifier() == destroyedTimelineID;
            });
            if (existingTimelines.isEmpty())
                emptySources.add(source);
        }
    }

    for (auto& emptySource : emptySources)
        processTimelines.remove(emptySource);

    if (processTimelines.isEmpty())
        m_timelines.remove(processIdentifier);
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

HashSet<Ref<RemoteProgressBasedTimeline>> RemoteProgressBasedTimelineRegistry::timelinesForScrollingNodeIDForTesting(WebCore::ScrollingNodeID scrollingNodeID) const
{
    auto processIterator = m_timelines.find(scrollingNodeID.processIdentifier());
    if (processIterator == m_timelines.end())
        return { };
    auto& processTimelines = processIterator->value;
    auto sourceIterator = processTimelines.find(scrollingNodeID);
    if (sourceIterator == processTimelines.end())
        return { };
    return sourceIterator->value;
}

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATIONS)
