#include "precompiled.h"
//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// InputLayoutCache.cpp: Defines InputLayoutCache, a class that builds and caches
// D3D11 input layouts.

#include "libGLESv2/renderer/d3d11/InputLayoutCache.h"
#include "libGLESv2/renderer/d3d11/VertexBuffer11.h"
#include "libGLESv2/renderer/d3d11/BufferStorage11.h"
#include "libGLESv2/renderer/d3d11/ShaderExecutable11.h"
#include "libGLESv2/ProgramBinary.h"
#include "libGLESv2/VertexAttribute.h"
#include "libGLESv2/renderer/VertexDataManager.h"
#include "libGLESv2/renderer/d3d11/formatutils11.h"

#include "third_party/murmurhash/MurmurHash3.h"

namespace rx
{

static void GetInputLayout(const TranslatedAttribute translatedAttributes[gl::MAX_VERTEX_ATTRIBS],
                           gl::VertexFormat inputLayout[gl::MAX_VERTEX_ATTRIBS])
{
    for (unsigned int attributeIndex = 0; attributeIndex < gl::MAX_VERTEX_ATTRIBS; attributeIndex++)
    {
        const TranslatedAttribute &translatedAttribute = translatedAttributes[attributeIndex];

        if (translatedAttributes[attributeIndex].active)
        {
            inputLayout[attributeIndex] = gl::VertexFormat(*translatedAttribute.attribute,
                                                           translatedAttribute.currentValueType);
        }
    }
}

const unsigned int InputLayoutCache::kMaxInputLayouts = 1024;

InputLayoutCache::InputLayoutCache() : mInputLayoutMap(kMaxInputLayouts, hashInputLayout, compareInputLayouts)
{
    mCounter = 0;
    mDevice = NULL;
    mDeviceContext = NULL;
    mCurrentIL = NULL;
    for (unsigned int i = 0; i < gl::MAX_VERTEX_ATTRIBS; i++)
    {
        mCurrentBuffers[i] = NULL;
        mCurrentVertexStrides[i] = -1;
        mCurrentVertexOffsets[i] = -1;
    }
}

InputLayoutCache::~InputLayoutCache()
{
    clear();
}

void InputLayoutCache::initialize(ID3D11Device *device, ID3D11DeviceContext *context)
{
    clear();
    mDevice = device;
    mDeviceContext = context;
}

void InputLayoutCache::clear()
{
    for (InputLayoutMap::iterator i = mInputLayoutMap.begin(); i != mInputLayoutMap.end(); i++)
    {
        SafeRelease(i->second.inputLayout);
    }
    mInputLayoutMap.clear();
    markDirty();
}

void InputLayoutCache::markDirty()
{
    mCurrentIL = NULL;
    for (unsigned int i = 0; i < gl::MAX_VERTEX_ATTRIBS; i++)
    {
        mCurrentBuffers[i] = NULL;
        mCurrentVertexStrides[i] = -1;
        mCurrentVertexOffsets[i] = -1;
    }
}

GLenum InputLayoutCache::applyVertexBuffers(TranslatedAttribute attributes[gl::MAX_VERTEX_ATTRIBS],
                                            gl::ProgramBinary *programBinary)
{
    int sortedSemanticIndices[gl::MAX_VERTEX_ATTRIBS];
    programBinary->sortAttributesByLayout(attributes, sortedSemanticIndices);

    if (!mDevice || !mDeviceContext)
    {
        ERR("InputLayoutCache is not initialized.");
        return GL_INVALID_OPERATION;
    }

    InputLayoutKey ilKey = { 0 };

    ID3D11Buffer *vertexBuffers[gl::MAX_VERTEX_ATTRIBS] = { NULL };
    UINT vertexStrides[gl::MAX_VERTEX_ATTRIBS] = { 0 };
    UINT vertexOffsets[gl::MAX_VERTEX_ATTRIBS] = { 0 };

    static const char* semanticName = "TEXCOORD";

    for (unsigned int i = 0; i < gl::MAX_VERTEX_ATTRIBS; i++)
    {
        if (attributes[i].active)
        {
            VertexBuffer11 *vertexBuffer = VertexBuffer11::makeVertexBuffer11(attributes[i].vertexBuffer);
            BufferStorage11 *bufferStorage = attributes[i].storage ? BufferStorage11::makeBufferStorage11(attributes[i].storage) : NULL;

            D3D11_INPUT_CLASSIFICATION inputClass = attributes[i].divisor > 0 ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;

            gl::VertexFormat vertexFormat(*attributes[i].attribute, attributes[i].currentValueType);
            DXGI_FORMAT dxgiFormat = gl_d3d11::GetNativeVertexFormat(vertexFormat);

            // Record the type of the associated vertex shader vector in our key
            // This will prevent mismatched vertex shaders from using the same input layout
            GLint attributeSize;
            programBinary->getActiveAttribute(ilKey.elementCount, 0, NULL, &attributeSize, &ilKey.elements[ilKey.elementCount].glslElementType, NULL);

            ilKey.elements[ilKey.elementCount].desc.SemanticName = semanticName;
            ilKey.elements[ilKey.elementCount].desc.SemanticIndex = sortedSemanticIndices[i];
            ilKey.elements[ilKey.elementCount].desc.Format = dxgiFormat;
            ilKey.elements[ilKey.elementCount].desc.InputSlot = i;
            ilKey.elements[ilKey.elementCount].desc.AlignedByteOffset = 0;
            ilKey.elements[ilKey.elementCount].desc.InputSlotClass = inputClass;
            ilKey.elements[ilKey.elementCount].desc.InstanceDataStepRate = attributes[i].divisor;
            ilKey.elementCount++;

            vertexBuffers[i] = bufferStorage ? bufferStorage->getBuffer(BUFFER_USAGE_VERTEX_OR_TRANSFORM_FEEDBACK)
                                             : vertexBuffer->getBuffer();
            vertexStrides[i] = attributes[i].stride;
            vertexOffsets[i] = attributes[i].offset;
        }
    }

    ID3D11InputLayout *inputLayout = NULL;

    InputLayoutMap::iterator keyIter = mInputLayoutMap.find(ilKey);
    if (keyIter != mInputLayoutMap.end())
    {
        inputLayout = keyIter->second.inputLayout;
        keyIter->second.lastUsedTime = mCounter++;
    }
    else
    {
        gl::VertexFormat shaderInputLayout[gl::MAX_VERTEX_ATTRIBS];
        GetInputLayout(attributes, shaderInputLayout);
        ShaderExecutable11 *shader = ShaderExecutable11::makeShaderExecutable11(programBinary->getVertexExecutableForInputLayout(shaderInputLayout));

        D3D11_INPUT_ELEMENT_DESC descs[gl::MAX_VERTEX_ATTRIBS];
        for (unsigned int j = 0; j < ilKey.elementCount; ++j)
        {
            descs[j] = ilKey.elements[j].desc;
        }

        HRESULT result = mDevice->CreateInputLayout(descs, ilKey.elementCount, shader->getFunction(), shader->getLength(), &inputLayout);
        if (FAILED(result))
        {
            ERR("Failed to crate input layout, result: 0x%08x", result);
            return GL_INVALID_OPERATION;
        }

        if (mInputLayoutMap.size() >= kMaxInputLayouts)
        {
            TRACE("Overflowed the limit of %u input layouts, removing the least recently used "
                  "to make room.", kMaxInputLayouts);

            InputLayoutMap::iterator leastRecentlyUsed = mInputLayoutMap.begin();
            for (InputLayoutMap::iterator i = mInputLayoutMap.begin(); i != mInputLayoutMap.end(); i++)
            {
                if (i->second.lastUsedTime < leastRecentlyUsed->second.lastUsedTime)
                {
                    leastRecentlyUsed = i;
                }
            }
            SafeRelease(leastRecentlyUsed->second.inputLayout);
            mInputLayoutMap.erase(leastRecentlyUsed);
        }

        InputLayoutCounterPair inputCounterPair;
        inputCounterPair.inputLayout = inputLayout;
        inputCounterPair.lastUsedTime = mCounter++;

        mInputLayoutMap.insert(std::make_pair(ilKey, inputCounterPair));
    }

    if (inputLayout != mCurrentIL)
    {
        mDeviceContext->IASetInputLayout(inputLayout);
        mCurrentIL = inputLayout;
    }

    for (unsigned int i = 0; i < gl::MAX_VERTEX_ATTRIBS; i++)
    {
        if (vertexBuffers[i] != mCurrentBuffers[i] || vertexStrides[i] != mCurrentVertexStrides[i] ||
            vertexOffsets[i] != mCurrentVertexOffsets[i])
        {
            mDeviceContext->IASetVertexBuffers(i, 1, &vertexBuffers[i], &vertexStrides[i], &vertexOffsets[i]);
            mCurrentBuffers[i] = vertexBuffers[i];
            mCurrentVertexStrides[i] = vertexStrides[i];
            mCurrentVertexOffsets[i] = vertexOffsets[i];
        }
    }

    return GL_NO_ERROR;
}

std::size_t InputLayoutCache::hashInputLayout(const InputLayoutKey &inputLayout)
{
    static const unsigned int seed = 0xDEADBEEF;

    std::size_t hash = 0;
    MurmurHash3_x86_32(inputLayout.begin(), inputLayout.end() - inputLayout.begin(), seed, &hash);
    return hash;
}

bool InputLayoutCache::compareInputLayouts(const InputLayoutKey &a, const InputLayoutKey &b)
{
    if (a.elementCount != b.elementCount)
    {
        return false;
    }

    return std::equal(a.begin(), a.end(), b.begin());
}

}
