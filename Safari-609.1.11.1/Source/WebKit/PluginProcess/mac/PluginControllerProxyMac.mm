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

#if ENABLE(NETSCAPE_PLUGIN_API)

#import "LayerHostingContext.h"
#import "PluginCreationParameters.h"
#import "PluginProcess.h"
#import "PluginProcessProxyMessages.h"
#import "PluginProxyMessages.h"
#import "WebProcessConnection.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/GraphicsContextCG.h>

namespace WebKit {

void PluginControllerProxy::pluginFocusOrWindowFocusChanged(bool pluginHasFocusAndWindowHasFocus)
{
    m_connection->connection()->send(Messages::PluginProxy::PluginFocusOrWindowFocusChanged(pluginHasFocusAndWindowHasFocus), m_pluginInstanceID);
}

void PluginControllerProxy::setComplexTextInputState(PluginComplexTextInputState pluginComplexTextInputState)
{
    m_connection->connection()->send(Messages::PluginProxy::SetComplexTextInputState(pluginComplexTextInputState), m_pluginInstanceID, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

const MachSendRight& PluginControllerProxy::compositingRenderServerPort()
{
    return PluginProcess::singleton().compositingRenderServerPort();
}

void PluginControllerProxy::platformInitialize(const PluginCreationParameters& creationParameters)
{
    ASSERT(!m_layerHostingContext);
    updateLayerHostingContext(creationParameters.parameters.layerHostingMode);
}

void PluginControllerProxy::platformDestroy()
{
    if (!m_layerHostingContext)
        return;

    m_layerHostingContext->invalidate();
    m_layerHostingContext = nullptr;
}

uint32_t PluginControllerProxy::remoteLayerClientID() const
{
    if (!m_layerHostingContext)
        return 0;

    return m_layerHostingContext->contextID();
}

void PluginControllerProxy::platformGeometryDidChange()
{
    CALayer *pluginLayer = m_plugin->pluginLayer();

    // We don't want to animate to the new size so we disable actions for this transaction.
    [CATransaction begin];
    [CATransaction setValue:@YES forKey:kCATransactionDisableActions];
    [pluginLayer setFrame:CGRectMake(0, 0, m_pluginSize.width(), m_pluginSize.height())];
    [CATransaction commit];
}

void PluginControllerProxy::windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates)
{
    m_plugin->windowAndViewFramesChanged(windowFrameInScreenCoordinates, viewFrameInWindowCoordinates);
}

void PluginControllerProxy::sendComplexTextInput(const String& textInput)
{
    m_plugin->sendComplexTextInput(textInput);
}

void PluginControllerProxy::setLayerHostingMode(uint32_t opaqueLayerHostingMode)
{
    LayerHostingMode layerHostingMode = static_cast<LayerHostingMode>(opaqueLayerHostingMode);

    m_plugin->setLayerHostingMode(layerHostingMode);
    updateLayerHostingContext(layerHostingMode);

    if (m_layerHostingContext)
        m_connection->connection()->send(Messages::PluginProxy::SetLayerHostingContextID(m_layerHostingContext->contextID()), m_pluginInstanceID);
}

void PluginControllerProxy::updateLayerHostingContext(LayerHostingMode layerHostingMode)
{
    CALayer *platformLayer = m_plugin->pluginLayer();
    if (!platformLayer)
        return;

    if (m_layerHostingContext) {
        if (m_layerHostingContext->layerHostingMode() == layerHostingMode)
            return;

        m_layerHostingContext->invalidate();
    }

    switch (layerHostingMode) {
        case LayerHostingMode::InProcess:
            m_layerHostingContext = LayerHostingContext::createForPort(PluginProcess::singleton().compositingRenderServerPort());
            break;
#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
        case LayerHostingMode::OutOfProcess:
            m_layerHostingContext = LayerHostingContext::createForExternalPluginHostingProcess();
            break;
#endif
    }

    m_layerHostingContext->setColorSpace(WebCore::sRGBColorSpaceRef());
    m_layerHostingContext->setColorMatchUntaggedContent(true);

    m_layerHostingContext->setRootLayer(platformLayer);
}

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
