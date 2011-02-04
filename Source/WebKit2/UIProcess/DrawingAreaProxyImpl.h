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

#ifndef DrawingAreaProxyImpl_h
#define DrawingAreaProxyImpl_h

#include "BackingStore.h"
#include "DrawingAreaProxy.h"
#include "LayerTreeContext.h"

namespace WebKit {

class Region;

class DrawingAreaProxyImpl : public DrawingAreaProxy {
public:
    static PassOwnPtr<DrawingAreaProxyImpl> create(WebPageProxy*);
    virtual ~DrawingAreaProxyImpl();

    void paint(BackingStore::PlatformGraphicsContext, const WebCore::IntRect&, Region& unpaintedRegion);

private:
    explicit DrawingAreaProxyImpl(WebPageProxy*);

    // DrawingAreaProxy
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    virtual void didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*);
    virtual bool paint(const WebCore::IntRect&, PlatformDrawingContext);
    virtual void sizeDidChange();
    virtual void visibilityDidChange();
    virtual void setPageIsVisible(bool);
    virtual void attachCompositingContext(uint32_t contextID);
    virtual void detachCompositingContext();

    // CoreIPC message handlers
    virtual void update(uint64_t sequenceNumber, const UpdateInfo&);
    virtual void didSetSize(uint64_t sequenceNumber, const UpdateInfo&, const LayerTreeContext&);
    virtual void enterAcceleratedCompositingMode(uint64_t sequenceNumber, const LayerTreeContext&);
    virtual void exitAcceleratedCompositingMode(uint64_t sequenceNumber, const UpdateInfo&);

    void incorporateUpdate(const UpdateInfo&);
    void sendSetSize();
    void waitForAndDispatchDidSetSize();

    void enterAcceleratedCompositingMode(const LayerTreeContext&);
    void exitAcceleratedCompositingMode();

    bool isInAcceleratedCompositingMode() const { return !m_layerTreeContext.isEmpty(); }

    // The current layer tree context.
    LayerTreeContext m_layerTreeContext;
    
    // Whether we've sent a SetSize message and are now waiting for a DidSetSize message.
    // Used to throttle SetSize messages so we don't send them faster than the Web process can handle.
    bool m_isWaitingForDidSetSize;

    // The sequence number of the last DidSetSize message
    uint64_t m_lastDidSetSizeSequenceNumber;

    OwnPtr<BackingStore> m_backingStore;
};

} // namespace WebKit

#endif // DrawingAreaProxyImpl_h
