//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// queryutils.cpp: Utilities for querying values from GL objects

#include "libANGLE/queryutils.h"

#include "common/utilities.h"

#include "libANGLE/Buffer.h"
#include "libANGLE/Config.h"
#include "libANGLE/Context.h"
#include "libANGLE/Fence.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/Program.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/Sampler.h"
#include "libANGLE/Shader.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Texture.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/VertexAttribute.h"
#include "libANGLE/queryconversions.h"

namespace gl
{

namespace
{

template <typename ParamType>
void QueryTexLevelParameterBase(const Texture *texture,
                                GLenum target,
                                GLint level,
                                GLenum pname,
                                ParamType *params)
{
    ASSERT(texture != nullptr);
    const InternalFormat *info = texture->getTextureState().getImageDesc(target, level).format.info;

    switch (pname)
    {
        case GL_TEXTURE_RED_TYPE:
            *params = CastFromGLintStateValue<ParamType>(
                pname, info->redBits ? info->componentType : GL_NONE);
            break;
        case GL_TEXTURE_GREEN_TYPE:
            *params = CastFromGLintStateValue<ParamType>(
                pname, info->greenBits ? info->componentType : GL_NONE);
            break;
        case GL_TEXTURE_BLUE_TYPE:
            *params = CastFromGLintStateValue<ParamType>(
                pname, info->blueBits ? info->componentType : GL_NONE);
            break;
        case GL_TEXTURE_ALPHA_TYPE:
            *params = CastFromGLintStateValue<ParamType>(
                pname, info->alphaBits ? info->componentType : GL_NONE);
            break;
        case GL_TEXTURE_DEPTH_TYPE:
            *params = CastFromGLintStateValue<ParamType>(
                pname, info->depthBits ? info->componentType : GL_NONE);
            break;
        case GL_TEXTURE_RED_SIZE:
            *params = CastFromGLintStateValue<ParamType>(pname, info->redBits);
            break;
        case GL_TEXTURE_GREEN_SIZE:
            *params = CastFromGLintStateValue<ParamType>(pname, info->greenBits);
            break;
        case GL_TEXTURE_BLUE_SIZE:
            *params = CastFromGLintStateValue<ParamType>(pname, info->blueBits);
            break;
        case GL_TEXTURE_ALPHA_SIZE:
            *params = CastFromGLintStateValue<ParamType>(pname, info->alphaBits);
            break;
        case GL_TEXTURE_DEPTH_SIZE:
            *params = CastFromGLintStateValue<ParamType>(pname, info->depthBits);
            break;
        case GL_TEXTURE_STENCIL_SIZE:
            *params = CastFromGLintStateValue<ParamType>(pname, info->stencilBits);
            break;
        case GL_TEXTURE_SHARED_SIZE:
            *params = CastFromGLintStateValue<ParamType>(pname, info->sharedBits);
            break;
        case GL_TEXTURE_INTERNAL_FORMAT:
            *params = CastFromGLintStateValue<ParamType>(
                pname, info->internalFormat ? info->internalFormat : GL_RGBA);
            break;
        case GL_TEXTURE_WIDTH:
            *params = CastFromGLintStateValue<ParamType>(
                pname, static_cast<uint32_t>(texture->getWidth(target, level)));
            break;
        case GL_TEXTURE_HEIGHT:
            *params = CastFromGLintStateValue<ParamType>(
                pname, static_cast<uint32_t>(texture->getHeight(target, level)));
            break;
        case GL_TEXTURE_DEPTH:
            *params = CastFromGLintStateValue<ParamType>(
                pname, static_cast<uint32_t>(texture->getDepth(target, level)));
            break;
        case GL_TEXTURE_SAMPLES:
            *params = CastFromStateValue<ParamType>(pname, texture->getSamples(target, level));
            break;
        case GL_TEXTURE_FIXED_SAMPLE_LOCATIONS:
            *params = CastFromStateValue<ParamType>(
                pname, static_cast<GLint>(texture->getFixedSampleLocations(target, level)));
            break;
        case GL_TEXTURE_COMPRESSED:
            *params = CastFromStateValue<ParamType>(pname, static_cast<GLint>(info->compressed));
            break;
        default:
            UNREACHABLE();
            break;
    }
}

template <typename ParamType>
void QueryTexParameterBase(const Texture *texture, GLenum pname, ParamType *params)
{
    ASSERT(texture != nullptr);

    switch (pname)
    {
        case GL_TEXTURE_MAG_FILTER:
            *params = CastFromGLintStateValue<ParamType>(pname, texture->getMagFilter());
            break;
        case GL_TEXTURE_MIN_FILTER:
            *params = CastFromGLintStateValue<ParamType>(pname, texture->getMinFilter());
            break;
        case GL_TEXTURE_WRAP_S:
            *params = CastFromGLintStateValue<ParamType>(pname, texture->getWrapS());
            break;
        case GL_TEXTURE_WRAP_T:
            *params = CastFromGLintStateValue<ParamType>(pname, texture->getWrapT());
            break;
        case GL_TEXTURE_WRAP_R:
            *params = CastFromGLintStateValue<ParamType>(pname, texture->getWrapR());
            break;
        case GL_TEXTURE_IMMUTABLE_FORMAT:
            *params = CastFromGLintStateValue<ParamType>(pname, texture->getImmutableFormat());
            break;
        case GL_TEXTURE_IMMUTABLE_LEVELS:
            *params = CastFromGLintStateValue<ParamType>(pname, texture->getImmutableLevels());
            break;
        case GL_TEXTURE_USAGE_ANGLE:
            *params = CastFromGLintStateValue<ParamType>(pname, texture->getUsage());
            break;
        case GL_TEXTURE_MAX_ANISOTROPY_EXT:
            *params = CastFromStateValue<ParamType>(pname, texture->getMaxAnisotropy());
            break;
        case GL_TEXTURE_SWIZZLE_R:
            *params = CastFromGLintStateValue<ParamType>(pname, texture->getSwizzleRed());
            break;
        case GL_TEXTURE_SWIZZLE_G:
            *params = CastFromGLintStateValue<ParamType>(pname, texture->getSwizzleGreen());
            break;
        case GL_TEXTURE_SWIZZLE_B:
            *params = CastFromGLintStateValue<ParamType>(pname, texture->getSwizzleBlue());
            break;
        case GL_TEXTURE_SWIZZLE_A:
            *params = CastFromGLintStateValue<ParamType>(pname, texture->getSwizzleAlpha());
            break;
        case GL_TEXTURE_BASE_LEVEL:
            *params = CastFromGLintStateValue<ParamType>(pname, texture->getBaseLevel());
            break;
        case GL_TEXTURE_MAX_LEVEL:
            *params = CastFromGLintStateValue<ParamType>(pname, texture->getMaxLevel());
            break;
        case GL_TEXTURE_MIN_LOD:
            *params = CastFromStateValue<ParamType>(pname, texture->getSamplerState().minLod);
            break;
        case GL_TEXTURE_MAX_LOD:
            *params = CastFromStateValue<ParamType>(pname, texture->getSamplerState().maxLod);
            break;
        case GL_TEXTURE_COMPARE_MODE:
            *params = CastFromGLintStateValue<ParamType>(pname, texture->getCompareMode());
            break;
        case GL_TEXTURE_COMPARE_FUNC:
            *params = CastFromGLintStateValue<ParamType>(pname, texture->getCompareFunc());
            break;
        case GL_TEXTURE_SRGB_DECODE_EXT:
            *params = CastFromGLintStateValue<ParamType>(pname, texture->getSRGBDecode());
            break;
        default:
            UNREACHABLE();
            break;
    }
}

template <typename ParamType>
void SetTexParameterBase(Context *context, Texture *texture, GLenum pname, const ParamType *params)
{
    ASSERT(texture != nullptr);

    switch (pname)
    {
        case GL_TEXTURE_WRAP_S:
            texture->setWrapS(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_WRAP_T:
            texture->setWrapT(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_WRAP_R:
            texture->setWrapR(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_MIN_FILTER:
            texture->setMinFilter(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_MAG_FILTER:
            texture->setMagFilter(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_USAGE_ANGLE:
            texture->setUsage(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_MAX_ANISOTROPY_EXT:
            texture->setMaxAnisotropy(CastQueryValueTo<GLfloat>(pname, params[0]));
            break;
        case GL_TEXTURE_COMPARE_MODE:
            texture->setCompareMode(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_COMPARE_FUNC:
            texture->setCompareFunc(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_SWIZZLE_R:
            texture->setSwizzleRed(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_SWIZZLE_G:
            texture->setSwizzleGreen(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_SWIZZLE_B:
            texture->setSwizzleBlue(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_SWIZZLE_A:
            texture->setSwizzleAlpha(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_BASE_LEVEL:
        {
            context->handleError(texture->setBaseLevel(
                context, clampCast<GLuint>(CastQueryValueTo<GLint>(pname, params[0]))));
            break;
        }
        case GL_TEXTURE_MAX_LEVEL:
            texture->setMaxLevel(clampCast<GLuint>(CastQueryValueTo<GLint>(pname, params[0])));
            break;
        case GL_TEXTURE_MIN_LOD:
            texture->setMinLod(CastQueryValueTo<GLfloat>(pname, params[0]));
            break;
        case GL_TEXTURE_MAX_LOD:
            texture->setMaxLod(CastQueryValueTo<GLfloat>(pname, params[0]));
            break;
        case GL_DEPTH_STENCIL_TEXTURE_MODE:
            texture->setDepthStencilTextureMode(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_SRGB_DECODE_EXT:
            texture->setSRGBDecode(ConvertToGLenum(pname, params[0]));
            break;
        default:
            UNREACHABLE();
            break;
    }
}

template <typename ParamType>
void QuerySamplerParameterBase(const Sampler *sampler, GLenum pname, ParamType *params)
{
    switch (pname)
    {
        case GL_TEXTURE_MIN_FILTER:
            *params = CastFromGLintStateValue<ParamType>(pname, sampler->getMinFilter());
            break;
        case GL_TEXTURE_MAG_FILTER:
            *params = CastFromGLintStateValue<ParamType>(pname, sampler->getMagFilter());
            break;
        case GL_TEXTURE_WRAP_S:
            *params = CastFromGLintStateValue<ParamType>(pname, sampler->getWrapS());
            break;
        case GL_TEXTURE_WRAP_T:
            *params = CastFromGLintStateValue<ParamType>(pname, sampler->getWrapT());
            break;
        case GL_TEXTURE_WRAP_R:
            *params = CastFromGLintStateValue<ParamType>(pname, sampler->getWrapR());
            break;
        case GL_TEXTURE_MAX_ANISOTROPY_EXT:
            *params = CastFromStateValue<ParamType>(pname, sampler->getMaxAnisotropy());
            break;
        case GL_TEXTURE_MIN_LOD:
            *params = CastFromStateValue<ParamType>(pname, sampler->getMinLod());
            break;
        case GL_TEXTURE_MAX_LOD:
            *params = CastFromStateValue<ParamType>(pname, sampler->getMaxLod());
            break;
        case GL_TEXTURE_COMPARE_MODE:
            *params = CastFromGLintStateValue<ParamType>(pname, sampler->getCompareMode());
            break;
        case GL_TEXTURE_COMPARE_FUNC:
            *params = CastFromGLintStateValue<ParamType>(pname, sampler->getCompareFunc());
            break;
        case GL_TEXTURE_SRGB_DECODE_EXT:
            *params = CastFromGLintStateValue<ParamType>(pname, sampler->getSRGBDecode());
            break;
        default:
            UNREACHABLE();
            break;
    }
}

template <typename ParamType>
void SetSamplerParameterBase(Sampler *sampler, GLenum pname, const ParamType *params)
{
    switch (pname)
    {
        case GL_TEXTURE_WRAP_S:
            sampler->setWrapS(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_WRAP_T:
            sampler->setWrapT(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_WRAP_R:
            sampler->setWrapR(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_MIN_FILTER:
            sampler->setMinFilter(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_MAG_FILTER:
            sampler->setMagFilter(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_MAX_ANISOTROPY_EXT:
            sampler->setMaxAnisotropy(CastQueryValueTo<GLfloat>(pname, params[0]));
            break;
        case GL_TEXTURE_COMPARE_MODE:
            sampler->setCompareMode(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_COMPARE_FUNC:
            sampler->setCompareFunc(ConvertToGLenum(pname, params[0]));
            break;
        case GL_TEXTURE_MIN_LOD:
            sampler->setMinLod(CastQueryValueTo<GLfloat>(pname, params[0]));
            break;
        case GL_TEXTURE_MAX_LOD:
            sampler->setMaxLod(CastQueryValueTo<GLfloat>(pname, params[0]));
            break;
        case GL_TEXTURE_SRGB_DECODE_EXT:
            sampler->setSRGBDecode(ConvertToGLenum(pname, params[0]));
            break;
        default:
            UNREACHABLE();
            break;
    }
}

// Warning: you should ensure binding really matches attrib.bindingIndex before using this function.
template <typename ParamType, typename CurrentDataType, size_t CurrentValueCount>
void QueryVertexAttribBase(const VertexAttribute &attrib,
                           const VertexBinding &binding,
                           const CurrentDataType (&currentValueData)[CurrentValueCount],
                           GLenum pname,
                           ParamType *params)
{
    switch (pname)
    {
        case GL_CURRENT_VERTEX_ATTRIB:
            for (size_t i = 0; i < CurrentValueCount; ++i)
            {
                params[i] = CastFromStateValue<ParamType>(pname, currentValueData[i]);
            }
            break;
        case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
            *params = CastFromStateValue<ParamType>(pname, static_cast<GLint>(attrib.enabled));
            break;
        case GL_VERTEX_ATTRIB_ARRAY_SIZE:
            *params = CastFromGLintStateValue<ParamType>(pname, attrib.size);
            break;
        case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
            *params = CastFromGLintStateValue<ParamType>(pname, attrib.vertexAttribArrayStride);
            break;
        case GL_VERTEX_ATTRIB_ARRAY_TYPE:
            *params = CastFromGLintStateValue<ParamType>(pname, attrib.type);
            break;
        case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
            *params = CastFromStateValue<ParamType>(pname, static_cast<GLint>(attrib.normalized));
            break;
        case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
            *params = CastFromGLintStateValue<ParamType>(pname, binding.getBuffer().id());
            break;
        case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
            *params = CastFromGLintStateValue<ParamType>(pname, binding.getDivisor());
            break;
        case GL_VERTEX_ATTRIB_ARRAY_INTEGER:
            *params = CastFromGLintStateValue<ParamType>(pname, attrib.pureInteger);
            break;
        case GL_VERTEX_ATTRIB_BINDING:
            *params = CastFromGLintStateValue<ParamType>(pname, attrib.bindingIndex);
            break;
        case GL_VERTEX_ATTRIB_RELATIVE_OFFSET:
            *params = CastFromGLintStateValue<ParamType>(pname, attrib.relativeOffset);
            break;
        default:
            UNREACHABLE();
            break;
    }
}

template <typename ParamType>
void QueryBufferParameterBase(const Buffer *buffer, GLenum pname, ParamType *params)
{
    ASSERT(buffer != nullptr);

    switch (pname)
    {
        case GL_BUFFER_USAGE:
            *params = CastFromGLintStateValue<ParamType>(pname, ToGLenum(buffer->getUsage()));
            break;
        case GL_BUFFER_SIZE:
            *params = CastFromStateValue<ParamType>(pname, buffer->getSize());
            break;
        case GL_BUFFER_ACCESS_FLAGS:
            *params = CastFromGLintStateValue<ParamType>(pname, buffer->getAccessFlags());
            break;
        case GL_BUFFER_ACCESS_OES:
            *params = CastFromGLintStateValue<ParamType>(pname, buffer->getAccess());
            break;
        case GL_BUFFER_MAPPED:
            *params = CastFromStateValue<ParamType>(pname, buffer->isMapped());
            break;
        case GL_BUFFER_MAP_OFFSET:
            *params = CastFromStateValue<ParamType>(pname, buffer->getMapOffset());
            break;
        case GL_BUFFER_MAP_LENGTH:
            *params = CastFromStateValue<ParamType>(pname, buffer->getMapLength());
            break;
        default:
            UNREACHABLE();
            break;
    }
}

GLint GetCommonVariableProperty(const sh::ShaderVariable &var, GLenum prop)
{
    switch (prop)
    {
        case GL_TYPE:
            return clampCast<GLint>(var.type);

        case GL_ARRAY_SIZE:
            // Queryable variables are guaranteed not to be arrays of arrays or arrays of structs,
            // see GLES 3.1 spec section 7.3.1.1 page 77.
            return clampCast<GLint>(var.getBasicTypeElementCount());

        case GL_NAME_LENGTH:
            // ES31 spec p84: This counts the terminating null char.
            return clampCast<GLint>(var.name.size() + 1u);

        default:
            UNREACHABLE();
            return GL_INVALID_VALUE;
    }
}

GLint GetInputResourceProperty(const Program *program, GLuint index, GLenum prop)
{
    const auto &attribute = program->getInputResource(index);
    switch (prop)
    {
        case GL_TYPE:
        case GL_ARRAY_SIZE:
        case GL_NAME_LENGTH:
            return GetCommonVariableProperty(attribute, prop);

        case GL_LOCATION:
            return program->getAttributeLocation(attribute.name);

        case GL_REFERENCED_BY_VERTEX_SHADER:
            return 1;

        case GL_REFERENCED_BY_FRAGMENT_SHADER:
        case GL_REFERENCED_BY_COMPUTE_SHADER:
            return 0;

        default:
            UNREACHABLE();
            return GL_INVALID_VALUE;
    }
}

GLint GetOutputResourceProperty(const Program *program, GLuint index, const GLenum prop)
{
    const auto &outputVariable = program->getOutputResource(index);
    switch (prop)
    {
        case GL_TYPE:
        case GL_ARRAY_SIZE:
        case GL_NAME_LENGTH:
            return GetCommonVariableProperty(outputVariable, prop);

        case GL_LOCATION:
            return program->getFragDataLocation(outputVariable.name);

        case GL_REFERENCED_BY_VERTEX_SHADER:
            return 0;

        case GL_REFERENCED_BY_FRAGMENT_SHADER:
            return 1;

        case GL_REFERENCED_BY_COMPUTE_SHADER:
            return 0;

        default:
            UNREACHABLE();
            return GL_INVALID_VALUE;
    }
}

GLint QueryProgramInterfaceActiveResources(const Program *program, GLenum programInterface)
{
    switch (programInterface)
    {
        case GL_PROGRAM_INPUT:
            return clampCast<GLint>(program->getAttributes().size());

        case GL_PROGRAM_OUTPUT:
            return clampCast<GLint>(program->getState().getOutputVariables().size());

        case GL_UNIFORM:
            return clampCast<GLint>(program->getState().getUniforms().size());

        case GL_UNIFORM_BLOCK:
            return clampCast<GLint>(program->getState().getUniformBlocks().size());

        case GL_ATOMIC_COUNTER_BUFFER:
            return clampCast<GLint>(program->getState().getAtomicCounterBuffers().size());

        case GL_BUFFER_VARIABLE:
            return clampCast<GLint>(program->getState().getBufferVariables().size());

        case GL_SHADER_STORAGE_BLOCK:
            return clampCast<GLint>(program->getState().getShaderStorageBlocks().size());

        // TODO(jie.a.chen@intel.com): more interfaces.
        case GL_TRANSFORM_FEEDBACK_VARYING:
            UNIMPLEMENTED();
            return 0;

        default:
            UNREACHABLE();
            return 0;
    }
}

template <typename T, typename M>
GLint FindMaxSize(const std::vector<T> &resources, M member)
{
    GLint max = 0;
    for (const T &resource : resources)
    {
        max = std::max(max, clampCast<GLint>((resource.*member).size()));
    }
    return max;
}

GLint QueryProgramInterfaceMaxNameLength(const Program *program, GLenum programInterface)
{
    GLint maxNameLength = 0;
    switch (programInterface)
    {
        case GL_PROGRAM_INPUT:
            maxNameLength = FindMaxSize(program->getAttributes(), &sh::Attribute::name);
            break;

        case GL_PROGRAM_OUTPUT:
            maxNameLength =
                FindMaxSize(program->getState().getOutputVariables(), &sh::OutputVariable::name);
            break;

        case GL_UNIFORM:
            maxNameLength = FindMaxSize(program->getState().getUniforms(), &LinkedUniform::name);
            break;

        case GL_UNIFORM_BLOCK:
            maxNameLength =
                FindMaxSize(program->getState().getUniformBlocks(), &InterfaceBlock::name);
            break;

        case GL_BUFFER_VARIABLE:
            maxNameLength =
                FindMaxSize(program->getState().getBufferVariables(), &BufferVariable::name);
            break;

        case GL_SHADER_STORAGE_BLOCK:
            maxNameLength =
                FindMaxSize(program->getState().getShaderStorageBlocks(), &InterfaceBlock::name);
            break;

        // TODO(jie.a.chen@intel.com): more interfaces.
        case GL_TRANSFORM_FEEDBACK_VARYING:
            UNIMPLEMENTED();
            return 0;

        default:
            UNREACHABLE();
            return 0;
    }
    // This length includes an extra character for the null terminator.
    return (maxNameLength == 0 ? 0 : maxNameLength + 1);
}

GLint QueryProgramInterfaceMaxNumActiveVariables(const Program *program, GLenum programInterface)
{
    switch (programInterface)
    {
        case GL_UNIFORM_BLOCK:
            return FindMaxSize(program->getState().getUniformBlocks(),
                               &InterfaceBlock::memberIndexes);
        case GL_ATOMIC_COUNTER_BUFFER:
            return FindMaxSize(program->getState().getAtomicCounterBuffers(),
                               &AtomicCounterBuffer::memberIndexes);

        case GL_SHADER_STORAGE_BLOCK:
            return FindMaxSize(program->getState().getShaderStorageBlocks(),
                               &InterfaceBlock::memberIndexes);

        default:
            UNREACHABLE();
            return 0;
    }
}

GLenum GetUniformPropertyEnum(GLenum prop)
{
    switch (prop)
    {
        case GL_UNIFORM_TYPE:
            return GL_TYPE;
        case GL_UNIFORM_SIZE:
            return GL_ARRAY_SIZE;
        case GL_UNIFORM_NAME_LENGTH:
            return GL_NAME_LENGTH;
        case GL_UNIFORM_BLOCK_INDEX:
            return GL_BLOCK_INDEX;
        case GL_UNIFORM_OFFSET:
            return GL_OFFSET;
        case GL_UNIFORM_ARRAY_STRIDE:
            return GL_ARRAY_STRIDE;
        case GL_UNIFORM_MATRIX_STRIDE:
            return GL_MATRIX_STRIDE;
        case GL_UNIFORM_IS_ROW_MAJOR:
            return GL_IS_ROW_MAJOR;

        default:
            return prop;
    }
}

GLenum GetUniformBlockPropertyEnum(GLenum prop)
{
    switch (prop)
    {
        case GL_UNIFORM_BLOCK_BINDING:
            return GL_BUFFER_BINDING;

        case GL_UNIFORM_BLOCK_DATA_SIZE:
            return GL_BUFFER_DATA_SIZE;

        case GL_UNIFORM_BLOCK_NAME_LENGTH:
            return GL_NAME_LENGTH;

        case GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS:
            return GL_NUM_ACTIVE_VARIABLES;

        case GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES:
            return GL_ACTIVE_VARIABLES;

        case GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
            return GL_REFERENCED_BY_VERTEX_SHADER;

        case GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER:
            return GL_REFERENCED_BY_FRAGMENT_SHADER;

        default:
            return prop;
    }
}

void GetShaderVariableBufferResourceProperty(const ShaderVariableBuffer &buffer,
                                             GLenum pname,
                                             GLint *params,
                                             GLsizei bufSize,
                                             GLsizei *outputPosition)

{
    switch (pname)
    {
        case GL_BUFFER_BINDING:
            params[(*outputPosition)++] = buffer.binding;
            break;
        case GL_BUFFER_DATA_SIZE:
            params[(*outputPosition)++] = clampCast<GLint>(buffer.dataSize);
            break;
        case GL_NUM_ACTIVE_VARIABLES:
            params[(*outputPosition)++] = buffer.numActiveVariables();
            break;
        case GL_ACTIVE_VARIABLES:
            for (size_t memberIndex = 0;
                 memberIndex < buffer.memberIndexes.size() && *outputPosition < bufSize;
                 ++memberIndex)
            {
                params[(*outputPosition)++] = clampCast<GLint>(buffer.memberIndexes[memberIndex]);
            }
            break;
        case GL_REFERENCED_BY_VERTEX_SHADER:
            params[(*outputPosition)++] = static_cast<GLint>(buffer.vertexStaticUse);
            break;
        case GL_REFERENCED_BY_FRAGMENT_SHADER:
            params[(*outputPosition)++] = static_cast<GLint>(buffer.fragmentStaticUse);
            break;
        case GL_REFERENCED_BY_COMPUTE_SHADER:
            params[(*outputPosition)++] = static_cast<GLint>(buffer.computeStaticUse);
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void GetInterfaceBlockResourceProperty(const InterfaceBlock &block,
                                       GLenum pname,
                                       GLint *params,
                                       GLsizei bufSize,
                                       GLsizei *outputPosition)
{
    if (pname == GL_NAME_LENGTH)
    {
        params[(*outputPosition)++] = clampCast<GLint>(block.nameWithArrayIndex().size() + 1);
        return;
    }
    GetShaderVariableBufferResourceProperty(block, pname, params, bufSize, outputPosition);
}

void GetUniformBlockResourceProperty(const Program *program,
                                     GLuint blockIndex,
                                     GLenum pname,
                                     GLint *params,
                                     GLsizei bufSize,
                                     GLsizei *outputPosition)

{
    ASSERT(*outputPosition < bufSize);
    const auto &block = program->getUniformBlockByIndex(blockIndex);
    GetInterfaceBlockResourceProperty(block, pname, params, bufSize, outputPosition);
}

void GetShaderStorageBlockResourceProperty(const Program *program,
                                           GLuint blockIndex,
                                           GLenum pname,
                                           GLint *params,
                                           GLsizei bufSize,
                                           GLsizei *outputPosition)

{
    ASSERT(*outputPosition < bufSize);
    const auto &block = program->getShaderStorageBlockByIndex(blockIndex);
    GetInterfaceBlockResourceProperty(block, pname, params, bufSize, outputPosition);
}

void GetAtomicCounterBufferResourceProperty(const Program *program,
                                            GLuint index,
                                            GLenum pname,
                                            GLint *params,
                                            GLsizei bufSize,
                                            GLsizei *outputPosition)

{
    ASSERT(*outputPosition < bufSize);
    const auto &buffer = program->getState().getAtomicCounterBuffers()[index];
    GetShaderVariableBufferResourceProperty(buffer, pname, params, bufSize, outputPosition);
}

}  // anonymous namespace

void QueryFramebufferAttachmentParameteriv(const Framebuffer *framebuffer,
                                           GLenum attachment,
                                           GLenum pname,
                                           GLint *params)
{
    ASSERT(framebuffer);

    const FramebufferAttachment *attachmentObject = framebuffer->getAttachment(attachment);
    if (attachmentObject == nullptr)
    {
        // ES 2.0.25 spec pg 127 states that if the value of FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE
        // is NONE, then querying any other pname will generate INVALID_ENUM.

        // ES 3.0.2 spec pg 235 states that if the attachment type is none,
        // GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME will return zero and be an
        // INVALID_OPERATION for all other pnames

        switch (pname)
        {
            case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
                *params = GL_NONE;
                break;

            case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
                *params = 0;
                break;

            default:
                UNREACHABLE();
                break;
        }

        return;
    }

    switch (pname)
    {
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            *params = attachmentObject->type();
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
            *params = attachmentObject->id();
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
            *params = attachmentObject->mipLevel();
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
            *params = attachmentObject->cubeMapFace();
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
            *params = attachmentObject->getRedSize();
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
            *params = attachmentObject->getGreenSize();
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
            *params = attachmentObject->getBlueSize();
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
            *params = attachmentObject->getAlphaSize();
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
            *params = attachmentObject->getDepthSize();
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
            *params = attachmentObject->getStencilSize();
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
            *params = attachmentObject->getComponentType();
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
            *params = attachmentObject->getColorEncoding();
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
            *params = attachmentObject->layer();
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_ANGLE:
            *params = attachmentObject->getNumViews();
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_MULTIVIEW_LAYOUT_ANGLE:
            *params = static_cast<GLint>(attachmentObject->getMultiviewLayout());
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_ANGLE:
            *params = attachmentObject->getBaseViewIndex();
            break;

        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_VIEWPORT_OFFSETS_ANGLE:
        {
            const std::vector<Offset> &offsets = attachmentObject->getMultiviewViewportOffsets();
            for (size_t i = 0u; i < offsets.size(); ++i)
            {
                params[i * 2u]      = offsets[i].x;
                params[i * 2u + 1u] = offsets[i].y;
            }
        }
        break;

        default:
            UNREACHABLE();
            break;
    }
}

void QueryBufferParameteriv(const Buffer *buffer, GLenum pname, GLint *params)
{
    QueryBufferParameterBase(buffer, pname, params);
}

void QueryBufferParameteri64v(const Buffer *buffer, GLenum pname, GLint64 *params)
{
    QueryBufferParameterBase(buffer, pname, params);
}

void QueryBufferPointerv(const Buffer *buffer, GLenum pname, void **params)
{
    switch (pname)
    {
        case GL_BUFFER_MAP_POINTER:
            *params = buffer->getMapPointer();
            break;

        default:
            UNREACHABLE();
            break;
    }
}

void QueryProgramiv(const Context *context, const Program *program, GLenum pname, GLint *params)
{
    ASSERT(program != nullptr);

    switch (pname)
    {
        case GL_DELETE_STATUS:
            *params = program->isFlaggedForDeletion();
            return;
        case GL_LINK_STATUS:
            *params = program->isLinked();
            return;
        case GL_VALIDATE_STATUS:
            *params = program->isValidated();
            return;
        case GL_INFO_LOG_LENGTH:
            *params = program->getInfoLogLength();
            return;
        case GL_ATTACHED_SHADERS:
            *params = program->getAttachedShadersCount();
            return;
        case GL_ACTIVE_ATTRIBUTES:
            *params = program->getActiveAttributeCount();
            return;
        case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
            *params = program->getActiveAttributeMaxLength();
            return;
        case GL_ACTIVE_UNIFORMS:
            *params = program->getActiveUniformCount();
            return;
        case GL_ACTIVE_UNIFORM_MAX_LENGTH:
            *params = program->getActiveUniformMaxLength();
            return;
        case GL_PROGRAM_BINARY_LENGTH_OES:
            *params = program->getBinaryLength(context);
            return;
        case GL_ACTIVE_UNIFORM_BLOCKS:
            *params = program->getActiveUniformBlockCount();
            return;
        case GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH:
            *params = program->getActiveUniformBlockMaxLength();
            break;
        case GL_TRANSFORM_FEEDBACK_BUFFER_MODE:
            *params = program->getTransformFeedbackBufferMode();
            break;
        case GL_TRANSFORM_FEEDBACK_VARYINGS:
            *params = program->getTransformFeedbackVaryingCount();
            break;
        case GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH:
            *params = program->getTransformFeedbackVaryingMaxLength();
            break;
        case GL_PROGRAM_BINARY_RETRIEVABLE_HINT:
            *params = program->getBinaryRetrievableHint();
            break;
        case GL_PROGRAM_SEPARABLE:
            *params = program->isSeparable();
            break;
        case GL_COMPUTE_WORK_GROUP_SIZE:
        {
            const sh::WorkGroupSize &localSize = program->getComputeShaderLocalSize();
            params[0]                          = localSize[0];
            params[1]                          = localSize[1];
            params[2]                          = localSize[2];
        }
        break;
        case GL_ACTIVE_ATOMIC_COUNTER_BUFFERS:
            *params = program->getActiveAtomicCounterBufferCount();
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void QueryRenderbufferiv(const Context *context,
                         const Renderbuffer *renderbuffer,
                         GLenum pname,
                         GLint *params)
{
    ASSERT(renderbuffer != nullptr);

    switch (pname)
    {
        case GL_RENDERBUFFER_WIDTH:
            *params = renderbuffer->getWidth();
            break;
        case GL_RENDERBUFFER_HEIGHT:
            *params = renderbuffer->getHeight();
            break;
        case GL_RENDERBUFFER_INTERNAL_FORMAT:
            // Special case the WebGL 1 DEPTH_STENCIL format.
            if (context->isWebGL1() &&
                renderbuffer->getFormat().info->internalFormat == GL_DEPTH24_STENCIL8)
            {
                *params = GL_DEPTH_STENCIL;
            }
            else
            {
                *params = renderbuffer->getFormat().info->internalFormat;
            }
            break;
        case GL_RENDERBUFFER_RED_SIZE:
            *params = renderbuffer->getRedSize();
            break;
        case GL_RENDERBUFFER_GREEN_SIZE:
            *params = renderbuffer->getGreenSize();
            break;
        case GL_RENDERBUFFER_BLUE_SIZE:
            *params = renderbuffer->getBlueSize();
            break;
        case GL_RENDERBUFFER_ALPHA_SIZE:
            *params = renderbuffer->getAlphaSize();
            break;
        case GL_RENDERBUFFER_DEPTH_SIZE:
            *params = renderbuffer->getDepthSize();
            break;
        case GL_RENDERBUFFER_STENCIL_SIZE:
            *params = renderbuffer->getStencilSize();
            break;
        case GL_RENDERBUFFER_SAMPLES_ANGLE:
            *params = renderbuffer->getSamples();
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void QueryShaderiv(const Context *context, Shader *shader, GLenum pname, GLint *params)
{
    ASSERT(shader != nullptr);

    switch (pname)
    {
        case GL_SHADER_TYPE:
            *params = shader->getType();
            return;
        case GL_DELETE_STATUS:
            *params = shader->isFlaggedForDeletion();
            return;
        case GL_COMPILE_STATUS:
            *params = shader->isCompiled(context) ? GL_TRUE : GL_FALSE;
            return;
        case GL_INFO_LOG_LENGTH:
            *params = shader->getInfoLogLength(context);
            return;
        case GL_SHADER_SOURCE_LENGTH:
            *params = shader->getSourceLength();
            return;
        case GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE:
            *params = shader->getTranslatedSourceWithDebugInfoLength(context);
            return;
        default:
            UNREACHABLE();
            break;
    }
}

void QueryTexLevelParameterfv(const Texture *texture,
                              GLenum target,
                              GLint level,
                              GLenum pname,
                              GLfloat *params)
{
    QueryTexLevelParameterBase(texture, target, level, pname, params);
}

void QueryTexLevelParameteriv(const Texture *texture,
                              GLenum target,
                              GLint level,
                              GLenum pname,
                              GLint *params)
{
    QueryTexLevelParameterBase(texture, target, level, pname, params);
}

void QueryTexParameterfv(const Texture *texture, GLenum pname, GLfloat *params)
{
    QueryTexParameterBase(texture, pname, params);
}

void QueryTexParameteriv(const Texture *texture, GLenum pname, GLint *params)
{
    QueryTexParameterBase(texture, pname, params);
}

void QuerySamplerParameterfv(const Sampler *sampler, GLenum pname, GLfloat *params)
{
    QuerySamplerParameterBase(sampler, pname, params);
}

void QuerySamplerParameteriv(const Sampler *sampler, GLenum pname, GLint *params)
{
    QuerySamplerParameterBase(sampler, pname, params);
}

void QueryVertexAttribfv(const VertexAttribute &attrib,
                         const VertexBinding &binding,
                         const VertexAttribCurrentValueData &currentValueData,
                         GLenum pname,
                         GLfloat *params)
{
    QueryVertexAttribBase(attrib, binding, currentValueData.FloatValues, pname, params);
}

void QueryVertexAttribiv(const VertexAttribute &attrib,
                         const VertexBinding &binding,
                         const VertexAttribCurrentValueData &currentValueData,
                         GLenum pname,
                         GLint *params)
{
    QueryVertexAttribBase(attrib, binding, currentValueData.FloatValues, pname, params);
}

void QueryVertexAttribPointerv(const VertexAttribute &attrib, GLenum pname, void **pointer)
{
    switch (pname)
    {
        case GL_VERTEX_ATTRIB_ARRAY_POINTER:
            *pointer = const_cast<void *>(attrib.pointer);
            break;

        default:
            UNREACHABLE();
            break;
    }
}

void QueryVertexAttribIiv(const VertexAttribute &attrib,
                          const VertexBinding &binding,
                          const VertexAttribCurrentValueData &currentValueData,
                          GLenum pname,
                          GLint *params)
{
    QueryVertexAttribBase(attrib, binding, currentValueData.IntValues, pname, params);
}

void QueryVertexAttribIuiv(const VertexAttribute &attrib,
                           const VertexBinding &binding,
                           const VertexAttribCurrentValueData &currentValueData,
                           GLenum pname,
                           GLuint *params)
{
    QueryVertexAttribBase(attrib, binding, currentValueData.UnsignedIntValues, pname, params);
}

void QueryActiveUniformBlockiv(const Program *program,
                               GLuint uniformBlockIndex,
                               GLenum pname,
                               GLint *params)
{
    GLenum prop = GetUniformBlockPropertyEnum(pname);
    QueryProgramResourceiv(program, GL_UNIFORM_BLOCK, uniformBlockIndex, 1, &prop,
                           std::numeric_limits<GLsizei>::max(), nullptr, params);
}

void QueryInternalFormativ(const TextureCaps &format, GLenum pname, GLsizei bufSize, GLint *params)
{
    switch (pname)
    {
        case GL_NUM_SAMPLE_COUNTS:
            if (bufSize != 0)
            {
                *params = clampCast<GLint>(format.sampleCounts.size());
            }
            break;

        case GL_SAMPLES:
        {
            size_t returnCount   = std::min<size_t>(bufSize, format.sampleCounts.size());
            auto sampleReverseIt = format.sampleCounts.rbegin();
            for (size_t sampleIndex = 0; sampleIndex < returnCount; ++sampleIndex)
            {
                params[sampleIndex] = *sampleReverseIt++;
            }
        }
        break;

        default:
            UNREACHABLE();
            break;
    }
}

void QueryFramebufferParameteriv(const Framebuffer *framebuffer, GLenum pname, GLint *params)
{
    ASSERT(framebuffer);

    switch (pname)
    {
        case GL_FRAMEBUFFER_DEFAULT_WIDTH:
            *params = framebuffer->getDefaultWidth();
            break;
        case GL_FRAMEBUFFER_DEFAULT_HEIGHT:
            *params = framebuffer->getDefaultHeight();
            break;
        case GL_FRAMEBUFFER_DEFAULT_SAMPLES:
            *params = framebuffer->getDefaultSamples();
            break;
        case GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS:
            *params = ConvertToGLBoolean(framebuffer->getDefaultFixedSampleLocations());
            break;
        default:
            UNREACHABLE();
            break;
    }
}

Error QuerySynciv(const Sync *sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values)
{
    ASSERT(sync);

    // All queries return one value, exit early if the buffer can't fit anything.
    if (bufSize < 1)
    {
        if (length != nullptr)
        {
            *length = 0;
        }
        return NoError();
    }

    switch (pname)
    {
        case GL_OBJECT_TYPE:
            *values = clampCast<GLint>(GL_SYNC_FENCE);
            break;
        case GL_SYNC_CONDITION:
            *values = clampCast<GLint>(sync->getCondition());
            break;
        case GL_SYNC_FLAGS:
            *values = clampCast<GLint>(sync->getFlags());
            break;
        case GL_SYNC_STATUS:
            ANGLE_TRY(sync->getStatus(values));
            break;

        default:
            UNREACHABLE();
            break;
    }

    if (length != nullptr)
    {
        *length = 1;
    }

    return NoError();
}

void SetTexParameterf(Context *context, Texture *texture, GLenum pname, GLfloat param)
{
    SetTexParameterBase(context, texture, pname, &param);
}

void SetTexParameterfv(Context *context, Texture *texture, GLenum pname, const GLfloat *params)
{
    SetTexParameterBase(context, texture, pname, params);
}

void SetTexParameteri(Context *context, Texture *texture, GLenum pname, GLint param)
{
    SetTexParameterBase(context, texture, pname, &param);
}

void SetTexParameteriv(Context *context, Texture *texture, GLenum pname, const GLint *params)
{
    SetTexParameterBase(context, texture, pname, params);
}

void SetSamplerParameterf(Sampler *sampler, GLenum pname, GLfloat param)
{
    SetSamplerParameterBase(sampler, pname, &param);
}

void SetSamplerParameterfv(Sampler *sampler, GLenum pname, const GLfloat *params)
{
    SetSamplerParameterBase(sampler, pname, params);
}

void SetSamplerParameteri(Sampler *sampler, GLenum pname, GLint param)
{
    SetSamplerParameterBase(sampler, pname, &param);
}

void SetSamplerParameteriv(Sampler *sampler, GLenum pname, const GLint *params)
{
    SetSamplerParameterBase(sampler, pname, params);
}

void SetFramebufferParameteri(Framebuffer *framebuffer, GLenum pname, GLint param)
{
    ASSERT(framebuffer);

    switch (pname)
    {
        case GL_FRAMEBUFFER_DEFAULT_WIDTH:
            framebuffer->setDefaultWidth(param);
            break;
        case GL_FRAMEBUFFER_DEFAULT_HEIGHT:
            framebuffer->setDefaultHeight(param);
            break;
        case GL_FRAMEBUFFER_DEFAULT_SAMPLES:
            framebuffer->setDefaultSamples(param);
            break;
        case GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS:
            framebuffer->setDefaultFixedSampleLocations(ConvertToBool(param));
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void SetProgramParameteri(Program *program, GLenum pname, GLint value)
{
    ASSERT(program);

    switch (pname)
    {
        case GL_PROGRAM_BINARY_RETRIEVABLE_HINT:
            program->setBinaryRetrievableHint(ConvertToBool(value));
            break;
        case GL_PROGRAM_SEPARABLE:
            program->setSeparable(ConvertToBool(value));
            break;
        default:
            UNREACHABLE();
            break;
    }
}

GLint GetUniformResourceProperty(const Program *program, GLuint index, const GLenum prop)
{
    const auto &uniform = program->getUniformByIndex(index);
    GLenum resourceProp = GetUniformPropertyEnum(prop);
    switch (resourceProp)
    {
        case GL_TYPE:
        case GL_ARRAY_SIZE:
        case GL_NAME_LENGTH:
            return GetCommonVariableProperty(uniform, resourceProp);

        case GL_LOCATION:
            return program->getUniformLocation(uniform.name);

        case GL_BLOCK_INDEX:
            return (uniform.isAtomicCounter() ? -1 : uniform.bufferIndex);

        case GL_OFFSET:
            return uniform.blockInfo.offset;

        case GL_ARRAY_STRIDE:
            return uniform.blockInfo.arrayStride;

        case GL_MATRIX_STRIDE:
            return uniform.blockInfo.matrixStride;

        case GL_IS_ROW_MAJOR:
            return static_cast<GLint>(uniform.blockInfo.isRowMajorMatrix);

        case GL_REFERENCED_BY_VERTEX_SHADER:
            return uniform.vertexStaticUse;

        case GL_REFERENCED_BY_FRAGMENT_SHADER:
            return uniform.fragmentStaticUse;

        case GL_REFERENCED_BY_COMPUTE_SHADER:
            return uniform.computeStaticUse;

        case GL_ATOMIC_COUNTER_BUFFER_INDEX:
            return (uniform.isAtomicCounter() ? uniform.bufferIndex : -1);

        default:
            UNREACHABLE();
            return 0;
    }
}

GLint GetBufferVariableResourceProperty(const Program *program, GLuint index, const GLenum prop)
{
    const auto &bufferVariable = program->getBufferVariableByIndex(index);
    switch (prop)
    {
        case GL_TYPE:
        case GL_ARRAY_SIZE:
        case GL_NAME_LENGTH:
            return GetCommonVariableProperty(bufferVariable, prop);

        case GL_BLOCK_INDEX:
            return bufferVariable.bufferIndex;

        case GL_OFFSET:
            return bufferVariable.blockInfo.offset;

        case GL_ARRAY_STRIDE:
            return bufferVariable.blockInfo.arrayStride;

        case GL_MATRIX_STRIDE:
            return bufferVariable.blockInfo.matrixStride;

        case GL_IS_ROW_MAJOR:
            return static_cast<GLint>(bufferVariable.blockInfo.isRowMajorMatrix);

        case GL_REFERENCED_BY_VERTEX_SHADER:
            return bufferVariable.vertexStaticUse;

        case GL_REFERENCED_BY_FRAGMENT_SHADER:
            return bufferVariable.fragmentStaticUse;

        case GL_REFERENCED_BY_COMPUTE_SHADER:
            return bufferVariable.computeStaticUse;

        case GL_TOP_LEVEL_ARRAY_SIZE:
            return bufferVariable.topLevelArraySize;

        case GL_TOP_LEVEL_ARRAY_STRIDE:
            return bufferVariable.blockInfo.topLevelArrayStride;

        default:
            UNREACHABLE();
            return 0;
    }
}

GLuint QueryProgramResourceIndex(const Program *program,
                                 GLenum programInterface,
                                 const GLchar *name)
{
    switch (programInterface)
    {
        case GL_PROGRAM_INPUT:
            return program->getInputResourceIndex(name);

        case GL_PROGRAM_OUTPUT:
            return program->getOutputResourceIndex(name);

        case GL_UNIFORM:
            return program->getState().getUniformIndexFromName(name);

        case GL_BUFFER_VARIABLE:
            return program->getState().getBufferVariableIndexFromName(name);

        case GL_SHADER_STORAGE_BLOCK:
            return program->getShaderStorageBlockIndex(name);

        case GL_UNIFORM_BLOCK:
            return program->getUniformBlockIndex(name);

        // TODO(jie.a.chen@intel.com): more interfaces.
        case GL_TRANSFORM_FEEDBACK_VARYING:
            UNIMPLEMENTED();
            return GL_INVALID_INDEX;

        default:
            UNREACHABLE();
            return GL_INVALID_INDEX;
    }
}

void QueryProgramResourceName(const Program *program,
                              GLenum programInterface,
                              GLuint index,
                              GLsizei bufSize,
                              GLsizei *length,
                              GLchar *name)
{
    switch (programInterface)
    {
        case GL_PROGRAM_INPUT:
            program->getInputResourceName(index, bufSize, length, name);
            break;

        case GL_PROGRAM_OUTPUT:
            program->getOutputResourceName(index, bufSize, length, name);
            break;

        case GL_UNIFORM:
            program->getUniformResourceName(index, bufSize, length, name);
            break;

        case GL_BUFFER_VARIABLE:
            program->getBufferVariableResourceName(index, bufSize, length, name);
            break;

        case GL_SHADER_STORAGE_BLOCK:
            program->getActiveShaderStorageBlockName(index, bufSize, length, name);
            break;

        case GL_UNIFORM_BLOCK:
            program->getActiveUniformBlockName(index, bufSize, length, name);
            break;

        // TODO(jie.a.chen@intel.com): more interfaces.
        case GL_TRANSFORM_FEEDBACK_VARYING:
            UNIMPLEMENTED();
            break;

        default:
            UNREACHABLE();
    }
}

GLint QueryProgramResourceLocation(const Program *program,
                                   GLenum programInterface,
                                   const GLchar *name)
{
    switch (programInterface)
    {
        case GL_PROGRAM_INPUT:
            return program->getAttributeLocation(name);

        case GL_PROGRAM_OUTPUT:
            return program->getFragDataLocation(name);

        case GL_UNIFORM:
            return program->getUniformLocation(name);

        default:
            UNREACHABLE();
            return -1;
    }
}

void QueryProgramResourceiv(const Program *program,
                            GLenum programInterface,
                            GLuint index,
                            GLsizei propCount,
                            const GLenum *props,
                            GLsizei bufSize,
                            GLsizei *length,
                            GLint *params)
{
    if (!program->isLinked())
    {
        if (length != nullptr)
        {
            *length = 0;
        }
        return;
    }

    GLsizei pos = 0;
    for (GLsizei i = 0; i < propCount; i++)
    {
        switch (programInterface)
        {
            case GL_PROGRAM_INPUT:
                params[i] = GetInputResourceProperty(program, index, props[i]);
                ++pos;
                break;

            case GL_PROGRAM_OUTPUT:
                params[i] = GetOutputResourceProperty(program, index, props[i]);
                ++pos;
                break;

            case GL_UNIFORM:
                params[i] = GetUniformResourceProperty(program, index, props[i]);
                ++pos;
                break;

            case GL_BUFFER_VARIABLE:
                params[i] = GetBufferVariableResourceProperty(program, index, props[i]);
                ++pos;
                break;

            case GL_UNIFORM_BLOCK:
                GetUniformBlockResourceProperty(program, index, props[i], params, bufSize, &pos);
                break;

            case GL_SHADER_STORAGE_BLOCK:
                GetShaderStorageBlockResourceProperty(program, index, props[i], params, bufSize,
                                                      &pos);
                break;

            case GL_ATOMIC_COUNTER_BUFFER:
                GetAtomicCounterBufferResourceProperty(program, index, props[i], params, bufSize,
                                                       &pos);
                break;
            // TODO(jie.a.chen@intel.com): more interfaces.
            case GL_TRANSFORM_FEEDBACK_VARYING:
                UNIMPLEMENTED();
                params[i] = GL_INVALID_VALUE;
                break;

            default:
                UNREACHABLE();
                params[i] = GL_INVALID_VALUE;
        }
        if (pos == bufSize)
        {
            // Most properties return one value, but GL_ACTIVE_VARIABLES returns an array of values.
            // This checks not to break buffer bounds for such case.
            break;
        }
    }

    if (length != nullptr)
    {
        *length = pos;
    }
}

void QueryProgramInterfaceiv(const Program *program,
                             GLenum programInterface,
                             GLenum pname,
                             GLint *params)
{
    switch (pname)
    {
        case GL_ACTIVE_RESOURCES:
            *params = QueryProgramInterfaceActiveResources(program, programInterface);
            break;

        case GL_MAX_NAME_LENGTH:
            *params = QueryProgramInterfaceMaxNameLength(program, programInterface);
            break;

        case GL_MAX_NUM_ACTIVE_VARIABLES:
            *params = QueryProgramInterfaceMaxNumActiveVariables(program, programInterface);
            break;

        default:
            UNREACHABLE();
    }
}

}  // namespace gl

namespace egl
{

void QueryConfigAttrib(const Config *config, EGLint attribute, EGLint *value)
{
    ASSERT(config != nullptr);
    switch (attribute)
    {
        case EGL_BUFFER_SIZE:
            *value = config->bufferSize;
            break;
        case EGL_ALPHA_SIZE:
            *value = config->alphaSize;
            break;
        case EGL_BLUE_SIZE:
            *value = config->blueSize;
            break;
        case EGL_GREEN_SIZE:
            *value = config->greenSize;
            break;
        case EGL_RED_SIZE:
            *value = config->redSize;
            break;
        case EGL_DEPTH_SIZE:
            *value = config->depthSize;
            break;
        case EGL_STENCIL_SIZE:
            *value = config->stencilSize;
            break;
        case EGL_CONFIG_CAVEAT:
            *value = config->configCaveat;
            break;
        case EGL_CONFIG_ID:
            *value = config->configID;
            break;
        case EGL_LEVEL:
            *value = config->level;
            break;
        case EGL_NATIVE_RENDERABLE:
            *value = config->nativeRenderable;
            break;
        case EGL_NATIVE_VISUAL_ID:
            *value = config->nativeVisualID;
            break;
        case EGL_NATIVE_VISUAL_TYPE:
            *value = config->nativeVisualType;
            break;
        case EGL_SAMPLES:
            *value = config->samples;
            break;
        case EGL_SAMPLE_BUFFERS:
            *value = config->sampleBuffers;
            break;
        case EGL_SURFACE_TYPE:
            *value = config->surfaceType;
            break;
        case EGL_TRANSPARENT_TYPE:
            *value = config->transparentType;
            break;
        case EGL_TRANSPARENT_BLUE_VALUE:
            *value = config->transparentBlueValue;
            break;
        case EGL_TRANSPARENT_GREEN_VALUE:
            *value = config->transparentGreenValue;
            break;
        case EGL_TRANSPARENT_RED_VALUE:
            *value = config->transparentRedValue;
            break;
        case EGL_BIND_TO_TEXTURE_RGB:
            *value = config->bindToTextureRGB;
            break;
        case EGL_BIND_TO_TEXTURE_RGBA:
            *value = config->bindToTextureRGBA;
            break;
        case EGL_MIN_SWAP_INTERVAL:
            *value = config->minSwapInterval;
            break;
        case EGL_MAX_SWAP_INTERVAL:
            *value = config->maxSwapInterval;
            break;
        case EGL_LUMINANCE_SIZE:
            *value = config->luminanceSize;
            break;
        case EGL_ALPHA_MASK_SIZE:
            *value = config->alphaMaskSize;
            break;
        case EGL_COLOR_BUFFER_TYPE:
            *value = config->colorBufferType;
            break;
        case EGL_RENDERABLE_TYPE:
            *value = config->renderableType;
            break;
        case EGL_MATCH_NATIVE_PIXMAP:
            *value = false;
            UNIMPLEMENTED();
            break;
        case EGL_CONFORMANT:
            *value = config->conformant;
            break;
        case EGL_MAX_PBUFFER_WIDTH:
            *value = config->maxPBufferWidth;
            break;
        case EGL_MAX_PBUFFER_HEIGHT:
            *value = config->maxPBufferHeight;
            break;
        case EGL_MAX_PBUFFER_PIXELS:
            *value = config->maxPBufferPixels;
            break;
        case EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE:
            *value = config->optimalOrientation;
            break;
        case EGL_COLOR_COMPONENT_TYPE_EXT:
            *value = config->colorComponentType;
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void QueryContextAttrib(const gl::Context *context, EGLint attribute, EGLint *value)
{
    switch (attribute)
    {
        case EGL_CONFIG_ID:
            *value = context->getConfig()->configID;
            break;
        case EGL_CONTEXT_CLIENT_TYPE:
            *value = context->getClientType();
            break;
        case EGL_CONTEXT_CLIENT_VERSION:
            *value = context->getClientMajorVersion();
            break;
        case EGL_RENDER_BUFFER:
            *value = context->getRenderBuffer();
            break;
        case EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE:
            *value = context->isRobustResourceInitEnabled();
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void QuerySurfaceAttrib(const Surface *surface, EGLint attribute, EGLint *value)
{
    switch (attribute)
    {
        case EGL_GL_COLORSPACE:
            *value = surface->getGLColorspace();
            break;
        case EGL_VG_ALPHA_FORMAT:
            *value = surface->getVGAlphaFormat();
            break;
        case EGL_VG_COLORSPACE:
            *value = surface->getVGColorspace();
            break;
        case EGL_CONFIG_ID:
            *value = surface->getConfig()->configID;
            break;
        case EGL_HEIGHT:
            *value = surface->getHeight();
            break;
        case EGL_HORIZONTAL_RESOLUTION:
            *value = surface->getHorizontalResolution();
            break;
        case EGL_LARGEST_PBUFFER:
            // The EGL spec states that value is not written if the surface is not a pbuffer
            if (surface->getType() == EGL_PBUFFER_BIT)
            {
                *value = surface->getLargestPbuffer();
            }
            break;
        case EGL_MIPMAP_TEXTURE:
            // The EGL spec states that value is not written if the surface is not a pbuffer
            if (surface->getType() == EGL_PBUFFER_BIT)
            {
                *value = surface->getMipmapTexture();
            }
            break;
        case EGL_MIPMAP_LEVEL:
            // The EGL spec states that value is not written if the surface is not a pbuffer
            if (surface->getType() == EGL_PBUFFER_BIT)
            {
                *value = surface->getMipmapLevel();
            }
            break;
        case EGL_MULTISAMPLE_RESOLVE:
            *value = surface->getMultisampleResolve();
            break;
        case EGL_PIXEL_ASPECT_RATIO:
            *value = surface->getPixelAspectRatio();
            break;
        case EGL_RENDER_BUFFER:
            *value = surface->getRenderBuffer();
            break;
        case EGL_SWAP_BEHAVIOR:
            *value = surface->getSwapBehavior();
            break;
        case EGL_TEXTURE_FORMAT:
            // The EGL spec states that value is not written if the surface is not a pbuffer
            if (surface->getType() == EGL_PBUFFER_BIT)
            {
                *value = surface->getTextureFormat();
            }
            break;
        case EGL_TEXTURE_TARGET:
            // The EGL spec states that value is not written if the surface is not a pbuffer
            if (surface->getType() == EGL_PBUFFER_BIT)
            {
                *value = surface->getTextureTarget();
            }
            break;
        case EGL_VERTICAL_RESOLUTION:
            *value = surface->getVerticalResolution();
            break;
        case EGL_WIDTH:
            *value = surface->getWidth();
            break;
        case EGL_POST_SUB_BUFFER_SUPPORTED_NV:
            *value = surface->isPostSubBufferSupported();
            break;
        case EGL_FIXED_SIZE_ANGLE:
            *value = surface->isFixedSize();
            break;
        case EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE:
            *value = surface->flexibleSurfaceCompatibilityRequested();
            break;
        case EGL_SURFACE_ORIENTATION_ANGLE:
            *value = surface->getOrientation();
            break;
        case EGL_DIRECT_COMPOSITION_ANGLE:
            *value = surface->directComposition();
            break;
        case EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE:
            *value = surface->isRobustResourceInitEnabled();
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void SetSurfaceAttrib(Surface *surface, EGLint attribute, EGLint value)
{
    switch (attribute)
    {
        case EGL_MIPMAP_LEVEL:
            surface->setMipmapLevel(value);
            break;
        case EGL_MULTISAMPLE_RESOLVE:
            surface->setMultisampleResolve(value);
            break;
        case EGL_SWAP_BEHAVIOR:
            surface->setSwapBehavior(value);
            break;
        default:
            UNREACHABLE();
            break;
    }
}

}  // namespace egl
