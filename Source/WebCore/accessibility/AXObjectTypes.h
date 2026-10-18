/*
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/AffineTransform.h>
#include <WebCore/IntPoint.h>
#include <WebCore/Node.h>
#include <optional>
#include <wtf/ProcessID.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class ContainerNode;

enum class AccessibilityMode : uint8_t {
    Off = 0,
    MainThread,
    AXThread,
    OffWasMainThread,
    OffWasAXThread,
};

inline bool isAccessibilityModeOff(AccessibilityMode mode)
{
    return mode == AccessibilityMode::Off
        || mode == AccessibilityMode::OffWasMainThread
        || mode == AccessibilityMode::OffWasAXThread;
}

// Ranks modes by how much accessibility is on (off, on, or on with an accessibility thread).
// Deliberately not the numeric order of AccessibilityMode, where OffWasMainThread and OffWasAXThread
// sort above AXThread despite both being off.
inline unsigned accessibilityModeRank(AccessibilityMode mode)
{
    if (isAccessibilityModeOff(mode))
        return 0;
    return mode == AccessibilityMode::AXThread ? 2 : 1;
}

std::optional<AccessibilityMode> LEGACY_NODELETE resolveAccessibilityModeTransition(AccessibilityMode current, AccessibilityMode requested);

enum class TextMarkerOrigin : uint16_t;

struct CharacterOffset {
    RefPtr<Node> node;
    int startIndex;
    int offset;
    int remainingOffset;

    CharacterOffset(Node* n = nullptr, int startIndex = 0, int offset = 0, int remaining = 0)
        : node(n)
        , startIndex(startIndex)
        , offset(offset)
        , remainingOffset(remaining)
    { }

    int remaining() const { return remainingOffset; }
    bool isNull() const { return !node; }
    inline bool isEqual(const CharacterOffset& other) const;
    inline String debugDescription();
};

struct VisiblePositionIndex {
    int value = -1;
    RefPtr<ContainerNode> scope;
};

struct VisiblePositionIndexRange {
    VisiblePositionIndex startIndex;
    VisiblePositionIndex endIndex;
    bool isNull() const { return startIndex.value == -1 || endIndex.value == -1; }
};

struct InheritedFrameState {
    bool isAXHidden { false };
    bool isInert { false };
    bool isRenderHidden { false };
};

// When this is updated, WebCoreArgumentCoders.serialization.in must be updated as well.
struct AXDebugInfo {
    AccessibilityMode accessibilityMode;
    String liveTree;
    String isolatedTree;
    Vector<String> warnings;
    uint64_t remoteTokenHash;
    uint64_t webProcessLocalTokenHash;
};

#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)
// Describes a frame's position and scale on screen for accessibility coordinate conversion.
// Sent from the UIProcess to the WebProcess via IPC whenever the frame scrolls, moves, or resizes.
// When this is updated, WebCoreArgumentCoders.serialization.in must be updated as well.
struct AXFrameGeometry {
    // The frame's content origin in screen coordinates.
    //   - Coordinate space: bottom-left on macOS, top-left on other platforms.
    //   - Points to: the top-left of the frame's document.
    //   - Units: display pixels.
    //   - Scroll: document-origin-based, so accounts for the frame's scroll position
    //     (e.g. scrolling down moves the document origin up on screen).
    // Element rects in content space compose with this directly to produce screen coordinates.
    IntPoint screenPosition;

    // Scale accounts for page zoom and device scale factor, among other things.
    // Applied to the element rect before adding screenPosition.
    AffineTransform screenTransform;
};
#endif

} // namespace WebCore
