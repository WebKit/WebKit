/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/LayerHostingContextIdentifier.h>
#include <WebCore/PlatformLayerIdentifier.h>
#include <WebCore/TransformationMatrix.h>
#include <wtf/AbstractRefCountedAndCanMakeWeakPtr.h>
#include <wtf/Forward.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WebCore {

class FloatPoint3D;
class GraphicsLayer;
class HTMLModelElement;
class ModelPlayer;
class ResourceError;

class WEBCORE_EXPORT ModelPlayerClient : public AbstractRefCountedAndCanMakeWeakPtr<ModelPlayerClient> {
public:
    virtual ~ModelPlayerClient();

    virtual void didFinishLoading(ModelPlayer&) = 0;
    virtual void didFailLoading(ModelPlayer&, const ResourceError&) = 0;
#if ENABLE(MODEL_PROCESS)
    virtual void didConvertModelData(ModelPlayer&, Ref<SharedBuffer>&& convertedData, const String& convertedMIMEType) = 0;
#endif
#if ENABLE(MODEL_ELEMENT_ENVIRONMENT_MAP)
    // FIXME: This should be made consistent with didFinishLoading/didFailLoading, by splitting it into a didFinishEnvironmentMapLoading and a didFailEnvironmentMapLoading which takes a `const ResourceError&`.
    virtual void didFinishEnvironmentMapLoading(ModelPlayer&, bool succeeded) = 0;
#endif
    virtual void didUnload(ModelPlayer&) = 0;
    virtual void didUpdate(ModelPlayer&) = 0;

#if ENABLE(MODEL_ELEMENT_ENTITY_TRANSFORM)
    virtual void didUpdateEntityTransform(ModelPlayer&, const TransformationMatrix&) = 0;
#endif
#if ENABLE(MODEL_ELEMENT_BOUNDING_BOX)
    virtual void didUpdateBoundingBox(ModelPlayer&, const FloatPoint3D&, const FloatPoint3D&) = 0;
#endif

    virtual RefPtr<GraphicsLayer> graphicsLayer() const = 0;

    virtual bool isVisible() const = 0;
    virtual void logWarning(ModelPlayer&, const String& warningMessage) = 0;
};

}
