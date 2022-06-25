//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLRasterizationRate.hpp
//
// Copyright 2020-2021 Apple Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#pragma once

#include "MTLDefines.hpp"
#include "MTLHeaderBridge.hpp"
#include "MTLPrivate.hpp"

#include <Foundation/Foundation.hpp>

#include "MTLDevice.hpp"
#include "MTLTypes.hpp"

namespace MTL
{
class RasterizationRateSampleArray : public NS::Referencing<RasterizationRateSampleArray>
{
public:
    static class RasterizationRateSampleArray* alloc();

    class RasterizationRateSampleArray*        init();

    NS::Number*                                object(NS::UInteger index);

    void                                       setObject(const NS::Number* value, NS::UInteger index);
};

class RasterizationRateLayerDescriptor : public NS::Copying<RasterizationRateLayerDescriptor>
{
public:
    static class RasterizationRateLayerDescriptor* alloc();

    MTL::RasterizationRateLayerDescriptor*         init();

    MTL::RasterizationRateLayerDescriptor*         init(MTL::Size sampleCount);

    MTL::RasterizationRateLayerDescriptor*         init(MTL::Size sampleCount, const float* horizontal, const float* vertical);

    MTL::Size                                      sampleCount() const;

    MTL::Size                                      maxSampleCount() const;

    float*                                         horizontalSampleStorage() const;

    float*                                         verticalSampleStorage() const;

    class RasterizationRateSampleArray*            horizontal() const;

    class RasterizationRateSampleArray*            vertical() const;

    void                                           setSampleCount(MTL::Size sampleCount);
};

class RasterizationRateLayerArray : public NS::Referencing<RasterizationRateLayerArray>
{
public:
    static class RasterizationRateLayerArray* alloc();

    class RasterizationRateLayerArray*        init();

    class RasterizationRateLayerDescriptor*   object(NS::UInteger layerIndex);

    void                                      setObject(const class RasterizationRateLayerDescriptor* layer, NS::UInteger layerIndex);
};

class RasterizationRateMapDescriptor : public NS::Copying<RasterizationRateMapDescriptor>
{
public:
    static class RasterizationRateMapDescriptor* alloc();

    class RasterizationRateMapDescriptor*        init();

    static class RasterizationRateMapDescriptor* rasterizationRateMapDescriptor(MTL::Size screenSize);

    static class RasterizationRateMapDescriptor* rasterizationRateMapDescriptor(MTL::Size screenSize, const class RasterizationRateLayerDescriptor* layer);

    static class RasterizationRateMapDescriptor* rasterizationRateMapDescriptor(MTL::Size screenSize, NS::UInteger layerCount, MTL::RasterizationRateLayerDescriptor* const* layers);

    class RasterizationRateLayerDescriptor*      layer(NS::UInteger layerIndex);

    void                                         setLayer(const class RasterizationRateLayerDescriptor* layer, NS::UInteger layerIndex);

    class RasterizationRateLayerArray*           layers() const;

    MTL::Size                                    screenSize() const;
    void                                         setScreenSize(MTL::Size screenSize);

    NS::String*                                  label() const;
    void                                         setLabel(const NS::String* label);

    NS::UInteger                                 layerCount() const;
};

class RasterizationRateMap : public NS::Referencing<RasterizationRateMap>
{
public:
    class Device*     device() const;

    NS::String*       label() const;

    MTL::Size         screenSize() const;

    MTL::Size         physicalGranularity() const;

    NS::UInteger      layerCount() const;

    MTL::SizeAndAlign parameterBufferSizeAndAlign() const;

    void              copyParameterDataToBuffer(const class Buffer* buffer, NS::UInteger offset);

    MTL::Size         physicalSize(NS::UInteger layerIndex);

    MTL::Coordinate2D mapScreenToPhysicalCoordinates(MTL::Coordinate2D screenCoordinates, NS::UInteger layerIndex);

    MTL::Coordinate2D mapPhysicalToScreenCoordinates(MTL::Coordinate2D physicalCoordinates, NS::UInteger layerIndex);
};

}

