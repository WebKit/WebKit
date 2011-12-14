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
#import "PluginProxy.h"

#if ENABLE(PLUGIN_PROCESS)

#import <WebKitSystemInterface.h>

namespace WebKit {

PlatformLayer* PluginProxy::pluginLayer()
{
    if (!m_pluginLayer && m_remoteLayerClientID) {
        CALayer *renderLayer = WKMakeRenderLayer(m_remoteLayerClientID);

        // Create a layer with flipped geometry and add the real plug-in layer as a sublayer
        // so the coordinate system will match the event coordinate system.
        m_pluginLayer.adoptNS([[CALayer alloc] init]);
        [m_pluginLayer.get() setGeometryFlipped:YES];
        
        [renderLayer setAutoresizingMask:kCALayerWidthSizable | kCALayerHeightSizable];
        [m_pluginLayer.get() addSublayer:renderLayer];
    }

    return m_pluginLayer.get();
}

bool PluginProxy::needsBackingStore() const
{
    return !m_remoteLayerClientID;
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
