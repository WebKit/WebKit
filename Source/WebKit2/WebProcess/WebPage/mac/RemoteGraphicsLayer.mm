/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "RemoteGraphicsLayer.h"

#include "RemoteLayerTreeTransaction.h"

#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebKit {

PassOwnPtr<GraphicsLayer> RemoteGraphicsLayer::create(GraphicsLayerClient* client, RemoteLayerTreeController* controller)
{
    return adoptPtr(new RemoteGraphicsLayer(client, controller));
}

RemoteGraphicsLayer::RemoteGraphicsLayer(GraphicsLayerClient* client, RemoteLayerTreeController* controller)
    : GraphicsLayer(client)
    , m_controller(controller)
    , m_uncommittedLayerChanges(RemoteLayerTreeTransaction::NoChange)
{
    // FIXME: This is in place to silence a compiler warning. Remove this
    // once we actually start using m_controller.
    (void)m_controller;
}

RemoteGraphicsLayer::~RemoteGraphicsLayer()
{
}

void RemoteGraphicsLayer::setName(const String& name)
{
    String longName = String::format("RemoteGraphicsLayer(%p) ", this) + name;
    GraphicsLayer::setName(longName);

    noteLayerPropertiesChanged(RemoteLayerTreeTransaction::NameChanged);
}

void RemoteGraphicsLayer::setNeedsDisplay()
{
    FloatRect hugeRect(-std::numeric_limits<float>::max() / 2, -std::numeric_limits<float>::max() / 2,
                       std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    setNeedsDisplayInRect(hugeRect);
}

void RemoteGraphicsLayer::setNeedsDisplayInRect(const FloatRect&)
{
    // FIXME: Implement this.
}

void RemoteGraphicsLayer::noteLayerPropertiesChanged(unsigned layerChanges)
{
    if (!m_uncommittedLayerChanges && m_client)
        m_client->notifyFlushRequired(this);
    m_uncommittedLayerChanges |= layerChanges;
}

} // namespace WebKit