// static method: alloc
_MTL_INLINE MTL::RasterizationRateSampleArray* MTL::RasterizationRateSampleArray::alloc()
{
    return NS::Object::alloc<MTL::RasterizationRateSampleArray>(_MTL_PRIVATE_CLS(MTLRasterizationRateSampleArray));
}

// method: init
_MTL_INLINE MTL::RasterizationRateSampleArray* MTL::RasterizationRateSampleArray::init()
{
    return NS::Object::init<MTL::RasterizationRateSampleArray>();
}

// method: objectAtIndexedSubscript:
_MTL_INLINE NS::Number* MTL::RasterizationRateSampleArray::object(NS::UInteger index)
{
    return Object::sendMessage<NS::Number*>(this, _MTL_PRIVATE_SEL(objectAtIndexedSubscript_), index);
}

// method: setObject:atIndexedSubscript:
_MTL_INLINE void MTL::RasterizationRateSampleArray::setObject(const NS::Number* value, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setObject_atIndexedSubscript_), value, index);
}

// static method: alloc
_MTL_INLINE MTL::RasterizationRateLayerDescriptor* MTL::RasterizationRateLayerDescriptor::alloc()
{
    return NS::Object::alloc<MTL::RasterizationRateLayerDescriptor>(_MTL_PRIVATE_CLS(MTLRasterizationRateLayerDescriptor));
}

// method: init
_MTL_INLINE MTL::RasterizationRateLayerDescriptor* MTL::RasterizationRateLayerDescriptor::init()
{
    return NS::Object::init<MTL::RasterizationRateLayerDescriptor>();
}

// method: initWithSampleCount:
_MTL_INLINE MTL::RasterizationRateLayerDescriptor* MTL::RasterizationRateLayerDescriptor::init(MTL::Size sampleCount)
{
    return Object::sendMessage<MTL::RasterizationRateLayerDescriptor*>(this, _MTL_PRIVATE_SEL(initWithSampleCount_), sampleCount);
}

// method: initWithSampleCount:horizontal:vertical:
_MTL_INLINE MTL::RasterizationRateLayerDescriptor* MTL::RasterizationRateLayerDescriptor::init(MTL::Size sampleCount, const float* horizontal, const float* vertical)
{
    return Object::sendMessage<MTL::RasterizationRateLayerDescriptor*>(this, _MTL_PRIVATE_SEL(initWithSampleCount_horizontal_vertical_), sampleCount, horizontal, vertical);
}

// property: sampleCount
_MTL_INLINE MTL::Size MTL::RasterizationRateLayerDescriptor::sampleCount() const
{
    return Object::sendMessage<MTL::Size>(this, _MTL_PRIVATE_SEL(sampleCount));
}

// property: maxSampleCount
_MTL_INLINE MTL::Size MTL::RasterizationRateLayerDescriptor::maxSampleCount() const
{
    return Object::sendMessage<MTL::Size>(this, _MTL_PRIVATE_SEL(maxSampleCount));
}

// property: horizontalSampleStorage
_MTL_INLINE float* MTL::RasterizationRateLayerDescriptor::horizontalSampleStorage() const
{
    return Object::sendMessage<float*>(this, _MTL_PRIVATE_SEL(horizontalSampleStorage));
}

// property: verticalSampleStorage
_MTL_INLINE float* MTL::RasterizationRateLayerDescriptor::verticalSampleStorage() const
{
    return Object::sendMessage<float*>(this, _MTL_PRIVATE_SEL(verticalSampleStorage));
}

// property: horizontal
_MTL_INLINE MTL::RasterizationRateSampleArray* MTL::RasterizationRateLayerDescriptor::horizontal() const
{
    return Object::sendMessage<MTL::RasterizationRateSampleArray*>(this, _MTL_PRIVATE_SEL(horizontal));
}

// property: vertical
_MTL_INLINE MTL::RasterizationRateSampleArray* MTL::RasterizationRateLayerDescriptor::vertical() const
{
    return Object::sendMessage<MTL::RasterizationRateSampleArray*>(this, _MTL_PRIVATE_SEL(vertical));
}

