//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// IndexBuffer11.cpp: Defines the D3D11 IndexBuffer implementation.

#include "libANGLE/renderer/d3d/d3d11/IndexBuffer11.h"

#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

namespace rx
{

IndexBuffer11::IndexBuffer11(Renderer11 *const renderer)
    : mRenderer(renderer), mBuffer(), mBufferSize(0), mIndexType(GL_NONE), mDynamicUsage(false)
{
}

IndexBuffer11::~IndexBuffer11()
{
}

gl::Error IndexBuffer11::initialize(unsigned int bufferSize, GLenum indexType, bool dynamic)
{
    mBuffer.reset();

    updateSerial();

    if (bufferSize > 0)
    {
        D3D11_BUFFER_DESC bufferDesc;
        bufferDesc.ByteWidth = bufferSize;
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bufferDesc.MiscFlags = 0;
        bufferDesc.StructureByteStride = 0;

        ANGLE_TRY(mRenderer->allocateResource(bufferDesc, &mBuffer));

        if (dynamic)
        {
            mBuffer.setDebugName("IndexBuffer11 (dynamic)");
        }
        else
        {
            mBuffer.setDebugName("IndexBuffer11 (static)");
        }
    }

    mBufferSize = bufferSize;
    mIndexType = indexType;
    mDynamicUsage = dynamic;

    return gl::NoError();
}

gl::Error IndexBuffer11::mapBuffer(unsigned int offset, unsigned int size, void** outMappedMemory)
{
    if (!mBuffer.valid())
    {
        return gl::OutOfMemory() << "Internal index buffer is not initialized.";
    }

    // Check for integer overflows and out-out-bounds map requests
    if (offset + size < offset || offset + size > mBufferSize)
    {
        return gl::OutOfMemory() << "Index buffer map range is not inside the buffer.";
    }

    ID3D11DeviceContext *dxContext = mRenderer->getDeviceContext();

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT result =
        dxContext->Map(mBuffer.get(), 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mappedResource);
    if (FAILED(result))
    {
        return gl::OutOfMemory() << "Failed to map internal index buffer, " << gl::FmtHR(result);
    }

    *outMappedMemory = reinterpret_cast<char*>(mappedResource.pData) + offset;
    return gl::NoError();
}

gl::Error IndexBuffer11::unmapBuffer()
{
    if (!mBuffer.valid())
    {
        return gl::OutOfMemory() << "Internal index buffer is not initialized.";
    }

    ID3D11DeviceContext *dxContext = mRenderer->getDeviceContext();
    dxContext->Unmap(mBuffer.get(), 0);
    return gl::NoError();
}

GLenum IndexBuffer11::getIndexType() const
{
    return mIndexType;
}

unsigned int IndexBuffer11::getBufferSize() const
{
    return mBufferSize;
}

gl::Error IndexBuffer11::setSize(unsigned int bufferSize, GLenum indexType)
{
    if (bufferSize > mBufferSize || indexType != mIndexType)
    {
        return initialize(bufferSize, indexType, mDynamicUsage);
    }
    else
    {
        return gl::NoError();
    }
}

gl::Error IndexBuffer11::discard()
{
    if (!mBuffer.valid())
    {
        return gl::OutOfMemory() << "Internal index buffer is not initialized.";
    }

    ID3D11DeviceContext *dxContext = mRenderer->getDeviceContext();

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT result = dxContext->Map(mBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result))
    {
        return gl::OutOfMemory() << "Failed to map internal index buffer, " << gl::FmtHR(result);
    }

    dxContext->Unmap(mBuffer.get(), 0);

    return gl::NoError();
}

DXGI_FORMAT IndexBuffer11::getIndexFormat() const
{
    switch (mIndexType)
    {
      case GL_UNSIGNED_BYTE:    return DXGI_FORMAT_R16_UINT;
      case GL_UNSIGNED_SHORT:   return DXGI_FORMAT_R16_UINT;
      case GL_UNSIGNED_INT:     return DXGI_FORMAT_R32_UINT;
      default: UNREACHABLE();   return DXGI_FORMAT_UNKNOWN;
    }
}

const d3d11::Buffer &IndexBuffer11::getBuffer() const
{
    return mBuffer;
}

}  // namespace rx
