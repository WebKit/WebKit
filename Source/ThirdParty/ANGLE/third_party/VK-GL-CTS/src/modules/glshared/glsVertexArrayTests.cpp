/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Vertex array and buffer tests
 *//*--------------------------------------------------------------------*/

#include "glsVertexArrayTests.hpp"

#include "deRandom.h"

#include "tcuTestLog.hpp"
#include "tcuPixelFormat.hpp"
#include "tcuRGBA.hpp"
#include "tcuSurface.hpp"
#include "tcuVector.hpp"
#include "tcuTestLog.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuImageCompare.hpp"

#include "gluPixelTransfer.hpp"
#include "gluCallLogWrapper.hpp"

#include "sglrContext.hpp"
#include "sglrReferenceContext.hpp"
#include "sglrGLContext.hpp"

#include "deMath.h"
#include "deStringUtil.hpp"
#include "deArrayUtil.hpp"

#include <cstring>
#include <cmath>
#include <vector>
#include <sstream>
#include <limits>
#include <algorithm>

#include "glwDefs.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gls
{

using tcu::TestLog;
using namespace glw; // GL types

std::string Array::targetToString(Target target)
{
    static const char *targets[] = {
        "element_array", // TARGET_ELEMENT_ARRAY = 0,
        "array"          // TARGET_ARRAY,
    };

    return de::getSizedArrayElement<Array::TARGET_LAST>(targets, (int)target);
}

std::string Array::inputTypeToString(InputType type)
{
    static const char *types[] = {
        "float",  // INPUTTYPE_FLOAT = 0,
        "fixed",  // INPUTTYPE_FIXED,
        "double", // INPUTTYPE_DOUBLE

        "byte",  // INPUTTYPE_BYTE,
        "short", // INPUTTYPE_SHORT,

        "unsigned_byte",  // INPUTTYPE_UNSIGNED_BYTE,
        "unsigned_short", // INPUTTYPE_UNSIGNED_SHORT,

        "int",                   // INPUTTYPE_INT,
        "unsigned_int",          // INPUTTYPE_UNSIGNED_INT,
        "half",                  // INPUTTYPE_HALF,
        "usigned_int2_10_10_10", // INPUTTYPE_UNSIGNED_INT_2_10_10_10,
        "int2_10_10_10"          // INPUTTYPE_INT_2_10_10_10,
    };

    return de::getSizedArrayElement<Array::INPUTTYPE_LAST>(types, (int)type);
}

std::string Array::outputTypeToString(OutputType type)
{
    static const char *types[] = {
        "float", // OUTPUTTYPE_FLOAT = 0,
        "vec2",  // OUTPUTTYPE_VEC2,
        "vec3",  // OUTPUTTYPE_VEC3,
        "vec4",  // OUTPUTTYPE_VEC4,

        "int",  // OUTPUTTYPE_INT,
        "uint", // OUTPUTTYPE_UINT,

        "ivec2", // OUTPUTTYPE_IVEC2,
        "ivec3", // OUTPUTTYPE_IVEC3,
        "ivec4", // OUTPUTTYPE_IVEC4,

        "uvec2", // OUTPUTTYPE_UVEC2,
        "uvec3", // OUTPUTTYPE_UVEC3,
        "uvec4", // OUTPUTTYPE_UVEC4,
    };

    return de::getSizedArrayElement<Array::OUTPUTTYPE_LAST>(types, (int)type);
}

std::string Array::usageTypeToString(Usage usage)
{
    static const char *usages[] = {
        "dynamic_draw", // USAGE_DYNAMIC_DRAW = 0,
        "static_draw",  // USAGE_STATIC_DRAW,
        "stream_draw",  // USAGE_STREAM_DRAW,

        "stream_read", // USAGE_STREAM_READ,
        "stream_copy", // USAGE_STREAM_COPY,

        "static_read", // USAGE_STATIC_READ,
        "static_copy", // USAGE_STATIC_COPY,

        "dynamic_read", // USAGE_DYNAMIC_READ,
        "dynamic_copy", // USAGE_DYNAMIC_COPY,
    };

    return de::getSizedArrayElement<Array::USAGE_LAST>(usages, (int)usage);
}

std::string Array::storageToString(Storage storage)
{
    static const char *storages[] = {
        "user_ptr", // STORAGE_USER = 0,
        "buffer"    // STORAGE_BUFFER,
    };

    return de::getSizedArrayElement<Array::STORAGE_LAST>(storages, (int)storage);
}

std::string Array::primitiveToString(Primitive primitive)
{
    static const char *primitives[] = {
        "points",        // PRIMITIVE_POINTS ,
        "triangles",     // PRIMITIVE_TRIANGLES,
        "triangle_fan",  // PRIMITIVE_TRIANGLE_FAN,
        "triangle_strip" // PRIMITIVE_TRIANGLE_STRIP,
    };

    return de::getSizedArrayElement<Array::PRIMITIVE_LAST>(primitives, (int)primitive);
}

int Array::inputTypeSize(InputType type)
{
    static const int size[] = {
        (int)sizeof(float),   // INPUTTYPE_FLOAT = 0,
        (int)sizeof(int32_t), // INPUTTYPE_FIXED,
        (int)sizeof(double),  // INPUTTYPE_DOUBLE

        (int)sizeof(int8_t),  // INPUTTYPE_BYTE,
        (int)sizeof(int16_t), // INPUTTYPE_SHORT,

        (int)sizeof(uint8_t),  // INPUTTYPE_UNSIGNED_BYTE,
        (int)sizeof(uint16_t), // INPUTTYPE_UNSIGNED_SHORT,

        (int)sizeof(int32_t),      // INPUTTYPE_INT,
        (int)sizeof(uint32_t),     // INPUTTYPE_UNSIGNED_INT,
        (int)sizeof(deFloat16),    // INPUTTYPE_HALF,
        (int)sizeof(uint32_t) / 4, // INPUTTYPE_UNSIGNED_INT_2_10_10_10,
        (int)sizeof(uint32_t) / 4  // INPUTTYPE_INT_2_10_10_10,
    };

    return de::getSizedArrayElement<Array::INPUTTYPE_LAST>(size, (int)type);
}

static bool inputTypeIsFloatType(Array::InputType type)
{
    if (type == Array::INPUTTYPE_FLOAT)
        return true;
    if (type == Array::INPUTTYPE_FIXED)
        return true;
    if (type == Array::INPUTTYPE_DOUBLE)
        return true;
    if (type == Array::INPUTTYPE_HALF)
        return true;
    return false;
}

static bool outputTypeIsFloatType(Array::OutputType type)
{
    if (type == Array::OUTPUTTYPE_FLOAT || type == Array::OUTPUTTYPE_VEC2 || type == Array::OUTPUTTYPE_VEC3 ||
        type == Array::OUTPUTTYPE_VEC4)
        return true;

    return false;
}

template <class T>
inline T getRandom(deRandom &rnd, T min, T max);

template <>
inline GLValue::Float getRandom(deRandom &rnd, GLValue::Float min, GLValue::Float max)
{
    if (max < min)
        return min;

    return GLValue::Float::create(min + deRandom_getFloat(&rnd) * (max.to<float>() - min.to<float>()));
}

template <>
inline GLValue::Short getRandom(deRandom &rnd, GLValue::Short min, GLValue::Short max)
{
    if (max < min)
        return min;

    return GLValue::Short::create(
        (min == max ? min : (int16_t)(min + (deRandom_getUint32(&rnd) % (max.to<int>() - min.to<int>())))));
}

template <>
inline GLValue::Ushort getRandom(deRandom &rnd, GLValue::Ushort min, GLValue::Ushort max)
{
    if (max < min)
        return min;

    return GLValue::Ushort::create(
        (min == max ? min : (uint16_t)(min + (deRandom_getUint32(&rnd) % (max.to<int>() - min.to<int>())))));
}

template <>
inline GLValue::Byte getRandom(deRandom &rnd, GLValue::Byte min, GLValue::Byte max)
{
    if (max < min)
        return min;

    return GLValue::Byte::create(
        (min == max ? min : (int8_t)(min + (deRandom_getUint32(&rnd) % (max.to<int>() - min.to<int>())))));
}

template <>
inline GLValue::Ubyte getRandom(deRandom &rnd, GLValue::Ubyte min, GLValue::Ubyte max)
{
    if (max < min)
        return min;

    return GLValue::Ubyte::create(
        (min == max ? min : (uint8_t)(min + (deRandom_getUint32(&rnd) % (max.to<int>() - min.to<int>())))));
}

template <>
inline GLValue::Fixed getRandom(deRandom &rnd, GLValue::Fixed min, GLValue::Fixed max)
{
    if (max < min)
        return min;

    return GLValue::Fixed::create(
        (min == max ? min : min + (deRandom_getUint32(&rnd) % (max.to<uint32_t>() - min.to<uint32_t>()))));
}

template <>
inline GLValue::Half getRandom(deRandom &rnd, GLValue::Half min, GLValue::Half max)
{
    if (max < min)
        return min;

    float fMax      = max.to<float>();
    float fMin      = min.to<float>();
    GLValue::Half h = GLValue::Half::create(fMin + deRandom_getFloat(&rnd) * (fMax - fMin));
    return h;
}

template <>
inline GLValue::Int getRandom(deRandom &rnd, GLValue::Int min, GLValue::Int max)
{
    if (max < min)
        return min;

    return GLValue::Int::create(
        (min == max ? min : min + (deRandom_getUint32(&rnd) % (max.to<uint32_t>() - min.to<uint32_t>()))));
}

template <>
inline GLValue::Uint getRandom(deRandom &rnd, GLValue::Uint min, GLValue::Uint max)
{
    if (max < min)
        return min;

    return GLValue::Uint::create(
        (min == max ? min : min + (deRandom_getUint32(&rnd) % (max.to<uint32_t>() - min.to<uint32_t>()))));
}

template <>
inline GLValue::Double getRandom(deRandom &rnd, GLValue::Double min, GLValue::Double max)
{
    if (max < min)
        return min;

    return GLValue::Double::create(min + deRandom_getFloat(&rnd) * (max.to<float>() - min.to<float>()));
}

// Minimum difference required between coordinates
template <class T>
inline T minValue(void);

template <>
inline GLValue::Float minValue(void)
{
    return GLValue::Float::create(4 * 1.0f);
}

template <>
inline GLValue::Short minValue(void)
{
    return GLValue::Short::create(4 * 256);
}

template <>
inline GLValue::Ushort minValue(void)
{
    return GLValue::Ushort::create(4 * 256);
}

template <>
inline GLValue::Byte minValue(void)
{
    return GLValue::Byte::create(4 * 1);
}

template <>
inline GLValue::Ubyte minValue(void)
{
    return GLValue::Ubyte::create(4 * 2);
}

template <>
inline GLValue::Fixed minValue(void)
{
    return GLValue::Fixed::create(4 * 512);
}

template <>
inline GLValue::Int minValue(void)
{
    return GLValue::Int::create(4 * 16777216);
}

template <>
inline GLValue::Uint minValue(void)
{
    return GLValue::Uint::create(4 * 16777216);
}

template <>
inline GLValue::Half minValue(void)
{
    return GLValue::Half::create(4 * 1.0f);
}

template <>
inline GLValue::Double minValue(void)
{
    return GLValue::Double::create(4 * 1.0f);
}

template <class T>
static inline void alignmentSafeAssignment(char *dst, T val)
{
    std::memcpy(dst, &val, sizeof(T));
}

ContextArray::ContextArray(Storage storage, sglr::Context &context)
    : m_storage(storage)
    , m_ctx(context)
    , m_glBuffer(0)
    , m_bound(false)
    , m_attribNdx(0)
    , m_size(0)
    , m_data(nullptr)
    , m_componentCount(1)
    , m_target(Array::TARGET_ARRAY)
    , m_inputType(Array::INPUTTYPE_FLOAT)
    , m_outputType(Array::OUTPUTTYPE_VEC4)
    , m_normalize(false)
    , m_stride(0)
    , m_offset(0)
{
    if (m_storage == STORAGE_BUFFER)
    {
        m_ctx.genBuffers(1, &m_glBuffer);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glGenBuffers()");
    }
}

ContextArray::~ContextArray(void)
{
    if (m_storage == STORAGE_BUFFER)
    {
        m_ctx.deleteBuffers(1, &m_glBuffer);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glDeleteBuffers()");
    }
    else if (m_storage == STORAGE_USER)
        delete[] m_data;
    else
        DE_ASSERT(false);
}

Array *ContextArrayPack::getArray(int i)
{
    return m_arrays.at(i);
}

void ContextArray::data(Target target, int size, const char *ptr, Usage usage)
{
    m_size   = size;
    m_target = target;

    if (m_storage == STORAGE_BUFFER)
    {
        m_ctx.bindBuffer(targetToGL(target), m_glBuffer);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glBindBuffer()");

        m_ctx.bufferData(targetToGL(target), size, ptr, usageToGL(usage));
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glBufferData()");
    }
    else if (m_storage == STORAGE_USER)
    {
        if (m_data)
            delete[] m_data;

        m_data = new char[size];
        std::memcpy(m_data, ptr, size);
    }
    else
        DE_ASSERT(false);
}

void ContextArray::subdata(Target target, int offset, int size, const char *ptr)
{
    m_target = target;

    if (m_storage == STORAGE_BUFFER)
    {
        m_ctx.bindBuffer(targetToGL(target), m_glBuffer);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glBindBuffer()");

        m_ctx.bufferSubData(targetToGL(target), offset, size, ptr);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glBufferSubData()");
    }
    else if (m_storage == STORAGE_USER)
        std::memcpy(m_data + offset, ptr, size);
    else
        DE_ASSERT(false);
}

void ContextArray::bind(int attribNdx, int offset, int size, InputType inputType, OutputType outType, bool normalized,
                        int stride)
{
    m_attribNdx      = attribNdx;
    m_bound          = true;
    m_componentCount = size;
    m_inputType      = inputType;
    m_outputType     = outType;
    m_normalize      = normalized;
    m_stride         = stride;
    m_offset         = offset;
}

void ContextArray::bindIndexArray(Array::Target target)
{
    if (m_storage == STORAGE_USER)
    {
    }
    else if (m_storage == STORAGE_BUFFER)
    {
        m_ctx.bindBuffer(targetToGL(target), m_glBuffer);
    }
}

void ContextArray::glBind(uint32_t loc)
{
    if (m_storage == STORAGE_BUFFER)
    {
        m_ctx.bindBuffer(targetToGL(m_target), m_glBuffer);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glBindBuffer()");

        if (!inputTypeIsFloatType(m_inputType))
        {
            // Input is not float type

            if (outputTypeIsFloatType(m_outputType))
            {
                // Output type is float type
                m_ctx.vertexAttribPointer(loc, m_componentCount, inputTypeToGL(m_inputType), m_normalize, m_stride,
                                          (GLvoid *)((GLintptr)m_offset));
                GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glVertexAttribPointer()");
            }
            else
            {
                // Output type is int type
                m_ctx.vertexAttribIPointer(loc, m_componentCount, inputTypeToGL(m_inputType), m_stride,
                                           (GLvoid *)((GLintptr)m_offset));
                GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glVertexAttribIPointer()");
            }
        }
        else
        {
            // Input type is float type

            // Output type must be float type
            DE_ASSERT(m_outputType == OUTPUTTYPE_FLOAT || m_outputType == OUTPUTTYPE_VEC2 ||
                      m_outputType == OUTPUTTYPE_VEC3 || m_outputType == OUTPUTTYPE_VEC4);

            m_ctx.vertexAttribPointer(loc, m_componentCount, inputTypeToGL(m_inputType), m_normalize, m_stride,
                                      (GLvoid *)((GLintptr)m_offset));
            GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glVertexAttribPointer()");
        }

        m_ctx.bindBuffer(targetToGL(m_target), 0);
    }
    else if (m_storage == STORAGE_USER)
    {
        m_ctx.bindBuffer(targetToGL(m_target), 0);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glBindBuffer()");

        if (!inputTypeIsFloatType(m_inputType))
        {
            // Input is not float type

            if (outputTypeIsFloatType(m_outputType))
            {
                // Output type is float type
                m_ctx.vertexAttribPointer(loc, m_componentCount, inputTypeToGL(m_inputType), m_normalize, m_stride,
                                          m_data + m_offset);
                GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glVertexAttribPointer()");
            }
            else
            {
                // Output type is int type
                m_ctx.vertexAttribIPointer(loc, m_componentCount, inputTypeToGL(m_inputType), m_stride,
                                           m_data + m_offset);
                GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glVertexAttribIPointer()");
            }
        }
        else
        {
            // Input type is float type

            // Output type must be float type
            DE_ASSERT(m_outputType == OUTPUTTYPE_FLOAT || m_outputType == OUTPUTTYPE_VEC2 ||
                      m_outputType == OUTPUTTYPE_VEC3 || m_outputType == OUTPUTTYPE_VEC4);

            m_ctx.vertexAttribPointer(loc, m_componentCount, inputTypeToGL(m_inputType), m_normalize, m_stride,
                                      m_data + m_offset);
            GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glVertexAttribPointer()");
        }
    }
    else
        DE_ASSERT(false);
}

GLenum ContextArray::targetToGL(Array::Target target)
{
    static const GLenum targets[] = {
        GL_ELEMENT_ARRAY_BUFFER, // TARGET_ELEMENT_ARRAY = 0,
        GL_ARRAY_BUFFER          // TARGET_ARRAY,
    };

    return de::getSizedArrayElement<Array::TARGET_LAST>(targets, (int)target);
}

GLenum ContextArray::usageToGL(Array::Usage usage)
{
    static const GLenum usages[] = {
        GL_DYNAMIC_DRAW, // USAGE_DYNAMIC_DRAW = 0,
        GL_STATIC_DRAW,  // USAGE_STATIC_DRAW,
        GL_STREAM_DRAW,  // USAGE_STREAM_DRAW,

        GL_STREAM_READ, // USAGE_STREAM_READ,
        GL_STREAM_COPY, // USAGE_STREAM_COPY,

        GL_STATIC_READ, // USAGE_STATIC_READ,
        GL_STATIC_COPY, // USAGE_STATIC_COPY,

        GL_DYNAMIC_READ, // USAGE_DYNAMIC_READ,
        GL_DYNAMIC_COPY  // USAGE_DYNAMIC_COPY,
    };

    return de::getSizedArrayElement<Array::USAGE_LAST>(usages, (int)usage);
}

GLenum ContextArray::inputTypeToGL(Array::InputType type)
{
    static const GLenum types[] = {
        GL_FLOAT,          // INPUTTYPE_FLOAT = 0,
        GL_FIXED,          // INPUTTYPE_FIXED,
        GL_DOUBLE,         // INPUTTYPE_DOUBLE
        GL_BYTE,           // INPUTTYPE_BYTE,
        GL_SHORT,          // INPUTTYPE_SHORT,
        GL_UNSIGNED_BYTE,  // INPUTTYPE_UNSIGNED_BYTE,
        GL_UNSIGNED_SHORT, // INPUTTYPE_UNSIGNED_SHORT,

        GL_INT,                         // INPUTTYPE_INT,
        GL_UNSIGNED_INT,                // INPUTTYPE_UNSIGNED_INT,
        GL_HALF_FLOAT,                  // INPUTTYPE_HALF,
        GL_UNSIGNED_INT_2_10_10_10_REV, // INPUTTYPE_UNSIGNED_INT_2_10_10_10,
        GL_INT_2_10_10_10_REV           // INPUTTYPE_INT_2_10_10_10,
    };

    return de::getSizedArrayElement<Array::INPUTTYPE_LAST>(types, (int)type);
}

std::string ContextArray::outputTypeToGLType(Array::OutputType type)
{
    static const char *types[] = {
        "float", // OUTPUTTYPE_FLOAT = 0,
        "vec2",  // OUTPUTTYPE_VEC2,
        "vec3",  // OUTPUTTYPE_VEC3,
        "vec4",  // OUTPUTTYPE_VEC4,

        "int",  // OUTPUTTYPE_INT,
        "uint", // OUTPUTTYPE_UINT,

        "ivec2", // OUTPUTTYPE_IVEC2,
        "ivec3", // OUTPUTTYPE_IVEC3,
        "ivec4", // OUTPUTTYPE_IVEC4,

        "uvec2", // OUTPUTTYPE_UVEC2,
        "uvec3", // OUTPUTTYPE_UVEC3,
        "uvec4", // OUTPUTTYPE_UVEC4,
    };

    return de::getSizedArrayElement<Array::OUTPUTTYPE_LAST>(types, (int)type);
}

GLenum ContextArray::primitiveToGL(Array::Primitive primitive)
{
    static const GLenum primitives[] = {
        GL_POINTS,        // PRIMITIVE_POINTS = 0,
        GL_TRIANGLES,     // PRIMITIVE_TRIANGLES,
        GL_TRIANGLE_FAN,  // PRIMITIVE_TRIANGLE_FAN,
        GL_TRIANGLE_STRIP // PRIMITIVE_TRIANGLE_STRIP,
    };

    return de::getSizedArrayElement<Array::PRIMITIVE_LAST>(primitives, (int)primitive);
}

ContextArrayPack::ContextArrayPack(glu::RenderContext &renderCtx, sglr::Context &drawContext)
    : m_renderCtx(renderCtx)
    , m_ctx(drawContext)
    , m_program(nullptr)
    , m_screen(std::min(512, renderCtx.getRenderTarget().getWidth()),
               std::min(512, renderCtx.getRenderTarget().getHeight()))
{
}

ContextArrayPack::~ContextArrayPack(void)
{
    for (std::vector<ContextArray *>::iterator itr = m_arrays.begin(); itr != m_arrays.end(); itr++)
        delete *itr;

    delete m_program;
}

int ContextArrayPack::getArrayCount(void)
{
    return (int)m_arrays.size();
}

void ContextArrayPack::newArray(Array::Storage storage)
{
    m_arrays.push_back(new ContextArray(storage, m_ctx));
}

class ContextShaderProgram : public sglr::ShaderProgram
{
public:
    ContextShaderProgram(const glu::RenderContext &ctx, const std::vector<ContextArray *> &arrays);

    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const;
    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const;

private:
    static std::string genVertexSource(const glu::RenderContext &ctx, const std::vector<ContextArray *> &arrays);
    static std::string genFragmentSource(const glu::RenderContext &ctx);
    static rr::GenericVecType mapOutputType(const Array::OutputType &type);
    static int getComponentCount(const Array::OutputType &type);

    static sglr::pdec::ShaderProgramDeclaration createProgramDeclaration(const glu::RenderContext &ctx,
                                                                         const std::vector<ContextArray *> &arrays);

    std::vector<int> m_componentCount;
    std::vector<rr::GenericVecType> m_attrType;
};

ContextShaderProgram::ContextShaderProgram(const glu::RenderContext &ctx, const std::vector<ContextArray *> &arrays)
    : sglr::ShaderProgram(createProgramDeclaration(ctx, arrays))
    , m_componentCount(arrays.size())
    , m_attrType(arrays.size())
{
    for (int arrayNdx = 0; arrayNdx < (int)arrays.size(); arrayNdx++)
    {
        m_componentCount[arrayNdx] = getComponentCount(arrays[arrayNdx]->getOutputType());
        m_attrType[arrayNdx]       = mapOutputType(arrays[arrayNdx]->getOutputType());
    }
}

template <typename T>
void calcShaderColorCoord(tcu::Vec2 &coord, tcu::Vec3 &color, const tcu::Vector<T, 4> &attribValue, bool isCoordinate,
                          int numComponents)
{
    if (isCoordinate)
        switch (numComponents)
        {
        case 1:
            coord = tcu::Vec2((float)attribValue.x(), (float)attribValue.x());
            break;
        case 2:
            coord = tcu::Vec2((float)attribValue.x(), (float)attribValue.y());
            break;
        case 3:
            coord = tcu::Vec2((float)attribValue.x() + (float)attribValue.z(), (float)attribValue.y());
            break;
        case 4:
            coord = tcu::Vec2((float)attribValue.x() + (float)attribValue.z(),
                              (float)attribValue.y() + (float)attribValue.w());
            break;

        default:
            DE_ASSERT(false);
        }
    else
    {
        switch (numComponents)
        {
        case 1:
            color = color * (float)attribValue.x();
            break;

        case 2:
            color.x() = color.x() * (float)attribValue.x();
            color.y() = color.y() * (float)attribValue.y();
            break;

        case 3:
            color.x() = color.x() * (float)attribValue.x();
            color.y() = color.y() * (float)attribValue.y();
            color.z() = color.z() * (float)attribValue.z();
            break;

        case 4:
            color.x() = color.x() * (float)attribValue.x() * (float)attribValue.w();
            color.y() = color.y() * (float)attribValue.y() * (float)attribValue.w();
            color.z() = color.z() * (float)attribValue.z() * (float)attribValue.w();
            break;

        default:
            DE_ASSERT(false);
        }
    }
}

void ContextShaderProgram::shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                                         const int numPackets) const
{
    const float u_coordScale = getUniformByName("u_coordScale").value.f;
    const float u_colorScale = getUniformByName("u_colorScale").value.f;

    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        const size_t varyingLocColor = 0;

        rr::VertexPacket &packet = *packets[packetNdx];

        // Calc output color
        tcu::Vec2 coord = tcu::Vec2(1.0, 1.0);
        tcu::Vec3 color = tcu::Vec3(1.0, 1.0, 1.0);

        for (int attribNdx = 0; attribNdx < (int)m_attrType.size(); attribNdx++)
        {
            const int numComponents = m_componentCount[attribNdx];

            switch (m_attrType[attribNdx])
            {
            case rr::GENERICVECTYPE_FLOAT:
                calcShaderColorCoord(coord, color,
                                     rr::readVertexAttribFloat(inputs[attribNdx], packet.instanceNdx, packet.vertexNdx),
                                     attribNdx == 0, numComponents);
                break;
            case rr::GENERICVECTYPE_INT32:
                calcShaderColorCoord(coord, color,
                                     rr::readVertexAttribInt(inputs[attribNdx], packet.instanceNdx, packet.vertexNdx),
                                     attribNdx == 0, numComponents);
                break;
            case rr::GENERICVECTYPE_UINT32:
                calcShaderColorCoord(coord, color,
                                     rr::readVertexAttribUint(inputs[attribNdx], packet.instanceNdx, packet.vertexNdx),
                                     attribNdx == 0, numComponents);
                break;
            default:
                DE_ASSERT(false);
            }
        }

        // Transform position
        {
            packet.position = tcu::Vec4(u_coordScale * coord.x(), u_coordScale * coord.y(), 1.0f, 1.0f);
        }

        // Pass color to FS
        {
            packet.outputs[varyingLocColor] =
                tcu::Vec4(u_colorScale * color.x(), u_colorScale * color.y(), u_colorScale * color.z(), 1.0f);
        }
    }
}

