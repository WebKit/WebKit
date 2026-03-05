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

#pragma once

#include <WebCore/AXID.h>
#include <WebCore/AXLoggerBase.h>
#include <wtf/FixedVector.h>

namespace WebCore {

// This class represents a stitch-group. All members in a stitch-group will not
// be exposed in the outwardly facing accessibility tree except for the group
// representative (m_representativeID), who will respond to accessibility APIs
// using the stitched representation of all members of the group.
//
// The intention of this is to expose a simpler accessibility tree, making
// the job of assistive technologies easier, and the lives of users better, e.g.
// by reducing the number of navigation stops an AT user has to make.
//
// Stitch groups are per-block-flow element, so only nodes within the same flow
// can be stitched together. Stitch group membership is maintained by the
// AXObjectCache on the main-thread, and stored as an AXProperty on each block-flow
// AXIsolatedObject on the accessibility thread. A stitch group is only valid if it
// has 2 or more elements within it — a group of one makes no sense.
class AXStitchGroup {
public:
    explicit AXStitchGroup(const Vector<AXID>& members, AXID representativeID)
        : m_members(members)
        , m_representativeID(representativeID)
    {
        AX_ASSERT(isValid());
    }

    explicit AXStitchGroup(Vector<AXID>&& members, AXID representativeID)
        : m_members(WTF::move(members))
        , m_representativeID(representativeID)
    {
        AX_ASSERT(isValid());
    }

    bool isEmpty() const { return m_members.isEmpty(); }
    bool isValid() const
    {
        // Some usages of this class intentionally create an instance containing only
        // the representativeID to avoid an unneeded copy of m_members, so empty m_members
        // can be a valid state.
        return m_members.isEmpty() || m_members.contains(representativeID());
    }

    const FixedVector<AXID>& members() const { return m_members; }
    AXID representativeID() const { return m_representativeID; }

private:
    FixedVector<AXID> m_members;
    // The ID of the object that will be exposed in the accessibility tree.
    AXID m_representativeID;
};

} // namespace WebCore

