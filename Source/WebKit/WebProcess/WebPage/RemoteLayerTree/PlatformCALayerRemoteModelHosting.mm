/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "PlatformCALayerRemoteModelHosting.h"

#if ENABLE(MODEL_ELEMENT)

#import "RemoteLayerTreeContext.h"
#import "WebProcess.h"
#import <WebCore/GraphicsLayerCA.h>
#import <WebCore/PlatformCALayerCocoa.h>
#import <wtf/RetainPtr.h>

namespace WebKit {

Ref<PlatformCALayerRemote> PlatformCALayerRemoteModelHosting::create(Ref<WebCore::Model> model, WebCore::PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
{
    auto layer = adoptRef(*new PlatformCALayerRemoteModelHosting(model, owner, context));
    context.layerDidEnterContext(layer.get(), layer->layerType());
    return WTFMove(layer);
}

PlatformCALayerRemoteModelHosting::PlatformCALayerRemoteModelHosting(Ref<WebCore::Model> model, WebCore::PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
    : PlatformCALayerRemote(WebCore::PlatformCALayer::LayerType::LayerTypeModelLayer, owner, context)
    , m_model(model)
{
}

PlatformCALayerRemoteModelHosting::~PlatformCALayerRemoteModelHosting() = default;

Ref<WebCore::PlatformCALayer> PlatformCALayerRemoteModelHosting::clone(WebCore::PlatformCALayerClient* owner) const
{
    auto clone = adoptRef(*new PlatformCALayerRemoteModelHosting(m_model, owner, *context()));
    context()->layerDidEnterContext(clone.get(), clone->layerType());

    updateClonedLayerProperties(clone.get(), false);

    clone->setClonedLayer(this);
    return WTFMove(clone);
}

void PlatformCALayerRemoteModelHosting::populateCreationProperties(RemoteLayerTreeTransaction::LayerCreationProperties& properties, const RemoteLayerTreeContext& context, PlatformCALayer::LayerType type)
{
    PlatformCALayerRemote::populateCreationProperties(properties, context, type);
    ASSERT(std::holds_alternative<RemoteLayerTreeTransaction::LayerCreationProperties::NoAdditionalData>(properties.additionalData));
    properties.additionalData = m_model;
}

void PlatformCALayerRemoteModelHosting::dumpAdditionalProperties(TextStream& ts, OptionSet<WebCore::PlatformLayerTreeAsTextFlags> flags)
{
    if (flags.contains(WebCore::PlatformLayerTreeAsTextFlags::IncludeModels))
        ts << indent << "(model data size " << m_model->data()->size() << ")\n";
}

} // namespace WebKit

#endif // ENABLE(MODEL_ELEMENT)