void ContextShaderProgram::shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                          const rr::FragmentShadingContext &context) const
{
    const size_t varyingLocColor = 0;

    // Triangles are flashaded
    tcu::Vec4 color = rr::readTriangleVarying<float>(packets[0], context, varyingLocColor, 0);

    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
            rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
}

std::string ContextShaderProgram::genVertexSource(const glu::RenderContext &ctx,
                                                  const std::vector<ContextArray *> &arrays)
{
    std::stringstream vertexShaderTmpl;
    std::map<std::string, std::string> params;

    if (glu::isGLSLVersionSupported(ctx.getType(), glu::GLSL_VERSION_300_ES))
    {
        params["VTX_IN"]     = "in";
        params["VTX_OUT"]    = "out";
        params["FRAG_IN"]    = "in";
        params["FRAG_COLOR"] = "dEQP_FragColor";
        params["VTX_HDR"]    = "#version 300 es\n";
        params["FRAG_HDR"]   = "#version 300 es\nlayout(location = 0) out mediump vec4 dEQP_FragColor;\n";
    }
    else if (glu::isGLSLVersionSupported(ctx.getType(), glu::GLSL_VERSION_100_ES))
    {
        params["VTX_IN"]     = "attribute";
        params["VTX_OUT"]    = "varying";
        params["FRAG_IN"]    = "varying";
        params["FRAG_COLOR"] = "gl_FragColor";
        params["VTX_HDR"]    = "";
        params["FRAG_HDR"]   = "";
    }
    else if (glu::isGLSLVersionSupported(ctx.getType(), glu::GLSL_VERSION_330))
    {
        params["VTX_IN"]     = "in";
        params["VTX_OUT"]    = "out";
        params["FRAG_IN"]    = "in";
        params["FRAG_COLOR"] = "dEQP_FragColor";
        params["VTX_HDR"]    = "#version 330\n";
        params["FRAG_HDR"]   = "#version 330\nlayout(location = 0) out mediump vec4 dEQP_FragColor;\n";
    }
    else
        DE_ASSERT(false);

    vertexShaderTmpl << "${VTX_HDR}";

    for (int arrayNdx = 0; arrayNdx < (int)arrays.size(); arrayNdx++)
    {
        vertexShaderTmpl << "${VTX_IN} highp " << ContextArray::outputTypeToGLType(arrays[arrayNdx]->getOutputType())
                         << " a_" << arrays[arrayNdx]->getAttribNdx() << ";\n";
    }

    vertexShaderTmpl << "uniform highp float u_coordScale;\n"
                        "uniform highp float u_colorScale;\n"
                        "${VTX_OUT} mediump vec4 v_color;\n"
                        "void main(void)\n"
                        "{\n"
                        "\tgl_PointSize = 1.0;\n"
                        "\thighp vec2 coord = vec2(1.0, 1.0);\n"
                        "\thighp vec3 color = vec3(1.0, 1.0, 1.0);\n";

    for (int arrayNdx = 0; arrayNdx < (int)arrays.size(); arrayNdx++)
    {
        if (arrays[arrayNdx]->getAttribNdx() == 0)
        {
            switch (arrays[arrayNdx]->getOutputType())
            {
            case (Array::OUTPUTTYPE_FLOAT):
                vertexShaderTmpl << "\tcoord = vec2(a_0);\n";
                break;

            case (Array::OUTPUTTYPE_VEC2):
                vertexShaderTmpl << "\tcoord = a_0.xy;\n";
                break;

            case (Array::OUTPUTTYPE_VEC3):
                vertexShaderTmpl << "\tcoord = a_0.xy;\n"
                                    "\tcoord.x = coord.x + a_0.z;\n";
                break;

            case (Array::OUTPUTTYPE_VEC4):
                vertexShaderTmpl << "\tcoord = a_0.xy;\n"
                                    "\tcoord += a_0.zw;\n";
                break;

            case (Array::OUTPUTTYPE_IVEC2):
            case (Array::OUTPUTTYPE_UVEC2):
                vertexShaderTmpl << "\tcoord = vec2(a_0.xy);\n";
                break;

            case (Array::OUTPUTTYPE_IVEC3):
            case (Array::OUTPUTTYPE_UVEC3):
                vertexShaderTmpl << "\tcoord = vec2(a_0.xy);\n"
                                    "\tcoord.x = coord.x + float(a_0.z);\n";
                break;

            case (Array::OUTPUTTYPE_IVEC4):
            case (Array::OUTPUTTYPE_UVEC4):
                vertexShaderTmpl << "\tcoord = vec2(a_0.xy);\n"
                                    "\tcoord += vec2(a_0.zw);\n";
                break;

            default:
                DE_ASSERT(false);
                break;
            }
            continue;
        }

        switch (arrays[arrayNdx]->getOutputType())
        {
        case (Array::OUTPUTTYPE_FLOAT):
            vertexShaderTmpl << "\tcolor = color * a_" << arrays[arrayNdx]->getAttribNdx() << ";\n";
            break;

        case (Array::OUTPUTTYPE_VEC2):
            vertexShaderTmpl << "\tcolor.rg = color.rg * a_" << arrays[arrayNdx]->getAttribNdx() << ".xy;\n";
            break;

        case (Array::OUTPUTTYPE_VEC3):
            vertexShaderTmpl << "\tcolor = color.rgb * a_" << arrays[arrayNdx]->getAttribNdx() << ".xyz;\n";
            break;

        case (Array::OUTPUTTYPE_VEC4):
            vertexShaderTmpl << "\tcolor = color.rgb * a_" << arrays[arrayNdx]->getAttribNdx() << ".xyz * a_"
                             << arrays[arrayNdx]->getAttribNdx() << ".w;\n";
            break;

        default:
            DE_ASSERT(false);
            break;
        }
    }

    vertexShaderTmpl << "\tv_color = vec4(u_colorScale * color, 1.0);\n"
                        "\tgl_Position = vec4(u_coordScale * coord, 1.0, 1.0);\n"
                        "}\n";

    return tcu::StringTemplate(vertexShaderTmpl.str().c_str()).specialize(params);
}

