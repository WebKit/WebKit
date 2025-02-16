//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_context_device.mm:
//      Implementation of Metal framework's MTLDevice wrapper per context.
//

#include "libANGLE/renderer/metal/mtl_context_device.h"
#if ANGLE_USE_METAL_OWNERSHIP_IDENTITY
#    include "libANGLE/renderer/metal/mtl_resource_spi.h"
#endif

namespace rx
{
namespace mtl
{

ContextDevice::ContextDevice(GLint ownershipIdentity)
{
#if ANGLE_USE_METAL_OWNERSHIP_IDENTITY
    mOwnershipIdentity = static_cast<task_id_token_t>(ownershipIdentity);
    if (mOwnershipIdentity != TASK_ID_TOKEN_NULL)
    {
        kern_return_t kr =
            mach_port_mod_refs(mach_task_self(), mOwnershipIdentity, MACH_PORT_RIGHT_SEND, 1);
        if (ANGLE_UNLIKELY(kr != KERN_SUCCESS))
        {
            ERR() << "mach_port_mod_refs failed with: %s (%x)" << mach_error_string(kr) << kr;
            ASSERT(false);
        }
    }
#endif
}

ContextDevice::~ContextDevice()
{
#if ANGLE_USE_METAL_OWNERSHIP_IDENTITY
    if (mOwnershipIdentity != TASK_ID_TOKEN_NULL)
    {
        kern_return_t kr =
            mach_port_mod_refs(mach_task_self(), mOwnershipIdentity, MACH_PORT_RIGHT_SEND, -1);
        if (ANGLE_UNLIKELY(kr != KERN_SUCCESS))
        {
            ERR() << "mach_port_mod_refs failed with: %s (%x)" << mach_error_string(kr) << kr;
            ASSERT(false);
        }
    }
#endif
}

angle::ObjCPtr<id<MTLSamplerState>> ContextDevice::newSamplerStateWithDescriptor(
    MTLSamplerDescriptor *descriptor) const
{
    return angle::adoptObjCPtr([get() newSamplerStateWithDescriptor:descriptor]);
}

angle::ObjCPtr<id<MTLTexture>> ContextDevice::newTextureWithDescriptor(
    MTLTextureDescriptor *descriptor) const
{

    auto resource = angle::adoptObjCPtr([get() newTextureWithDescriptor:descriptor]);
    setOwnerWithIdentity(resource);
    return resource;
}

angle::ObjCPtr<id<MTLTexture>> ContextDevice::newTextureWithDescriptor(
    MTLTextureDescriptor *descriptor,
    IOSurfaceRef iosurface,
    NSUInteger plane) const
{
    return angle::adoptObjCPtr([get() newTextureWithDescriptor:descriptor
                                                     iosurface:iosurface
                                                         plane:plane]);
}

angle::ObjCPtr<id<MTLBuffer>> ContextDevice::newBufferWithLength(NSUInteger length,
                                                                 MTLResourceOptions options) const
{
    auto resource = angle::adoptObjCPtr([get() newBufferWithLength:length options:options]);
    setOwnerWithIdentity(resource);
    return resource;
}

angle::ObjCPtr<id<MTLBuffer>> ContextDevice::newBufferWithBytes(const void *pointer,
                                                                NSUInteger length,
                                                                MTLResourceOptions options) const
{
    auto resource = angle::adoptObjCPtr([get() newBufferWithBytes:pointer
                                                           length:length
                                                          options:options]);
    setOwnerWithIdentity(resource);
    return resource;
}

angle::ObjCPtr<id<MTLComputePipelineState>> ContextDevice::newComputePipelineStateWithFunction(
    id<MTLFunction> computeFunction,
    __autoreleasing NSError **error) const
{
    return angle::adoptObjCPtr([get() newComputePipelineStateWithFunction:computeFunction
                                                                    error:error]);
}

angle::ObjCPtr<id<MTLRenderPipelineState>> ContextDevice::newRenderPipelineStateWithDescriptor(
    MTLRenderPipelineDescriptor *descriptor,
    __autoreleasing NSError **error) const
{
    return angle::adoptObjCPtr([get() newRenderPipelineStateWithDescriptor:descriptor error:error]);
}

angle::ObjCPtr<id<MTLDepthStencilState>> ContextDevice::newDepthStencilStateWithDescriptor(
    MTLDepthStencilDescriptor *descriptor) const
{
    return angle::adoptObjCPtr([get() newDepthStencilStateWithDescriptor:descriptor]);
}

angle::ObjCPtr<id<MTLSharedEvent>> ContextDevice::newSharedEvent() const
{
    return angle::adoptObjCPtr([get() newSharedEvent]);
}

angle::ObjCPtr<id<MTLEvent>> ContextDevice::newEvent() const
{
    return angle::adoptObjCPtr([get() newEvent]);
}

void ContextDevice::setOwnerWithIdentity(id<MTLResource> resource) const
{
#if ANGLE_USE_METAL_OWNERSHIP_IDENTITY
    mtl::setOwnerWithIdentity(resource, mOwnershipIdentity);
#endif
}

bool ContextDevice::hasUnifiedMemory() const
{
    return [get() hasUnifiedMemory];
}

}  // namespace mtl
}  // namespace rx
