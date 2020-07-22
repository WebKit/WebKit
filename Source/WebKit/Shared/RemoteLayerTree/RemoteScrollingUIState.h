/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebCore {
using ScrollingNodeID = uint64_t;
}

namespace WebKit {

class RemoteScrollingUIState {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Changes : uint8_t {
        ScrollSnapNodes     = 1 << 0,
        UserScrollNodes     = 1 << 1,
    };

    void encode(IPC::Encoder&) const;
    static Optional<RemoteScrollingUIState> decode(IPC::Decoder&);

    OptionSet<Changes> changes() const { return m_changes; }
    void clearChanges() { m_changes = { }; }
    
    void reset();

    const HashSet<WebCore::ScrollingNodeID>& nodesWithActiveScrollSnap() const { return m_nodesWithActiveScrollSnap; }
    void addNodeWithActiveScrollSnap(WebCore::ScrollingNodeID);
    void removeNodeWithActiveScrollSnap(WebCore::ScrollingNodeID);
    
    const HashSet<WebCore::ScrollingNodeID>& nodesWithActiveUserScrolls() const { return m_nodesWithActiveUserScrolls; }
    void addNodeWithActiveUserScroll(WebCore::ScrollingNodeID);
    void removeNodeWithActiveUserScroll(WebCore::ScrollingNodeID);

private:
    OptionSet<Changes> m_changes;
    HashSet<WebCore::ScrollingNodeID> m_nodesWithActiveScrollSnap;
    HashSet<WebCore::ScrollingNodeID> m_nodesWithActiveUserScrolls;
};

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::RemoteScrollingUIState::Changes> {
    using values = EnumValues<
        WebKit::RemoteScrollingUIState::Changes,
        WebKit::RemoteScrollingUIState::Changes::ScrollSnapNodes,
        WebKit::RemoteScrollingUIState::Changes::UserScrollNodes
    >;
};

} // namespace WTF