std::string ContextShaderProgram::genFragmentSource(const glu::RenderContext &ctx)
{
    std::map<std::string, std::string> params;

    if (glu::isGLSLVersionSupported(ctx.getType(), glu::GLSL_VERSION_300_ES))
    {
        params["VTX_IN"]     = "in";
        params["VTX_OUT"]    = "out";
        params["FRAG_IN"]    = "in";
        params["FRAG_COLOR"] = "dEQP_FragColor";
        params["VTX_HDR"]    = "#version 300 es\n";
        params["FRAG_HDR"]   = "#version 300 es\nlayout(location = 0) out mediump vec4 dEQP_FragColor;\n";
    }
    else if (glu::isGLSLVersionSupported(ctx.getType(), glu::GLSL_VERSION_100_ES))
    {
        params["VTX_IN"]     = "attribute";
        params["VTX_OUT"]    = "varying";
        params["FRAG_IN"]    = "varying";
        params["FRAG_COLOR"] = "gl_FragColor";
        params["VTX_HDR"]    = "";
        params["FRAG_HDR"]   = "";
    }
    else if (glu::isGLSLVersionSupported(ctx.getType(), glu::GLSL_VERSION_330))
    {
        params["VTX_IN"]     = "in";
        params["VTX_OUT"]    = "out";
        params["FRAG_IN"]    = "in";
        params["FRAG_COLOR"] = "dEQP_FragColor";
        params["VTX_HDR"]    = "#version 330\n";
        params["FRAG_HDR"]   = "#version 330\nlayout(location = 0) out mediump vec4 dEQP_FragColor;\n";
    }
    else
        DE_ASSERT(false);

    static const char *fragmentShaderTmpl = "${FRAG_HDR}"
                                            "${FRAG_IN} mediump vec4 v_color;\n"
                                            "void main(void)\n"
                                            "{\n"
                                            "\t${FRAG_COLOR} = v_color;\n"
                                            "}\n";

    return tcu::StringTemplate(fragmentShaderTmpl).specialize(params);
}