// method: setSampleCount:
_MTL_INLINE void MTL::RasterizationRateLayerDescriptor::setSampleCount(MTL::Size sampleCount)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSampleCount_), sampleCount);
}

// static method: alloc
_MTL_INLINE MTL::RasterizationRateLayerArray* MTL::RasterizationRateLayerArray::alloc()
{
    return NS::Object::alloc<MTL::RasterizationRateLayerArray>(_MTL_PRIVATE_CLS(MTLRasterizationRateLayerArray));
}

// method: init
_MTL_INLINE MTL::RasterizationRateLayerArray* MTL::RasterizationRateLayerArray::init()
{
    return NS::Object::init<MTL::RasterizationRateLayerArray>();
}

// method: objectAtIndexedSubscript:
_MTL_INLINE MTL::RasterizationRateLayerDescriptor* MTL::RasterizationRateLayerArray::object(NS::UInteger layerIndex)
{
    return Object::sendMessage<MTL::RasterizationRateLayerDescriptor*>(this, _MTL_PRIVATE_SEL(objectAtIndexedSubscript_), layerIndex);
}

// method: setObject:atIndexedSubscript:
_MTL_INLINE void MTL::RasterizationRateLayerArray::setObject(const MTL::RasterizationRateLayerDescriptor* layer, NS::UInteger layerIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setObject_atIndexedSubscript_), layer, layerIndex);
}

// static method: alloc
_MTL_INLINE MTL::RasterizationRateMapDescriptor* MTL::RasterizationRateMapDescriptor::alloc()
{
    return NS::Object::alloc<MTL::RasterizationRateMapDescriptor>(_MTL_PRIVATE_CLS(MTLRasterizationRateMapDescriptor));
}

// method: init
_MTL_INLINE MTL::RasterizationRateMapDescriptor* MTL::RasterizationRateMapDescriptor::init()
{
    return NS::Object::init<MTL::RasterizationRateMapDescriptor>();
}

// static method: rasterizationRateMapDescriptorWithScreenSize:
_MTL_INLINE MTL::RasterizationRateMapDescriptor* MTL::RasterizationRateMapDescriptor::rasterizationRateMapDescriptor(MTL::Size screenSize)
{
    return Object::sendMessage<MTL::RasterizationRateMapDescriptor*>(_MTL_PRIVATE_CLS(MTLRasterizationRateMapDescriptor), _MTL_PRIVATE_SEL(rasterizationRateMapDescriptorWithScreenSize_), screenSize);
}

// static method: rasterizationRateMapDescriptorWithScreenSize:layer:
_MTL_INLINE MTL::RasterizationRateMapDescriptor* MTL::RasterizationRateMapDescriptor::rasterizationRateMapDescriptor(MTL::Size screenSize, const MTL::RasterizationRateLayerDescriptor* layer)
{
    return Object::sendMessage<MTL::RasterizationRateMapDescriptor*>(_MTL_PRIVATE_CLS(MTLRasterizationRateMapDescriptor), _MTL_PRIVATE_SEL(rasterizationRateMapDescriptorWithScreenSize_layer_), screenSize, layer);
}

// static method: rasterizationRateMapDescriptorWithScreenSize:layerCount:layers:
_MTL_INLINE MTL::RasterizationRateMapDescriptor* MTL::RasterizationRateMapDescriptor::rasterizationRateMapDescriptor(MTL::Size screenSize, NS::UInteger layerCount, MTL::RasterizationRateLayerDescriptor* const* layers)
{
    return Object::sendMessage<MTL::RasterizationRateMapDescriptor*>(_MTL_PRIVATE_CLS(MTLRasterizationRateMapDescriptor), _MTL_PRIVATE_SEL(rasterizationRateMapDescriptorWithScreenSize_layerCount_layers_), screenSize, layerCount, layers);
}

// method: layerAtIndex:
_MTL_INLINE MTL::RasterizationRateLayerDescriptor* MTL::RasterizationRateMapDescriptor::layer(NS::UInteger layerIndex)
{
    return Object::sendMessage<MTL::RasterizationRateLayerDescriptor*>(this, _MTL_PRIVATE_SEL(layerAtIndex_), layerIndex);
}

