//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VertexBuffer11.cpp: Defines the D3D11 VertexBuffer implementation.

#include "libANGLE/renderer/d3d/d3d11/VertexBuffer11.h"

#include "libANGLE/Buffer.h"
#include "libANGLE/VertexAttribute.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/d3d/d3d11/Buffer11.h"
#include "libANGLE/renderer/d3d/d3d11/formatutils11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

namespace rx
{

VertexBuffer11::VertexBuffer11(Renderer11 *const renderer)
    : mRenderer(renderer),
      mBuffer(),
      mBufferSize(0),
      mDynamicUsage(false),
      mMappedResourceData(nullptr)
{
}

VertexBuffer11::~VertexBuffer11()
{
    ASSERT(mMappedResourceData == nullptr);
}

gl::Error VertexBuffer11::initialize(unsigned int size, bool dynamicUsage)
{
    mBuffer.reset();
    updateSerial();

    if (size > 0)
    {
        D3D11_BUFFER_DESC bufferDesc;
        bufferDesc.ByteWidth           = size;
        bufferDesc.Usage               = D3D11_USAGE_DYNAMIC;
        bufferDesc.BindFlags           = D3D11_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
        bufferDesc.MiscFlags           = 0;
        bufferDesc.StructureByteStride = 0;

        ANGLE_TRY(mRenderer->allocateResource(bufferDesc, &mBuffer));

        if (dynamicUsage)
        {
            mBuffer.setDebugName("VertexBuffer11 (dynamic)");
        }
        else
        {
            mBuffer.setDebugName("VertexBuffer11 (static)");
        }
    }

    mBufferSize   = size;
    mDynamicUsage = dynamicUsage;

    return gl::NoError();
}

gl::Error VertexBuffer11::mapResource()
{
    if (mMappedResourceData == nullptr)
    {
        ID3D11DeviceContext *dxContext = mRenderer->getDeviceContext();

        D3D11_MAPPED_SUBRESOURCE mappedResource;

        HRESULT result =
            dxContext->Map(mBuffer.get(), 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mappedResource);
        if (FAILED(result))
        {
            return gl::OutOfMemory()
                   << "Failed to map internal vertex buffer, " << gl::FmtHR(result);
        }

        mMappedResourceData = reinterpret_cast<uint8_t *>(mappedResource.pData);
    }

    return gl::NoError();
}

void VertexBuffer11::hintUnmapResource()
{
    if (mMappedResourceData != nullptr)
    {
        ID3D11DeviceContext *dxContext = mRenderer->getDeviceContext();
        dxContext->Unmap(mBuffer.get(), 0);

        mMappedResourceData = nullptr;
    }
}

gl::Error VertexBuffer11::storeVertexAttributes(const gl::VertexAttribute &attrib,
                                                const gl::VertexBinding &binding,
                                                GLenum currentValueType,
                                                GLint start,
                                                GLsizei count,
                                                GLsizei instances,
                                                unsigned int offset,
                                                const uint8_t *sourceData)
{
    if (!mBuffer.valid())
    {
        return gl::OutOfMemory() << "Internal vertex buffer is not initialized.";
    }

    int inputStride = static_cast<int>(ComputeVertexAttributeStride(attrib, binding));

    // This will map the resource if it isn't already mapped.
    ANGLE_TRY(mapResource());

    uint8_t *output = mMappedResourceData + offset;

    const uint8_t *input = sourceData;

    if (instances == 0 || binding.getDivisor() == 0)
    {
        input += inputStride * start;
    }

    gl::VertexFormatType vertexFormatType = gl::GetVertexFormatType(attrib, currentValueType);
    const D3D_FEATURE_LEVEL featureLevel  = mRenderer->getRenderer11DeviceCaps().featureLevel;
    const d3d11::VertexFormat &vertexFormatInfo =
        d3d11::GetVertexFormatInfo(vertexFormatType, featureLevel);
    ASSERT(vertexFormatInfo.copyFunction != nullptr);
    vertexFormatInfo.copyFunction(input, inputStride, count, output);

    return gl::NoError();
}

unsigned int VertexBuffer11::getBufferSize() const
{
    return mBufferSize;
}

gl::Error VertexBuffer11::setBufferSize(unsigned int size)
{
    if (size > mBufferSize)
    {
        return initialize(size, mDynamicUsage);
    }
    else
    {
        return gl::NoError();
    }
}

gl::Error VertexBuffer11::discard()
{
    if (!mBuffer.valid())
    {
        return gl::OutOfMemory() << "Internal vertex buffer is not initialized.";
    }

    ID3D11DeviceContext *dxContext = mRenderer->getDeviceContext();

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT result = dxContext->Map(mBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result))
    {
        return gl::OutOfMemory() << "Failed to map internal buffer for discarding, "
                                 << gl::FmtHR(result);
    }

    dxContext->Unmap(mBuffer.get(), 0);

    return gl::NoError();
}

const d3d11::Buffer &VertexBuffer11::getBuffer() const
{
    return mBuffer;
}

}  // namespace rx