rr::GenericVecType ContextShaderProgram::mapOutputType(const Array::OutputType &type)
{
    switch (type)
    {
    case (Array::OUTPUTTYPE_FLOAT):
    case (Array::OUTPUTTYPE_VEC2):
    case (Array::OUTPUTTYPE_VEC3):
    case (Array::OUTPUTTYPE_VEC4):
        return rr::GENERICVECTYPE_FLOAT;

    case (Array::OUTPUTTYPE_INT):
    case (Array::OUTPUTTYPE_IVEC2):
    case (Array::OUTPUTTYPE_IVEC3):
    case (Array::OUTPUTTYPE_IVEC4):
        return rr::GENERICVECTYPE_INT32;

    case (Array::OUTPUTTYPE_UINT):
    case (Array::OUTPUTTYPE_UVEC2):
    case (Array::OUTPUTTYPE_UVEC3):
    case (Array::OUTPUTTYPE_UVEC4):
        return rr::GENERICVECTYPE_UINT32;

    default:
        DE_ASSERT(false);
        return rr::GENERICVECTYPE_LAST;
    }
}

int ContextShaderProgram::getComponentCount(const Array::OutputType &type)
{
    switch (type)
    {
    case (Array::OUTPUTTYPE_FLOAT):
    case (Array::OUTPUTTYPE_INT):
    case (Array::OUTPUTTYPE_UINT):
        return 1;

    case (Array::OUTPUTTYPE_VEC2):
    case (Array::OUTPUTTYPE_IVEC2):
    case (Array::OUTPUTTYPE_UVEC2):
        return 2;

    case (Array::OUTPUTTYPE_VEC3):
    case (Array::OUTPUTTYPE_IVEC3):
    case (Array::OUTPUTTYPE_UVEC3):
        return 3;

    case (Array::OUTPUTTYPE_VEC4):
    case (Array::OUTPUTTYPE_IVEC4):
    case (Array::OUTPUTTYPE_UVEC4):
        return 4;

    default:
        DE_ASSERT(false);
        return 0;
    }
}

sglr::pdec::ShaderProgramDeclaration ContextShaderProgram::createProgramDeclaration(
    const glu::RenderContext &ctx, const std::vector<ContextArray *> &arrays)
{
    sglr::pdec::ShaderProgramDeclaration decl;

    for (int arrayNdx = 0; arrayNdx < (int)arrays.size(); arrayNdx++)
        decl << sglr::pdec::VertexAttribute(std::string("a_") + de::toString(arrayNdx),
                                            mapOutputType(arrays[arrayNdx]->getOutputType()));

    decl << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT);
    decl << sglr::pdec::FragmentOutput(rr::GENERICVECTYPE_FLOAT);

    decl << sglr::pdec::VertexSource(genVertexSource(ctx, arrays));
    decl << sglr::pdec::FragmentSource(genFragmentSource(ctx));

    decl << sglr::pdec::Uniform("u_coordScale", glu::TYPE_FLOAT);
    decl << sglr::pdec::Uniform("u_colorScale", glu::TYPE_FLOAT);

    return decl;
}

void ContextArrayPack::updateProgram(void)
{
    delete m_program;
    m_program = new ContextShaderProgram(m_renderCtx, m_arrays);
}

void ContextArrayPack::render(Array::Primitive primitive, int firstVertex, int vertexCount, bool useVao,
                              float coordScale, float colorScale)
{
    uint32_t program = 0;
    uint32_t vaoId   = 0;

    updateProgram();

    m_ctx.viewport(0, 0, m_screen.getWidth(), m_screen.getHeight());
    m_ctx.clearColor(0.0, 0.0, 0.0, 1.0);
    m_ctx.clear(GL_COLOR_BUFFER_BIT);

    program = m_ctx.createProgram(m_program);

    m_ctx.useProgram(program);
    GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glUseProgram()");

    m_ctx.uniform1f(m_ctx.getUniformLocation(program, "u_coordScale"), coordScale);
    m_ctx.uniform1f(m_ctx.getUniformLocation(program, "u_colorScale"), colorScale);

    if (useVao)
    {
        m_ctx.genVertexArrays(1, &vaoId);
        m_ctx.bindVertexArray(vaoId);
    }

    for (int arrayNdx = 0; arrayNdx < (int)m_arrays.size(); arrayNdx++)
    {
        if (m_arrays[arrayNdx]->isBound())
        {
            std::stringstream attribName;
            attribName << "a_" << m_arrays[arrayNdx]->getAttribNdx();

            uint32_t loc = m_ctx.getAttribLocation(program, attribName.str().c_str());
            m_ctx.enableVertexAttribArray(loc);
            GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glEnableVertexAttribArray()");

            m_arrays[arrayNdx]->glBind(loc);
        }
    }

    DE_ASSERT((firstVertex % 6) == 0);
    m_ctx.drawArrays(ContextArray::primitiveToGL(primitive), firstVertex, vertexCount - firstVertex);
    GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glDrawArrays()");

    for (int arrayNdx = 0; arrayNdx < (int)m_arrays.size(); arrayNdx++)
    {
        if (m_arrays[arrayNdx]->isBound())
        {
            std::stringstream attribName;
            attribName << "a_" << m_arrays[arrayNdx]->getAttribNdx();

            uint32_t loc = m_ctx.getAttribLocation(program, attribName.str().c_str());

            m_ctx.disableVertexAttribArray(loc);
            GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glDisableVertexAttribArray()");
        }
    }

    if (useVao)
        m_ctx.deleteVertexArrays(1, &vaoId);

    m_ctx.deleteProgram(program);
    m_ctx.useProgram(0);
    m_ctx.readPixels(m_screen, 0, 0, m_screen.getWidth(), m_screen.getHeight());
}

// GLValue

