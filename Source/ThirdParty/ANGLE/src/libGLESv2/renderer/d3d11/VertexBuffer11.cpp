#include "precompiled.h"
//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VertexBuffer11.cpp: Defines the D3D11 VertexBuffer implementation.

#include "libGLESv2/renderer/d3d11/VertexBuffer11.h"
#include "libGLESv2/renderer/BufferStorage.h"

#include "libGLESv2/Buffer.h"
#include "libGLESv2/renderer/d3d11/Renderer11.h"
#include "libGLESv2/VertexAttribute.h"
#include "libGLESv2/renderer/d3d11/formatutils11.h"

namespace rx
{

VertexBuffer11::VertexBuffer11(rx::Renderer11 *const renderer) : mRenderer(renderer)
{
    mBuffer = NULL;
    mBufferSize = 0;
    mDynamicUsage = false;
}

VertexBuffer11::~VertexBuffer11()
{
    SafeRelease(mBuffer);
}

bool VertexBuffer11::initialize(unsigned int size, bool dynamicUsage)
{
    SafeRelease(mBuffer);

    updateSerial();

    if (size > 0)
    {
        ID3D11Device* dxDevice = mRenderer->getDevice();

        D3D11_BUFFER_DESC bufferDesc;
        bufferDesc.ByteWidth = size;
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bufferDesc.MiscFlags = 0;
        bufferDesc.StructureByteStride = 0;

        HRESULT result = dxDevice->CreateBuffer(&bufferDesc, NULL, &mBuffer);
        if (FAILED(result))
        {
            return false;
        }
    }

    mBufferSize = size;
    mDynamicUsage = dynamicUsage;
    return true;
}

VertexBuffer11 *VertexBuffer11::makeVertexBuffer11(VertexBuffer *vetexBuffer)
{
    ASSERT(HAS_DYNAMIC_TYPE(VertexBuffer11*, vetexBuffer));
    return static_cast<VertexBuffer11*>(vetexBuffer);
}

bool VertexBuffer11::storeVertexAttributes(const gl::VertexAttribute &attrib, const gl::VertexAttribCurrentValueData &currentValue,
                                           GLint start, GLsizei count, GLsizei instances, unsigned int offset)
{
    if (mBuffer)
    {
        gl::Buffer *buffer = attrib.mBoundBuffer.get();
        int inputStride = attrib.stride();
        ID3D11DeviceContext *dxContext = mRenderer->getDeviceContext();

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT result = dxContext->Map(mBuffer, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mappedResource);
        if (FAILED(result))
        {
            ERR("Vertex buffer map failed with error 0x%08x", result);
            return false;
        }

        char* output = reinterpret_cast<char*>(mappedResource.pData) + offset;

        const char *input = NULL;
        if (attrib.mArrayEnabled)
        {
            if (buffer)
            {
                BufferStorage *storage = buffer->getStorage();
                input = static_cast<const char*>(storage->getData()) + static_cast<int>(attrib.mOffset);
            }
            else
            {
                input = static_cast<const char*>(attrib.mPointer);
            }
        }
        else
        {
            input = reinterpret_cast<const char*>(currentValue.FloatValues);
        }

        if (instances == 0 || attrib.mDivisor == 0)
        {
            input += inputStride * start;
        }

        gl::VertexFormat vertexFormat(attrib, currentValue.Type);
        VertexCopyFunction conversionFunc = gl_d3d11::GetVertexCopyFunction(vertexFormat);
        ASSERT(conversionFunc != NULL);
        conversionFunc(input, inputStride, count, output);

        dxContext->Unmap(mBuffer, 0);

        return true;
    }
    else
    {
        ERR("Vertex buffer not initialized.");
        return false;
    }
}

bool VertexBuffer11::getSpaceRequired(const gl::VertexAttribute &attrib, GLsizei count,
                                      GLsizei instances, unsigned int *outSpaceRequired) const
{
    unsigned int elementCount = 0;
    if (attrib.mArrayEnabled)
    {
        if (instances == 0 || attrib.mDivisor == 0)
        {
            elementCount = count;
        }
        else
        {
            if (static_cast<unsigned int>(instances) < std::numeric_limits<unsigned int>::max() - (attrib.mDivisor - 1))
            {
                // Round up
                elementCount = rx::roundUp(static_cast<unsigned int>(instances), attrib.mDivisor);
            }
            else
            {
                elementCount = instances / attrib.mDivisor;
            }
        }

        gl::VertexFormat vertexFormat(attrib);
        unsigned int elementSize = static_cast<unsigned int>(gl_d3d11::GetVertexElementSize(vertexFormat));
        if (elementSize <= std::numeric_limits<unsigned int>::max() / elementCount)
        {
            if (outSpaceRequired)
            {
                *outSpaceRequired = elementSize * elementCount;
            }
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        const unsigned int elementSize = 4;
        if (outSpaceRequired)
        {
            *outSpaceRequired = elementSize * 4;
        }
        return true;
    }
}

unsigned int VertexBuffer11::getBufferSize() const
{
    return mBufferSize;
}

bool VertexBuffer11::setBufferSize(unsigned int size)
{
    if (size > mBufferSize)
    {
        return initialize(size, mDynamicUsage);
    }
    else
    {
        return true;
    }
}

bool VertexBuffer11::discard()
{
    if (mBuffer)
    {
        ID3D11DeviceContext *dxContext = mRenderer->getDeviceContext();

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT result = dxContext->Map(mBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if (FAILED(result))
        {
            ERR("Vertex buffer map failed with error 0x%08x", result);
            return false;
        }

        dxContext->Unmap(mBuffer, 0);

        return true;
    }
    else
    {
        ERR("Vertex buffer not initialized.");
        return false;
    }
}

ID3D11Buffer *VertexBuffer11::getBuffer() const
{
    return mBuffer;
}

}
