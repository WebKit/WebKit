/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef LayerTreeHostCAMac_h
#define LayerTreeHostCAMac_h

#include "LayerTreeHostCA.h"
#include <WebCore/LayerFlushScheduler.h>
#include <WebCore/LayerFlushSchedulerClient.h>
#include <wtf/RetainPtr.h>

typedef struct __WKCARemoteLayerClientRef* WKCARemoteLayerClientRef;

namespace WebKit {

class LayerTreeHostCAMac : public LayerTreeHostCA, public WebCore::LayerFlushSchedulerClient {
public:
    static PassRefPtr<LayerTreeHostCAMac> create(WebPage*);
    virtual ~LayerTreeHostCAMac();

private:
    explicit LayerTreeHostCAMac(WebPage*);

    // LayerTreeHost.
    virtual void scheduleLayerFlush();
    virtual void setLayerFlushSchedulingEnabled(bool);
    virtual void invalidate();
    virtual void sizeDidChange(const WebCore::IntSize& newSize);
    virtual void forceRepaint();
    virtual void pauseRendering();
    virtual void resumeRendering();

    // LayerTreeHostCA
    virtual void platformInitialize(LayerTreeContext&);
    virtual void didPerformScheduledLayerFlush();
    
    // LayerFlushSchedulerClient
    virtual bool flushLayers();

    RetainPtr<WKCARemoteLayerClientRef> m_remoteLayerClient;
    WebCore::LayerFlushScheduler m_layerFlushScheduler;
};

} // namespace WebKit

#endif // LayerTreeHostCAMac_h