GLValue GLValue::getMaxValue(Array::InputType type)
{
    GLValue rangesHi[(int)Array::INPUTTYPE_LAST];

    rangesHi[(int)Array::INPUTTYPE_FLOAT]          = GLValue(Float::create(127.0f));
    rangesHi[(int)Array::INPUTTYPE_DOUBLE]         = GLValue(Double::create(127.0f));
    rangesHi[(int)Array::INPUTTYPE_BYTE]           = GLValue(Byte::create(127));
    rangesHi[(int)Array::INPUTTYPE_UNSIGNED_BYTE]  = GLValue(Ubyte::create(255));
    rangesHi[(int)Array::INPUTTYPE_UNSIGNED_SHORT] = GLValue(Ushort::create(65530));
    rangesHi[(int)Array::INPUTTYPE_SHORT]          = GLValue(Short::create(32760));
    rangesHi[(int)Array::INPUTTYPE_FIXED]          = GLValue(Fixed::create(32760));
    rangesHi[(int)Array::INPUTTYPE_INT]            = GLValue(Int::create(2147483647));
    rangesHi[(int)Array::INPUTTYPE_UNSIGNED_INT]   = GLValue(Uint::create(4294967295u));
    rangesHi[(int)Array::INPUTTYPE_HALF]           = GLValue(Half::create(256.0f));

    return rangesHi[(int)type];
}

GLValue GLValue::getMinValue(Array::InputType type)
{
    GLValue rangesLo[(int)Array::INPUTTYPE_LAST];

    rangesLo[(int)Array::INPUTTYPE_FLOAT]          = GLValue(Float::create(-127.0f));
    rangesLo[(int)Array::INPUTTYPE_DOUBLE]         = GLValue(Double::create(-127.0f));
    rangesLo[(int)Array::INPUTTYPE_BYTE]           = GLValue(Byte::create(-127));
    rangesLo[(int)Array::INPUTTYPE_UNSIGNED_BYTE]  = GLValue(Ubyte::create(0));
    rangesLo[(int)Array::INPUTTYPE_UNSIGNED_SHORT] = GLValue(Ushort::create(0));
    rangesLo[(int)Array::INPUTTYPE_SHORT]          = GLValue(Short::create(-32760));
    rangesLo[(int)Array::INPUTTYPE_FIXED]          = GLValue(Fixed::create(-32760));
    rangesLo[(int)Array::INPUTTYPE_INT]            = GLValue(Int::create(-2147483647));
    rangesLo[(int)Array::INPUTTYPE_UNSIGNED_INT]   = GLValue(Uint::create(0));
    rangesLo[(int)Array::INPUTTYPE_HALF]           = GLValue(Half::create(-256.0f));

    return rangesLo[(int)type];
}

float GLValue::toFloat(void) const
{
    switch (type)
    {
    case Array::INPUTTYPE_FLOAT:
        return fl.getValue();

    case Array::INPUTTYPE_BYTE:
        return b.getValue();

    case Array::INPUTTYPE_UNSIGNED_BYTE:
        return ub.getValue();

    case Array::INPUTTYPE_SHORT:
        return s.getValue();

    case Array::INPUTTYPE_UNSIGNED_SHORT:
        return us.getValue();

    case Array::INPUTTYPE_FIXED:
    {
        int maxValue = 65536;
        return (float)(double(2 * fi.getValue() + 1) / (maxValue - 1));
    }

    case Array::INPUTTYPE_UNSIGNED_INT:
        return (float)ui.getValue();

    case Array::INPUTTYPE_INT:
        return (float)i.getValue();

    case Array::INPUTTYPE_HALF:
        return h.to<float>();

    case Array::INPUTTYPE_DOUBLE:
        return (float)d.getValue();

    default:
        DE_ASSERT(false);
        return 0.0f;
    }
}

class RandomArrayGenerator
{
public:
    static char *generateArray(int seed, GLValue min, GLValue max, int count, int componentCount, int stride,
                               Array::InputType type);
    static char *generateQuads(int seed, int count, int componentCount, int offset, int stride,
                               Array::Primitive primitive, Array::InputType type, GLValue min, GLValue max,
                               float gridSize);
    static char *generatePerQuad(int seed, int count, int componentCount, int stride, Array::Primitive primitive,
                                 Array::InputType type, GLValue min, GLValue max);

private:
    template <typename T>
    static char *createQuads(int seed, int count, int componentCount, int offset, int stride,
                             Array::Primitive primitive, T min, T max, float gridSize);
    template <typename T>
    static char *createPerQuads(int seed, int count, int componentCount, int stride, Array::Primitive primitive, T min,
                                T max);
    static char *createQuadsPacked(int seed, int count, int componentCount, int offset, int stride,
                                   Array::Primitive primitive);
    static void setData(char *data, Array::InputType type, deRandom &rnd, GLValue min, GLValue max);
};

