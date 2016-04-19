//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// InputLayoutCache.cpp: Defines InputLayoutCache, a class that builds and caches
// D3D11 input layouts.

#include "libANGLE/renderer/d3d/d3d11/InputLayoutCache.h"

#include "common/BitSetIterator.h"
#include "common/utilities.h"
#include "libANGLE/Program.h"
#include "libANGLE/VertexAttribute.h"
#include "libANGLE/renderer/d3d/IndexDataManager.h"
#include "libANGLE/renderer/d3d/ProgramD3D.h"
#include "libANGLE/renderer/d3d/VertexDataManager.h"
#include "libANGLE/renderer/d3d/d3d11/Buffer11.h"
#include "libANGLE/renderer/d3d/d3d11/ShaderExecutable11.h"
#include "libANGLE/renderer/d3d/d3d11/VertexBuffer11.h"
#include "libANGLE/renderer/d3d/d3d11/formatutils11.h"
#include "third_party/murmurhash/MurmurHash3.h"

namespace rx
{

namespace
{

size_t GetReservedBufferCount(bool usesPointSpriteEmulation)
{
    return usesPointSpriteEmulation ? 1 : 0;
}

gl::InputLayout GetInputLayout(const std::vector<const TranslatedAttribute *> &translatedAttributes)
{
    gl::InputLayout inputLayout(translatedAttributes.size(), gl::VERTEX_FORMAT_INVALID);

    for (size_t attributeIndex = 0; attributeIndex < translatedAttributes.size(); ++attributeIndex)
    {
        const TranslatedAttribute *translatedAttribute = translatedAttributes[attributeIndex];
        inputLayout[attributeIndex] = gl::GetVertexFormatType(
            *translatedAttribute->attribute, translatedAttribute->currentValueType);
    }
    return inputLayout;
}

GLenum GetGLSLAttributeType(const std::vector<sh::Attribute> &shaderAttributes, int index)
{
    // Count matrices differently
    for (const sh::Attribute &attrib : shaderAttributes)
    {
        if (attrib.location == -1)
        {
            continue;
        }

        GLenum transposedType = gl::TransposeMatrixType(attrib.type);
        int rows              = gl::VariableRowCount(transposedType);

        if (index >= attrib.location && index < attrib.location + rows)
        {
            return transposedType;
        }
    }

    UNREACHABLE();
    return GL_NONE;
}

const unsigned int kDefaultCacheSize = 1024;

struct PackedAttribute
{
    uint8_t attribType;
    uint8_t semanticIndex;
    uint8_t vertexFormatType;
    uint8_t divisor;
};

Optional<size_t> FindFirstNonInstanced(
    const std::vector<const TranslatedAttribute *> &currentAttributes)
{
    for (size_t index = 0; index < currentAttributes.size(); ++index)
    {
        if (currentAttributes[index]->divisor == 0)
        {
            return Optional<size_t>(index);
        }
    }

    return Optional<size_t>::Invalid();
}

void SortAttributesByLayout(const gl::Program *program,
                            const std::vector<TranslatedAttribute> &vertexArrayAttribs,
                            const std::vector<TranslatedAttribute> &currentValueAttribs,
                            AttribIndexArray *sortedD3DSemanticsOut,
                            std::vector<const TranslatedAttribute *> *sortedAttributesOut)
{
    sortedAttributesOut->clear();

    const auto &locationToSemantic =
        GetImplAs<ProgramD3D>(program)->getAttribLocationToD3DSemantics();

    for (auto locationIndex : angle::IterateBitSet(program->getActiveAttribLocationsMask()))
    {
        int d3dSemantic = locationToSemantic[locationIndex];
        if (sortedAttributesOut->size() <= static_cast<size_t>(d3dSemantic))
        {
            sortedAttributesOut->resize(d3dSemantic + 1);
        }

        (*sortedD3DSemanticsOut)[d3dSemantic] = d3dSemantic;

        const auto *arrayAttrib = &vertexArrayAttribs[locationIndex];
        if (arrayAttrib->attribute && arrayAttrib->attribute->enabled)
        {
            (*sortedAttributesOut)[d3dSemantic] = arrayAttrib;
        }
        else
        {
            ASSERT(currentValueAttribs[locationIndex].attribute);
            (*sortedAttributesOut)[d3dSemantic] = &currentValueAttribs[locationIndex];
        }
    }
}

} // anonymous namespace

void InputLayoutCache::PackedAttributeLayout::addAttributeData(
    GLenum glType,
    UINT semanticIndex,
    gl::VertexFormatType vertexFormatType,
    unsigned int divisor)
{
    gl::AttributeType attribType = gl::GetAttributeType(glType);

    PackedAttribute packedAttrib;
    packedAttrib.attribType = static_cast<uint8_t>(attribType);
    packedAttrib.semanticIndex = static_cast<uint8_t>(semanticIndex);
    packedAttrib.vertexFormatType = static_cast<uint8_t>(vertexFormatType);
    packedAttrib.divisor = static_cast<uint8_t>(divisor);

    ASSERT(static_cast<gl::AttributeType>(packedAttrib.attribType) == attribType);
    ASSERT(static_cast<UINT>(packedAttrib.semanticIndex) == semanticIndex);
    ASSERT(static_cast<gl::VertexFormatType>(packedAttrib.vertexFormatType) == vertexFormatType);
    ASSERT(static_cast<unsigned int>(packedAttrib.divisor) == divisor);

    static_assert(sizeof(uint32_t) == sizeof(PackedAttribute), "PackedAttributes must be 32-bits exactly.");

    attributeData[numAttributes++] = gl::bitCast<uint32_t>(packedAttrib);
}

bool InputLayoutCache::PackedAttributeLayout::operator<(const PackedAttributeLayout &other) const
{
    if (numAttributes != other.numAttributes)
    {
        return numAttributes < other.numAttributes;
    }

    if (flags != other.flags)
    {
        return flags < other.flags;
    }

    return memcmp(attributeData, other.attributeData, sizeof(uint32_t) * numAttributes) < 0;
}

InputLayoutCache::InputLayoutCache()
    : mCurrentIL(nullptr),
      mPointSpriteVertexBuffer(nullptr),
      mPointSpriteIndexBuffer(nullptr),
      mCacheSize(kDefaultCacheSize),
      mDevice(nullptr),
      mDeviceContext(nullptr)
{
    mCurrentBuffers.fill(nullptr);
    mCurrentVertexStrides.fill(std::numeric_limits<UINT>::max());
    mCurrentVertexOffsets.fill(std::numeric_limits<UINT>::max());
    mCurrentAttributes.reserve(gl::MAX_VERTEX_ATTRIBS);
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
    mFeatureLevel = device->GetFeatureLevel();
}

void InputLayoutCache::clear()
{
    for (auto &layout : mLayoutMap)
    {
        SafeRelease(layout.second);
    }
    mLayoutMap.clear();
    SafeRelease(mPointSpriteVertexBuffer);
    SafeRelease(mPointSpriteIndexBuffer);
    markDirty();
}

void InputLayoutCache::markDirty()
{
    mCurrentIL = nullptr;
    for (unsigned int i = 0; i < gl::MAX_VERTEX_ATTRIBS; i++)
    {
        mCurrentBuffers[i]       = nullptr;
        mCurrentVertexStrides[i] = static_cast<UINT>(-1);
        mCurrentVertexOffsets[i] = static_cast<UINT>(-1);
    }
}

gl::Error InputLayoutCache::applyVertexBuffers(
    const gl::State &state,
    const std::vector<TranslatedAttribute> &vertexArrayAttribs,
    const std::vector<TranslatedAttribute> &currentValueAttribs,
    GLenum mode,
    GLint start,
    TranslatedIndexData *indexInfo,
    GLsizei numIndicesPerInstance)
{
    ASSERT(mDevice && mDeviceContext);

    gl::Program *program   = state.getProgram();
    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(program);

    bool programUsesInstancedPointSprites = programD3D->usesPointSize() && programD3D->usesInstancedPointSpriteEmulation();
    bool instancedPointSpritesActive = programUsesInstancedPointSprites && (mode == GL_POINTS);

    AttribIndexArray sortedSemanticIndices;
    SortAttributesByLayout(program, vertexArrayAttribs, currentValueAttribs, &sortedSemanticIndices,
                           &mCurrentAttributes);

    // If we are using FL 9_3, make sure the first attribute is not instanced
    if (mFeatureLevel <= D3D_FEATURE_LEVEL_9_3 && !mCurrentAttributes.empty())
    {
        if (mCurrentAttributes[0]->divisor > 0)
        {
            Optional<size_t> firstNonInstancedIndex = FindFirstNonInstanced(mCurrentAttributes);
            if (firstNonInstancedIndex.valid())
            {
                size_t index = firstNonInstancedIndex.value();
                std::swap(mCurrentAttributes[0], mCurrentAttributes[index]);
                std::swap(sortedSemanticIndices[0], sortedSemanticIndices[index]);
            }
        }
    }

    ANGLE_TRY(updateInputLayout(state, mode, sortedSemanticIndices, numIndicesPerInstance));

    bool dirtyBuffers = false;
    size_t minDiff    = gl::MAX_VERTEX_ATTRIBS;
    size_t maxDiff    = 0;

    // Note that if we use instance emulation, we reserve the first buffer slot.
    size_t reservedBuffers = GetReservedBufferCount(programUsesInstancedPointSprites);

    for (size_t attribIndex = 0; attribIndex < (gl::MAX_VERTEX_ATTRIBS - reservedBuffers);
         ++attribIndex)
    {
        ID3D11Buffer *buffer = nullptr;
        UINT vertexStride    = 0;
        UINT vertexOffset    = 0;

        if (attribIndex < mCurrentAttributes.size())
        {
            const auto &attrib      = *mCurrentAttributes[attribIndex];
            Buffer11 *bufferStorage = attrib.storage ? GetAs<Buffer11>(attrib.storage) : nullptr;

            // If indexed pointsprite emulation is active, then we need to take a less efficent code path.
            // Emulated indexed pointsprite rendering requires that the vertex buffers match exactly to
            // the indices passed by the caller.  This could expand or shrink the vertex buffer depending
            // on the number of points indicated by the index list or how many duplicates are found on the index list.
            if (bufferStorage == nullptr)
            {
                ASSERT(attrib.vertexBuffer.get());
                buffer = GetAs<VertexBuffer11>(attrib.vertexBuffer.get())->getBuffer();
            }
            else if (instancedPointSpritesActive && (indexInfo != nullptr))
            {
                if (indexInfo->srcIndexData.srcBuffer != nullptr)
                {
                    const uint8_t *bufferData = nullptr;
                    ANGLE_TRY(indexInfo->srcIndexData.srcBuffer->getData(&bufferData));
                    ASSERT(bufferData != nullptr);

                    ptrdiff_t offset =
                        reinterpret_cast<ptrdiff_t>(indexInfo->srcIndexData.srcIndices);
                    indexInfo->srcIndexData.srcBuffer  = nullptr;
                    indexInfo->srcIndexData.srcIndices = bufferData + offset;
                }

                ANGLE_TRY_RESULT(bufferStorage->getEmulatedIndexedBuffer(&indexInfo->srcIndexData,
                                                                         attrib, start),
                                 buffer);
            }
            else
            {
                ANGLE_TRY_RESULT(
                    bufferStorage->getBuffer(BUFFER_USAGE_VERTEX_OR_TRANSFORM_FEEDBACK), buffer);
            }

            vertexStride = attrib.stride;
            ANGLE_TRY_RESULT(attrib.computeOffset(start), vertexOffset);
        }

        size_t bufferIndex = reservedBuffers + attribIndex;

        if (buffer != mCurrentBuffers[bufferIndex] ||
            vertexStride != mCurrentVertexStrides[bufferIndex] ||
            vertexOffset != mCurrentVertexOffsets[bufferIndex])
        {
            dirtyBuffers = true;
            minDiff      = std::min(minDiff, bufferIndex);
            maxDiff      = std::max(maxDiff, bufferIndex);

            mCurrentBuffers[bufferIndex]       = buffer;
            mCurrentVertexStrides[bufferIndex] = vertexStride;
            mCurrentVertexOffsets[bufferIndex] = vertexOffset;
        }
    }

    // Instanced PointSprite emulation requires two additional ID3D11Buffers. A vertex buffer needs
    // to be created and added to the list of current buffers, strides and offsets collections.
    // This buffer contains the vertices for a single PointSprite quad.
    // An index buffer also needs to be created and applied because rendering instanced data on
    // D3D11 FL9_3 requires DrawIndexedInstanced() to be used. Shaders that contain gl_PointSize and
    // used without the GL_POINTS rendering mode require a vertex buffer because some drivers cannot
    // handle missing vertex data and will TDR the system.
    if (programUsesInstancedPointSprites)
    {
        HRESULT result = S_OK;
        const UINT pointSpriteVertexStride = sizeof(float) * 5;

        if (!mPointSpriteVertexBuffer)
        {
            static const float pointSpriteVertices[] =
            {
                // Position        // TexCoord
               -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
               -1.0f,  1.0f, 0.0f, 0.0f, 0.0f,
                1.0f,  1.0f, 0.0f, 1.0f, 0.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 1.0f,
               -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
                1.0f,  1.0f, 0.0f, 1.0f, 0.0f,
            };

            D3D11_SUBRESOURCE_DATA vertexBufferData = { pointSpriteVertices, 0, 0 };
            D3D11_BUFFER_DESC vertexBufferDesc;
            vertexBufferDesc.ByteWidth = sizeof(pointSpriteVertices);
            vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
            vertexBufferDesc.CPUAccessFlags = 0;
            vertexBufferDesc.MiscFlags = 0;
            vertexBufferDesc.StructureByteStride = 0;

            result = mDevice->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &mPointSpriteVertexBuffer);
            if (FAILED(result))
            {
                return gl::Error(GL_OUT_OF_MEMORY, "Failed to create instanced pointsprite emulation vertex buffer, HRESULT: 0x%08x", result);
            }
        }

        mCurrentBuffers[0] = mPointSpriteVertexBuffer;
        // Set the stride to 0 if GL_POINTS mode is not being used to instruct the driver to avoid
        // indexing into the vertex buffer.
        mCurrentVertexStrides[0] = instancedPointSpritesActive ? pointSpriteVertexStride : 0;
        mCurrentVertexOffsets[0] = 0;

        // Update maxDiff to include the additional point sprite vertex buffer
        // to ensure that IASetVertexBuffers uses the correct buffer count.
        minDiff = 0;
        maxDiff = std::max(maxDiff, static_cast<size_t>(0));

        if (!mPointSpriteIndexBuffer)
        {
            // Create an index buffer and set it for pointsprite rendering
            static const unsigned short pointSpriteIndices[] =
            {
                0, 1, 2, 3, 4, 5,
            };

            D3D11_SUBRESOURCE_DATA indexBufferData = { pointSpriteIndices, 0, 0 };
            D3D11_BUFFER_DESC indexBufferDesc;
            indexBufferDesc.ByteWidth = sizeof(pointSpriteIndices);
            indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
            indexBufferDesc.CPUAccessFlags = 0;
            indexBufferDesc.MiscFlags = 0;
            indexBufferDesc.StructureByteStride = 0;

            result = mDevice->CreateBuffer(&indexBufferDesc, &indexBufferData, &mPointSpriteIndexBuffer);
            if (FAILED(result))
            {
                SafeRelease(mPointSpriteVertexBuffer);
                return gl::Error(GL_OUT_OF_MEMORY, "Failed to create instanced pointsprite emulation index buffer, HRESULT: 0x%08x", result);
            }
        }

        if (instancedPointSpritesActive)
        {
            // The index buffer is applied here because Instanced PointSprite emulation uses the a
            // non-indexed rendering path in ANGLE (DrawArrays). This means that applyIndexBuffer()
            // on the renderer will not be called and setting this buffer here ensures that the
            // rendering path will contain the correct index buffers.
            mDeviceContext->IASetIndexBuffer(mPointSpriteIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
        }
    }

    if (dirtyBuffers)
    {
        ASSERT(minDiff <= maxDiff && maxDiff < gl::MAX_VERTEX_ATTRIBS);
        mDeviceContext->IASetVertexBuffers(
            static_cast<UINT>(minDiff), static_cast<UINT>(maxDiff - minDiff + 1),
            &mCurrentBuffers[minDiff], &mCurrentVertexStrides[minDiff],
            &mCurrentVertexOffsets[minDiff]);
    }

    return gl::NoError();
}

gl::Error InputLayoutCache::updateVertexOffsetsForPointSpritesEmulation(GLint startVertex,
                                                                        GLsizei emulatedInstanceId)
{
    size_t reservedBuffers = GetReservedBufferCount(true);
    for (size_t attribIndex = 0; attribIndex < mCurrentAttributes.size(); ++attribIndex)
    {
        const auto &attrib = *mCurrentAttributes[attribIndex];
        size_t bufferIndex = reservedBuffers + attribIndex;

        if (attrib.divisor > 0)
        {
            unsigned int offset = 0;
            ANGLE_TRY_RESULT(attrib.computeOffset(startVertex), offset);
            mCurrentVertexOffsets[bufferIndex] =
                offset + (attrib.stride * (emulatedInstanceId / attrib.divisor));
        }
    }

    mDeviceContext->IASetVertexBuffers(0, gl::MAX_VERTEX_ATTRIBS, mCurrentBuffers.data(),
                                       mCurrentVertexStrides.data(), mCurrentVertexOffsets.data());

    return gl::NoError();
}

gl::Error InputLayoutCache::updateInputLayout(const gl::State &state,
                                              GLenum mode,
                                              const AttribIndexArray &sortedSemanticIndices,
                                              GLsizei numIndicesPerInstance)
{
    gl::Program *program         = state.getProgram();
    const auto &shaderAttributes = program->getAttributes();
    PackedAttributeLayout layout;

    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(program);
    bool programUsesInstancedPointSprites =
        programD3D->usesPointSize() && programD3D->usesInstancedPointSpriteEmulation();
    bool instancedPointSpritesActive = programUsesInstancedPointSprites && (mode == GL_POINTS);

    if (programUsesInstancedPointSprites)
    {
        layout.flags |= PackedAttributeLayout::FLAG_USES_INSTANCED_SPRITES;
    }

    if (instancedPointSpritesActive)
    {
        layout.flags |= PackedAttributeLayout::FLAG_INSTANCED_SPRITES_ACTIVE;
    }

    if (numIndicesPerInstance > 0)
    {
        layout.flags |= PackedAttributeLayout::FLAG_INSTANCED_RENDERING_ACTIVE;
    }

    const auto &attribs            = state.getVertexArray()->getVertexAttributes();
    const auto &locationToSemantic = programD3D->getAttribLocationToD3DSemantics();

    for (unsigned long attribIndex : angle::IterateBitSet(program->getActiveAttribLocationsMask()))
    {
        // Record the type of the associated vertex shader vector in our key
        // This will prevent mismatched vertex shaders from using the same input layout
        GLenum glslElementType = GetGLSLAttributeType(shaderAttributes, attribIndex);

        const auto &attrib = attribs[attribIndex];
        int d3dSemantic    = locationToSemantic[attribIndex];

        const auto &currentValue              = state.getVertexAttribCurrentValue(attribIndex);
        gl::VertexFormatType vertexFormatType = gl::GetVertexFormatType(attrib, currentValue.Type);

        layout.addAttributeData(glslElementType, d3dSemantic, vertexFormatType, attrib.divisor);
    }

    ID3D11InputLayout *inputLayout = nullptr;
    if (layout.numAttributes > 0 || layout.flags != 0)
    {
        auto layoutMapIt = mLayoutMap.find(layout);
        if (layoutMapIt != mLayoutMap.end())
        {
            inputLayout = layoutMapIt->second;
        }
        else
        {
            ANGLE_TRY(createInputLayout(sortedSemanticIndices, mode, program, numIndicesPerInstance,
                                        &inputLayout));
            if (mLayoutMap.size() >= mCacheSize)
            {
                TRACE("Overflowed the limit of %u input layouts, purging half the cache.",
                      mCacheSize);

                // Randomly release every second element
                auto it = mLayoutMap.begin();
                while (it != mLayoutMap.end())
                {
                    it++;
                    if (it != mLayoutMap.end())
                    {
                        // c++11 erase allows us to easily delete the current iterator.
                        SafeRelease(it->second);
                        it = mLayoutMap.erase(it);
                    }
                }
            }

            mLayoutMap[layout] = inputLayout;
        }
    }

    if (inputLayout != mCurrentIL)
    {
        mDeviceContext->IASetInputLayout(inputLayout);
        mCurrentIL = inputLayout;
    }

    return gl::NoError();
}

gl::Error InputLayoutCache::createInputLayout(const AttribIndexArray &sortedSemanticIndices,
                                              GLenum mode,
                                              gl::Program *program,
                                              GLsizei numIndicesPerInstance,
                                              ID3D11InputLayout **inputLayoutOut)
{
    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(program);

    bool programUsesInstancedPointSprites =
        programD3D->usesPointSize() && programD3D->usesInstancedPointSpriteEmulation();

    unsigned int inputElementCount = 0;
    std::array<D3D11_INPUT_ELEMENT_DESC, gl::MAX_VERTEX_ATTRIBS> inputElements;

    for (size_t attribIndex = 0; attribIndex < mCurrentAttributes.size(); ++attribIndex)
    {
        const auto &attrib    = *mCurrentAttributes[attribIndex];
        const int sortedIndex = sortedSemanticIndices[attribIndex];

        D3D11_INPUT_CLASSIFICATION inputClass =
            attrib.divisor > 0 ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;

        const auto &vertexFormatType =
            gl::GetVertexFormatType(*attrib.attribute, attrib.currentValueType);
        const auto &vertexFormatInfo = d3d11::GetVertexFormatInfo(vertexFormatType, mFeatureLevel);

        auto *inputElement = &inputElements[inputElementCount];

        inputElement->SemanticName         = "TEXCOORD";
        inputElement->SemanticIndex        = sortedIndex;
        inputElement->Format               = vertexFormatInfo.nativeFormat;
        inputElement->InputSlot            = static_cast<UINT>(attribIndex);
        inputElement->AlignedByteOffset    = 0;
        inputElement->InputSlotClass       = inputClass;
        inputElement->InstanceDataStepRate = attrib.divisor;

        inputElementCount++;
    }

    // Instanced PointSprite emulation requires additional entries in the
    // inputlayout to support the vertices that make up the pointsprite quad.
    // We do this even if mode != GL_POINTS, since the shader signature has these inputs, and the
    // input layout must match the shader
    if (programUsesInstancedPointSprites)
    {
        // On 9_3, we must ensure that slot 0 contains non-instanced data.
        // If slot 0 currently contains instanced data then we swap it with a non-instanced element.
        // Note that instancing is only available on 9_3 via ANGLE_instanced_arrays, since 9_3
        // doesn't support OpenGL ES 3.0.
        // As per the spec for ANGLE_instanced_arrays, not all attributes can be instanced
        // simultaneously, so a non-instanced element must exist.
        for (size_t elementIndex = 0; elementIndex < inputElementCount; ++elementIndex)
        {
            // If rendering points and instanced pointsprite emulation is being used, the
            // inputClass is required to be configured as per instance data
            if (mode == GL_POINTS)
            {
                inputElements[elementIndex].InputSlotClass       = D3D11_INPUT_PER_INSTANCE_DATA;
                inputElements[elementIndex].InstanceDataStepRate = 1;
                if (numIndicesPerInstance > 0 && mCurrentAttributes[elementIndex]->divisor > 0)
                {
                    inputElements[elementIndex].InstanceDataStepRate = numIndicesPerInstance;
                }
            }
            inputElements[elementIndex].InputSlot++;
        }

        inputElements[inputElementCount].SemanticName         = "SPRITEPOSITION";
        inputElements[inputElementCount].SemanticIndex        = 0;
        inputElements[inputElementCount].Format               = DXGI_FORMAT_R32G32B32_FLOAT;
        inputElements[inputElementCount].InputSlot            = 0;
        inputElements[inputElementCount].AlignedByteOffset    = 0;
        inputElements[inputElementCount].InputSlotClass       = D3D11_INPUT_PER_VERTEX_DATA;
        inputElements[inputElementCount].InstanceDataStepRate = 0;
        inputElementCount++;

        inputElements[inputElementCount].SemanticName         = "SPRITETEXCOORD";
        inputElements[inputElementCount].SemanticIndex        = 0;
        inputElements[inputElementCount].Format               = DXGI_FORMAT_R32G32_FLOAT;
        inputElements[inputElementCount].InputSlot            = 0;
        inputElements[inputElementCount].AlignedByteOffset    = sizeof(float) * 3;
        inputElements[inputElementCount].InputSlotClass       = D3D11_INPUT_PER_VERTEX_DATA;
        inputElements[inputElementCount].InstanceDataStepRate = 0;
        inputElementCount++;
    }

    const gl::InputLayout &shaderInputLayout = GetInputLayout(mCurrentAttributes);

    ShaderExecutableD3D *shader = nullptr;
    ANGLE_TRY(programD3D->getVertexExecutableForInputLayout(shaderInputLayout, &shader, nullptr));

    ShaderExecutableD3D *shader11 = GetAs<ShaderExecutable11>(shader);

    HRESULT result =
        mDevice->CreateInputLayout(inputElements.data(), inputElementCount, shader11->getFunction(),
                                   shader11->getLength(), inputLayoutOut);
    if (FAILED(result))
    {
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Failed to create internal input layout, HRESULT: 0x%08x", result);
    }

    return gl::NoError();
}

}  // namespace rx
