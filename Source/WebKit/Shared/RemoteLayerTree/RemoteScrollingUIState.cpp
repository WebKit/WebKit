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

#import "config.h"
#import "RemoteScrollingUIState.h"

#import "ArgumentCoders.h"
#import <wtf/text/TextStream.h>

namespace WebKit {

void RemoteScrollingUIState::encode(IPC::Encoder& encoder) const
{
    encoder.encode(m_changes);

    if (m_changes.contains(Changes::ScrollSnapNodes))
        encoder << m_nodesWithActiveScrollSnap;

    if (m_changes.contains(Changes::UserScrollNodes))
        encoder << m_nodesWithActiveUserScrolls;
}

Optional<RemoteScrollingUIState> RemoteScrollingUIState::decode(IPC::Decoder& decoder)
{
    RemoteScrollingUIState uiState;

    if (!decoder.decode(uiState.m_changes))
        return WTF::nullopt;

    if (uiState.m_changes.contains(Changes::ScrollSnapNodes)) {
        if (!decoder.decode(uiState.m_nodesWithActiveScrollSnap))
            return WTF::nullopt;
    }

    if (uiState.m_changes.contains(Changes::UserScrollNodes)) {
        if (!decoder.decode(uiState.m_nodesWithActiveUserScrolls))
            return WTF::nullopt;
    }

    return uiState;
}

void RemoteScrollingUIState::reset()
{
    clearChanges();
    m_nodesWithActiveScrollSnap.clear();
    m_nodesWithActiveUserScrolls.clear();
}

void RemoteScrollingUIState::addNodeWithActiveScrollSnap(WebCore::ScrollingNodeID nodeID)
{
    auto addResult = m_nodesWithActiveScrollSnap.add(nodeID);
    if (addResult.isNewEntry)
        m_changes.add(Changes::ScrollSnapNodes);
}

void RemoteScrollingUIState::removeNodeWithActiveScrollSnap(WebCore::ScrollingNodeID nodeID)
{
    if (m_nodesWithActiveScrollSnap.remove(nodeID))
        m_changes.add(Changes::ScrollSnapNodes);
}

void RemoteScrollingUIState::addNodeWithActiveUserScroll(WebCore::ScrollingNodeID nodeID)
{
    auto addResult = m_nodesWithActiveUserScrolls.add(nodeID);
    if (addResult.isNewEntry)
        m_changes.add(Changes::UserScrollNodes);
}

void RemoteScrollingUIState::removeNodeWithActiveUserScroll(WebCore::ScrollingNodeID nodeID)
{
    if (m_nodesWithActiveUserScrolls.remove(nodeID))
        m_changes.add(Changes::UserScrollNodes);
}

} // namespace WebKit