void RandomArrayGenerator::setData(char *data, Array::InputType type, deRandom &rnd, GLValue min, GLValue max)
{
    switch (type)
    {
    case Array::INPUTTYPE_FLOAT:
    {
        alignmentSafeAssignment<float>(data, getRandom<GLValue::Float>(rnd, min.fl, max.fl));
        break;
    }

    case Array::INPUTTYPE_DOUBLE:
    {
        alignmentSafeAssignment<double>(data, getRandom<GLValue::Float>(rnd, min.fl, max.fl));
        break;
    }

    case Array::INPUTTYPE_SHORT:
    {
        alignmentSafeAssignment<int16_t>(data, getRandom<GLValue::Short>(rnd, min.s, max.s));
        break;
    }

    case Array::INPUTTYPE_UNSIGNED_SHORT:
    {
        alignmentSafeAssignment<uint16_t>(data, getRandom<GLValue::Ushort>(rnd, min.us, max.us));
        break;
    }

    case Array::INPUTTYPE_BYTE:
    {
        alignmentSafeAssignment<int8_t>(data, getRandom<GLValue::Byte>(rnd, min.b, max.b));
        break;
    }

    case Array::INPUTTYPE_UNSIGNED_BYTE:
    {
        alignmentSafeAssignment<uint8_t>(data, getRandom<GLValue::Ubyte>(rnd, min.ub, max.ub));
        break;
    }

    case Array::INPUTTYPE_FIXED:
    {
        alignmentSafeAssignment<int32_t>(data, getRandom<GLValue::Fixed>(rnd, min.fi, max.fi));
        break;
    }

    case Array::INPUTTYPE_INT:
    {
        alignmentSafeAssignment<int32_t>(data, getRandom<GLValue::Int>(rnd, min.i, max.i));
        break;
    }

    case Array::INPUTTYPE_UNSIGNED_INT:
    {
        alignmentSafeAssignment<uint32_t>(data, getRandom<GLValue::Uint>(rnd, min.ui, max.ui));
        break;
    }

    case Array::INPUTTYPE_HALF:
    {
        alignmentSafeAssignment<deFloat16>(data, getRandom<GLValue::Half>(rnd, min.h, max.h).getValue());
        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }
}

char *RandomArrayGenerator::generateArray(int seed, GLValue min, GLValue max, int count, int componentCount, int stride,
                                          Array::InputType type)
{
    char *data = NULL;

    deRandom rnd;
    deRandom_init(&rnd, seed);

    if (stride == 0)
        stride = componentCount * Array::inputTypeSize(type);

    data = new char[stride * count];

    for (int vertexNdx = 0; vertexNdx < count; vertexNdx++)
    {
        for (int componentNdx = 0; componentNdx < componentCount; componentNdx++)
        {
            setData(&(data[vertexNdx * stride + Array::inputTypeSize(type) * componentNdx]), type, rnd, min, max);
        }
    }

    return data;
}

char *RandomArrayGenerator::generateQuads(int seed, int count, int componentCount, int offset, int stride,
                                          Array::Primitive primitive, Array::InputType type, GLValue min, GLValue max,
                                          float gridSize)
{
    char *data = nullptr;

    switch (type)
    {
    case Array::INPUTTYPE_FLOAT:
        data = createQuads<GLValue::Float>(seed, count, componentCount, offset, stride, primitive, min.fl, max.fl,
                                           gridSize);
        break;

    case Array::INPUTTYPE_FIXED:
        data = createQuads<GLValue::Fixed>(seed, count, componentCount, offset, stride, primitive, min.fi, max.fi,
                                           gridSize);
        break;

    case Array::INPUTTYPE_DOUBLE:
        data = createQuads<GLValue::Double>(seed, count, componentCount, offset, stride, primitive, min.d, max.d,
                                            gridSize);
        break;

    case Array::INPUTTYPE_BYTE:
        data =
            createQuads<GLValue::Byte>(seed, count, componentCount, offset, stride, primitive, min.b, max.b, gridSize);
        break;

    case Array::INPUTTYPE_SHORT:
        data =
            createQuads<GLValue::Short>(seed, count, componentCount, offset, stride, primitive, min.s, max.s, gridSize);
        break;

    case Array::INPUTTYPE_UNSIGNED_BYTE:
        data = createQuads<GLValue::Ubyte>(seed, count, componentCount, offset, stride, primitive, min.ub, max.ub,
                                           gridSize);
        break;

    case Array::INPUTTYPE_UNSIGNED_SHORT:
        data = createQuads<GLValue::Ushort>(seed, count, componentCount, offset, stride, primitive, min.us, max.us,
                                            gridSize);
        break;

    case Array::INPUTTYPE_UNSIGNED_INT:
        data = createQuads<GLValue::Uint>(seed, count, componentCount, offset, stride, primitive, min.ui, max.ui,
                                          gridSize);
        break;

    case Array::INPUTTYPE_INT:
        data =
            createQuads<GLValue::Int>(seed, count, componentCount, offset, stride, primitive, min.i, max.i, gridSize);
        break;

    case Array::INPUTTYPE_HALF:
        data =
            createQuads<GLValue::Half>(seed, count, componentCount, offset, stride, primitive, min.h, max.h, gridSize);
        break;

    case Array::INPUTTYPE_INT_2_10_10_10:
    case Array::INPUTTYPE_UNSIGNED_INT_2_10_10_10:
        data = createQuadsPacked(seed, count, componentCount, offset, stride, primitive);
        break;

    default:
        DE_ASSERT(false);
        break;
    }

    return data;
}

char *RandomArrayGenerator::createQuadsPacked(int seed, int count, int componentCount, int offset, int stride,
                                              Array::Primitive primitive)
{
    DE_ASSERT(componentCount == 4);
    DE_UNREF(componentCount);
    int quadStride = 0;

    if (stride == 0)
        stride = sizeof(uint32_t);

    switch (primitive)
    {
    case Array::PRIMITIVE_TRIANGLES:
        quadStride = stride * 6;
        break;

    default:
        DE_ASSERT(false);
        break;
    }

    char *const _data =
        new char[offset + quadStride * (count - 1) + stride * 5 +
                 componentCount *
                     Array::inputTypeSize(Array::INPUTTYPE_INT_2_10_10_10)]; // last element must be fully in the array
    char *const resultData = _data + offset;

    const uint32_t max  = 1024;
    const uint32_t min  = 10;
    const uint32_t max2 = 4;

    deRandom rnd;
    deRandom_init(&rnd, seed);

    switch (primitive)
    {
    case Array::PRIMITIVE_TRIANGLES:
    {
        for (int quadNdx = 0; quadNdx < count; quadNdx++)
        {
            uint32_t x1 = min + deRandom_getUint32(&rnd) % (max - min);
            uint32_t x2 = min + deRandom_getUint32(&rnd) % (max - x1);

            uint32_t y1 = min + deRandom_getUint32(&rnd) % (max - min);
            uint32_t y2 = min + deRandom_getUint32(&rnd) % (max - y1);

            uint32_t z = min + deRandom_getUint32(&rnd) % (max - min);
            uint32_t w = deRandom_getUint32(&rnd) % max2;

            uint32_t val1 = (w << 30) | (z << 20) | (y1 << 10) | x1;
            uint32_t val2 = (w << 30) | (z << 20) | (y1 << 10) | x2;
            uint32_t val3 = (w << 30) | (z << 20) | (y2 << 10) | x1;

            uint32_t val4 = (w << 30) | (z << 20) | (y2 << 10) | x1;
            uint32_t val5 = (w << 30) | (z << 20) | (y1 << 10) | x2;
            uint32_t val6 = (w << 30) | (z << 20) | (y2 << 10) | x2;

            alignmentSafeAssignment<uint32_t>(&(resultData[quadNdx * quadStride + stride * 0]), val1);
            alignmentSafeAssignment<uint32_t>(&(resultData[quadNdx * quadStride + stride * 1]), val2);
            alignmentSafeAssignment<uint32_t>(&(resultData[quadNdx * quadStride + stride * 2]), val3);
            alignmentSafeAssignment<uint32_t>(&(resultData[quadNdx * quadStride + stride * 3]), val4);
            alignmentSafeAssignment<uint32_t>(&(resultData[quadNdx * quadStride + stride * 4]), val5);
            alignmentSafeAssignment<uint32_t>(&(resultData[quadNdx * quadStride + stride * 5]), val6);
        }

        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }

    return _data;
}

template <typename T>
T roundTo(const T &step, const T &value)
{
    return value - (value % step);
}

template <typename T>
char *RandomArrayGenerator::createQuads(int seed, int count, int componentCount, int offset, int stride,
                                        Array::Primitive primitive, T min, T max, float gridSize)
{
    int componentStride = sizeof(T);
    int quadStride      = 0;

    if (stride == 0)
        stride = componentCount * componentStride;

    DE_ASSERT(stride >= componentCount * componentStride);

    switch (primitive)
    {
    case Array::PRIMITIVE_TRIANGLES:
        quadStride = stride * 6;
        break;

    default:
        DE_ASSERT(false);
        break;
    }

    char *resultData = new char[offset + quadStride * count];
    char *_data      = resultData;
    resultData       = resultData + offset;

    deRandom rnd;
    deRandom_init(&rnd, seed);

    switch (primitive)
    {
    case Array::PRIMITIVE_TRIANGLES:
    {
        const T minQuadSize = T::fromFloat(deFloatAbs(max.template to<float>() - min.template to<float>()) * gridSize);
        const T minDiff     = minValue<T>() > minQuadSize ? minValue<T>() : minQuadSize;
        const T maxRounded  = roundTo(minDiff, max);

        for (int quadNdx = 0; quadNdx < count; ++quadNdx)
        {
            T x1, x2;
            T y1, y2;
            T z, w;

            x1 = roundTo(minDiff, getRandom<T>(rnd, min, maxRounded - minDiff));
            x2 = roundTo(minDiff, getRandom<T>(rnd, x1 + minDiff, maxRounded));

            y1 = roundTo(minDiff, getRandom<T>(rnd, min, maxRounded - minDiff));
            y2 = roundTo(minDiff, getRandom<T>(rnd, y1 + minDiff, maxRounded));

            // Make sure the rounding doesn't drop the result below the original range of the random function.
            if (x2 < x1 + minDiff)
                x2 = x1 + minDiff;
            if (y2 < y1 + minDiff)
                y2 = y1 + minDiff;

            z = (componentCount > 2) ? roundTo(minDiff, (getRandom<T>(rnd, min, max))) : (T::create(0));
            w = (componentCount > 3) ? roundTo(minDiff, (getRandom<T>(rnd, min, max))) : (T::create(1));

            // Make sure the quad is not too thin.
            DE_ASSERT(
                (deFloatAbs(x2.template to<float>() - x1.template to<float>()) >=
                 minDiff.template to<float>() * 0.8f) &&
                (deFloatAbs(y2.template to<float>() - y1.template to<float>()) >= minDiff.template to<float>() * 0.8f));

            alignmentSafeAssignment<T>(&(resultData[quadNdx * quadStride]), x1);
            alignmentSafeAssignment<T>(&(resultData[quadNdx * quadStride + componentStride]), y1);

            alignmentSafeAssignment<T>(&(resultData[quadNdx * quadStride + stride]), x2);
            alignmentSafeAssignment<T>(&(resultData[quadNdx * quadStride + stride + componentStride]), y1);

            alignmentSafeAssignment<T>(&(resultData[quadNdx * quadStride + stride * 2]), x1);
            alignmentSafeAssignment<T>(&(resultData[quadNdx * quadStride + stride * 2 + componentStride]), y2);

            alignmentSafeAssignment<T>(&(resultData[quadNdx * quadStride + stride * 3]), x1);
            alignmentSafeAssignment<T>(&(resultData[quadNdx * quadStride + stride * 3 + componentStride]), y2);

            alignmentSafeAssignment<T>(&(resultData[quadNdx * quadStride + stride * 4]), x2);
            alignmentSafeAssignment<T>(&(resultData[quadNdx * quadStride + stride * 4 + componentStride]), y1);

            alignmentSafeAssignment<T>(&(resultData[quadNdx * quadStride + stride * 5]), x2);
            alignmentSafeAssignment<T>(&(resultData[quadNdx * quadStride + stride * 5 + componentStride]), y2);

            if (componentCount > 2)
            {
                for (int i = 0; i < 6; i++)
                    alignmentSafeAssignment<T>(&(resultData[quadNdx * quadStride + stride * i + componentStride * 2]),
                                               z);
            }

            if (componentCount > 3)
            {
                for (int i = 0; i < 6; i++)
                    alignmentSafeAssignment<T>(&(resultData[quadNdx * quadStride + stride * i + componentStride * 3]),
                                               w);
            }
        }

        break;
    }

    default:
        DE_ASSERT(false);
        break;
    }

    return _data;
}

char *RandomArrayGenerator::generatePerQuad(int seed, int count, int componentCount, int stride,
                                            Array::Primitive primitive, Array::InputType type, GLValue min, GLValue max)
{
    char *data = nullptr;

    switch (type)
    {
    case Array::INPUTTYPE_FLOAT:
        data = createPerQuads<GLValue::Float>(seed, count, componentCount, stride, primitive, min.fl, max.fl);
        break;

    case Array::INPUTTYPE_FIXED:
        data = createPerQuads<GLValue::Fixed>(seed, count, componentCount, stride, primitive, min.fi, max.fi);
        break;

    case Array::INPUTTYPE_DOUBLE:
        data = createPerQuads<GLValue::Double>(seed, count, componentCount, stride, primitive, min.d, max.d);
        break;

    case Array::INPUTTYPE_BYTE:
        data = createPerQuads<GLValue::Byte>(seed, count, componentCount, stride, primitive, min.b, max.b);
        break;

    case Array::INPUTTYPE_SHORT:
        data = createPerQuads<GLValue::Short>(seed, count, componentCount, stride, primitive, min.s, max.s);
        break;

    case Array::INPUTTYPE_UNSIGNED_BYTE:
        data = createPerQuads<GLValue::Ubyte>(seed, count, componentCount, stride, primitive, min.ub, max.ub);
        break;

    case Array::INPUTTYPE_UNSIGNED_SHORT:
        data = createPerQuads<GLValue::Ushort>(seed, count, componentCount, stride, primitive, min.us, max.us);
        break;

    case Array::INPUTTYPE_UNSIGNED_INT:
        data = createPerQuads<GLValue::Uint>(seed, count, componentCount, stride, primitive, min.ui, max.ui);
        break;

    case Array::INPUTTYPE_INT:
        data = createPerQuads<GLValue::Int>(seed, count, componentCount, stride, primitive, min.i, max.i);
        break;

    case Array::INPUTTYPE_HALF:
        data = createPerQuads<GLValue::Half>(seed, count, componentCount, stride, primitive, min.h, max.h);
        break;

    default:
        DE_ASSERT(false);
        break;
    }

    return data;
}

template <typename T>
char *RandomArrayGenerator::createPerQuads(int seed, int count, int componentCount, int stride,
                                           Array::Primitive primitive, T min, T max)
{
    deRandom rnd;
    deRandom_init(&rnd, seed);

    int componentStride = sizeof(T);

    if (stride == 0)
        stride = componentStride * componentCount;

    int quadStride = 0;

    switch (primitive)
    {
    case Array::PRIMITIVE_TRIANGLES:
        quadStride = stride * 6;
        break;

    default:
        DE_ASSERT(false);
        break;
    }

    char *data = new char[count * quadStride];

    for (int quadNdx = 0; quadNdx < count; quadNdx++)
    {
        for (int componentNdx = 0; componentNdx < componentCount; componentNdx++)
        {
            T val = getRandom<T>(rnd, min, max);

            alignmentSafeAssignment<T>(data + quadNdx * quadStride + stride * 0 + componentStride * componentNdx, val);
            alignmentSafeAssignment<T>(data + quadNdx * quadStride + stride * 1 + componentStride * componentNdx, val);
            alignmentSafeAssignment<T>(data + quadNdx * quadStride + stride * 2 + componentStride * componentNdx, val);
            alignmentSafeAssignment<T>(data + quadNdx * quadStride + stride * 3 + componentStride * componentNdx, val);
            alignmentSafeAssignment<T>(data + quadNdx * quadStride + stride * 4 + componentStride * componentNdx, val);
            alignmentSafeAssignment<T>(data + quadNdx * quadStride + stride * 5 + componentStride * componentNdx, val);
        }
    }

    return data;
}

// VertexArrayTest

VertexArrayTest::VertexArrayTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                 const char *desc)
    : TestCase(testCtx, name, desc)
    , m_renderCtx(renderCtx)
    , m_refBuffers(nullptr)
    , m_refContext(nullptr)
    , m_glesContext(nullptr)
    , m_glArrayPack(nullptr)
    , m_rrArrayPack(nullptr)
    , m_isOk(false)
    , m_maxDiffRed(
          deCeilFloatToInt32(256.0f * (2.0f / (float)(1 << m_renderCtx.getRenderTarget().getPixelFormat().redBits))))
    , m_maxDiffGreen(
          deCeilFloatToInt32(256.0f * (2.0f / (float)(1 << m_renderCtx.getRenderTarget().getPixelFormat().greenBits))))
    , m_maxDiffBlue(
          deCeilFloatToInt32(256.0f * (2.0f / (float)(1 << m_renderCtx.getRenderTarget().getPixelFormat().blueBits))))
{
}

VertexArrayTest::~VertexArrayTest(void)
{
    deinit();
}

void VertexArrayTest::init(void)
{
    const int renderTargetWidth  = de::min(512, m_renderCtx.getRenderTarget().getWidth());
    const int renderTargetHeight = de::min(512, m_renderCtx.getRenderTarget().getHeight());
    sglr::ReferenceContextLimits limits(m_renderCtx);

    m_glesContext =
        new sglr::GLContext(m_renderCtx, m_testCtx.getLog(), sglr::GLCONTEXT_LOG_CALLS | sglr::GLCONTEXT_LOG_PROGRAMS,
                            tcu::IVec4(0, 0, renderTargetWidth, renderTargetHeight));

    m_refBuffers = new sglr::ReferenceContextBuffers(m_renderCtx.getRenderTarget().getPixelFormat(), 0, 0,
                                                     renderTargetWidth, renderTargetHeight);
    m_refContext = new sglr::ReferenceContext(limits, m_refBuffers->getColorbuffer(), m_refBuffers->getDepthbuffer(),
                                              m_refBuffers->getStencilbuffer());

    m_glArrayPack = new ContextArrayPack(m_renderCtx, *m_glesContext);
    m_rrArrayPack = new ContextArrayPack(m_renderCtx, *m_refContext);
}

void VertexArrayTest::deinit(void)
{
    delete m_glArrayPack;
    delete m_rrArrayPack;
    delete m_refBuffers;
    delete m_refContext;
    delete m_glesContext;

    m_glArrayPack = nullptr;
    m_rrArrayPack = nullptr;
    m_refBuffers  = nullptr;
    m_refContext  = nullptr;
    m_glesContext = nullptr;
}

void VertexArrayTest::compare(void)
{
    const tcu::Surface &ref    = m_rrArrayPack->getSurface();
    const tcu::Surface &screen = m_glArrayPack->getSurface();

    if (m_renderCtx.getRenderTarget().getNumSamples() > 1)
    {
        // \todo [mika] Improve compare when using multisampling
        m_testCtx.getLog() << tcu::TestLog::Message
                           << "Warning: Comparision of result from multisample render targets are not as stricts as "
                              "without multisampling. Might produce false positives!"
                           << tcu::TestLog::EndMessage;
        m_isOk = tcu::fuzzyCompare(m_testCtx.getLog(), "Compare Results", "Compare Results", ref.getAccess(),
                                   screen.getAccess(), 1.5f, tcu::COMPARE_LOG_RESULT);
    }
    else
    {
        tcu::RGBA threshold(m_maxDiffRed, m_maxDiffGreen, m_maxDiffBlue, 255);
        tcu::Surface error(ref.getWidth(), ref.getHeight());

        m_isOk = true;

        for (int y = 0; y < ref.getHeight(); y++)
        {
            for (int x = 0; x < ref.getWidth(); x++)
            {
                tcu::RGBA refPixel    = ref.getPixel(x, y);
                tcu::RGBA screenPixel = screen.getPixel(x, y);
                bool isOkPixel        = false;

                if (y == 0 || y + 1 == ref.getHeight() || x == 0 || x + 1 == ref.getWidth())
                {
                    // Don't check borders since the pixel neighborhood is undefined
                    error.setPixel(x, y,
                                   tcu::RGBA(screenPixel.getRed(), (screenPixel.getGreen() + 255) / 2,
                                             screenPixel.getBlue(), 255));
                    continue;
                }

                // Don't do comparisons for this pixel if it belongs to a one-pixel-thin part (i.e. it doesn't have similar-color neighbors in both x and y directions) in both result and reference.
                // This fixes some false negatives.
                bool refThin = (!tcu::compareThreshold(refPixel, ref.getPixel(x - 1, y), threshold) &&
                                !tcu::compareThreshold(refPixel, ref.getPixel(x + 1, y), threshold)) ||
                               (!tcu::compareThreshold(refPixel, ref.getPixel(x, y - 1), threshold) &&
                                !tcu::compareThreshold(refPixel, ref.getPixel(x, y + 1), threshold));
                bool screenThin = (!tcu::compareThreshold(screenPixel, screen.getPixel(x - 1, y), threshold) &&
                                   !tcu::compareThreshold(screenPixel, screen.getPixel(x + 1, y), threshold)) ||
                                  (!tcu::compareThreshold(screenPixel, screen.getPixel(x, y - 1), threshold) &&
                                   !tcu::compareThreshold(screenPixel, screen.getPixel(x, y + 1), threshold));

                if (refThin && screenThin)
                    isOkPixel = true;
                else
                {
                    for (int dy = -1; dy < 2 && !isOkPixel; dy++)
                    {
                        for (int dx = -1; dx < 2 && !isOkPixel; dx++)
                        {
                            // Check reference pixel against screen pixel
                            {
                                tcu::RGBA screenCmpPixel = screen.getPixel(x + dx, y + dy);
                                uint8_t r = (uint8_t)deAbs32(refPixel.getRed() - screenCmpPixel.getRed());
                                uint8_t g = (uint8_t)deAbs32(refPixel.getGreen() - screenCmpPixel.getGreen());
                                uint8_t b = (uint8_t)deAbs32(refPixel.getBlue() - screenCmpPixel.getBlue());

                                if (r <= m_maxDiffRed && g <= m_maxDiffGreen && b <= m_maxDiffBlue)
                                    isOkPixel = true;
                            }

                            // Check screen pixels against reference pixel
                            {
                                tcu::RGBA refCmpPixel = ref.getPixel(x + dx, y + dy);
                                uint8_t r             = (uint8_t)deAbs32(refCmpPixel.getRed() - screenPixel.getRed());
                                uint8_t g = (uint8_t)deAbs32(refCmpPixel.getGreen() - screenPixel.getGreen());
                                uint8_t b = (uint8_t)deAbs32(refCmpPixel.getBlue() - screenPixel.getBlue());

                                if (r <= m_maxDiffRed && g <= m_maxDiffGreen && b <= m_maxDiffBlue)
                                    isOkPixel = true;
                            }
                        }
                    }
                }

                if (isOkPixel)
                    error.setPixel(x, y,
                                   tcu::RGBA(screen.getPixel(x, y).getRed(),
                                             (screen.getPixel(x, y).getGreen() + 255) / 2,
                                             screen.getPixel(x, y).getBlue(), 255));
                else
                {
                    error.setPixel(x, y, tcu::RGBA(255, 0, 0, 255));
                    m_isOk = false;
                }
            }
        }

        tcu::TestLog &log = m_testCtx.getLog();
        if (!m_isOk)
        {
            log << TestLog::Message << "Image comparison failed, threshold = (" << m_maxDiffRed << ", "
                << m_maxDiffGreen << ", " << m_maxDiffBlue << ")" << TestLog::EndMessage;
            log << TestLog::ImageSet("Compare result", "Result of rendering")
                << TestLog::Image("Result", "Result", screen) << TestLog::Image("Reference", "Reference", ref)
                << TestLog::Image("ErrorMask", "Error mask", error) << TestLog::EndImageSet;
        }
        else
        {
            log << TestLog::ImageSet("Compare result", "Result of rendering")
                << TestLog::Image("Result", "Result", screen) << TestLog::EndImageSet;
        }
    }
}

// MultiVertexArrayTest

MultiVertexArrayTest::Spec::ArraySpec::ArraySpec(Array::InputType inputType_, Array::OutputType outputType_,
                                                 Array::Storage storage_, Array::Usage usage_, int componentCount_,
                                                 int offset_, int stride_, bool normalize_, GLValue min_, GLValue max_)
    : inputType(inputType_)
    , outputType(outputType_)
    , storage(storage_)
    , usage(usage_)
    , componentCount(componentCount_)
    , offset(offset_)
    , stride(stride_)
    , normalize(normalize_)
    , min(min_)
    , max(max_)
{
}

std::string MultiVertexArrayTest::Spec::getName(void) const
{
    std::stringstream name;

    for (size_t ndx = 0; ndx < arrays.size(); ++ndx)
    {
        const ArraySpec &array = arrays[ndx];

        if (arrays.size() > 1)
            name << "array" << ndx << "_";

        name << Array::storageToString(array.storage) << "_" << array.offset << "_" << array.stride << "_"
             << Array::inputTypeToString((Array::InputType)array.inputType);
        if (array.inputType != Array::INPUTTYPE_UNSIGNED_INT_2_10_10_10 &&
            array.inputType != Array::INPUTTYPE_INT_2_10_10_10)
            name << array.componentCount;
        name << "_" << (array.normalize ? "normalized_" : "") << Array::outputTypeToString(array.outputType) << "_"
             << Array::usageTypeToString(array.usage) << "_";
    }

    if (first)
        name << "first" << first << "_";

    switch (primitive)
    {
    case Array::PRIMITIVE_TRIANGLES:
        name << "quads_";
        break;
    case Array::PRIMITIVE_POINTS:
        name << "points_";
        break;

    default:
        DE_ASSERT(false);
        break;
    }

    name << drawCount;

    return name.str();
}

std::string MultiVertexArrayTest::Spec::getDesc(void) const
{
    std::stringstream desc;

    for (size_t ndx = 0; ndx < arrays.size(); ++ndx)
    {
        const ArraySpec &array = arrays[ndx];

        desc << "Array " << ndx << ": "
             << "Storage in " << Array::storageToString(array.storage) << ", "
             << "stride " << array.stride << ", "
             << "input datatype " << Array::inputTypeToString((Array::InputType)array.inputType) << ", "
             << "input component count " << array.componentCount << ", " << (array.normalize ? "normalized, " : "")
             << "used as " << Array::outputTypeToString(array.outputType) << ", ";
    }

    desc << "drawArrays(), "
         << "first " << first << ", " << drawCount;

    switch (primitive)
    {
    case Array::PRIMITIVE_TRIANGLES:
        desc << "quads ";
        break;
    case Array::PRIMITIVE_POINTS:
        desc << "points";
        break;

    default:
        DE_ASSERT(false);
        break;
    }

    return desc.str();
}

MultiVertexArrayTest::MultiVertexArrayTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const Spec &spec,
                                           const char *name, const char *desc)
    : VertexArrayTest(testCtx, renderCtx, name, desc)
    , m_spec(spec)
    , m_iteration(0)
{
}

