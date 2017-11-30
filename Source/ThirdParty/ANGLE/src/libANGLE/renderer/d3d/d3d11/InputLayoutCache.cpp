//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// InputLayoutCache.cpp: Defines InputLayoutCache, a class that builds and caches
// D3D11 input layouts.

#include "libANGLE/renderer/d3d/d3d11/InputLayoutCache.h"

#include "common/bitset_utils.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/Program.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/VertexAttribute.h"
#include "libANGLE/renderer/d3d/IndexDataManager.h"
#include "libANGLE/renderer/d3d/ProgramD3D.h"
#include "libANGLE/renderer/d3d/VertexDataManager.h"
#include "libANGLE/renderer/d3d/d3d11/Buffer11.h"
#include "libANGLE/renderer/d3d/d3d11/Context11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/ShaderExecutable11.h"
#include "libANGLE/renderer/d3d/d3d11/VertexArray11.h"
#include "libANGLE/renderer/d3d/d3d11/VertexBuffer11.h"
#include "libANGLE/renderer/d3d/d3d11/formatutils11.h"

namespace rx
{

namespace
{

size_t GetReservedBufferCount(bool usesPointSpriteEmulation)
{
    return usesPointSpriteEmulation ? 1 : 0;
}

GLenum GetGLSLAttributeType(const std::vector<sh::Attribute> &shaderAttributes, size_t index)
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
        int intIndex          = static_cast<int>(index);

        if (intIndex >= attrib.location && intIndex < attrib.location + rows)
        {
            return transposedType;
        }
    }

    UNREACHABLE();
    return GL_NONE;
}

struct PackedAttribute
{
    uint8_t attribType;
    uint8_t semanticIndex;
    uint8_t vertexFormatType;
    uint8_t divisor;
};

} // anonymous namespace

PackedAttributeLayout::PackedAttributeLayout() : numAttributes(0), flags(0), attributeData({})
{
}

PackedAttributeLayout::PackedAttributeLayout(const PackedAttributeLayout &other) = default;

