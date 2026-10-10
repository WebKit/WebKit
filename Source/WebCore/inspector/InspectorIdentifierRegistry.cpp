/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "InspectorIdentifierRegistry.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "FrameDestructionObserverInlines.h"
#include "LocalFrameInlines.h"
#include "NavigationIdentifier.h"
#include "ProcessIdentifier.h"
#include "RemoteFrame.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <wtf/TZoneMallocInlines.h>

namespace Inspector {

WTF_MAKE_TZONE_ALLOCATED_IMPL(IdentifierRegistry);
WTF_MAKE_TZONE_ALLOCATED_IMPL(LegacyIdentifierRegistry);

// The WebContent process that hosts a frame's content: the current process for a
// LocalFrame, or the RemoteFrame's recorded hosting process for a remote stub. Both
// UIProcess and WebContent feed this into protocolFrameId() so they compute matching
// IDs for the same frame regardless of which process holds it. See webkit.org/b/310164.
static WebCore::ProcessIdentifier hostingProcessForFrame(const WebCore::Frame& frame)
{
    if (auto* remoteFrame = dynamicDowncast<WebCore::RemoteFrame>(frame))
        return remoteFrame->hostingProcessIdentifier();
    return WebCore::Process::identifier();
}

Protocol::Network::LoaderId IdentifierRegistry::loaderId(const WebCore::DocumentLoader* loader) const
{
    if (!loader)
        return emptyString();

    // Derive the id from the loader's navigationID, which is assigned at provisional-load start and
    // stays fixed for the load. The Network stream computes this at the main resource's
    // willSendRequest (before commit) and the Page stream at frameNavigated (after commit); anchoring
    // on navigationID makes both arrive at the same string. It is qualified with the process that
    // minted it, which may be the UIProcess or this process.
    auto navigationID = loader->navigationID();
    if (!navigationID)
        return emptyString();

    return makeString("loader-"_s, navigationID->processIdentifier().toUInt64(), '.', navigationID->object().toUInt64());
}

LegacyIdentifierRegistry::LegacyIdentifierRegistry() = default;
LegacyIdentifierRegistry::~LegacyIdentifierRegistry() = default;

WebCore::Frame* LegacyIdentifierRegistry::frameForId(const Protocol::Network::FrameId& frameId)
{
    return frameId.isEmpty() ? nullptr : m_identifierToFrame.get(frameId);
}

Protocol::Network::FrameId LegacyIdentifierRegistry::frameId(const WebCore::Frame* frame)
{
    if (!frame)
        return emptyString();
    return m_frameToIdentifier.ensure(*frame, [this, frame] {
        auto identifier = IdentifiersFactory::createIdentifier();
        m_identifierToFrame.set(identifier, frame);
        return identifier;
    }).iterator->value;
}

RefPtr<WebCore::LocalFrame> LegacyIdentifierRegistry::assertFrame(Protocol::ErrorString& errorString, const Protocol::Network::FrameId& frameId)
{
    RefPtr frame = dynamicDowncast<WebCore::LocalFrame>(frameForId(frameId));
    if (!frame)
        errorString = "Missing frame for given frameId"_s;
    return frame;
}

Protocol::Network::FrameId LegacyIdentifierRegistry::takeFrame(const WebCore::Frame& frame)
{
    auto identifier = m_frameToIdentifier.take(frame);
    if (!identifier.isNull())
        m_identifierToFrame.remove(identifier);
    return identifier;
}

// --- BackendIdentifierRegistry ---

WTF_MAKE_TZONE_ALLOCATED_IMPL(BackendIdentifierRegistry);

BackendIdentifierRegistry::BackendIdentifierRegistry() = default;
BackendIdentifierRegistry::~BackendIdentifierRegistry() = default;

Protocol::Network::FrameId BackendIdentifierRegistry::frameId(const WebCore::Frame* frame)
{
    if (!frame)
        return emptyString();
    auto identifier = protocolFrameId(frame->frameID(), hostingProcessForFrame(*frame));
    m_identifierToFrame.set(identifier, frame);
    return identifier;
}

WebCore::Frame* BackendIdentifierRegistry::frameForId(const Protocol::Network::FrameId& frameId)
{
    return frameId.isEmpty() ? nullptr : m_identifierToFrame.get(frameId);
}

RefPtr<WebCore::LocalFrame> BackendIdentifierRegistry::assertFrame(Protocol::ErrorString& errorString, const Protocol::Network::FrameId& frameId)
{
    RefPtr frame = dynamicDowncast<WebCore::LocalFrame>(frameForId(frameId));
    if (!frame)
        errorString = "Missing frame for given frameId"_s;
    return frame;
}

Protocol::Network::FrameId BackendIdentifierRegistry::takeFrame(const WebCore::Frame& frame)
{
    auto identifier = protocolFrameId(frame.frameID(), hostingProcessForFrame(frame));
    m_identifierToFrame.remove(identifier);
    return identifier;
}

} // namespace Inspector