MultiVertexArrayTest::~MultiVertexArrayTest(void)
{
}

MultiVertexArrayTest::IterateResult MultiVertexArrayTest::iterate(void)
{
    if (m_iteration == 0)
    {
        const size_t primitiveSize = (m_spec.primitive == Array::PRIMITIVE_TRIANGLES) ?
                                         (6) :
                                         (1); // in non-indexed draw Triangles means rectangles
        float coordScale           = 1.0f;
        float colorScale           = 1.0f;
        const bool useVao          = m_renderCtx.getType().getProfile() == glu::PROFILE_CORE;

        // Log info
        m_testCtx.getLog() << TestLog::Message << m_spec.getDesc() << TestLog::EndMessage;

        // Color and Coord scale
        {
            // First array is always position
            {
                Spec::ArraySpec arraySpec = m_spec.arrays[0];
                if (arraySpec.inputType == Array::INPUTTYPE_UNSIGNED_INT_2_10_10_10)
                {
                    if (arraySpec.normalize)
                        coordScale = 1.0f;
                    else
                        coordScale = 1.0 / 1024.0;
                }
                else if (arraySpec.inputType == Array::INPUTTYPE_INT_2_10_10_10)
                {
                    if (arraySpec.normalize)
                        coordScale = 1.0f;
                    else
                        coordScale = 1.0 / 512.0;
                }
                else
                    coordScale = (arraySpec.normalize && !inputTypeIsFloatType(arraySpec.inputType) ?
                                      1.0f :
                                      float(0.9 / double(arraySpec.max.toFloat())));

                if (arraySpec.outputType == Array::OUTPUTTYPE_VEC3 || arraySpec.outputType == Array::OUTPUTTYPE_VEC4 ||
                    arraySpec.outputType == Array::OUTPUTTYPE_IVEC3 ||
                    arraySpec.outputType == Array::OUTPUTTYPE_IVEC4 ||
                    arraySpec.outputType == Array::OUTPUTTYPE_UVEC3 || arraySpec.outputType == Array::OUTPUTTYPE_UVEC4)
                    coordScale = coordScale * 0.5f;
            }

            // And other arrays are color-like
            for (int arrayNdx = 1; arrayNdx < (int)m_spec.arrays.size(); arrayNdx++)
            {
                Spec::ArraySpec arraySpec = m_spec.arrays[arrayNdx];

                colorScale *= (arraySpec.normalize && !inputTypeIsFloatType(arraySpec.inputType) ?
                                   1.0f :
                                   float(1.0 / double(arraySpec.max.toFloat())));
                if (arraySpec.outputType == Array::OUTPUTTYPE_VEC4)
                    colorScale *= (arraySpec.normalize && !inputTypeIsFloatType(arraySpec.inputType) ?
                                       1.0f :
                                       float(1.0 / double(arraySpec.max.toFloat())));
            }
        }

        // Data
        for (int arrayNdx = 0; arrayNdx < (int)m_spec.arrays.size(); arrayNdx++)
        {
            Spec::ArraySpec arraySpec = m_spec.arrays[arrayNdx];
            const int seed = int(arraySpec.inputType) + 10 * int(arraySpec.outputType) + 100 * int(arraySpec.storage) +
                             1000 * int(m_spec.primitive) + 10000 * int(arraySpec.usage) + int(m_spec.drawCount) +
                             12 * int(arraySpec.componentCount) + int(arraySpec.stride) + int(arraySpec.normalize);
            const char *data        = nullptr;
            const size_t stride     = (arraySpec.stride == 0) ?
                                          (arraySpec.componentCount * Array::inputTypeSize(arraySpec.inputType)) :
                                          (arraySpec.stride);
            const size_t bufferSize = arraySpec.offset + stride * (m_spec.drawCount * primitiveSize - 1) +
                                      arraySpec.componentCount * Array::inputTypeSize(arraySpec.inputType);
            // Snap values to at least 3x3 grid
            const float gridSize = 3.0f / (float)(de::min(m_renderCtx.getRenderTarget().getWidth(),
                                                          m_renderCtx.getRenderTarget().getHeight()) -
                                                  1);

            switch (m_spec.primitive)
            {
                //            case Array::PRIMITIVE_POINTS:
                // data = RandomArrayGenerator::generateArray(seed, arraySpec.min, arraySpec.max, arraySpec.count, arraySpec.componentCount, arraySpec.stride, arraySpec.inputType);
                // break;
            case Array::PRIMITIVE_TRIANGLES:
                if (arrayNdx == 0)
                {
                    data = RandomArrayGenerator::generateQuads(
                        seed, m_spec.drawCount, arraySpec.componentCount, arraySpec.offset, arraySpec.stride,
                        m_spec.primitive, arraySpec.inputType, arraySpec.min, arraySpec.max, gridSize);
                }
                else
                {
                    DE_ASSERT(arraySpec.offset == 0); // \note [jarkko] it just hasn't been implemented
                    data = RandomArrayGenerator::generatePerQuad(seed, m_spec.drawCount, arraySpec.componentCount,
                                                                 arraySpec.stride, m_spec.primitive,
                                                                 arraySpec.inputType, arraySpec.min, arraySpec.max);
                }
                break;

            default:
                DE_ASSERT(false);
                break;
            }

            m_glArrayPack->newArray(arraySpec.storage);
            m_rrArrayPack->newArray(arraySpec.storage);

            m_glArrayPack->getArray(arrayNdx)->data(Array::TARGET_ARRAY, (int)bufferSize, data, arraySpec.usage);
            m_rrArrayPack->getArray(arrayNdx)->data(Array::TARGET_ARRAY, (int)bufferSize, data, arraySpec.usage);

            m_glArrayPack->getArray(arrayNdx)->bind(arrayNdx, arraySpec.offset, arraySpec.componentCount,
                                                    arraySpec.inputType, arraySpec.outputType, arraySpec.normalize,
                                                    arraySpec.stride);
            m_rrArrayPack->getArray(arrayNdx)->bind(arrayNdx, arraySpec.offset, arraySpec.componentCount,
                                                    arraySpec.inputType, arraySpec.outputType, arraySpec.normalize,
                                                    arraySpec.stride);

            delete[] data;
        }

        try
        {
            m_glArrayPack->render(m_spec.primitive, m_spec.first, m_spec.drawCount * (int)primitiveSize, useVao,
                                  coordScale, colorScale);
            m_testCtx.touchWatchdog();
            m_rrArrayPack->render(m_spec.primitive, m_spec.first, m_spec.drawCount * (int)primitiveSize, useVao,
                                  coordScale, colorScale);
        }
        catch (glu::Error &err)
        {
            // GL Errors are ok if the mode is not properly aligned

            m_testCtx.getLog() << TestLog::Message << "Got error: " << err.what() << TestLog::EndMessage;

            if (isUnalignedBufferOffsetTest())
                m_testCtx.setTestResult(QP_TEST_RESULT_COMPATIBILITY_WARNING, "Failed to draw with unaligned buffers.");
            else if (isUnalignedBufferStrideTest())
                m_testCtx.setTestResult(QP_TEST_RESULT_COMPATIBILITY_WARNING, "Failed to draw with unaligned stride.");
            else
                throw;

            return STOP;
        }

        m_iteration++;
        return CONTINUE;
    }
    else if (m_iteration == 1)
    {
        compare();

        if (m_isOk)
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        }
        else
        {
            if (isUnalignedBufferOffsetTest())
                m_testCtx.setTestResult(QP_TEST_RESULT_COMPATIBILITY_WARNING, "Failed to draw with unaligned buffers.");
            else if (isUnalignedBufferStrideTest())
                m_testCtx.setTestResult(QP_TEST_RESULT_COMPATIBILITY_WARNING, "Failed to draw with unaligned stride.");
            else
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed.");
        }

        m_iteration++;
        return STOP;
    }
    else
    {
        DE_ASSERT(false);
        return STOP;
    }
}