// method: setLayer:atIndex:
_MTL_INLINE void MTL::RasterizationRateMapDescriptor::setLayer(const MTL::RasterizationRateLayerDescriptor* layer, NS::UInteger layerIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLayer_atIndex_), layer, layerIndex);
}

// property: layers
_MTL_INLINE MTL::RasterizationRateLayerArray* MTL::RasterizationRateMapDescriptor::layers() const
{
    return Object::sendMessage<MTL::RasterizationRateLayerArray*>(this, _MTL_PRIVATE_SEL(layers));
}

// property: screenSize
_MTL_INLINE MTL::Size MTL::RasterizationRateMapDescriptor::screenSize() const
{
    return Object::sendMessage<MTL::Size>(this, _MTL_PRIVATE_SEL(screenSize));
}

_MTL_INLINE void MTL::RasterizationRateMapDescriptor::setScreenSize(MTL::Size screenSize)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setScreenSize_), screenSize);
}

// property: label
_MTL_INLINE NS::String* MTL::RasterizationRateMapDescriptor::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::RasterizationRateMapDescriptor::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}

// property: layerCount
_MTL_INLINE NS::UInteger MTL::RasterizationRateMapDescriptor::layerCount() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(layerCount));
}

// property: device
_MTL_INLINE MTL::Device* MTL::RasterizationRateMap::device() const
{
    return Object::sendMessage<MTL::Device*>(this, _MTL_PRIVATE_SEL(device));
}

// property: label
_MTL_INLINE NS::String* MTL::RasterizationRateMap::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

// property: screenSize
_MTL_INLINE MTL::Size MTL::RasterizationRateMap::screenSize() const
{
    return Object::sendMessage<MTL::Size>(this, _MTL_PRIVATE_SEL(screenSize));
}

// property: physicalGranularity
_MTL_INLINE MTL::Size MTL::RasterizationRateMap::physicalGranularity() const
{
    return Object::sendMessage<MTL::Size>(this, _MTL_PRIVATE_SEL(physicalGranularity));
}

// property: layerCount
_MTL_INLINE NS::UInteger MTL::RasterizationRateMap::layerCount() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(layerCount));
}

// property: parameterBufferSizeAndAlign
_MTL_INLINE MTL::SizeAndAlign MTL::RasterizationRateMap::parameterBufferSizeAndAlign() const
{
    return Object::sendMessage<MTL::SizeAndAlign>(this, _MTL_PRIVATE_SEL(parameterBufferSizeAndAlign));
}

// method: copyParameterDataToBuffer:offset:
_MTL_INLINE void MTL::RasterizationRateMap::copyParameterDataToBuffer(const MTL::Buffer* buffer, NS::UInteger offset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(copyParameterDataToBuffer_offset_), buffer, offset);
}

// method: physicalSizeForLayer:
_MTL_INLINE MTL::Size MTL::RasterizationRateMap::physicalSize(NS::UInteger layerIndex)
{
    return Object::sendMessage<MTL::Size>(this, _MTL_PRIVATE_SEL(physicalSizeForLayer_), layerIndex);
}

// method: mapScreenToPhysicalCoordinates:forLayer:
_MTL_INLINE MTL::Coordinate2D MTL::RasterizationRateMap::mapScreenToPhysicalCoordinates(MTL::Coordinate2D screenCoordinates, NS::UInteger layerIndex)
{
    return Object::sendMessage<MTL::Coordinate2D>(this, _MTL_PRIVATE_SEL(mapScreenToPhysicalCoordinates_forLayer_), screenCoordinates, layerIndex);
}

// method: mapPhysicalToScreenCoordinates:forLayer:
_MTL_INLINE MTL::Coordinate2D MTL::RasterizationRateMap::mapPhysicalToScreenCoordinates(MTL::Coordinate2D physicalCoordinates, NS::UInteger layerIndex)
{
    return Object::sendMessage<MTL::Coordinate2D>(this, _MTL_PRIVATE_SEL(mapPhysicalToScreenCoordinates_forLayer_), physicalCoordinates, layerIndex);
}