void PackedAttributeLayout::addAttributeData(GLenum glType,
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

bool PackedAttributeLayout::operator==(const PackedAttributeLayout &other) const
{
    return (numAttributes == other.numAttributes) && (flags == other.flags) &&
           (attributeData == other.attributeData);
}

InputLayoutCache::InputLayoutCache()
    : mLayoutCache(kDefaultCacheSize * 2), mPointSpriteVertexBuffer(), mPointSpriteIndexBuffer()
{
}

InputLayoutCache::~InputLayoutCache()
{
}

void InputLayoutCache::clear()
{
    mLayoutCache.Clear();
    mPointSpriteVertexBuffer.reset();
    mPointSpriteIndexBuffer.reset();
}

gl::Error InputLayoutCache::applyVertexBuffers(
    const gl::Context *context,
    const std::vector<const TranslatedAttribute *> &currentAttributes,
    GLenum mode,
    GLint start,
    bool isIndexedRendering)
{
    Renderer11 *renderer   = GetImplAs<Context11>(context)->getRenderer();
    const gl::State &state = context->getGLState();
    auto *stateManager     = renderer->getStateManager();
    gl::Program *program   = state.getProgram();
    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(program);

    bool programUsesInstancedPointSprites = programD3D->usesPointSize() && programD3D->usesInstancedPointSpriteEmulation();
    bool instancedPointSpritesActive = programUsesInstancedPointSprites && (mode == GL_POINTS);

    // Note that if we use instance emulation, we reserve the first buffer slot.
    size_t reservedBuffers = GetReservedBufferCount(programUsesInstancedPointSprites);

    for (size_t attribIndex = 0; attribIndex < (gl::MAX_VERTEX_ATTRIBS - reservedBuffers);
         ++attribIndex)
    {
        ID3D11Buffer *buffer = nullptr;
        UINT vertexStride    = 0;
        UINT vertexOffset    = 0;

        if (attribIndex < currentAttributes.size())
        {
            const auto &attrib      = *currentAttributes[attribIndex];
            Buffer11 *bufferStorage = attrib.storage ? GetAs<Buffer11>(attrib.storage) : nullptr;

            // If indexed pointsprite emulation is active, then we need to take a less efficent code path.
            // Emulated indexed pointsprite rendering requires that the vertex buffers match exactly to
            // the indices passed by the caller.  This could expand or shrink the vertex buffer depending
            // on the number of points indicated by the index list or how many duplicates are found on the index list.
            if (bufferStorage == nullptr)
            {
                ASSERT(attrib.vertexBuffer.get());
                buffer = GetAs<VertexBuffer11>(attrib.vertexBuffer.get())->getBuffer().get();
            }
            else if (instancedPointSpritesActive && isIndexedRendering)
            {
                VertexArray11 *vao11 = GetImplAs<VertexArray11>(state.getVertexArray());
                ASSERT(vao11->isCachedIndexInfoValid());
                TranslatedIndexData *indexInfo = vao11->getCachedIndexInfo();
                if (indexInfo->srcIndexData.srcBuffer != nullptr)
                {
                    const uint8_t *bufferData = nullptr;
                    ANGLE_TRY(indexInfo->srcIndexData.srcBuffer->getData(context, &bufferData));
                    ASSERT(bufferData != nullptr);

                    ptrdiff_t offset =
                        reinterpret_cast<ptrdiff_t>(indexInfo->srcIndexData.srcIndices);
                    indexInfo->srcIndexData.srcBuffer  = nullptr;
                    indexInfo->srcIndexData.srcIndices = bufferData + offset;
                }

                ANGLE_TRY_RESULT(bufferStorage->getEmulatedIndexedBuffer(
                                     context, &indexInfo->srcIndexData, attrib, start),
                                 buffer);
            }
            else
            {
                ANGLE_TRY_RESULT(
                    bufferStorage->getBuffer(context, BUFFER_USAGE_VERTEX_OR_TRANSFORM_FEEDBACK),
                    buffer);
            }

            vertexStride = attrib.stride;
            ANGLE_TRY_RESULT(attrib.computeOffset(start), vertexOffset);
        }

        size_t bufferIndex = reservedBuffers + attribIndex;

        stateManager->queueVertexBufferChange(bufferIndex, buffer, vertexStride, vertexOffset);
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
        const UINT pointSpriteVertexStride = sizeof(float) * 5;

        if (!mPointSpriteVertexBuffer.valid())
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

            ANGLE_TRY(renderer->allocateResource(vertexBufferDesc, &vertexBufferData,
                                                 &mPointSpriteVertexBuffer));
        }

        // Set the stride to 0 if GL_POINTS mode is not being used to instruct the driver to avoid
        // indexing into the vertex buffer.
        UINT stride = instancedPointSpritesActive ? pointSpriteVertexStride : 0;
        stateManager->queueVertexBufferChange(0, mPointSpriteVertexBuffer.get(), stride, 0);

        if (!mPointSpriteIndexBuffer.valid())
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

            ANGLE_TRY(renderer->allocateResource(indexBufferDesc, &indexBufferData,
                                                 &mPointSpriteIndexBuffer));
        }

        if (instancedPointSpritesActive)
        {
            // The index buffer is applied here because Instanced PointSprite emulation uses the a
            // non-indexed rendering path in ANGLE (DrawArrays). This means that applyIndexBuffer()
            // on the renderer will not be called and setting this buffer here ensures that the
            // rendering path will contain the correct index buffers.
            stateManager->setIndexBuffer(mPointSpriteIndexBuffer.get(), DXGI_FORMAT_R16_UINT, 0);
        }
    }

    stateManager->applyVertexBufferChanges();
    return gl::NoError();
}

gl::Error InputLayoutCache::updateVertexOffsetsForPointSpritesEmulation(
    Renderer11 *renderer,
    const std::vector<const TranslatedAttribute *> &currentAttributes,
    GLint startVertex,
    GLsizei emulatedInstanceId)
{
    auto *stateManager = renderer->getStateManager();

    size_t reservedBuffers = GetReservedBufferCount(true);
    for (size_t attribIndex = 0; attribIndex < currentAttributes.size(); ++attribIndex)
    {
        const auto &attrib = *currentAttributes[attribIndex];
        size_t bufferIndex = reservedBuffers + attribIndex;

        if (attrib.divisor > 0)
        {
            unsigned int offset = 0;
            ANGLE_TRY_RESULT(attrib.computeOffset(startVertex), offset);
            offset += (attrib.stride * (emulatedInstanceId / attrib.divisor));
            stateManager->queueVertexOffsetChange(bufferIndex, offset);
        }
    }

    stateManager->applyVertexBufferChanges();
    return gl::NoError();
}