bool MultiVertexArrayTest::isUnalignedBufferOffsetTest(void) const
{
    // Buffer offsets should be data type size aligned
    for (size_t i = 0; i < m_spec.arrays.size(); ++i)
    {
        if (m_spec.arrays[i].storage == Array::STORAGE_BUFFER)
        {
            const bool inputTypePacked = m_spec.arrays[i].inputType == Array::INPUTTYPE_UNSIGNED_INT_2_10_10_10 ||
                                         m_spec.arrays[i].inputType == Array::INPUTTYPE_INT_2_10_10_10;

            int dataTypeSize = Array::inputTypeSize(m_spec.arrays[i].inputType);
            if (inputTypePacked)
                dataTypeSize = 4;

            if (m_spec.arrays[i].offset % dataTypeSize != 0)
                return true;
        }
    }

    return false;
}

bool MultiVertexArrayTest::isUnalignedBufferStrideTest(void) const
{
    // Buffer strides should be data type size aligned
    for (size_t i = 0; i < m_spec.arrays.size(); ++i)
    {
        if (m_spec.arrays[i].storage == Array::STORAGE_BUFFER)
        {
            const bool inputTypePacked = m_spec.arrays[i].inputType == Array::INPUTTYPE_UNSIGNED_INT_2_10_10_10 ||
                                         m_spec.arrays[i].inputType == Array::INPUTTYPE_INT_2_10_10_10;

            int dataTypeSize = Array::inputTypeSize(m_spec.arrays[i].inputType);
            if (inputTypePacked)
                dataTypeSize = 4;

            if (m_spec.arrays[i].stride % dataTypeSize != 0)
                return true;
        }
    }

    return false;
}

} // namespace gls
} // namespace deqp
