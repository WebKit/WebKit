/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#if ENABLE(PLUGIN_PROCESS)

#include "PluginControllerProxy.h"

#include "NotImplemented.h"
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

PassOwnPtr<PluginControllerProxy> PluginControllerProxy::create(WebProcessConnection* connection, uint64_t pluginInstanceID)
{
    return adoptPtr(new PluginControllerProxy(connection, pluginInstanceID));
}

PluginControllerProxy::PluginControllerProxy(WebProcessConnection* connection, uint64_t pluginInstanceID)
    : m_connection(connection)
    , m_pluginInstanceID(pluginInstanceID)
{
}

PluginControllerProxy::~PluginControllerProxy()
{
    ASSERT(!m_plugin);
}

void PluginControllerProxy::invalidate(const IntRect&)
{
    notImplemented();
}

String PluginControllerProxy::userAgent(const WebCore::KURL&)
{
    notImplemented();
    return String();
}

void PluginControllerProxy::loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, const HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups)
{
    notImplemented();
}

void PluginControllerProxy::cancelStreamLoad(uint64_t streamID)
{
    notImplemented();
}

void PluginControllerProxy::cancelManualStreamLoad()
{
    notImplemented();
}

NPObject* PluginControllerProxy::windowScriptNPObject()
{
    notImplemented();
    return 0;
}

NPObject* PluginControllerProxy::pluginElementNPObject()
{
    notImplemented();
    return 0;
}

bool PluginControllerProxy::evaluate(NPObject*, const String& scriptString, NPVariant* result, bool allowPopups)
{
    notImplemented();
    return false;
}

void PluginControllerProxy::setStatusbarText(const WTF::String&)
{
    notImplemented();
}

bool PluginControllerProxy::isAcceleratedCompositingEnabled()
{
    notImplemented();
    return false;
}

void PluginControllerProxy::pluginProcessCrashed()
{
    notImplemented();
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
