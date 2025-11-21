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

#include "config.h"
#include "TrackSizingAlgorithm.h"

#include "LayoutIntegrationUtils.h"
#include "NotImplemented.h"
#include "PlacedGridItem.h"
#include "TrackSizingFunctions.h"
#include <wtf/Range.h>
#include <wtf/Vector.h>
#include <wtf/ZippedRange.h>

namespace WebCore {
namespace Layout {

struct UnsizedTrack {
    LayoutUnit baseSize;
    LayoutUnit growthLimit;
    const TrackSizingFunctions trackSizingFunction;
};

static auto singleSpanningItemsWithinTrack(size_t trackIndex, const PlacedGridItems& gridItems, const PlacedGridItemSpanList& gridItemSpanList)
{
    PlacedGridItems nonSpanningItems;
    for (auto [gridItemIndex, gridItem] : WTF::indexedRange(gridItems)) {
        auto& gridItemSpan = gridItemSpanList[gridItemIndex];
        if (gridItemSpan.distance() == 1 && gridItemSpan.begin() == trackIndex)
            nonSpanningItems.append(gridItem);
    }
    return nonSpanningItems;
}

// https://drafts.csswg.org/css-grid-1/#algo-content
static void resolveIntrinsicTrackSizes(UnsizedTracks& unsizedTracks, const PlacedGridItems& gridItems, const PlacedGridItemSpanList& gridItemSpanList,
    const IntegrationUtils& integrationUtils)
{
    // 1. Shim baseline-aligned items so their intrinsic size contributions reflect their
    // baseline alignment.
    auto shimBaselineAligedItems = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(shimBaselineAligedItems);

    using IndexedTrackList = Vector<std::pair<unsigned, UnsizedTrack&>>;
    auto tracksWithIntrinsicSizingFunction = [&] {
        IndexedTrackList trackList;

        for (auto [index, track] : WTF::indexedRange(unsizedTracks)) {
            auto& minimumTrackSizingFunction = track.trackSizingFunction.min;
            auto& maximumTrackSizingFunction = track.trackSizingFunction.max;

            if (minimumTrackSizingFunction.isFlex() || maximumTrackSizingFunction.isFlex())
                continue;
            if (minimumTrackSizingFunction.isContentSized() || maximumTrackSizingFunction.isContentSized())
                trackList.append({ index, track });
        }
        return trackList;
    };

    // 2. Size tracks to fit non-spanning items.
    // For each track with an intrinsic track sizing function and not a flexible sizing function, consider the items in it with a span of 1:
    for (auto& [trackIndex, track] : tracksWithIntrinsicSizingFunction()) {
        auto singleSpanningItems = singleSpanningItemsWithinTrack(trackIndex, gridItems, gridItemSpanList);

        auto& minimumTrackSizingFunction = track.trackSizingFunction.min;
        track.baseSize = WTF::switchOn(minimumTrackSizingFunction,
            // If the track has a min-content min track sizing function, set its base size
            // to the maximum of the items’ min-content contributions, floored at zero.
            [&](const CSS::Keyword::MinContent&) {
                auto minContentContributions = singleSpanningItems | std::views::transform([&](const PlacedGridItem& gridItem) {
                    return integrationUtils.minContentWidth(gridItem.layoutBox());
                });
                return std::ranges::max(minContentContributions);
            },
            [&](const CSS::Keyword::MaxContent&) -> LayoutUnit {
                ASSERT_NOT_IMPLEMENTED_YET();
                return { };
            },
            [&](const CSS::Keyword::Auto&) -> LayoutUnit {
                ASSERT_NOT_IMPLEMENTED_YET();
                return { };
            },
            [&](const auto&) -> LayoutUnit {
                ASSERT_NOT_REACHED();
                return { };
            }
        );

        auto& maximumTrackSizingFunction = track.trackSizingFunction.max;
        track.growthLimit = WTF::switchOn(maximumTrackSizingFunction,
            [&](const CSS::Keyword::MinContent&) -> LayoutUnit {
                // If the track has a min-content max track sizing function, set its growth
                // limit to the maximum of the items’ min-content contributions.
                auto minContentContributions = singleSpanningItems | std::views::transform([&](const PlacedGridItem& gridItem) {
                    return integrationUtils.minContentWidth(gridItem.layoutBox());
                });
                return std::ranges::max(minContentContributions);
            },
            [&](const CSS::Keyword::MaxContent&) -> LayoutUnit {
                ASSERT_NOT_IMPLEMENTED_YET();
                return { };
            },
            [&](const CSS::Keyword::Auto&) -> LayoutUnit {
                ASSERT_NOT_IMPLEMENTED_YET();
                return { };
            },
            [&](const auto&) -> LayoutUnit {
                ASSERT_NOT_REACHED();
                return { };
            }
        );
    }

    // 3. Increase sizes to accommodate spanning items crossing content-sized tracks:
    // Next, consider the items with a span of 2 that do not span a track with a flexible
    // sizing function.
    auto increaseSizesToAccommodateSpanningItemsCrosszingContentSizedTracks = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(increaseSizesToAccommodateSpanningItemsCrosszingContentSizedTracks);

    // 4. Increase sizes to accommodate spanning items crossing flexible tracks:
    auto increaseSizesToAccommodateSpanningItemsCrossingFlexibleTracks = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(increaseSizesToAccommodateSpanningItemsCrossingFlexibleTracks);


    // 5. If any track still has an infinite growth limit, set its growth limit to its base size.
    auto setInfiniteGrowthLimitsToBaseSize = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(setInfiniteGrowthLimitsToBaseSize);
}


// https://drafts.csswg.org/css-grid-1/#algo-track-sizing
TrackSizes TrackSizingAlgorithm::sizeTracks(const PlacedGridItems& gridItems, const PlacedGridItemSpanList& gridItemSpanList, const TrackSizingFunctionsList& trackSizingFunctions, const IntegrationUtils& integrationUtils)
{
    // 1. Initialize Track Sizes
    auto unsizedTracks = initializeTrackSizes(trackSizingFunctions);

    // 2. Resolve Intrinsic Track Sizes
    resolveIntrinsicTrackSizes(unsizedTracks, gridItems, gridItemSpanList, integrationUtils);

    // 3. Maximize Tracks
    auto maximizeTracks = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(maximizeTracks);

    // 4. Expand Flexible Tracks
    auto expandFlexibleTracks = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(expandFlexibleTracks);

    // 5. Expand Stretched auto Tracks
    auto expandStretchedAutoTracks = [] {
        notImplemented();
    };
    UNUSED_VARIABLE(expandStretchedAutoTracks);

    // Each track has a base size, a <length> which grows throughout the algorithm and
    // which will eventually be the track’s final size...
    return unsizedTracks.map([](const UnsizedTrack& unsizedTrack) {
        return unsizedTrack.baseSize;
    });
}

// https://www.w3.org/TR/css-grid-1/#algo-init
UnsizedTracks TrackSizingAlgorithm::initializeTrackSizes(const TrackSizingFunctionsList& trackSizingFunctionsList)
{
    return trackSizingFunctionsList.map([](const TrackSizingFunctions& trackSizingFunctions) -> UnsizedTrack {
        // For each track, if the track’s min track sizing function is:
        auto baseSize = [&] -> LayoutUnit {
            auto& minTrackSizingFunction = trackSizingFunctions.min;

            // A fixed sizing function
            // Resolve to an absolute length and use that size as the track’s initial base size.
            if (minTrackSizingFunction.isLength()) {
                auto& trackBreadthLength = minTrackSizingFunction.length();
                if (auto fixedValue = trackBreadthLength.tryFixed())
                    return LayoutUnit { fixedValue->resolveZoom(Style::ZoomNeeded { }) };

                if (auto percentValue = trackBreadthLength.tryPercentage()) {
                    ASSERT_NOT_IMPLEMENTED_YET();
                    return { };
                }

            }

            // An intrinsic sizing function
            // Use an initial base size of zero.
            if (minTrackSizingFunction.isContentSized())
                return { };

            ASSERT_NOT_REACHED();
            return { };
        };

        // For each track, if the track’s max track sizing function is:
        auto growthLimit = [&] -> LayoutUnit {
            auto& maxTrackSizingFunction = trackSizingFunctions.max;

            // A fixed sizing function
            // Resolve to an absolute length and use that size as the track’s initial growth limit.
            if (maxTrackSizingFunction.isLength()) {
                auto trackBreadthLength = maxTrackSizingFunction.length();
                if (auto fixedValue = trackBreadthLength.tryFixed())
                    return LayoutUnit { fixedValue->resolveZoom(Style::ZoomNeeded { }) };
            }

            // An intrinsic sizing function
            // A flexible sizing function
            // Use an initial growth limit of infinity.
            if (maxTrackSizingFunction.isContentSized() || maxTrackSizingFunction.isFlex())
                return LayoutUnit::max();

            ASSERT_NOT_REACHED();
            return { };
        };

        return { baseSize(), growthLimit(), trackSizingFunctions };
    });
}

} // namespace Layout
} // namespace WebCore