gl::Error InputLayoutCache::updateInputLayout(
    Renderer11 *renderer,
    const gl::State &state,
    const std::vector<const TranslatedAttribute *> &currentAttributes,
    GLenum mode,
    const AttribIndexArray &sortedSemanticIndices,
    const DrawCallVertexParams &vertexParams)
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

    if (vertexParams.instances() > 0)
    {
        layout.flags |= PackedAttributeLayout::FLAG_INSTANCED_RENDERING_ACTIVE;
    }

    const auto &attribs            = state.getVertexArray()->getVertexAttributes();
    const auto &bindings           = state.getVertexArray()->getVertexBindings();
    const auto &locationToSemantic = programD3D->getAttribLocationToD3DSemantics();
    int divisorMultiplier          = program->usesMultiview() ? program->getNumViews() : 1;

    for (size_t attribIndex : program->getActiveAttribLocationsMask())
    {
        // Record the type of the associated vertex shader vector in our key
        // This will prevent mismatched vertex shaders from using the same input layout
        GLenum glslElementType = GetGLSLAttributeType(shaderAttributes, attribIndex);

        const auto &attrib = attribs[attribIndex];
        const auto &binding = bindings[attrib.bindingIndex];
        int d3dSemantic    = locationToSemantic[attribIndex];

        const auto &currentValue =
            state.getVertexAttribCurrentValue(static_cast<unsigned int>(attribIndex));
        gl::VertexFormatType vertexFormatType = gl::GetVertexFormatType(attrib, currentValue.Type);

        layout.addAttributeData(glslElementType, d3dSemantic, vertexFormatType,
                                binding.getDivisor() * divisorMultiplier);
    }

    const d3d11::InputLayout *inputLayout = nullptr;
    if (layout.numAttributes > 0 || layout.flags != 0)
    {
        auto it = mLayoutCache.Get(layout);
        if (it != mLayoutCache.end())
        {
            inputLayout = &it->second;
        }
        else
        {
            angle::TrimCache(mLayoutCache.max_size() / 2, kGCLimit, "input layout", &mLayoutCache);

            d3d11::InputLayout newInputLayout;
            ANGLE_TRY(createInputLayout(renderer, sortedSemanticIndices, currentAttributes, mode,
                                        program, vertexParams, &newInputLayout));

            auto insertIt = mLayoutCache.Put(layout, std::move(newInputLayout));
            inputLayout   = &insertIt->second;
        }
    }

    renderer->getStateManager()->setInputLayout(inputLayout);
    return gl::NoError();
}

gl::Error InputLayoutCache::createInputLayout(
    Renderer11 *renderer,
    const AttribIndexArray &sortedSemanticIndices,
    const std::vector<const TranslatedAttribute *> &currentAttributes,
    GLenum mode,
    gl::Program *program,
    const DrawCallVertexParams &vertexParams,
    d3d11::InputLayout *inputLayoutOut)
{
    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(program);
    auto featureLevel      = renderer->getRenderer11DeviceCaps().featureLevel;

    bool programUsesInstancedPointSprites =
        programD3D->usesPointSize() && programD3D->usesInstancedPointSpriteEmulation();

    unsigned int inputElementCount = 0;
    std::array<D3D11_INPUT_ELEMENT_DESC, gl::MAX_VERTEX_ATTRIBS> inputElements;

    for (size_t attribIndex = 0; attribIndex < currentAttributes.size(); ++attribIndex)
    {
        const auto &attrib    = *currentAttributes[attribIndex];
        const int sortedIndex = sortedSemanticIndices[attribIndex];

        D3D11_INPUT_CLASSIFICATION inputClass =
            attrib.divisor > 0 ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;

        const auto &vertexFormatType =
            gl::GetVertexFormatType(*attrib.attribute, attrib.currentValueType);
        const auto &vertexFormatInfo = d3d11::GetVertexFormatInfo(vertexFormatType, featureLevel);

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

        GLsizei numIndicesPerInstance = 0;
        if (vertexParams.instances() > 0)
        {
            // This may trigger an evaluation of the index range.
            numIndicesPerInstance = vertexParams.vertexCount();
        }

        for (size_t elementIndex = 0; elementIndex < inputElementCount; ++elementIndex)
        {
            // If rendering points and instanced pointsprite emulation is being used, the
            // inputClass is required to be configured as per instance data
            if (mode == GL_POINTS)
            {
                inputElements[elementIndex].InputSlotClass       = D3D11_INPUT_PER_INSTANCE_DATA;
                inputElements[elementIndex].InstanceDataStepRate = 1;
                if (numIndicesPerInstance > 0 && currentAttributes[elementIndex]->divisor > 0)
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

    ShaderExecutableD3D *shader = nullptr;
    ANGLE_TRY(programD3D->getVertexExecutableForCachedInputLayout(&shader, nullptr));

    ShaderExecutableD3D *shader11 = GetAs<ShaderExecutable11>(shader);

    InputElementArray inputElementArray(inputElements.data(), inputElementCount);
    ShaderData vertexShaderData(shader11->getFunction(), shader11->getLength());

    ANGLE_TRY(renderer->allocateResource(inputElementArray, &vertexShaderData, inputLayoutOut));
    return gl::NoError();
}

void InputLayoutCache::setCacheSize(size_t newCacheSize)
{
    // Forces a reset of the cache.
    LayoutCache newCache(newCacheSize);
    mLayoutCache.Swap(newCache);
}

}  // namespace rx
