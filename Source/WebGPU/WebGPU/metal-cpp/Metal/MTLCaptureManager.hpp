//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLCaptureManager.hpp
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

#include "MTLCaptureManager.hpp"

namespace MTL
{
_MTL_ENUM(NS::Integer, CaptureError) {
    CaptureErrorNotSupported = 1,
    CaptureErrorAlreadyCapturing = 2,
    CaptureErrorInvalidDescriptor = 3,
};

_MTL_ENUM(NS::Integer, CaptureDestination) {
    CaptureDestinationDeveloperTools = 1,
    CaptureDestinationGPUTraceDocument = 2,
};

class CaptureDescriptor : public NS::Copying<CaptureDescriptor>
{
public:
    static class CaptureDescriptor* alloc();

    class CaptureDescriptor*        init();

    id                              captureObject() const;
    void                            setCaptureObject(id captureObject);

    MTL::CaptureDestination         destination() const;
    void                            setDestination(MTL::CaptureDestination destination);

    NS::URL*                        outputURL() const;
    void                            setOutputURL(const NS::URL* outputURL);
};

class CaptureManager : public NS::Referencing<CaptureManager>
{
public:
    static class CaptureManager* alloc();

    static class CaptureManager* sharedCaptureManager();

    MTL::CaptureManager*         init();

    class CaptureScope*          newCaptureScope(const class Device* device);

    class CaptureScope*          newCaptureScope(const class CommandQueue* commandQueue);

    bool                         supportsDestination(MTL::CaptureDestination destination);

    bool                         startCapture(const class CaptureDescriptor* descriptor, NS::Error** error);

    void                         startCapture(const class Device* device);

    void                         startCapture(const class CommandQueue* commandQueue);

    void                         startCapture(const class CaptureScope* captureScope);

    void                         stopCapture();

    class CaptureScope*          defaultCaptureScope() const;
    void                         setDefaultCaptureScope(const class CaptureScope* defaultCaptureScope);

    bool                         isCapturing() const;
};

}

// static method: alloc
_MTL_INLINE MTL::CaptureDescriptor* MTL::CaptureDescriptor::alloc()
{
    return NS::Object::alloc<MTL::CaptureDescriptor>(_MTL_PRIVATE_CLS(MTLCaptureDescriptor));
}

// method: init
_MTL_INLINE MTL::CaptureDescriptor* MTL::CaptureDescriptor::init()
{
    return NS::Object::init<MTL::CaptureDescriptor>();
}

// property: captureObject
_MTL_INLINE id MTL::CaptureDescriptor::captureObject() const
{
    return Object::sendMessage<id>(this, _MTL_PRIVATE_SEL(captureObject));
}

_MTL_INLINE void MTL::CaptureDescriptor::setCaptureObject(id captureObject)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setCaptureObject_), captureObject);
}

// property: destination
_MTL_INLINE MTL::CaptureDestination MTL::CaptureDescriptor::destination() const
{
    return Object::sendMessage<MTL::CaptureDestination>(this, _MTL_PRIVATE_SEL(destination));
}

_MTL_INLINE void MTL::CaptureDescriptor::setDestination(MTL::CaptureDestination destination)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDestination_), destination);
}

// property: outputURL
_MTL_INLINE NS::URL* MTL::CaptureDescriptor::outputURL() const
{
    return Object::sendMessage<NS::URL*>(this, _MTL_PRIVATE_SEL(outputURL));
}

_MTL_INLINE void MTL::CaptureDescriptor::setOutputURL(const NS::URL* outputURL)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setOutputURL_), outputURL);
}

// static method: alloc
_MTL_INLINE MTL::CaptureManager* MTL::CaptureManager::alloc()
{
    return NS::Object::alloc<MTL::CaptureManager>(_MTL_PRIVATE_CLS(MTLCaptureManager));
}

// static method: sharedCaptureManager
_MTL_INLINE MTL::CaptureManager* MTL::CaptureManager::sharedCaptureManager()
{
    return Object::sendMessage<MTL::CaptureManager*>(_MTL_PRIVATE_CLS(MTLCaptureManager), _MTL_PRIVATE_SEL(sharedCaptureManager));
}

// method: init
_MTL_INLINE MTL::CaptureManager* MTL::CaptureManager::init()
{
    return NS::Object::init<MTL::CaptureManager>();
}

// method: newCaptureScopeWithDevice:
_MTL_INLINE MTL::CaptureScope* MTL::CaptureManager::newCaptureScope(const MTL::Device* device)
{
    return Object::sendMessage<MTL::CaptureScope*>(this, _MTL_PRIVATE_SEL(newCaptureScopeWithDevice_), device);
}

// method: newCaptureScopeWithCommandQueue:
_MTL_INLINE MTL::CaptureScope* MTL::CaptureManager::newCaptureScope(const MTL::CommandQueue* commandQueue)
{
    return Object::sendMessage<MTL::CaptureScope*>(this, _MTL_PRIVATE_SEL(newCaptureScopeWithCommandQueue_), commandQueue);
}

// method: supportsDestination:
_MTL_INLINE bool MTL::CaptureManager::supportsDestination(MTL::CaptureDestination destination)
{
    return Object::sendMessageSafe<bool>(this, _MTL_PRIVATE_SEL(supportsDestination_), destination);
}

// method: startCaptureWithDescriptor:error:
_MTL_INLINE bool MTL::CaptureManager::startCapture(const MTL::CaptureDescriptor* descriptor, NS::Error** error)
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(startCaptureWithDescriptor_error_), descriptor, error);
}

// method: startCaptureWithDevice:
_MTL_INLINE void MTL::CaptureManager::startCapture(const MTL::Device* device)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(startCaptureWithDevice_), device);
}

// method: startCaptureWithCommandQueue:
_MTL_INLINE void MTL::CaptureManager::startCapture(const MTL::CommandQueue* commandQueue)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(startCaptureWithCommandQueue_), commandQueue);
}

// method: startCaptureWithScope:
_MTL_INLINE void MTL::CaptureManager::startCapture(const MTL::CaptureScope* captureScope)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(startCaptureWithScope_), captureScope);
}

// method: stopCapture
_MTL_INLINE void MTL::CaptureManager::stopCapture()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(stopCapture));
}

// property: defaultCaptureScope
_MTL_INLINE MTL::CaptureScope* MTL::CaptureManager::defaultCaptureScope() const
{
    return Object::sendMessage<MTL::CaptureScope*>(this, _MTL_PRIVATE_SEL(defaultCaptureScope));
}

_MTL_INLINE void MTL::CaptureManager::setDefaultCaptureScope(const MTL::CaptureScope* defaultCaptureScope)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDefaultCaptureScope_), defaultCaptureScope);
}

// property: isCapturing
_MTL_INLINE bool MTL::CaptureManager::isCapturing() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isCapturing));
}
