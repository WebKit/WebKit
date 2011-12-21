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

#import "config.h"
#import "PluginControllerProxy.h"

#if ENABLE(PLUGIN_PROCESS)

#import <QuartzCore/QuartzCore.h>
#import "PluginProcess.h"
#import "WebKitSystemInterface.h"

using namespace WebCore;

namespace WebKit {

void PluginControllerProxy::platformInitialize()
{
    CALayer * platformLayer = m_plugin->pluginLayer();
    if (!platformLayer)
        return;

    ASSERT(!m_remoteLayerClient);

    m_remoteLayerClient = WKCARemoteLayerClientMakeWithServerPort(PluginProcess::shared().compositingRenderServerPort());
    ASSERT(m_remoteLayerClient);

    WKCARemoteLayerClientSetLayer(m_remoteLayerClient.get(), platformLayer);
}

void PluginControllerProxy::platformDestroy()
{
    if (!m_remoteLayerClient)
        return;

    WKCARemoteLayerClientInvalidate(m_remoteLayerClient.get());
    m_remoteLayerClient = nullptr;
}

uint32_t PluginControllerProxy::remoteLayerClientID() const
{
    if (!m_remoteLayerClient)
        return 0;

    return WKCARemoteLayerClientGetClientId(m_remoteLayerClient.get());
}

void PluginControllerProxy::platformGeometryDidChange()
{
    CALayer * pluginLayer = m_plugin->pluginLayer();

    // We don't want to animate to the new size so we disable actions for this transaction.
    [CATransaction begin];
    [CATransaction setValue:[NSNumber numberWithBool:YES] forKey:kCATransactionDisableActions];
    [pluginLayer setFrame:CGRectMake(0, 0, m_frameRect.width(), m_frameRect.height())];
    [CATransaction commit];
}

void PluginControllerProxy::windowFocusChanged(bool hasFocus)
{
    m_plugin->windowFocusChanged(hasFocus);
}

void PluginControllerProxy::windowAndViewFramesChanged(const IntRect& windowFrameInScreenCoordinates, const IntRect& viewFrameInWindowCoordinates)
{
    m_plugin->windowAndViewFramesChanged(windowFrameInScreenCoordinates, viewFrameInWindowCoordinates);
}

void PluginControllerProxy::windowVisibilityChanged(bool isVisible)
{
    m_plugin->windowVisibilityChanged(isVisible);
}

void PluginControllerProxy::sendComplexTextInput(const String& textInput)
{
    m_plugin->sendComplexTextInput(textInput);
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
