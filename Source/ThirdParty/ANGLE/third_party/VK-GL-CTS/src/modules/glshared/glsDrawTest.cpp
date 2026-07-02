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
 * \brief Draw tests
 *//*--------------------------------------------------------------------*/

#include "glsDrawTest.hpp"

#include "deRandom.h"
#include "deRandom.hpp"
#include "deMath.h"
#include "deStringUtil.hpp"
#include "deFloat16.h"
#include "deUniquePtr.hpp"
#include "deArrayUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuPixelFormat.hpp"
#include "tcuRGBA.hpp"
#include "tcuSurface.hpp"
#include "tcuVector.hpp"
#include "tcuTestLog.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuImageCompare.hpp"
#include "tcuFloat.hpp"
#include "tcuTextureUtil.hpp"

#include "gluContextInfo.hpp"
#include "gluPixelTransfer.hpp"
#include "gluCallLogWrapper.hpp"

#include "sglrContext.hpp"
#include "sglrReferenceContext.hpp"
#include "sglrGLContext.hpp"

#include "rrGenericVector.hpp"

#include <cstring>
#include <cmath>
#include <vector>
#include <sstream>
#include <limits>
#include <cstdint>

#include "glwDefs.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gls
{
namespace
{

using tcu::TestLog;
using namespace glw; // GL types

const int MAX_RENDER_TARGET_SIZE = 512;

// Utils

static GLenum targetToGL(DrawTestSpec::Target target)
{
    static const GLenum targets[] = {
        GL_ELEMENT_ARRAY_BUFFER, // TARGET_ELEMENT_ARRAY = 0,
        GL_ARRAY_BUFFER          // TARGET_ARRAY,
    };

    return de::getSizedArrayElement<DrawTestSpec::TARGET_LAST>(targets, (int)target);
}

static GLenum usageToGL(DrawTestSpec::Usage usage)
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

    return de::getSizedArrayElement<DrawTestSpec::USAGE_LAST>(usages, (int)usage);
}

static GLenum inputTypeToGL(DrawTestSpec::InputType type)
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

    return de::getSizedArrayElement<DrawTestSpec::INPUTTYPE_LAST>(types, (int)type);
}

static std::string outputTypeToGLType(DrawTestSpec::OutputType type)
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

    return de::getSizedArrayElement<DrawTestSpec::OUTPUTTYPE_LAST>(types, (int)type);
}

static GLenum primitiveToGL(DrawTestSpec::Primitive primitive)
{
    static const GLenum primitives[] = {
        GL_POINTS,                   // PRIMITIVE_POINTS = 0,
        GL_TRIANGLES,                // PRIMITIVE_TRIANGLES,
        GL_TRIANGLE_FAN,             // PRIMITIVE_TRIANGLE_FAN,
        GL_TRIANGLE_STRIP,           // PRIMITIVE_TRIANGLE_STRIP,
        GL_LINES,                    // PRIMITIVE_LINES
        GL_LINE_STRIP,               // PRIMITIVE_LINE_STRIP
        GL_LINE_LOOP,                // PRIMITIVE_LINE_LOOP
        GL_LINES_ADJACENCY,          // PRIMITIVE_LINES_ADJACENCY
        GL_LINE_STRIP_ADJACENCY,     // PRIMITIVE_LINE_STRIP_ADJACENCY
        GL_TRIANGLES_ADJACENCY,      // PRIMITIVE_TRIANGLES_ADJACENCY
        GL_TRIANGLE_STRIP_ADJACENCY, // PRIMITIVE_TRIANGLE_STRIP_ADJACENCY
    };

    return de::getSizedArrayElement<DrawTestSpec::PRIMITIVE_LAST>(primitives, (int)primitive);
}

static uint32_t indexTypeToGL(DrawTestSpec::IndexType indexType)
{
    static const GLenum indexTypes[] = {
        GL_UNSIGNED_BYTE,  // INDEXTYPE_BYTE = 0,
        GL_UNSIGNED_SHORT, // INDEXTYPE_SHORT,
        GL_UNSIGNED_INT,   // INDEXTYPE_INT,
    };

    return de::getSizedArrayElement<DrawTestSpec::INDEXTYPE_LAST>(indexTypes, (int)indexType);
}

static bool inputTypeIsFloatType(DrawTestSpec::InputType type)
{
    if (type == DrawTestSpec::INPUTTYPE_FLOAT)
        return true;
    if (type == DrawTestSpec::INPUTTYPE_FIXED)
        return true;
    if (type == DrawTestSpec::INPUTTYPE_HALF)
        return true;
    if (type == DrawTestSpec::INPUTTYPE_DOUBLE)
        return true;
    return false;
}

static bool outputTypeIsFloatType(DrawTestSpec::OutputType type)
{
    if (type == DrawTestSpec::OUTPUTTYPE_FLOAT || type == DrawTestSpec::OUTPUTTYPE_VEC2 ||
        type == DrawTestSpec::OUTPUTTYPE_VEC3 || type == DrawTestSpec::OUTPUTTYPE_VEC4)
        return true;

    return false;
}

static bool outputTypeIsIntType(DrawTestSpec::OutputType type)
{
    if (type == DrawTestSpec::OUTPUTTYPE_INT || type == DrawTestSpec::OUTPUTTYPE_IVEC2 ||
        type == DrawTestSpec::OUTPUTTYPE_IVEC3 || type == DrawTestSpec::OUTPUTTYPE_IVEC4)
        return true;

    return false;
}

static bool outputTypeIsUintType(DrawTestSpec::OutputType type)
{
    if (type == DrawTestSpec::OUTPUTTYPE_UINT || type == DrawTestSpec::OUTPUTTYPE_UVEC2 ||
        type == DrawTestSpec::OUTPUTTYPE_UVEC3 || type == DrawTestSpec::OUTPUTTYPE_UVEC4)
        return true;

    return false;
}

static size_t getElementCount(DrawTestSpec::Primitive primitive, size_t primitiveCount)
{
    switch (primitive)
    {
    case DrawTestSpec::PRIMITIVE_POINTS:
        return primitiveCount;
    case DrawTestSpec::PRIMITIVE_TRIANGLES:
        return primitiveCount * 3;
    case DrawTestSpec::PRIMITIVE_TRIANGLE_FAN:
        return primitiveCount + 2;
    case DrawTestSpec::PRIMITIVE_TRIANGLE_STRIP:
        return primitiveCount + 2;
    case DrawTestSpec::PRIMITIVE_LINES:
        return primitiveCount * 2;
    case DrawTestSpec::PRIMITIVE_LINE_STRIP:
        return primitiveCount + 1;
    case DrawTestSpec::PRIMITIVE_LINE_LOOP:
        return (primitiveCount == 1) ? (2) : (primitiveCount);
    case DrawTestSpec::PRIMITIVE_LINES_ADJACENCY:
        return primitiveCount * 4;
    case DrawTestSpec::PRIMITIVE_LINE_STRIP_ADJACENCY:
        return primitiveCount + 3;
    case DrawTestSpec::PRIMITIVE_TRIANGLES_ADJACENCY:
        return primitiveCount * 6;
    case DrawTestSpec::PRIMITIVE_TRIANGLE_STRIP_ADJACENCY:
        return primitiveCount * 2 + 4;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

struct MethodInfo
{
    bool indexed;
    bool instanced;
    bool ranged;
    bool first;
    bool baseVertex;
    bool indirect;
};

static MethodInfo getMethodInfo(gls::DrawTestSpec::DrawMethod method)
{
    static const MethodInfo infos[] = {
        //    indexed        instanced    ranged        first        baseVertex    indirect
        {false, false, false, true, false, false}, //!< DRAWMETHOD_DRAWARRAYS,
        {false, true, false, true, false, false},  //!< DRAWMETHOD_DRAWARRAYS_INSTANCED,
        {false, true, false, true, false, true},   //!< DRAWMETHOD_DRAWARRAYS_INDIRECT,
        {true, false, false, false, false, false}, //!< DRAWMETHOD_DRAWELEMENTS,
        {true, false, true, false, false, false},  //!< DRAWMETHOD_DRAWELEMENTS_RANGED,
        {true, true, false, false, false, false},  //!< DRAWMETHOD_DRAWELEMENTS_INSTANCED,
        {true, true, false, false, true, true},    //!< DRAWMETHOD_DRAWELEMENTS_INDIRECT,
        {true, false, false, false, true, false},  //!< DRAWMETHOD_DRAWELEMENTS_BASEVERTEX,
        {true, true, false, false, true, false},   //!< DRAWMETHOD_DRAWELEMENTS_INSTANCED_BASEVERTEX,
        {true, false, true, false, true, false},   //!< DRAWMETHOD_DRAWELEMENTS_RANGED_BASEVERTEX,
    };

    return de::getSizedArrayElement<DrawTestSpec::DRAWMETHOD_LAST>(infos, (int)method);
}

template <class T>
inline static void alignmentSafeAssignment(char *dst, T val)
{
    std::memcpy(dst, &val, sizeof(T));
}

static bool checkSpecsShaderCompatible(const DrawTestSpec &a, const DrawTestSpec &b)
{
    // Only the attributes matter
    if (a.attribs.size() != b.attribs.size())
        return false;

    for (size_t ndx = 0; ndx < a.attribs.size(); ++ndx)
    {
        // Only the output type (== shader input type) matters and the usage in the shader.

        if (a.attribs[ndx].additionalPositionAttribute != b.attribs[ndx].additionalPositionAttribute)
            return false;

        // component counts need not to match
        if (outputTypeIsFloatType(a.attribs[ndx].outputType) && outputTypeIsFloatType(b.attribs[ndx].outputType))
            continue;
        if (outputTypeIsIntType(a.attribs[ndx].outputType) && outputTypeIsIntType(b.attribs[ndx].outputType))
            continue;
        if (outputTypeIsUintType(a.attribs[ndx].outputType) && outputTypeIsUintType(b.attribs[ndx].outputType))
            continue;

        return false;
    }

    return true;
}

// generate random vectors in a way that does not depend on argument evaluation order

tcu::Vec4 generateRandomVec4(de::Random &random)
{
    tcu::Vec4 retVal;

    for (int i = 0; i < 4; ++i)
        retVal[i] = random.getFloat();

    return retVal;
}

tcu::IVec4 generateRandomIVec4(de::Random &random)
{
    tcu::IVec4 retVal;

    for (int i = 0; i < 4; ++i)
        retVal[i] = random.getUint32();

    return retVal;
}

tcu::UVec4 generateRandomUVec4(de::Random &random)
{
    tcu::UVec4 retVal;

    for (int i = 0; i < 4; ++i)
        retVal[i] = random.getUint32();

    return retVal;
}

// IterationLogSectionEmitter

class IterationLogSectionEmitter
{
public:
    IterationLogSectionEmitter(tcu::TestLog &log, size_t testIteration, size_t testIterations,
                               const std::string &description, bool enabled);
    ~IterationLogSectionEmitter(void);

private:
    IterationLogSectionEmitter(const IterationLogSectionEmitter &);            // delete
    IterationLogSectionEmitter &operator=(const IterationLogSectionEmitter &); // delete

    tcu::TestLog &m_log;
    bool m_enabled;
};

IterationLogSectionEmitter::IterationLogSectionEmitter(tcu::TestLog &log, size_t testIteration, size_t testIterations,
                                                       const std::string &description, bool enabled)
    : m_log(log)
    , m_enabled(enabled)
{
    if (m_enabled)
    {
        std::ostringstream buf;
        buf << "Iteration " << (testIteration + 1) << "/" << testIterations;

        if (!description.empty())
            buf << " - " << description;

        m_log << tcu::TestLog::Section(buf.str(), buf.str());
    }
}

IterationLogSectionEmitter::~IterationLogSectionEmitter(void)
{
    if (m_enabled)
        m_log << tcu::TestLog::EndSection;
}

// GLValue

class GLValue
{
public:
    template <class Type>
    class WrappedType
    {
    public:
        static WrappedType<Type> create(Type value)
        {
            WrappedType<Type> v;
            v.m_value = value;
            return v;
        }
        inline Type getValue(void) const
        {
            return m_value;
        }

        inline WrappedType<Type> operator+(const WrappedType<Type> &other) const
        {
            return WrappedType<Type>::create((Type)(m_value + other.getValue()));
        }
        inline WrappedType<Type> operator*(const WrappedType<Type> &other) const
        {
            return WrappedType<Type>::create((Type)(m_value * other.getValue()));
        }
        inline WrappedType<Type> operator/(const WrappedType<Type> &other) const
        {
            return WrappedType<Type>::create((Type)(m_value / other.getValue()));
        }
        inline WrappedType<Type> operator-(const WrappedType<Type> &other) const
        {
            return WrappedType<Type>::create((Type)(m_value - other.getValue()));
        }

        inline WrappedType<Type> &operator+=(const WrappedType<Type> &other)
        {
            m_value += other.getValue();
            return *this;
        }
        inline WrappedType<Type> &operator*=(const WrappedType<Type> &other)
        {
            m_value *= other.getValue();
            return *this;
        }
        inline WrappedType<Type> &operator/=(const WrappedType<Type> &other)
        {
            m_value /= other.getValue();
            return *this;
        }
        inline WrappedType<Type> &operator-=(const WrappedType<Type> &other)
        {
            m_value -= other.getValue();
            return *this;
        }

        inline bool operator==(const WrappedType<Type> &other) const
        {
            return m_value == other.m_value;
        }
        inline bool operator!=(const WrappedType<Type> &other) const
        {
            return m_value != other.m_value;
        }
        inline bool operator<(const WrappedType<Type> &other) const
        {
            return m_value < other.m_value;
        }
        inline bool operator>(const WrappedType<Type> &other) const
        {
            return m_value > other.m_value;
        }
        inline bool operator<=(const WrappedType<Type> &other) const
        {
            return m_value <= other.m_value;
        }
        inline bool operator>=(const WrappedType<Type> &other) const
        {
            return m_value >= other.m_value;
        }

        inline operator Type(void) const
        {
            return m_value;
        }
        template <class T>
        inline T to(void) const
        {
            return (T)m_value;
        }

    private:
        Type m_value;
    };

    typedef WrappedType<int16_t> Short;
    typedef WrappedType<uint16_t> Ushort;

    typedef WrappedType<int8_t> Byte;
    typedef WrappedType<uint8_t> Ubyte;

    typedef WrappedType<float> Float;
    typedef WrappedType<double> Double;

    typedef WrappedType<uint32_t> Uint;

    // All operations are calculated using 64bit values to avoid signed integer overflow which is undefined.
    class Int
    {
    public:
        static Int create(int32_t value)
        {
            Int v;
            v.m_value = value;
            return v;
        }
        inline int32_t getValue(void) const
        {
            return m_value;
        }

        inline Int operator+(const Int &other) const
        {
            return Int::create((int32_t)((int64_t)m_value + (int64_t)other.getValue()));
        }
        inline Int operator*(const Int &other) const
        {
            return Int::create((int32_t)((int64_t)m_value * (int64_t)other.getValue()));
        }
        inline Int operator/(const Int &other) const
        {
            return Int::create((int32_t)((int64_t)m_value / (int64_t)other.getValue()));
        }
        inline Int operator-(const Int &other) const
        {
            return Int::create((int32_t)((int64_t)m_value - (int64_t)other.getValue()));
        }

        inline Int &operator+=(const Int &other)
        {
            m_value = (int32_t)((int64_t)m_value + (int64_t)other.getValue());
            return *this;
        }
        inline Int &operator*=(const Int &other)
        {
            m_value = (int32_t)((int64_t)m_value * (int64_t)other.getValue());
            return *this;
        }
        inline Int &operator/=(const Int &other)
        {
            m_value = (int32_t)((int64_t)m_value / (int64_t)other.getValue());
            return *this;
        }
        inline Int &operator-=(const Int &other)
        {
            m_value = (int32_t)((int64_t)m_value - (int64_t)other.getValue());
            return *this;
        }

        inline bool operator==(const Int &other) const
        {
            return m_value == other.m_value;
        }
        inline bool operator!=(const Int &other) const
        {
            return m_value != other.m_value;
        }
        inline bool operator<(const Int &other) const
        {
            return m_value < other.m_value;
        }
        inline bool operator>(const Int &other) const
        {
            return m_value > other.m_value;
        }
        inline bool operator<=(const Int &other) const
        {
            return m_value <= other.m_value;
        }
        inline bool operator>=(const Int &other) const
        {
            return m_value >= other.m_value;
        }

        inline operator int32_t(void) const
        {
            return m_value;
        }
        template <class T>
        inline T to(void) const
        {
            return (T)m_value;
        }

    private:
        int32_t m_value;
    };

    class Half
    {
    public:
        static Half create(float value)
        {
            Half h;
            h.m_value = floatToHalf(value);
            return h;
        }
        inline deFloat16 getValue(void) const
        {
            return m_value;
        }

        inline Half operator+(const Half &other) const
        {
            return create(halfToFloat(m_value) + halfToFloat(other.getValue()));
        }
        inline Half operator*(const Half &other) const
        {
            return create(halfToFloat(m_value) * halfToFloat(other.getValue()));
        }
        inline Half operator/(const Half &other) const
        {
            return create(halfToFloat(m_value) / halfToFloat(other.getValue()));
        }
        inline Half operator-(const Half &other) const
        {
            return create(halfToFloat(m_value) - halfToFloat(other.getValue()));
        }

        inline Half &operator+=(const Half &other)
        {
            m_value = floatToHalf(halfToFloat(other.getValue()) + halfToFloat(m_value));
            return *this;
        }
        inline Half &operator*=(const Half &other)
        {
            m_value = floatToHalf(halfToFloat(other.getValue()) * halfToFloat(m_value));
            return *this;
        }
        inline Half &operator/=(const Half &other)
        {
            m_value = floatToHalf(halfToFloat(other.getValue()) / halfToFloat(m_value));
            return *this;
        }
        inline Half &operator-=(const Half &other)
        {
            m_value = floatToHalf(halfToFloat(other.getValue()) - halfToFloat(m_value));
            return *this;
        }

        inline bool operator==(const Half &other) const
        {
            return m_value == other.m_value;
        }
        inline bool operator!=(const Half &other) const
        {
            return m_value != other.m_value;
        }
        inline bool operator<(const Half &other) const
        {
            return halfToFloat(m_value) < halfToFloat(other.m_value);
        }
        inline bool operator>(const Half &other) const
        {
            return halfToFloat(m_value) > halfToFloat(other.m_value);
        }
        inline bool operator<=(const Half &other) const
        {
            return halfToFloat(m_value) <= halfToFloat(other.m_value);
        }
        inline bool operator>=(const Half &other) const
        {
            return halfToFloat(m_value) >= halfToFloat(other.m_value);
        }

        template <class T>
        inline T to(void) const
        {
            return (T)halfToFloat(m_value);
        }

        inline static deFloat16 floatToHalf(float f);
        inline static float halfToFloat(deFloat16 h);

    private:
        deFloat16 m_value;
    };

    class Fixed
    {
    public:
        static Fixed create(int32_t value)
        {
            Fixed v;
            v.m_value = value;
            return v;
        }
        inline int32_t getValue(void) const
        {
            return m_value;
        }

        inline Fixed operator+(const Fixed &other) const
        {
            return create(m_value + other.getValue());
        }
        inline Fixed operator*(const Fixed &other) const
        {
            return create(m_value * other.getValue());
        }
        inline Fixed operator/(const Fixed &other) const
        {
            return create(m_value / other.getValue());
        }
        inline Fixed operator-(const Fixed &other) const
        {
            return create(m_value - other.getValue());
        }

        inline Fixed &operator+=(const Fixed &other)
        {
            m_value += other.getValue();
            return *this;
        }
        inline Fixed &operator*=(const Fixed &other)
        {
            m_value *= other.getValue();
            return *this;
        }
        inline Fixed &operator/=(const Fixed &other)
        {
            m_value /= other.getValue();
            return *this;
        }
        inline Fixed &operator-=(const Fixed &other)
        {
            m_value -= other.getValue();
            return *this;
        }

        inline bool operator==(const Fixed &other) const
        {
            return m_value == other.m_value;
        }
        inline bool operator!=(const Fixed &other) const
        {
            return m_value != other.m_value;
        }
        inline bool operator<(const Fixed &other) const
        {
            return m_value < other.m_value;
        }
        inline bool operator>(const Fixed &other) const
        {
            return m_value > other.m_value;
        }
        inline bool operator<=(const Fixed &other) const
        {
            return m_value <= other.m_value;
        }
        inline bool operator>=(const Fixed &other) const
        {
            return m_value >= other.m_value;
        }

        inline operator int32_t(void) const
        {
            return m_value;
        }
        template <class T>
        inline T to(void) const
        {
            return (T)m_value;
        }

    private:
        int32_t m_value;
    };

    // \todo [mika] This is pretty messy
    GLValue(void) : type(DrawTestSpec::INPUTTYPE_LAST)
    {
    }
    explicit GLValue(Float value) : type(DrawTestSpec::INPUTTYPE_FLOAT), fl(value)
    {
    }
    explicit GLValue(Fixed value) : type(DrawTestSpec::INPUTTYPE_FIXED), fi(value)
    {
    }
    explicit GLValue(Byte value) : type(DrawTestSpec::INPUTTYPE_BYTE), b(value)
    {
    }
    explicit GLValue(Ubyte value) : type(DrawTestSpec::INPUTTYPE_UNSIGNED_BYTE), ub(value)
    {
    }
    explicit GLValue(Short value) : type(DrawTestSpec::INPUTTYPE_SHORT), s(value)
    {
    }
    explicit GLValue(Ushort value) : type(DrawTestSpec::INPUTTYPE_UNSIGNED_SHORT), us(value)
    {
    }
    explicit GLValue(Int value) : type(DrawTestSpec::INPUTTYPE_INT), i(value)
    {
    }
    explicit GLValue(Uint value) : type(DrawTestSpec::INPUTTYPE_UNSIGNED_INT), ui(value)
    {
    }
    explicit GLValue(Half value) : type(DrawTestSpec::INPUTTYPE_HALF), h(value)
    {
    }
    explicit GLValue(Double value) : type(DrawTestSpec::INPUTTYPE_DOUBLE), d(value)
    {
    }

    float toFloat(void) const;

    static GLValue getMaxValue(DrawTestSpec::InputType type);
    static GLValue getMinValue(DrawTestSpec::InputType type);

    DrawTestSpec::InputType type;

    union
    {
        Float fl;
        Fixed fi;
        Double d;
        Byte b;
        Ubyte ub;
        Short s;
        Ushort us;
        Int i;
        Uint ui;
        Half h;
    };
};

inline deFloat16 GLValue::Half::floatToHalf(float f)
{
    // No denorm support.
    tcu::Float<uint16_t, 5, 10, 15, tcu::FLOAT_HAS_SIGN> v(f);
    DE_ASSERT(!v.isNaN() && !v.isInf());
    return v.bits();
}

inline float GLValue::Half::halfToFloat(deFloat16 h)
{
    return tcu::Float16((uint16_t)h).asFloat();
}

float GLValue::toFloat(void) const
{
    switch (type)
    {
    case DrawTestSpec::INPUTTYPE_FLOAT:
        return fl.getValue();

    case DrawTestSpec::INPUTTYPE_BYTE:
        return b.getValue();

    case DrawTestSpec::INPUTTYPE_UNSIGNED_BYTE:
        return ub.getValue();

    case DrawTestSpec::INPUTTYPE_SHORT:
        return s.getValue();

    case DrawTestSpec::INPUTTYPE_UNSIGNED_SHORT:
        return us.getValue();

    case DrawTestSpec::INPUTTYPE_FIXED:
    {
        int maxValue = 65536;
        return (float)(double(2 * fi.getValue() + 1) / (maxValue - 1));
    }

    case DrawTestSpec::INPUTTYPE_UNSIGNED_INT:
        return (float)ui.getValue();

    case DrawTestSpec::INPUTTYPE_INT:
        return (float)i.getValue();

    case DrawTestSpec::INPUTTYPE_HALF:
        return h.to<float>();

    case DrawTestSpec::INPUTTYPE_DOUBLE:
        return d.to<float>();

    default:
        DE_ASSERT(false);
        return 0.0f;
    }
}

GLValue GLValue::getMaxValue(DrawTestSpec::InputType type)
{
    GLValue rangesHi[(int)DrawTestSpec::INPUTTYPE_LAST];

    rangesHi[(int)DrawTestSpec::INPUTTYPE_FLOAT]          = GLValue(Float::create(127.0f));
    rangesHi[(int)DrawTestSpec::INPUTTYPE_DOUBLE]         = GLValue(Double::create(127.0f));
    rangesHi[(int)DrawTestSpec::INPUTTYPE_BYTE]           = GLValue(Byte::create(127));
    rangesHi[(int)DrawTestSpec::INPUTTYPE_UNSIGNED_BYTE]  = GLValue(Ubyte::create(255));
    rangesHi[(int)DrawTestSpec::INPUTTYPE_UNSIGNED_SHORT] = GLValue(Ushort::create(65530));
    rangesHi[(int)DrawTestSpec::INPUTTYPE_SHORT]          = GLValue(Short::create(32760));
    rangesHi[(int)DrawTestSpec::INPUTTYPE_FIXED]          = GLValue(Fixed::create(32760));
    rangesHi[(int)DrawTestSpec::INPUTTYPE_INT]            = GLValue(Int::create(2147483647));
    rangesHi[(int)DrawTestSpec::INPUTTYPE_UNSIGNED_INT]   = GLValue(Uint::create(4294967295u));
    rangesHi[(int)DrawTestSpec::INPUTTYPE_HALF]           = GLValue(Half::create(256.0f));

    return rangesHi[(int)type];
}

GLValue GLValue::getMinValue(DrawTestSpec::InputType type)
{
    GLValue rangesLo[(int)DrawTestSpec::INPUTTYPE_LAST];

    rangesLo[(int)DrawTestSpec::INPUTTYPE_FLOAT]          = GLValue(Float::create(-127.0f));
    rangesLo[(int)DrawTestSpec::INPUTTYPE_DOUBLE]         = GLValue(Double::create(-127.0f));
    rangesLo[(int)DrawTestSpec::INPUTTYPE_BYTE]           = GLValue(Byte::create(-127));
    rangesLo[(int)DrawTestSpec::INPUTTYPE_UNSIGNED_BYTE]  = GLValue(Ubyte::create(0));
    rangesLo[(int)DrawTestSpec::INPUTTYPE_UNSIGNED_SHORT] = GLValue(Ushort::create(0));
    rangesLo[(int)DrawTestSpec::INPUTTYPE_SHORT]          = GLValue(Short::create(-32760));
    rangesLo[(int)DrawTestSpec::INPUTTYPE_FIXED]          = GLValue(Fixed::create(-32760));
    rangesLo[(int)DrawTestSpec::INPUTTYPE_INT]            = GLValue(Int::create(-2147483647));
    rangesLo[(int)DrawTestSpec::INPUTTYPE_UNSIGNED_INT]   = GLValue(Uint::create(0));
    rangesLo[(int)DrawTestSpec::INPUTTYPE_HALF]           = GLValue(Half::create(-256.0f));

    return rangesLo[(int)type];
}

template <typename T>
struct GLValueTypeTraits;

template <>
struct GLValueTypeTraits<GLValue::Float>
{
    static const DrawTestSpec::InputType Type = DrawTestSpec::INPUTTYPE_FLOAT;
};
template <>
struct GLValueTypeTraits<GLValue::Double>
{
    static const DrawTestSpec::InputType Type = DrawTestSpec::INPUTTYPE_DOUBLE;
};
template <>
struct GLValueTypeTraits<GLValue::Byte>
{
    static const DrawTestSpec::InputType Type = DrawTestSpec::INPUTTYPE_BYTE;
};
template <>
struct GLValueTypeTraits<GLValue::Ubyte>
{
    static const DrawTestSpec::InputType Type = DrawTestSpec::INPUTTYPE_UNSIGNED_BYTE;
};
template <>
struct GLValueTypeTraits<GLValue::Ushort>
{
    static const DrawTestSpec::InputType Type = DrawTestSpec::INPUTTYPE_UNSIGNED_SHORT;
};
template <>
struct GLValueTypeTraits<GLValue::Short>
{
    static const DrawTestSpec::InputType Type = DrawTestSpec::INPUTTYPE_SHORT;
};
template <>
struct GLValueTypeTraits<GLValue::Fixed>
{
    static const DrawTestSpec::InputType Type = DrawTestSpec::INPUTTYPE_FIXED;
};
template <>
struct GLValueTypeTraits<GLValue::Int>
{
    static const DrawTestSpec::InputType Type = DrawTestSpec::INPUTTYPE_INT;
};
template <>
struct GLValueTypeTraits<GLValue::Uint>
{
    static const DrawTestSpec::InputType Type = DrawTestSpec::INPUTTYPE_UNSIGNED_INT;
};
template <>
struct GLValueTypeTraits<GLValue::Half>
{
    static const DrawTestSpec::InputType Type = DrawTestSpec::INPUTTYPE_HALF;
};

template <typename T>
inline T extractGLValue(const GLValue &v);

template <>
GLValue::Float inline extractGLValue<GLValue::Float>(const GLValue &v)
{
    return v.fl;
}
template <>
GLValue::Double inline extractGLValue<GLValue::Double>(const GLValue &v)
{
    return v.d;
}
template <>
GLValue::Byte inline extractGLValue<GLValue::Byte>(const GLValue &v)
{
    return v.b;
}
template <>
GLValue::Ubyte inline extractGLValue<GLValue::Ubyte>(const GLValue &v)
{
    return v.ub;
}
template <>
GLValue::Ushort inline extractGLValue<GLValue::Ushort>(const GLValue &v)
{
    return v.us;
}
template <>
GLValue::Short inline extractGLValue<GLValue::Short>(const GLValue &v)
{
    return v.s;
}
template <>
GLValue::Fixed inline extractGLValue<GLValue::Fixed>(const GLValue &v)
{
    return v.fi;
}
template <>
GLValue::Int inline extractGLValue<GLValue::Int>(const GLValue &v)
{
    return v.i;
}
template <>
GLValue::Uint inline extractGLValue<GLValue::Uint>(const GLValue &v)
{
    return v.ui;
}
template <>
GLValue::Half inline extractGLValue<GLValue::Half>(const GLValue &v)
{
    return v.h;
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
inline GLValue::Double getRandom(deRandom &rnd, GLValue::Double min, GLValue::Double max)
{
    if (max < min)
        return min;

    return GLValue::Double::create(min + deRandom_getFloat(&rnd) * (max.to<float>() - min.to<float>()));
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

// Minimum difference required between coordinates
template <class T>
inline T minValue(void);

template <>
inline GLValue::Float minValue(void)
{
    return GLValue::Float::create(4 * 1.0f);
}

template <>
inline GLValue::Double minValue(void)
{
    return GLValue::Double::create(4 * 1.0f);
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
    return GLValue::Fixed::create(4 * 1);
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

template <class T>
inline T abs(T val);

template <>
inline GLValue::Fixed abs(GLValue::Fixed val)
{
    return GLValue::Fixed::create(0x7FFFu & val.getValue());
}

template <>
inline GLValue::Ubyte abs(GLValue::Ubyte val)
{
    return val;
}

template <>
inline GLValue::Byte abs(GLValue::Byte val)
{
    return GLValue::Byte::create(0x7Fu & val.getValue());
}

template <>
inline GLValue::Ushort abs(GLValue::Ushort val)
{
    return val;
}

template <>
inline GLValue::Short abs(GLValue::Short val)
{
    return GLValue::Short::create(0x7FFFu & val.getValue());
}

template <>
inline GLValue::Float abs(GLValue::Float val)
{
    return GLValue::Float::create(std::fabs(val.to<float>()));
}

template <>
inline GLValue::Double abs(GLValue::Double val)
{
    return GLValue::Double::create(std::fabs(val.to<float>()));
}

template <>
inline GLValue::Uint abs(GLValue::Uint val)
{
    return val;
}

template <>
inline GLValue::Int abs(GLValue::Int val)
{
    return GLValue::Int::create(0x7FFFFFFFu & val.getValue());
}

template <>
inline GLValue::Half abs(GLValue::Half val)
{
    return GLValue::Half::create(std::fabs(val.to<float>()));
}

// AttributeArray

class AttributeArray
{
public:
    AttributeArray(DrawTestSpec::Storage storage, sglr::Context &context);
    ~AttributeArray(void);

    void data(DrawTestSpec::Target target, size_t size, const char *data, DrawTestSpec::Usage usage);
    void setupArray(bool bound, int offset, int size, DrawTestSpec::InputType inType, DrawTestSpec::OutputType outType,
                    bool normalized, int stride, int instanceDivisor, const rr::GenericVec4 &defaultAttrib,
                    bool isPositionAttr, bool bgraComponentOrder);
    void bindAttribute(uint32_t loc);
    void bindIndexArray(DrawTestSpec::Target storage);

    int getComponentCount(void) const
    {
        return m_componentCount;
    }
    DrawTestSpec::Target getTarget(void) const
    {
        return m_target;
    }
    DrawTestSpec::InputType getInputType(void) const
    {
        return m_inputType;
    }
    DrawTestSpec::OutputType getOutputType(void) const
    {
        return m_outputType;
    }
    DrawTestSpec::Storage getStorageType(void) const
    {
        return m_storage;
    }
    bool getNormalized(void) const
    {
        return m_normalize;
    }
    int getStride(void) const
    {
        return m_stride;
    }
    bool isBound(void) const
    {
        return m_bound;
    }
    bool isPositionAttribute(void) const
    {
        return m_isPositionAttr;
    }

private:
    DrawTestSpec::Storage m_storage;
    sglr::Context &m_ctx;
    uint32_t m_glBuffer;

    int m_size;
    char *m_data;
    int m_componentCount;
    bool m_bound;
    DrawTestSpec::Target m_target;
    DrawTestSpec::InputType m_inputType;
    DrawTestSpec::OutputType m_outputType;
    bool m_normalize;
    int m_stride;
    int m_offset;
    rr::GenericVec4 m_defaultAttrib;
    int m_instanceDivisor;
    bool m_isPositionAttr;
    bool m_bgraOrder;
};

AttributeArray::AttributeArray(DrawTestSpec::Storage storage, sglr::Context &context)
    : m_storage(storage)
    , m_ctx(context)
    , m_glBuffer(0)
    , m_size(0)
    , m_data(nullptr)
    , m_componentCount(1)
    , m_bound(false)
    , m_target(DrawTestSpec::TARGET_ARRAY)
    , m_inputType(DrawTestSpec::INPUTTYPE_FLOAT)
    , m_outputType(DrawTestSpec::OUTPUTTYPE_VEC4)
    , m_normalize(false)
    , m_stride(0)
    , m_offset(0)
    , m_instanceDivisor(0)
    , m_isPositionAttr(false)
    , m_bgraOrder(false)
{
    if (m_storage == DrawTestSpec::STORAGE_BUFFER)
    {
        m_ctx.genBuffers(1, &m_glBuffer);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glGenBuffers()");
    }
}

AttributeArray::~AttributeArray(void)
{
    if (m_storage == DrawTestSpec::STORAGE_BUFFER)
    {
        m_ctx.deleteBuffers(1, &m_glBuffer);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glDeleteBuffers()");
    }
    else if (m_storage == DrawTestSpec::STORAGE_USER)
        delete[] m_data;
    else
        DE_ASSERT(false);
}

void AttributeArray::data(DrawTestSpec::Target target, size_t size, const char *ptr, DrawTestSpec::Usage usage)
{
    m_size   = (int)size;
    m_target = target;

    if (m_storage == DrawTestSpec::STORAGE_BUFFER)
    {
        m_ctx.bindBuffer(targetToGL(target), m_glBuffer);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glBindBuffer()");

        m_ctx.bufferData(targetToGL(target), size, ptr, usageToGL(usage));
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glBufferData()");
    }
    else if (m_storage == DrawTestSpec::STORAGE_USER)
    {
        if (m_data)
            delete[] m_data;

        m_data = new char[size];
        std::memcpy(m_data, ptr, size);
    }
    else
        DE_ASSERT(false);
}

void AttributeArray::setupArray(bool bound, int offset, int size, DrawTestSpec::InputType inputType,
                                DrawTestSpec::OutputType outType, bool normalized, int stride, int instanceDivisor,
                                const rr::GenericVec4 &defaultAttrib, bool isPositionAttr, bool bgraComponentOrder)
{
    m_componentCount  = size;
    m_bound           = bound;
    m_inputType       = inputType;
    m_outputType      = outType;
    m_normalize       = normalized;
    m_stride          = stride;
    m_offset          = offset;
    m_defaultAttrib   = defaultAttrib;
    m_instanceDivisor = instanceDivisor;
    m_isPositionAttr  = isPositionAttr;
    m_bgraOrder       = bgraComponentOrder;
}

void AttributeArray::bindAttribute(uint32_t loc)
{
    if (!isBound())
    {
        switch (m_inputType)
        {
        case DrawTestSpec::INPUTTYPE_FLOAT:
        {
            tcu::Vec4 attr = m_defaultAttrib.get<float>();

            switch (m_componentCount)
            {
            case 1:
                m_ctx.vertexAttrib1f(loc, attr.x());
                break;
            case 2:
                m_ctx.vertexAttrib2f(loc, attr.x(), attr.y());
                break;
            case 3:
                m_ctx.vertexAttrib3f(loc, attr.x(), attr.y(), attr.z());
                break;
            case 4:
                m_ctx.vertexAttrib4f(loc, attr.x(), attr.y(), attr.z(), attr.w());
                break;
            default:
                DE_ASSERT(false);
                break;
            }
            break;
        }
        case DrawTestSpec::INPUTTYPE_INT:
        {
            tcu::IVec4 attr = m_defaultAttrib.get<int32_t>();
            m_ctx.vertexAttribI4i(loc, attr.x(), attr.y(), attr.z(), attr.w());
            break;
        }
        case DrawTestSpec::INPUTTYPE_UNSIGNED_INT:
        {
            tcu::UVec4 attr = m_defaultAttrib.get<uint32_t>();
            m_ctx.vertexAttribI4ui(loc, attr.x(), attr.y(), attr.z(), attr.w());
            break;
        }
        default:
            DE_ASSERT(false);
            break;
        }
    }
    else
    {
        const uint8_t *basePtr = nullptr;

        if (m_storage == DrawTestSpec::STORAGE_BUFFER)
        {
            m_ctx.bindBuffer(targetToGL(m_target), m_glBuffer);
            GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glBindBuffer()");

            basePtr = nullptr;
        }
        else if (m_storage == DrawTestSpec::STORAGE_USER)
        {
            m_ctx.bindBuffer(targetToGL(m_target), 0);
            GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glBindBuffer()");

            basePtr = (const uint8_t *)m_data;
        }
        else
            DE_ASSERT(false);

        if (!inputTypeIsFloatType(m_inputType))
        {
            // Input is not float type

            if (outputTypeIsFloatType(m_outputType))
            {
                const int size = (m_bgraOrder) ? (GL_BGRA) : (m_componentCount);

                DE_ASSERT(!(m_bgraOrder && m_componentCount != 4));

                // Output type is float type
                m_ctx.vertexAttribPointer(loc, size, inputTypeToGL(m_inputType), m_normalize, m_stride,
                                          basePtr ? basePtr + m_offset : reinterpret_cast<uint8_t *>(m_offset));
                GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glVertexAttribPointer()");
            }
            else
            {
                // Output type is int type
                m_ctx.vertexAttribIPointer(loc, m_componentCount, inputTypeToGL(m_inputType), m_stride,
                                           basePtr ? basePtr + m_offset : reinterpret_cast<uint8_t *>(m_offset));
                GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glVertexAttribIPointer()");
            }
        }
        else
        {
            // Input type is float type

            // Output type must be float type
            DE_ASSERT(outputTypeIsFloatType(m_outputType));

            m_ctx.vertexAttribPointer(loc, m_componentCount, inputTypeToGL(m_inputType), m_normalize, m_stride,
                                      basePtr ? basePtr + m_offset : reinterpret_cast<uint8_t *>(m_offset));
            GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glVertexAttribPointer()");
        }

        if (m_instanceDivisor)
            m_ctx.vertexAttribDivisor(loc, m_instanceDivisor);
    }
}

void AttributeArray::bindIndexArray(DrawTestSpec::Target target)
{
    if (m_storage == DrawTestSpec::STORAGE_USER)
    {
    }
    else if (m_storage == DrawTestSpec::STORAGE_BUFFER)
    {
        m_ctx.bindBuffer(targetToGL(target), m_glBuffer);
    }
}

// DrawTestShaderProgram

class DrawTestShaderProgram : public sglr::ShaderProgram
{
public:
    DrawTestShaderProgram(const glu::RenderContext &ctx, const std::vector<AttributeArray *> &arrays);

    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const;
    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const;

private:
    static std::string genVertexSource(const glu::RenderContext &ctx, const std::vector<AttributeArray *> &arrays);
    static std::string genFragmentSource(const glu::RenderContext &ctx);
    static void generateShaderParams(std::map<std::string, std::string> &params, glu::ContextType type);
    static rr::GenericVecType mapOutputType(const DrawTestSpec::OutputType &type);
    static int getComponentCount(const DrawTestSpec::OutputType &type);

    static sglr::pdec::ShaderProgramDeclaration createProgramDeclaration(const glu::RenderContext &ctx,
                                                                         const std::vector<AttributeArray *> &arrays);

    std::vector<int> m_componentCount;
    std::vector<bool> m_isCoord;
    std::vector<rr::GenericVecType> m_attrType;
};

DrawTestShaderProgram::DrawTestShaderProgram(const glu::RenderContext &ctx, const std::vector<AttributeArray *> &arrays)
    : sglr::ShaderProgram(createProgramDeclaration(ctx, arrays))
    , m_componentCount(arrays.size())
    , m_isCoord(arrays.size())
    , m_attrType(arrays.size())
{
    for (int arrayNdx = 0; arrayNdx < (int)arrays.size(); arrayNdx++)
    {
        m_componentCount[arrayNdx] = getComponentCount(arrays[arrayNdx]->getOutputType());
        m_isCoord[arrayNdx]        = arrays[arrayNdx]->isPositionAttribute();
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
            coord += tcu::Vec2((float)attribValue.x(), (float)attribValue.x());
            break;
        case 2:
            coord += tcu::Vec2((float)attribValue.x(), (float)attribValue.y());
            break;
        case 3:
            coord += tcu::Vec2((float)attribValue.x() + (float)attribValue.z(), (float)attribValue.y());
            break;
        case 4:
            coord += tcu::Vec2((float)attribValue.x() + (float)attribValue.z(),
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

void DrawTestShaderProgram::shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                                          const int numPackets) const
{
    const float u_coordScale = getUniformByName("u_coordScale").value.f;
    const float u_colorScale = getUniformByName("u_colorScale").value.f;

    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        const size_t varyingLocColor = 0;

        rr::VertexPacket &packet = *packets[packetNdx];

        // Calc output color
        tcu::Vec2 coord = tcu::Vec2(0.0, 0.0);
        tcu::Vec3 color = tcu::Vec3(1.0, 1.0, 1.0);

        for (int attribNdx = 0; attribNdx < (int)m_attrType.size(); attribNdx++)
        {
            const int numComponents = m_componentCount[attribNdx];
            const bool isCoord      = m_isCoord[attribNdx];

            switch (m_attrType[attribNdx])
            {
            case rr::GENERICVECTYPE_FLOAT:
                calcShaderColorCoord(coord, color,
                                     rr::readVertexAttribFloat(inputs[attribNdx], packet.instanceNdx, packet.vertexNdx),
                                     isCoord, numComponents);
                break;
            case rr::GENERICVECTYPE_INT32:
                calcShaderColorCoord(coord, color,
                                     rr::readVertexAttribInt(inputs[attribNdx], packet.instanceNdx, packet.vertexNdx),
                                     isCoord, numComponents);
                break;
            case rr::GENERICVECTYPE_UINT32:
                calcShaderColorCoord(coord, color,
                                     rr::readVertexAttribUint(inputs[attribNdx], packet.instanceNdx, packet.vertexNdx),
                                     isCoord, numComponents);
                break;
            default:
                DE_ASSERT(false);
            }
        }

        // Transform position
        {
            packet.position  = tcu::Vec4(u_coordScale * coord.x(), u_coordScale * coord.y(), 1.0f, 1.0f);
            packet.pointSize = 1.0f;
        }

        // Pass color to FS
        {
            packet.outputs[varyingLocColor] =
                tcu::Vec4(u_colorScale * color.x(), u_colorScale * color.y(), u_colorScale * color.z(), 1.0f) * 0.5f +
                tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f);
        }
    }
}

void DrawTestShaderProgram::shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                           const rr::FragmentShadingContext &context) const
{
    const size_t varyingLocColor = 0;

    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
    {
        rr::FragmentPacket &packet = packets[packetNdx];

        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
            rr::writeFragmentOutput(context, packetNdx, fragNdx, 0,
                                    rr::readVarying<float>(packet, context, varyingLocColor, fragNdx));
    }
}

std::string DrawTestShaderProgram::genVertexSource(const glu::RenderContext &ctx,
                                                   const std::vector<AttributeArray *> &arrays)
{
    std::map<std::string, std::string> params;
    std::stringstream vertexShaderTmpl;

    generateShaderParams(params, ctx.getType());

    vertexShaderTmpl << "${VTX_HDR}";

    for (int arrayNdx = 0; arrayNdx < (int)arrays.size(); arrayNdx++)
    {
        vertexShaderTmpl << "${VTX_IN} highp " << outputTypeToGLType(arrays[arrayNdx]->getOutputType()) << " a_"
                         << arrayNdx << ";\n";
    }

    vertexShaderTmpl << "uniform highp float u_coordScale;\n"
                        "uniform highp float u_colorScale;\n"
                        "${VTX_OUT} ${COL_PRECISION} vec4 v_color;\n"
                        "void main(void)\n"
                        "{\n"
                        "\tgl_PointSize = 1.0;\n"
                        "\thighp vec2 coord = vec2(0.0, 0.0);\n"
                        "\thighp vec3 color = vec3(1.0, 1.0, 1.0);\n";

    for (int arrayNdx = 0; arrayNdx < (int)arrays.size(); arrayNdx++)
    {
        const bool isPositionAttr = arrays[arrayNdx]->isPositionAttribute();

        if (isPositionAttr)
        {
            switch (arrays[arrayNdx]->getOutputType())
            {
            case (DrawTestSpec::OUTPUTTYPE_FLOAT):
            case (DrawTestSpec::OUTPUTTYPE_INT):
            case (DrawTestSpec::OUTPUTTYPE_UINT):
                vertexShaderTmpl << "\tcoord += vec2(float(a_" << arrayNdx << "), float(a_" << arrayNdx << "));\n";
                break;

            case (DrawTestSpec::OUTPUTTYPE_VEC2):
            case (DrawTestSpec::OUTPUTTYPE_IVEC2):
            case (DrawTestSpec::OUTPUTTYPE_UVEC2):
                vertexShaderTmpl << "\tcoord += vec2(a_" << arrayNdx << ".xy);\n";
                break;

            case (DrawTestSpec::OUTPUTTYPE_VEC3):
            case (DrawTestSpec::OUTPUTTYPE_IVEC3):
            case (DrawTestSpec::OUTPUTTYPE_UVEC3):
                vertexShaderTmpl << "\tcoord += vec2(a_" << arrayNdx
                                 << ".xy);\n"
                                    "\tcoord.x += float(a_"
                                 << arrayNdx << ".z);\n";
                break;

            case (DrawTestSpec::OUTPUTTYPE_VEC4):
            case (DrawTestSpec::OUTPUTTYPE_IVEC4):
            case (DrawTestSpec::OUTPUTTYPE_UVEC4):
                vertexShaderTmpl << "\tcoord += vec2(a_" << arrayNdx
                                 << ".xy);\n"
                                    "\tcoord += vec2(a_"
                                 << arrayNdx << ".zw);\n";
                break;

            default:
                DE_ASSERT(false);
                break;
            }
        }
        else
        {
            switch (arrays[arrayNdx]->getOutputType())
            {
            case (DrawTestSpec::OUTPUTTYPE_FLOAT):
            case (DrawTestSpec::OUTPUTTYPE_INT):
            case (DrawTestSpec::OUTPUTTYPE_UINT):
                vertexShaderTmpl << "\tcolor = color * float(a_" << arrayNdx << ");\n";
                break;

            case (DrawTestSpec::OUTPUTTYPE_VEC2):
            case (DrawTestSpec::OUTPUTTYPE_IVEC2):
            case (DrawTestSpec::OUTPUTTYPE_UVEC2):
                vertexShaderTmpl << "\tcolor.rg = color.rg * vec2(a_" << arrayNdx << ".xy);\n";
                break;

            case (DrawTestSpec::OUTPUTTYPE_VEC3):
            case (DrawTestSpec::OUTPUTTYPE_IVEC3):
            case (DrawTestSpec::OUTPUTTYPE_UVEC3):
                vertexShaderTmpl << "\tcolor = color.rgb * vec3(a_" << arrayNdx << ".xyz);\n";
                break;

            case (DrawTestSpec::OUTPUTTYPE_VEC4):
            case (DrawTestSpec::OUTPUTTYPE_IVEC4):
            case (DrawTestSpec::OUTPUTTYPE_UVEC4):
                vertexShaderTmpl << "\tcolor = color.rgb * vec3(a_" << arrayNdx << ".xyz) * float(a_" << arrayNdx
                                 << ".w);\n";
                break;

            default:
                DE_ASSERT(false);
                break;
            }
        }
    }

    vertexShaderTmpl << "\tv_color = vec4(u_colorScale * color, 1.0) * 0.5 + vec4(0.5, 0.5, 0.5, 0.5);\n"
                        "\tgl_Position = vec4(u_coordScale * coord, 1.0, 1.0);\n"
                        "}\n";

    return tcu::StringTemplate(vertexShaderTmpl.str().c_str()).specialize(params);
}

std::string DrawTestShaderProgram::genFragmentSource(const glu::RenderContext &ctx)
{
    std::map<std::string, std::string> params;

    generateShaderParams(params, ctx.getType());

    static const char *fragmentShaderTmpl = "${FRAG_HDR}"
                                            "${FRAG_IN} ${COL_PRECISION} vec4 v_color;\n"
                                            "void main(void)\n"
                                            "{\n"
                                            "\t${FRAG_COLOR} = v_color;\n"
                                            "}\n";

    return tcu::StringTemplate(fragmentShaderTmpl).specialize(params);
}

void DrawTestShaderProgram::generateShaderParams(std::map<std::string, std::string> &params, glu::ContextType type)
{
    if (glu::isGLSLVersionSupported(type, glu::GLSL_VERSION_300_ES))
    {
        params["VTX_IN"]        = "in";
        params["VTX_OUT"]       = "out";
        params["FRAG_IN"]       = "in";
        params["FRAG_COLOR"]    = "dEQP_FragColor";
        params["VTX_HDR"]       = "#version 300 es\n";
        params["FRAG_HDR"]      = "#version 300 es\nlayout(location = 0) out mediump vec4 dEQP_FragColor;\n";
        params["COL_PRECISION"] = "mediump";
    }
    else if (glu::isGLSLVersionSupported(type, glu::GLSL_VERSION_100_ES))
    {
        params["VTX_IN"]        = "attribute";
        params["VTX_OUT"]       = "varying";
        params["FRAG_IN"]       = "varying";
        params["FRAG_COLOR"]    = "gl_FragColor";
        params["VTX_HDR"]       = "";
        params["FRAG_HDR"]      = "";
        params["COL_PRECISION"] = "mediump";
    }
    else if (glu::isGLSLVersionSupported(type, glu::GLSL_VERSION_430))
    {
        params["VTX_IN"]        = "in";
        params["VTX_OUT"]       = "out";
        params["FRAG_IN"]       = "in";
        params["FRAG_COLOR"]    = "dEQP_FragColor";
        params["VTX_HDR"]       = "#version 430\n";
        params["FRAG_HDR"]      = "#version 430\nlayout(location = 0) out highp vec4 dEQP_FragColor;\n";
        params["COL_PRECISION"] = "highp";
    }
    else if (glu::isGLSLVersionSupported(type, glu::GLSL_VERSION_330))
    {
        params["VTX_IN"]        = "in";
        params["VTX_OUT"]       = "out";
        params["FRAG_IN"]       = "in";
        params["FRAG_COLOR"]    = "dEQP_FragColor";
        params["VTX_HDR"]       = "#version 330\n";
        params["FRAG_HDR"]      = "#version 330\nlayout(location = 0) out mediump vec4 dEQP_FragColor;\n";
        params["COL_PRECISION"] = "mediump";
    }
    else
        DE_ASSERT(false);
}

rr::GenericVecType DrawTestShaderProgram::mapOutputType(const DrawTestSpec::OutputType &type)
{
    switch (type)
    {
    case (DrawTestSpec::OUTPUTTYPE_FLOAT):
    case (DrawTestSpec::OUTPUTTYPE_VEC2):
    case (DrawTestSpec::OUTPUTTYPE_VEC3):
    case (DrawTestSpec::OUTPUTTYPE_VEC4):
        return rr::GENERICVECTYPE_FLOAT;

    case (DrawTestSpec::OUTPUTTYPE_INT):
    case (DrawTestSpec::OUTPUTTYPE_IVEC2):
    case (DrawTestSpec::OUTPUTTYPE_IVEC3):
    case (DrawTestSpec::OUTPUTTYPE_IVEC4):
        return rr::GENERICVECTYPE_INT32;

    case (DrawTestSpec::OUTPUTTYPE_UINT):
    case (DrawTestSpec::OUTPUTTYPE_UVEC2):
    case (DrawTestSpec::OUTPUTTYPE_UVEC3):
    case (DrawTestSpec::OUTPUTTYPE_UVEC4):
        return rr::GENERICVECTYPE_UINT32;

    default:
        DE_ASSERT(false);
        return rr::GENERICVECTYPE_LAST;
    }
}

int DrawTestShaderProgram::getComponentCount(const DrawTestSpec::OutputType &type)
{
    switch (type)
    {
    case (DrawTestSpec::OUTPUTTYPE_FLOAT):
    case (DrawTestSpec::OUTPUTTYPE_INT):
    case (DrawTestSpec::OUTPUTTYPE_UINT):
        return 1;

    case (DrawTestSpec::OUTPUTTYPE_VEC2):
    case (DrawTestSpec::OUTPUTTYPE_IVEC2):
    case (DrawTestSpec::OUTPUTTYPE_UVEC2):
        return 2;

    case (DrawTestSpec::OUTPUTTYPE_VEC3):
    case (DrawTestSpec::OUTPUTTYPE_IVEC3):
    case (DrawTestSpec::OUTPUTTYPE_UVEC3):
        return 3;

    case (DrawTestSpec::OUTPUTTYPE_VEC4):
    case (DrawTestSpec::OUTPUTTYPE_IVEC4):
    case (DrawTestSpec::OUTPUTTYPE_UVEC4):
        return 4;

    default:
        DE_ASSERT(false);
        return 0;
    }
}

sglr::pdec::ShaderProgramDeclaration DrawTestShaderProgram::createProgramDeclaration(
    const glu::RenderContext &ctx, const std::vector<AttributeArray *> &arrays)
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

class RandomArrayGenerator
{
public:
    static char *generateArray(int seed, int elementCount, int componentCount, int offset, int stride,
                               DrawTestSpec::InputType type);
    static char *generateIndices(int seed, int elementCount, DrawTestSpec::IndexType type, int offset, int min, int max,
                                 int indexBase);
    static rr::GenericVec4 generateAttributeValue(int seed, DrawTestSpec::InputType type);

private:
    template <typename T>
    static char *createIndices(int seed, int elementCount, int offset, int min, int max, int indexBase);

    static char *generateBasicArray(int seed, int elementCount, int componentCount, int offset, int stride,
                                    DrawTestSpec::InputType type);
    template <typename T, typename GLType>
    static char *createBasicArray(int seed, int elementCount, int componentCount, int offset, int stride);
    static char *generatePackedArray(int seed, int elementCount, int componentCount, int offset, int stride);
};

char *RandomArrayGenerator::generateArray(int seed, int elementCount, int componentCount, int offset, int stride,
                                          DrawTestSpec::InputType type)
{
    if (type == DrawTestSpec::INPUTTYPE_INT_2_10_10_10 || type == DrawTestSpec::INPUTTYPE_UNSIGNED_INT_2_10_10_10)
        return generatePackedArray(seed, elementCount, componentCount, offset, stride);
    else
        return generateBasicArray(seed, elementCount, componentCount, offset, stride, type);
}

char *RandomArrayGenerator::generateBasicArray(int seed, int elementCount, int componentCount, int offset, int stride,
                                               DrawTestSpec::InputType type)
{
    switch (type)
    {
    case DrawTestSpec::INPUTTYPE_FLOAT:
        return createBasicArray<float, GLValue::Float>(seed, elementCount, componentCount, offset, stride);
    case DrawTestSpec::INPUTTYPE_DOUBLE:
        return createBasicArray<double, GLValue::Double>(seed, elementCount, componentCount, offset, stride);
    case DrawTestSpec::INPUTTYPE_SHORT:
        return createBasicArray<int16_t, GLValue::Short>(seed, elementCount, componentCount, offset, stride);
    case DrawTestSpec::INPUTTYPE_UNSIGNED_SHORT:
        return createBasicArray<uint16_t, GLValue::Ushort>(seed, elementCount, componentCount, offset, stride);
    case DrawTestSpec::INPUTTYPE_BYTE:
        return createBasicArray<int8_t, GLValue::Byte>(seed, elementCount, componentCount, offset, stride);
    case DrawTestSpec::INPUTTYPE_UNSIGNED_BYTE:
        return createBasicArray<uint8_t, GLValue::Ubyte>(seed, elementCount, componentCount, offset, stride);
    case DrawTestSpec::INPUTTYPE_FIXED:
        return createBasicArray<int32_t, GLValue::Fixed>(seed, elementCount, componentCount, offset, stride);
    case DrawTestSpec::INPUTTYPE_INT:
        return createBasicArray<int32_t, GLValue::Int>(seed, elementCount, componentCount, offset, stride);
    case DrawTestSpec::INPUTTYPE_UNSIGNED_INT:
        return createBasicArray<uint32_t, GLValue::Uint>(seed, elementCount, componentCount, offset, stride);
    case DrawTestSpec::INPUTTYPE_HALF:
        return createBasicArray<deFloat16, GLValue::Half>(seed, elementCount, componentCount, offset, stride);
    default:
        DE_ASSERT(false);
        break;
    }
    return nullptr;
}

#if (DE_COMPILER == DE_COMPILER_GCC) && (__GNUC__ == 4) && (__GNUC_MINOR__ >= 8)
// GCC 4.8/4.9 incorrectly emits array-bounds warning from createBasicArray()
#define GCC_ARRAY_BOUNDS_FALSE_NEGATIVE 1
#endif

#if defined(GCC_ARRAY_BOUNDS_FALSE_NEGATIVE)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

template <typename T, typename GLType>
char *RandomArrayGenerator::createBasicArray(int seed, int elementCount, int componentCount, int offset, int stride)
{
    DE_ASSERT(componentCount >= 1 && componentCount <= 4);
    // Make the bound explicit so GCC 14 doesn't emit false-positive -Wstringop-overflow on the writes below.
    if (componentCount > 4)
        componentCount = 4;

    const GLType min = extractGLValue<GLType>(GLValue::getMinValue(GLValueTypeTraits<GLType>::Type));
    const GLType max = extractGLValue<GLType>(GLValue::getMaxValue(GLValueTypeTraits<GLType>::Type));

    const size_t componentSize = sizeof(T);
    const size_t elementSize   = componentSize * componentCount;
    const size_t bufferSize    = offset + (elementCount - 1) * stride + elementSize;

    char *data     = new char[bufferSize];
    char *writePtr = data + offset;

    GLType previousComponents[4];

    deRandom rnd;
    deRandom_init(&rnd, seed);

    for (int vertexNdx = 0; vertexNdx < elementCount; vertexNdx++)
    {
        GLType components[4];

        for (int componentNdx = 0; componentNdx < componentCount; componentNdx++)
        {
            components[componentNdx] = getRandom<GLType>(rnd, min, max);

            // Try to not create vertex near previous
            if (vertexNdx != 0 && abs(components[componentNdx] - previousComponents[componentNdx]) < minValue<GLType>())
            {
                // Too close, try again (but only once)
                components[componentNdx] = getRandom<GLType>(rnd, min, max);
            }
        }

        for (int componentNdx = 0; componentNdx < componentCount; componentNdx++)
            previousComponents[componentNdx] = components[componentNdx];

        for (int componentNdx = 0; componentNdx < componentCount; componentNdx++)
            alignmentSafeAssignment(writePtr + componentNdx * componentSize, components[componentNdx].getValue());

        writePtr += stride;
    }

    return data;
}

#if defined(GCC_ARRAY_BOUNDS_FALSE_NEGATIVE)
#pragma GCC diagnostic pop
#endif

char *RandomArrayGenerator::generatePackedArray(int seed, int elementCount, int componentCount, int offset, int stride)
{
    DE_ASSERT(componentCount == 4);
    DE_UNREF(componentCount);

    const uint32_t limit10   = (1 << 10);
    const uint32_t limit2    = (1 << 2);
    const size_t elementSize = 4;
    const size_t bufferSize  = offset + (elementCount - 1) * stride + elementSize;

    char *data     = new char[bufferSize];
    char *writePtr = data + offset;

    deRandom rnd;
    deRandom_init(&rnd, seed);

    for (int vertexNdx = 0; vertexNdx < elementCount; vertexNdx++)
    {
        const uint32_t x           = deRandom_getUint32(&rnd) % limit10;
        const uint32_t y           = deRandom_getUint32(&rnd) % limit10;
        const uint32_t z           = deRandom_getUint32(&rnd) % limit10;
        const uint32_t w           = deRandom_getUint32(&rnd) % limit2;
        const uint32_t packedValue = (w << 30) | (z << 20) | (y << 10) | (x);

        alignmentSafeAssignment(writePtr, packedValue);
        writePtr += stride;
    }

    return data;
}

char *RandomArrayGenerator::generateIndices(int seed, int elementCount, DrawTestSpec::IndexType type, int offset,
                                            int min, int max, int indexBase)
{
    char *data = nullptr;

    switch (type)
    {
    case DrawTestSpec::INDEXTYPE_BYTE:
        data = createIndices<uint8_t>(seed, elementCount, offset, min, max, indexBase);
        break;

    case DrawTestSpec::INDEXTYPE_SHORT:
        data = createIndices<uint16_t>(seed, elementCount, offset, min, max, indexBase);
        break;

    case DrawTestSpec::INDEXTYPE_INT:
        data = createIndices<uint32_t>(seed, elementCount, offset, min, max, indexBase);
        break;

    default:
        DE_ASSERT(false);
        break;
    }

    return data;
}

template <typename T>
char *RandomArrayGenerator::createIndices(int seed, int elementCount, int offset, int min, int max, int indexBase)
{
    const size_t elementSize = sizeof(T);
    const size_t bufferSize  = offset + elementCount * elementSize;

    char *data     = new char[bufferSize];
    char *writePtr = data + offset;

    uint32_t oldNdx1 = uint32_t(-1);
    uint32_t oldNdx2 = uint32_t(-1);

    deRandom rnd;
    deRandom_init(&rnd, seed);

    DE_ASSERT(indexBase >= 0); // watch for underflows

    if (min < 0 || (size_t)min > std::numeric_limits<T>::max() || max < 0 ||
        (size_t)max > std::numeric_limits<T>::max() || min > max)
        DE_FATAL("Invalid range");

    for (int elementNdx = 0; elementNdx < elementCount; ++elementNdx)
    {
        uint32_t ndx = getRandom(rnd, GLValue::Uint::create(min), GLValue::Uint::create(max)).getValue();

        // Try not to generate same index as any of previous two. This prevents
        // generation of degenerate triangles and lines. If [min, max] is too
        // small this cannot be guaranteed.

        if (ndx == oldNdx1)
            ++ndx;
        if (ndx > (uint32_t)max)
            ndx = min;
        if (ndx == oldNdx2)
            ++ndx;
        if (ndx > (uint32_t)max)
            ndx = min;
        if (ndx == oldNdx1)
            ++ndx;
        if (ndx > (uint32_t)max)
            ndx = min;

        oldNdx2 = oldNdx1;
        oldNdx1 = ndx;

        ndx += indexBase;

        alignmentSafeAssignment<T>(writePtr + elementSize * elementNdx, T(ndx));
    }

    return data;
}

rr::GenericVec4 RandomArrayGenerator::generateAttributeValue(int seed, DrawTestSpec::InputType type)
{
    de::Random random(seed);

    switch (type)
    {
    case DrawTestSpec::INPUTTYPE_FLOAT:
        return rr::GenericVec4(generateRandomVec4(random));

    case DrawTestSpec::INPUTTYPE_INT:
        return rr::GenericVec4(generateRandomIVec4(random));

    case DrawTestSpec::INPUTTYPE_UNSIGNED_INT:
        return rr::GenericVec4(generateRandomUVec4(random));

    default:
        DE_ASSERT(false);
        return rr::GenericVec4(tcu::Vec4(1, 1, 1, 1));
    }
}

} // namespace

// AttributePack

class AttributePack
{
public:
    AttributePack(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, sglr::Context &drawContext,
                  const tcu::UVec2 &screenSize, bool useVao, bool logEnabled);
    ~AttributePack(void);

    AttributeArray *getArray(int i);
    int getArrayCount(void);

    void newArray(DrawTestSpec::Storage storage);
    void clearArrays(void);
    void updateProgram(void);

    void render(DrawTestSpec::Primitive primitive, DrawTestSpec::DrawMethod drawMethod, int firstVertex,
                int vertexCount, DrawTestSpec::IndexType indexType, const void *indexOffset, int rangeStart,
                int rangeEnd, int instanceCount, int indirectOffset, int baseVertex, float coordScale, float colorScale,
                AttributeArray *indexArray);

    const tcu::Surface &getSurface(void) const
    {
        return m_screen;
    }

private:
    tcu::TestContext &m_testCtx;
    glu::RenderContext &m_renderCtx;
    sglr::Context &m_ctx;

    std::vector<AttributeArray *> m_arrays;
    sglr::ShaderProgram *m_program;
    tcu::Surface m_screen;
    const bool m_useVao;
    const bool m_logEnabled;
    uint32_t m_programID;
    uint32_t m_vaoID;
};

AttributePack::AttributePack(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, sglr::Context &drawContext,
                             const tcu::UVec2 &screenSize, bool useVao, bool logEnabled)
    : m_testCtx(testCtx)
    , m_renderCtx(renderCtx)
    , m_ctx(drawContext)
    , m_program(nullptr)
    , m_screen(screenSize.x(), screenSize.y())
    , m_useVao(useVao)
    , m_logEnabled(logEnabled)
    , m_programID(0)
    , m_vaoID(0)
{
    if (m_useVao)
        m_ctx.genVertexArrays(1, &m_vaoID);
}

AttributePack::~AttributePack(void)
{
    clearArrays();

    if (m_programID)
        m_ctx.deleteProgram(m_programID);

    if (m_program)
        delete m_program;

    if (m_useVao)
        m_ctx.deleteVertexArrays(1, &m_vaoID);
}

AttributeArray *AttributePack::getArray(int i)
{
    return m_arrays.at(i);
}

int AttributePack::getArrayCount(void)
{
    return (int)m_arrays.size();
}

void AttributePack::newArray(DrawTestSpec::Storage storage)
{
    m_arrays.push_back(new AttributeArray(storage, m_ctx));
}

void AttributePack::clearArrays(void)
{
    for (std::vector<AttributeArray *>::iterator itr = m_arrays.begin(); itr != m_arrays.end(); itr++)
        delete *itr;
    m_arrays.clear();
}

void AttributePack::updateProgram(void)
{
    if (m_programID)
        m_ctx.deleteProgram(m_programID);
    if (m_program)
        delete m_program;

    m_program   = new DrawTestShaderProgram(m_renderCtx, m_arrays);
    m_programID = m_ctx.createProgram(m_program);
}

void AttributePack::render(DrawTestSpec::Primitive primitive, DrawTestSpec::DrawMethod drawMethod, int firstVertex,
                           int vertexCount, DrawTestSpec::IndexType indexType, const void *indexOffset, int rangeStart,
                           int rangeEnd, int instanceCount, int indirectOffset, int baseVertex, float coordScale,
                           float colorScale, AttributeArray *indexArray)
{
    DE_ASSERT(m_program != nullptr);
    DE_ASSERT(m_programID != 0);

    m_ctx.viewport(0, 0, m_screen.getWidth(), m_screen.getHeight());
    m_ctx.clearColor(0.0, 0.0, 0.0, 1.0);
    m_ctx.clear(GL_COLOR_BUFFER_BIT);

    m_ctx.useProgram(m_programID);
    GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glUseProgram()");

    m_ctx.uniform1f(m_ctx.getUniformLocation(m_programID, "u_coordScale"), coordScale);
    m_ctx.uniform1f(m_ctx.getUniformLocation(m_programID, "u_colorScale"), colorScale);

    if (m_useVao)
        m_ctx.bindVertexArray(m_vaoID);

    if (indexArray)
        indexArray->bindIndexArray(DrawTestSpec::TARGET_ELEMENT_ARRAY);

    for (int arrayNdx = 0; arrayNdx < (int)m_arrays.size(); arrayNdx++)
    {
        std::stringstream attribName;
        attribName << "a_" << arrayNdx;

        uint32_t loc = m_ctx.getAttribLocation(m_programID, attribName.str().c_str());

        if (m_arrays[arrayNdx]->isBound())
        {
            m_ctx.enableVertexAttribArray(loc);
            GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glEnableVertexAttribArray()");
        }

        m_arrays[arrayNdx]->bindAttribute(loc);
    }

    if (drawMethod == DrawTestSpec::DRAWMETHOD_DRAWARRAYS)
    {
        m_ctx.drawArrays(primitiveToGL(primitive), firstVertex, vertexCount);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glDrawArrays()");
    }
    else if (drawMethod == DrawTestSpec::DRAWMETHOD_DRAWARRAYS_INSTANCED)
    {
        m_ctx.drawArraysInstanced(primitiveToGL(primitive), firstVertex, vertexCount, instanceCount);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glDrawArraysInstanced()");
    }
    else if (drawMethod == DrawTestSpec::DRAWMETHOD_DRAWELEMENTS)
    {
        m_ctx.drawElements(primitiveToGL(primitive), vertexCount, indexTypeToGL(indexType), indexOffset);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glDrawElements()");
    }
    else if (drawMethod == DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_RANGED)
    {
        m_ctx.drawRangeElements(primitiveToGL(primitive), rangeStart, rangeEnd, vertexCount, indexTypeToGL(indexType),
                                indexOffset);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glDrawRangeElements()");
    }
    else if (drawMethod == DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_INSTANCED)
    {
        m_ctx.drawElementsInstanced(primitiveToGL(primitive), vertexCount, indexTypeToGL(indexType), indexOffset,
                                    instanceCount);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glDrawElementsInstanced()");
    }
    else if (drawMethod == DrawTestSpec::DRAWMETHOD_DRAWARRAYS_INDIRECT)
    {
        struct DrawCommand
        {
            GLuint count;
            GLuint primCount;
            GLuint first;
            GLuint reservedMustBeZero;
        };
        uint8_t *buffer = new uint8_t[sizeof(DrawCommand) + indirectOffset];

        {
            DrawCommand command;

            command.count              = vertexCount;
            command.primCount          = instanceCount;
            command.first              = firstVertex;
            command.reservedMustBeZero = 0;

            memcpy(buffer + indirectOffset, &command, sizeof(command));

            if (m_logEnabled)
                m_testCtx.getLog() << tcu::TestLog::Message << "DrawArraysIndirectCommand:\n"
                                   << "\tcount: " << command.count << "\n"
                                   << "\tprimCount: " << command.primCount << "\n"
                                   << "\tfirst: " << command.first << "\n"
                                   << "\treservedMustBeZero: " << command.reservedMustBeZero << "\n"
                                   << tcu::TestLog::EndMessage;
        }

        GLuint indirectBuf = 0;
        m_ctx.genBuffers(1, &indirectBuf);
        m_ctx.bindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuf);
        m_ctx.bufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawCommand) + indirectOffset, buffer, GL_STATIC_DRAW);
        delete[] buffer;

        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "Setup draw indirect buffer");

        m_ctx.drawArraysIndirect(primitiveToGL(primitive), glu::BufferOffsetAsPointer(indirectOffset));
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glDrawArraysIndirect()");

        m_ctx.deleteBuffers(1, &indirectBuf);
    }
    else if (drawMethod == DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_INDIRECT)
    {
        struct DrawCommand
        {
            GLuint count;
            GLuint primCount;
            GLuint firstIndex;
            GLint baseVertex;
            GLuint reservedMustBeZero;
        };
        uint8_t *buffer = new uint8_t[sizeof(DrawCommand) + indirectOffset];

        {
            DrawCommand command;

            // index offset must be converted to firstIndex by dividing with the index element size
            const auto offsetAsInteger = reinterpret_cast<uintptr_t>(indexOffset);
            DE_ASSERT(offsetAsInteger % gls::DrawTestSpec::indexTypeSize(indexType) ==
                      0); // \note This is checked in spec validation

            command.count              = vertexCount;
            command.primCount          = instanceCount;
            command.firstIndex         = (glw::GLuint)(offsetAsInteger / gls::DrawTestSpec::indexTypeSize(indexType));
            command.baseVertex         = baseVertex;
            command.reservedMustBeZero = 0;

            memcpy(buffer + indirectOffset, &command, sizeof(command));

            if (m_logEnabled)
                m_testCtx.getLog() << tcu::TestLog::Message << "DrawElementsIndirectCommand:\n"
                                   << "\tcount: " << command.count << "\n"
                                   << "\tprimCount: " << command.primCount << "\n"
                                   << "\tfirstIndex: " << command.firstIndex << "\n"
                                   << "\tbaseVertex: " << command.baseVertex << "\n"
                                   << "\treservedMustBeZero: " << command.reservedMustBeZero << "\n"
                                   << tcu::TestLog::EndMessage;
        }

        GLuint indirectBuf = 0;
        m_ctx.genBuffers(1, &indirectBuf);
        m_ctx.bindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuf);
        m_ctx.bufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawCommand) + indirectOffset, buffer, GL_STATIC_DRAW);
        delete[] buffer;

        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "Setup draw indirect buffer");

        m_ctx.drawElementsIndirect(primitiveToGL(primitive), indexTypeToGL(indexType),
                                   glu::BufferOffsetAsPointer(indirectOffset));
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glDrawArraysIndirect()");

        m_ctx.deleteBuffers(1, &indirectBuf);
    }
    else if (drawMethod == DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_BASEVERTEX)
    {
        m_ctx.drawElementsBaseVertex(primitiveToGL(primitive), vertexCount, indexTypeToGL(indexType), indexOffset,
                                     baseVertex);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glDrawElementsBaseVertex()");
    }
    else if (drawMethod == DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_INSTANCED_BASEVERTEX)
    {
        m_ctx.drawElementsInstancedBaseVertex(primitiveToGL(primitive), vertexCount, indexTypeToGL(indexType),
                                              indexOffset, instanceCount, baseVertex);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glDrawElementsInstancedBaseVertex()");
    }
    else if (drawMethod == DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_RANGED_BASEVERTEX)
    {
        m_ctx.drawRangeElementsBaseVertex(primitiveToGL(primitive), rangeStart, rangeEnd, vertexCount,
                                          indexTypeToGL(indexType), indexOffset, baseVertex);
        GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glDrawRangeElementsBaseVertex()");
    }
    else
        DE_ASSERT(false);

    for (int arrayNdx = 0; arrayNdx < (int)m_arrays.size(); arrayNdx++)
    {
        if (m_arrays[arrayNdx]->isBound())
        {
            std::stringstream attribName;
            attribName << "a_" << arrayNdx;

            uint32_t loc = m_ctx.getAttribLocation(m_programID, attribName.str().c_str());

            m_ctx.disableVertexAttribArray(loc);
            GLU_EXPECT_NO_ERROR(m_ctx.getError(), "glDisableVertexAttribArray()");
        }
    }

    if (m_useVao)
        m_ctx.bindVertexArray(0);

    m_ctx.useProgram(0);
    m_ctx.readPixels(m_screen, 0, 0, m_screen.getWidth(), m_screen.getHeight());
}

// DrawTestSpec

DrawTestSpec::AttributeSpec DrawTestSpec::AttributeSpec::createAttributeArray(InputType inputType,
                                                                              OutputType outputType, Storage storage,
                                                                              Usage usage, int componentCount,
                                                                              int offset, int stride, bool normalize,
                                                                              int instanceDivisor)
{
    DrawTestSpec::AttributeSpec spec;

    spec.inputType       = inputType;
    spec.outputType      = outputType;
    spec.storage         = storage;
    spec.usage           = usage;
    spec.componentCount  = componentCount;
    spec.offset          = offset;
    spec.stride          = stride;
    spec.normalize       = normalize;
    spec.instanceDivisor = instanceDivisor;

    spec.useDefaultAttribute = false;

    return spec;
}

DrawTestSpec::AttributeSpec DrawTestSpec::AttributeSpec::createDefaultAttribute(InputType inputType,
                                                                                OutputType outputType,
                                                                                int componentCount)
{
    DE_ASSERT(inputType == INPUTTYPE_INT || inputType == INPUTTYPE_UNSIGNED_INT || inputType == INPUTTYPE_FLOAT);
    DE_ASSERT(inputType == INPUTTYPE_FLOAT || componentCount == 4);

    DrawTestSpec::AttributeSpec spec;

    spec.inputType       = inputType;
    spec.outputType      = outputType;
    spec.storage         = DrawTestSpec::STORAGE_LAST;
    spec.usage           = DrawTestSpec::USAGE_LAST;
    spec.componentCount  = componentCount;
    spec.offset          = 0;
    spec.stride          = 0;
    spec.normalize       = 0;
    spec.instanceDivisor = 0;

    spec.useDefaultAttribute = true;

    return spec;
}

DrawTestSpec::AttributeSpec::AttributeSpec(void)
{
    inputType                   = DrawTestSpec::INPUTTYPE_LAST;
    outputType                  = DrawTestSpec::OUTPUTTYPE_LAST;
    storage                     = DrawTestSpec::STORAGE_LAST;
    usage                       = DrawTestSpec::USAGE_LAST;
    componentCount              = 0;
    offset                      = 0;
    stride                      = 0;
    normalize                   = false;
    instanceDivisor             = 0;
    useDefaultAttribute         = false;
    additionalPositionAttribute = false;
    bgraComponentOrder          = false;
}

int DrawTestSpec::AttributeSpec::hash(void) const
{
    if (useDefaultAttribute)
    {
        return 1 * int(inputType) + 7 * int(outputType) + 13 * componentCount;
    }
    else
    {
        return 1 * int(inputType) + 2 * int(outputType) + 3 * int(storage) + 5 * int(usage) + 7 * componentCount +
               11 * offset + 13 * stride + 17 * (normalize ? 0 : 1) + 19 * instanceDivisor;
    }
}

bool DrawTestSpec::AttributeSpec::valid(glu::ApiType ctxType) const
{
    const bool inputTypeFloat = inputType == DrawTestSpec::INPUTTYPE_FLOAT ||
                                inputType == DrawTestSpec::INPUTTYPE_FIXED || inputType == DrawTestSpec::INPUTTYPE_HALF;
    const bool inputTypeUnsignedInteger = inputType == DrawTestSpec::INPUTTYPE_UNSIGNED_BYTE ||
                                          inputType == DrawTestSpec::INPUTTYPE_UNSIGNED_SHORT ||
                                          inputType == DrawTestSpec::INPUTTYPE_UNSIGNED_INT ||
                                          inputType == DrawTestSpec::INPUTTYPE_UNSIGNED_INT_2_10_10_10;
    const bool inputTypeSignedInteger =
        inputType == DrawTestSpec::INPUTTYPE_BYTE || inputType == DrawTestSpec::INPUTTYPE_SHORT ||
        inputType == DrawTestSpec::INPUTTYPE_INT || inputType == DrawTestSpec::INPUTTYPE_INT_2_10_10_10;
    const bool inputTypePacked = inputType == DrawTestSpec::INPUTTYPE_UNSIGNED_INT_2_10_10_10 ||
                                 inputType == DrawTestSpec::INPUTTYPE_INT_2_10_10_10;

    const bool outputTypeFloat =
        outputType == DrawTestSpec::OUTPUTTYPE_FLOAT || outputType == DrawTestSpec::OUTPUTTYPE_VEC2 ||
        outputType == DrawTestSpec::OUTPUTTYPE_VEC3 || outputType == DrawTestSpec::OUTPUTTYPE_VEC4;
    const bool outputTypeSignedInteger =
        outputType == DrawTestSpec::OUTPUTTYPE_INT || outputType == DrawTestSpec::OUTPUTTYPE_IVEC2 ||
        outputType == DrawTestSpec::OUTPUTTYPE_IVEC3 || outputType == DrawTestSpec::OUTPUTTYPE_IVEC4;
    const bool outputTypeUnsignedInteger =
        outputType == DrawTestSpec::OUTPUTTYPE_UINT || outputType == DrawTestSpec::OUTPUTTYPE_UVEC2 ||
        outputType == DrawTestSpec::OUTPUTTYPE_UVEC3 || outputType == DrawTestSpec::OUTPUTTYPE_UVEC4;

    if (useDefaultAttribute)
    {
        if (inputType != DrawTestSpec::INPUTTYPE_INT && inputType != DrawTestSpec::INPUTTYPE_UNSIGNED_INT &&
            inputType != DrawTestSpec::INPUTTYPE_FLOAT)
            return false;

        if (inputType != DrawTestSpec::INPUTTYPE_FLOAT && componentCount != 4)
            return false;

        // no casting allowed (undefined results)
        if (inputType == DrawTestSpec::INPUTTYPE_INT && !outputTypeSignedInteger)
            return false;
        if (inputType == DrawTestSpec::INPUTTYPE_UNSIGNED_INT && !outputTypeUnsignedInteger)
            return false;
    }

    if (inputTypePacked && componentCount != 4)
        return false;

    // Invalid conversions:

    // float -> [u]int
    if (inputTypeFloat && !outputTypeFloat)
        return false;

    // uint -> int        (undefined results)
    if (inputTypeUnsignedInteger && outputTypeSignedInteger)
        return false;

    // int -> uint        (undefined results)
    if (inputTypeSignedInteger && outputTypeUnsignedInteger)
        return false;

    // packed -> non-float (packed formats are converted to floats)
    if (inputTypePacked && !outputTypeFloat)
        return false;

    // Invalid normalize. Normalize is only valid if output type is float
    if (normalize && !outputTypeFloat)
        return false;

    // Allow reverse order (GL_BGRA) only for packed and 4-component ubyte
    if (bgraComponentOrder && componentCount != 4)
        return false;
    if (bgraComponentOrder && inputType != DrawTestSpec::INPUTTYPE_UNSIGNED_INT_2_10_10_10 &&
        inputType != DrawTestSpec::INPUTTYPE_INT_2_10_10_10 && inputType != DrawTestSpec::INPUTTYPE_UNSIGNED_BYTE)
        return false;
    if (bgraComponentOrder && normalize != true)
        return false;

    // GLES2 limits
    if (ctxType == glu::ApiType::es(2, 0))
    {
        if (inputType != DrawTestSpec::INPUTTYPE_FLOAT && inputType != DrawTestSpec::INPUTTYPE_FIXED &&
            inputType != DrawTestSpec::INPUTTYPE_BYTE && inputType != DrawTestSpec::INPUTTYPE_UNSIGNED_BYTE &&
            inputType != DrawTestSpec::INPUTTYPE_SHORT && inputType != DrawTestSpec::INPUTTYPE_UNSIGNED_SHORT)
            return false;

        if (!outputTypeFloat)
            return false;

        if (bgraComponentOrder)
            return false;
    }

    // GLES3 limits
    if (ctxType.getProfile() == glu::PROFILE_ES && ctxType.getMajorVersion() == 3)
    {
        if (bgraComponentOrder)
            return false;
    }

    // No user pointers in GL core
    if (ctxType.getProfile() == glu::PROFILE_CORE)
    {
        if (!useDefaultAttribute && storage == DrawTestSpec::STORAGE_USER)
            return false;
    }

    return true;
}

bool DrawTestSpec::AttributeSpec::isBufferAligned(void) const
{
    const bool inputTypePacked = inputType == DrawTestSpec::INPUTTYPE_UNSIGNED_INT_2_10_10_10 ||
                                 inputType == DrawTestSpec::INPUTTYPE_INT_2_10_10_10;

    // Buffer alignment, offset is a multiple of underlying data type size?
    if (storage == STORAGE_BUFFER)
    {
        int dataTypeSize = gls::DrawTestSpec::inputTypeSize(inputType);
        if (inputTypePacked)
            dataTypeSize = 4;

        if (offset % dataTypeSize != 0)
            return false;
    }

    return true;
}

bool DrawTestSpec::AttributeSpec::isBufferStrideAligned(void) const
{
    const bool inputTypePacked = inputType == DrawTestSpec::INPUTTYPE_UNSIGNED_INT_2_10_10_10 ||
                                 inputType == DrawTestSpec::INPUTTYPE_INT_2_10_10_10;

    // Buffer alignment, offset is a multiple of underlying data type size?
    if (storage == STORAGE_BUFFER)
    {
        int dataTypeSize = gls::DrawTestSpec::inputTypeSize(inputType);
        if (inputTypePacked)
            dataTypeSize = 4;

        if (stride % dataTypeSize != 0)
            return false;
    }

    return true;
}

std::string DrawTestSpec::targetToString(Target target)
{
    static const char *targets[] = {
        "element_array", // TARGET_ELEMENT_ARRAY = 0,
        "array"          // TARGET_ARRAY,
    };

    return de::getSizedArrayElement<DrawTestSpec::TARGET_LAST>(targets, (int)target);
}

std::string DrawTestSpec::inputTypeToString(InputType type)
{
    static const char *types[] = {
        "float",  // INPUTTYPE_FLOAT = 0,
        "fixed",  // INPUTTYPE_FIXED,
        "double", // INPUTTYPE_DOUBLE

        "byte",  // INPUTTYPE_BYTE,
        "short", // INPUTTYPE_SHORT,

        "unsigned_byte",  // INPUTTYPE_UNSIGNED_BYTE,
        "unsigned_short", // INPUTTYPE_UNSIGNED_SHORT,

        "int",                    // INPUTTYPE_INT,
        "unsigned_int",           // INPUTTYPE_UNSIGNED_INT,
        "half",                   // INPUTTYPE_HALF,
        "unsigned_int2_10_10_10", // INPUTTYPE_UNSIGNED_INT_2_10_10_10,
        "int2_10_10_10"           // INPUTTYPE_INT_2_10_10_10,
    };

    return de::getSizedArrayElement<DrawTestSpec::INPUTTYPE_LAST>(types, (int)type);
}

std::string DrawTestSpec::outputTypeToString(OutputType type)
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

    return de::getSizedArrayElement<DrawTestSpec::OUTPUTTYPE_LAST>(types, (int)type);
}

std::string DrawTestSpec::usageTypeToString(Usage usage)
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

    return de::getSizedArrayElement<DrawTestSpec::USAGE_LAST>(usages, (int)usage);
}

std::string DrawTestSpec::storageToString(Storage storage)
{
    static const char *storages[] = {
        "user_ptr", // STORAGE_USER = 0,
        "buffer"    // STORAGE_BUFFER,
    };

    return de::getSizedArrayElement<DrawTestSpec::STORAGE_LAST>(storages, (int)storage);
}

std::string DrawTestSpec::primitiveToString(Primitive primitive)
{
    static const char *primitives[] = {
        "points",                   // PRIMITIVE_POINTS ,
        "triangles",                // PRIMITIVE_TRIANGLES,
        "triangle_fan",             // PRIMITIVE_TRIANGLE_FAN,
        "triangle_strip",           // PRIMITIVE_TRIANGLE_STRIP,
        "lines",                    // PRIMITIVE_LINES
        "line_strip",               // PRIMITIVE_LINE_STRIP
        "line_loop",                // PRIMITIVE_LINE_LOOP
        "lines_adjacency",          // PRIMITIVE_LINES_ADJACENCY
        "line_strip_adjacency",     // PRIMITIVE_LINE_STRIP_ADJACENCY
        "triangles_adjacency",      // PRIMITIVE_TRIANGLES_ADJACENCY
        "triangle_strip_adjacency", // PRIMITIVE_TRIANGLE_STRIP_ADJACENCY
    };

    return de::getSizedArrayElement<DrawTestSpec::PRIMITIVE_LAST>(primitives, (int)primitive);
}

std::string DrawTestSpec::indexTypeToString(IndexType type)
{
    static const char *indexTypes[] = {
        "byte",  // INDEXTYPE_BYTE = 0,
        "short", // INDEXTYPE_SHORT,
        "int",   // INDEXTYPE_INT,
    };

    return de::getSizedArrayElement<DrawTestSpec::INDEXTYPE_LAST>(indexTypes, (int)type);
}

std::string DrawTestSpec::drawMethodToString(DrawTestSpec::DrawMethod method)
{
    static const char *methods[] = {
        "draw_arrays",                         //!< DRAWMETHOD_DRAWARRAYS
        "draw_arrays_instanced",               //!< DRAWMETHOD_DRAWARRAYS_INSTANCED
        "draw_arrays_indirect",                //!< DRAWMETHOD_DRAWARRAYS_INDIRECT
        "draw_elements",                       //!< DRAWMETHOD_DRAWELEMENTS
        "draw_range_elements",                 //!< DRAWMETHOD_DRAWELEMENTS_RANGED
        "draw_elements_instanced",             //!< DRAWMETHOD_DRAWELEMENTS_INSTANCED
        "draw_elements_indirect",              //!< DRAWMETHOD_DRAWELEMENTS_INDIRECT
        "draw_elements_base_vertex",           //!< DRAWMETHOD_DRAWELEMENTS_BASEVERTEX,
        "draw_elements_instanced_base_vertex", //!< DRAWMETHOD_DRAWELEMENTS_INSTANCED_BASEVERTEX,
        "draw_range_elements_base_vertex",     //!< DRAWMETHOD_DRAWELEMENTS_RANGED_BASEVERTEX,
    };

    return de::getSizedArrayElement<DrawTestSpec::DRAWMETHOD_LAST>(methods, (int)method);
}

int DrawTestSpec::inputTypeSize(InputType type)
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

    return de::getSizedArrayElement<DrawTestSpec::INPUTTYPE_LAST>(size, (int)type);
}

int DrawTestSpec::indexTypeSize(IndexType type)
{
    static const int size[] = {
        sizeof(uint8_t),  // INDEXTYPE_BYTE,
        sizeof(uint16_t), // INDEXTYPE_SHORT,
        sizeof(uint32_t), // INDEXTYPE_INT,
    };

    return de::getSizedArrayElement<DrawTestSpec::INDEXTYPE_LAST>(size, (int)type);
}

std::string DrawTestSpec::getName(void) const
{
    const MethodInfo methodInfo = getMethodInfo(drawMethod);
    const bool hasFirst         = methodInfo.first;
    const bool instanced        = methodInfo.instanced;
    const bool ranged           = methodInfo.ranged;
    const bool indexed          = methodInfo.indexed;

    std::stringstream name;

    for (size_t ndx = 0; ndx < attribs.size(); ++ndx)
    {
        const AttributeSpec &attrib = attribs[ndx];

        if (attribs.size() > 1)
            name << "attrib" << ndx << "_";

        if (ndx == 0 || attrib.additionalPositionAttribute)
            name << "pos_";
        else
            name << "col_";

        if (attrib.useDefaultAttribute)
        {
            name << "non_array_" << DrawTestSpec::inputTypeToString((DrawTestSpec::InputType)attrib.inputType) << "_"
                 << attrib.componentCount << "_" << DrawTestSpec::outputTypeToString(attrib.outputType) << "_";
        }
        else
        {
            name << DrawTestSpec::storageToString(attrib.storage) << "_" << attrib.offset << "_" << attrib.stride << "_"
                 << DrawTestSpec::inputTypeToString((DrawTestSpec::InputType)attrib.inputType);
            if (attrib.inputType != DrawTestSpec::INPUTTYPE_UNSIGNED_INT_2_10_10_10 &&
                attrib.inputType != DrawTestSpec::INPUTTYPE_INT_2_10_10_10)
                name << attrib.componentCount;
            name << "_" << (attrib.normalize ? "normalized_" : "")
                 << DrawTestSpec::outputTypeToString(attrib.outputType) << "_"
                 << DrawTestSpec::usageTypeToString(attrib.usage) << "_" << attrib.instanceDivisor << "_";
        }
    }

    if (indexed)
        name << "index_" << DrawTestSpec::indexTypeToString(indexType) << "_"
             << DrawTestSpec::storageToString(indexStorage) << "_"
             << "offset" << indexPointerOffset << "_";
    if (hasFirst)
        name << "first" << first << "_";
    if (ranged)
        name << "ranged_" << indexMin << "_" << indexMax << "_";
    if (instanced)
        name << "instances" << instanceCount << "_";

    switch (primitive)
    {
    case DrawTestSpec::PRIMITIVE_POINTS:
        name << "points_";
        break;
    case DrawTestSpec::PRIMITIVE_TRIANGLES:
        name << "triangles_";
        break;
    case DrawTestSpec::PRIMITIVE_TRIANGLE_FAN:
        name << "triangle_fan_";
        break;
    case DrawTestSpec::PRIMITIVE_TRIANGLE_STRIP:
        name << "triangle_strip_";
        break;
    case DrawTestSpec::PRIMITIVE_LINES:
        name << "lines_";
        break;
    case DrawTestSpec::PRIMITIVE_LINE_STRIP:
        name << "line_strip_";
        break;
    case DrawTestSpec::PRIMITIVE_LINE_LOOP:
        name << "line_loop_";
        break;
    case DrawTestSpec::PRIMITIVE_LINES_ADJACENCY:
        name << "line_adjancency";
        break;
    case DrawTestSpec::PRIMITIVE_LINE_STRIP_ADJACENCY:
        name << "line_strip_adjancency";
        break;
    case DrawTestSpec::PRIMITIVE_TRIANGLES_ADJACENCY:
        name << "triangles_adjancency";
        break;
    case DrawTestSpec::PRIMITIVE_TRIANGLE_STRIP_ADJACENCY:
        name << "triangle_strip_adjancency";
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    name << primitiveCount;

    return name.str();
}

std::string DrawTestSpec::getDesc(void) const
{
    std::stringstream desc;

    for (size_t ndx = 0; ndx < attribs.size(); ++ndx)
    {
        const AttributeSpec &attrib = attribs[ndx];

        if (attrib.useDefaultAttribute)
        {
            desc << "Attribute " << ndx << ": default, "
                 << ((ndx == 0 || attrib.additionalPositionAttribute) ? ("position ,") : ("color ,"))
                 << "input datatype " << DrawTestSpec::inputTypeToString((DrawTestSpec::InputType)attrib.inputType)
                 << ", "
                 << "input component count " << attrib.componentCount << ", "
                 << "used as " << DrawTestSpec::outputTypeToString(attrib.outputType) << ", ";
        }
        else
        {
            desc << "Attribute " << ndx << ": "
                 << ((ndx == 0 || attrib.additionalPositionAttribute) ? ("position ,") : ("color ,")) << "Storage in "
                 << DrawTestSpec::storageToString(attrib.storage) << ", "
                 << "stride " << attrib.stride << ", "
                 << "input datatype " << DrawTestSpec::inputTypeToString((DrawTestSpec::InputType)attrib.inputType)
                 << ", "
                 << "input component count " << attrib.componentCount << ", "
                 << (attrib.normalize ? "normalized, " : "") << "used as "
                 << DrawTestSpec::outputTypeToString(attrib.outputType) << ", "
                 << "instance divisor " << attrib.instanceDivisor << ", ";
        }
    }

    if (drawMethod == DRAWMETHOD_DRAWARRAYS)
    {
        desc << "drawArrays(), "
             << "first " << first << ", ";
    }
    else if (drawMethod == DRAWMETHOD_DRAWARRAYS_INSTANCED)
    {
        desc << "drawArraysInstanced(), "
             << "first " << first << ", "
             << "instance count " << instanceCount << ", ";
    }
    else if (drawMethod == DRAWMETHOD_DRAWELEMENTS)
    {
        desc << "drawElements(), "
             << "index type " << DrawTestSpec::indexTypeToString(indexType) << ", "
             << "index storage in " << DrawTestSpec::storageToString(indexStorage) << ", "
             << "index offset " << indexPointerOffset << ", ";
    }
    else if (drawMethod == DRAWMETHOD_DRAWELEMENTS_RANGED)
    {
        desc << "drawElementsRanged(), "
             << "index type " << DrawTestSpec::indexTypeToString(indexType) << ", "
             << "index storage in " << DrawTestSpec::storageToString(indexStorage) << ", "
             << "index offset " << indexPointerOffset << ", "
             << "range start " << indexMin << ", "
             << "range end " << indexMax << ", ";
    }
    else if (drawMethod == DRAWMETHOD_DRAWELEMENTS_INSTANCED)
    {
        desc << "drawElementsInstanced(), "
             << "index type " << DrawTestSpec::indexTypeToString(indexType) << ", "
             << "index storage in " << DrawTestSpec::storageToString(indexStorage) << ", "
             << "index offset " << indexPointerOffset << ", "
             << "instance count " << instanceCount << ", ";
    }
    else if (drawMethod == DRAWMETHOD_DRAWARRAYS_INDIRECT)
    {
        desc << "drawArraysIndirect(), "
             << "first " << first << ", "
             << "instance count " << instanceCount << ", "
             << "indirect offset " << indirectOffset << ", ";
    }
    else if (drawMethod == DRAWMETHOD_DRAWELEMENTS_INDIRECT)
    {
        desc << "drawElementsIndirect(), "
             << "index type " << DrawTestSpec::indexTypeToString(indexType) << ", "
             << "index storage in " << DrawTestSpec::storageToString(indexStorage) << ", "
             << "index offset " << indexPointerOffset << ", "
             << "instance count " << instanceCount << ", "
             << "indirect offset " << indirectOffset << ", "
             << "base vertex " << baseVertex << ", ";
    }
    else
        DE_ASSERT(false);

    desc << primitiveCount;

    switch (primitive)
    {
    case DrawTestSpec::PRIMITIVE_POINTS:
        desc << "points";
        break;
    case DrawTestSpec::PRIMITIVE_TRIANGLES:
        desc << "triangles";
        break;
    case DrawTestSpec::PRIMITIVE_TRIANGLE_FAN:
        desc << "triangles (fan)";
        break;
    case DrawTestSpec::PRIMITIVE_TRIANGLE_STRIP:
        desc << "triangles (strip)";
        break;
    case DrawTestSpec::PRIMITIVE_LINES:
        desc << "lines";
        break;
    case DrawTestSpec::PRIMITIVE_LINE_STRIP:
        desc << "lines (strip)";
        break;
    case DrawTestSpec::PRIMITIVE_LINE_LOOP:
        desc << "lines (loop)";
        break;
    case DrawTestSpec::PRIMITIVE_LINES_ADJACENCY:
        desc << "lines (adjancency)";
        break;
    case DrawTestSpec::PRIMITIVE_LINE_STRIP_ADJACENCY:
        desc << "lines (strip, adjancency)";
        break;
    case DrawTestSpec::PRIMITIVE_TRIANGLES_ADJACENCY:
        desc << "triangles (adjancency)";
        break;
    case DrawTestSpec::PRIMITIVE_TRIANGLE_STRIP_ADJACENCY:
        desc << "triangles (strip, adjancency)";
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    return desc.str();
}

std::string DrawTestSpec::getMultilineDesc(void) const
{
    std::stringstream desc;

    for (size_t ndx = 0; ndx < attribs.size(); ++ndx)
    {
        const AttributeSpec &attrib = attribs[ndx];

        if (attrib.useDefaultAttribute)
        {
            desc << "Attribute " << ndx << ": default, "
                 << ((ndx == 0 || attrib.additionalPositionAttribute) ? ("position\n") : ("color\n"))
                 << "\tinput datatype " << DrawTestSpec::inputTypeToString((DrawTestSpec::InputType)attrib.inputType)
                 << "\n"
                 << "\tinput component count " << attrib.componentCount << "\n"
                 << "\tused as " << DrawTestSpec::outputTypeToString(attrib.outputType) << "\n";
        }
        else
        {
            desc << "Attribute " << ndx << ": "
                 << ((ndx == 0 || attrib.additionalPositionAttribute) ? ("position\n") : ("color\n")) << "\tStorage in "
                 << DrawTestSpec::storageToString(attrib.storage) << "\n"
                 << "\tstride " << attrib.stride << "\n"
                 << "\tinput datatype " << DrawTestSpec::inputTypeToString((DrawTestSpec::InputType)attrib.inputType)
                 << "\n"
                 << "\tinput component count " << attrib.componentCount << "\n"
                 << (attrib.normalize ? "\tnormalized\n" : "") << "\tused as "
                 << DrawTestSpec::outputTypeToString(attrib.outputType) << "\n"
                 << "\tinstance divisor " << attrib.instanceDivisor << "\n";
        }
    }

    if (drawMethod == DRAWMETHOD_DRAWARRAYS)
    {
        desc << "drawArrays()\n"
             << "\tfirst " << first << "\n";
    }
    else if (drawMethod == DRAWMETHOD_DRAWARRAYS_INSTANCED)
    {
        desc << "drawArraysInstanced()\n"
             << "\tfirst " << first << "\n"
             << "\tinstance count " << instanceCount << "\n";
    }
    else if (drawMethod == DRAWMETHOD_DRAWELEMENTS)
    {
        desc << "drawElements()\n"
             << "\tindex type " << DrawTestSpec::indexTypeToString(indexType) << "\n"
             << "\tindex storage in " << DrawTestSpec::storageToString(indexStorage) << "\n"
             << "\tindex offset " << indexPointerOffset << "\n";
    }
    else if (drawMethod == DRAWMETHOD_DRAWELEMENTS_RANGED)
    {
        desc << "drawElementsRanged()\n"
             << "\tindex type " << DrawTestSpec::indexTypeToString(indexType) << "\n"
             << "\tindex storage in " << DrawTestSpec::storageToString(indexStorage) << "\n"
             << "\tindex offset " << indexPointerOffset << "\n"
             << "\trange start " << indexMin << "\n"
             << "\trange end " << indexMax << "\n";
    }
    else if (drawMethod == DRAWMETHOD_DRAWELEMENTS_INSTANCED)
    {
        desc << "drawElementsInstanced()\n"
             << "\tindex type " << DrawTestSpec::indexTypeToString(indexType) << "\n"
             << "\tindex storage in " << DrawTestSpec::storageToString(indexStorage) << "\n"
             << "\tindex offset " << indexPointerOffset << "\n"
             << "\tinstance count " << instanceCount << "\n";
    }
    else if (drawMethod == DRAWMETHOD_DRAWARRAYS_INDIRECT)
    {
        desc << "drawArraysIndirect()\n"
             << "\tfirst " << first << "\n"
             << "\tinstance count " << instanceCount << "\n"
             << "\tindirect offset " << indirectOffset << "\n";
    }
    else if (drawMethod == DRAWMETHOD_DRAWELEMENTS_INDIRECT)
    {
        desc << "drawElementsIndirect()\n"
             << "\tindex type " << DrawTestSpec::indexTypeToString(indexType) << "\n"
             << "\tindex storage in " << DrawTestSpec::storageToString(indexStorage) << "\n"
             << "\tindex offset " << indexPointerOffset << "\n"
             << "\tinstance count " << instanceCount << "\n"
             << "\tindirect offset " << indirectOffset << "\n"
             << "\tbase vertex " << baseVertex << "\n";
    }
    else if (drawMethod == DRAWMETHOD_DRAWELEMENTS_BASEVERTEX)
    {
        desc << "drawElementsBaseVertex()\n"
             << "\tindex type " << DrawTestSpec::indexTypeToString(indexType) << "\n"
             << "\tindex storage in " << DrawTestSpec::storageToString(indexStorage) << "\n"
             << "\tindex offset " << indexPointerOffset << "\n"
             << "\tbase vertex " << baseVertex << "\n";
    }
    else if (drawMethod == DRAWMETHOD_DRAWELEMENTS_INSTANCED_BASEVERTEX)
    {
        desc << "drawElementsInstancedBaseVertex()\n"
             << "\tindex type " << DrawTestSpec::indexTypeToString(indexType) << "\n"
             << "\tindex storage in " << DrawTestSpec::storageToString(indexStorage) << "\n"
             << "\tindex offset " << indexPointerOffset << "\n"
             << "\tinstance count " << instanceCount << "\n"
             << "\tbase vertex " << baseVertex << "\n";
    }
    else if (drawMethod == DRAWMETHOD_DRAWELEMENTS_RANGED_BASEVERTEX)
    {
        desc << "drawRangeElementsBaseVertex()\n"
             << "\tindex type " << DrawTestSpec::indexTypeToString(indexType) << "\n"
             << "\tindex storage in " << DrawTestSpec::storageToString(indexStorage) << "\n"
             << "\tindex offset " << indexPointerOffset << "\n"
             << "\tbase vertex " << baseVertex << "\n"
             << "\trange start " << indexMin << "\n"
             << "\trange end " << indexMax << "\n";
    }
    else
        DE_ASSERT(false);

    desc << "\t" << primitiveCount << " ";

    switch (primitive)
    {
    case DrawTestSpec::PRIMITIVE_POINTS:
        desc << "points";
        break;
    case DrawTestSpec::PRIMITIVE_TRIANGLES:
        desc << "triangles";
        break;
    case DrawTestSpec::PRIMITIVE_TRIANGLE_FAN:
        desc << "triangles (fan)";
        break;
    case DrawTestSpec::PRIMITIVE_TRIANGLE_STRIP:
        desc << "triangles (strip)";
        break;
    case DrawTestSpec::PRIMITIVE_LINES:
        desc << "lines";
        break;
    case DrawTestSpec::PRIMITIVE_LINE_STRIP:
        desc << "lines (strip)";
        break;
    case DrawTestSpec::PRIMITIVE_LINE_LOOP:
        desc << "lines (loop)";
        break;
    case DrawTestSpec::PRIMITIVE_LINES_ADJACENCY:
        desc << "lines (adjancency)";
        break;
    case DrawTestSpec::PRIMITIVE_LINE_STRIP_ADJACENCY:
        desc << "lines (strip, adjancency)";
        break;
    case DrawTestSpec::PRIMITIVE_TRIANGLES_ADJACENCY:
        desc << "triangles (adjancency)";
        break;
    case DrawTestSpec::PRIMITIVE_TRIANGLE_STRIP_ADJACENCY:
        desc << "triangles (strip, adjancency)";
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    desc << "\n";

    return desc.str();
}

DrawTestSpec::DrawTestSpec(void)
{
    primitive          = PRIMITIVE_LAST;
    primitiveCount     = 0;
    drawMethod         = DRAWMETHOD_LAST;
    indexType          = INDEXTYPE_LAST;
    indexPointerOffset = 0;
    indexStorage       = STORAGE_LAST;
    first              = 0;
    indexMin           = 0;
    indexMax           = 0;
    instanceCount      = 0;
    indirectOffset     = 0;
    baseVertex         = 0;
}

int DrawTestSpec::hash(void) const
{
    // Use only drawmode-relevant values in "hashing" as the unrelevant values might not be set (causing non-deterministic behavior).
    const MethodInfo methodInfo = getMethodInfo(drawMethod);
    const bool arrayed          = methodInfo.first;
    const bool instanced        = methodInfo.instanced;
    const bool ranged           = methodInfo.ranged;
    const bool indexed          = methodInfo.indexed;
    const bool indirect         = methodInfo.indirect;
    const bool hasBaseVtx       = methodInfo.baseVertex;

    const int indexHash      = (!indexed) ? (0) : (int(indexType) + 10 * indexPointerOffset + 100 * int(indexStorage));
    const int arrayHash      = (!arrayed) ? (0) : (first);
    const int indexRangeHash = (!ranged) ? (0) : (indexMin + 10 * indexMax);
    const int instanceHash   = (!instanced) ? (0) : (instanceCount);
    const int indirectHash   = (!indirect) ? (0) : (indirectOffset);
    const int baseVtxHash    = (!hasBaseVtx) ? (0) : (baseVertex);
    const int basicHash      = int(primitive) + 10 * primitiveCount + 100 * int(drawMethod);

    return indexHash + 3 * arrayHash + 5 * indexRangeHash + 7 * instanceHash + 13 * basicHash +
           17 * (int)attribs.size() + 19 * primitiveCount + 23 * indirectHash + 27 * baseVtxHash;
}

bool DrawTestSpec::valid(void) const
{
    DE_ASSERT(apiType.getProfile() != glu::PROFILE_LAST);
    DE_ASSERT(primitive != PRIMITIVE_LAST);
    DE_ASSERT(drawMethod != DRAWMETHOD_LAST);

    const MethodInfo methodInfo = getMethodInfo(drawMethod);

    for (int ndx = 0; ndx < (int)attribs.size(); ++ndx)
        if (!attribs[ndx].valid(apiType))
            return false;

    if (methodInfo.ranged)
    {
        uint32_t maxIndexValue = 0;
        if (indexType == INDEXTYPE_BYTE)
            maxIndexValue = GLValue::getMaxValue(INPUTTYPE_UNSIGNED_BYTE).ub.getValue();
        else if (indexType == INDEXTYPE_SHORT)
            maxIndexValue = GLValue::getMaxValue(INPUTTYPE_UNSIGNED_SHORT).us.getValue();
        else if (indexType == INDEXTYPE_INT)
            maxIndexValue = GLValue::getMaxValue(INPUTTYPE_UNSIGNED_INT).ui.getValue();
        else
            DE_ASSERT(false);

        if (indexMin > indexMax)
            return false;
        if (indexMin < 0 || indexMax < 0)
            return false;
        if ((uint32_t)indexMin > maxIndexValue || (uint32_t)indexMax > maxIndexValue)
            return false;
    }

    if (methodInfo.first && first < 0)
        return false;

    // GLES2 limits
    if (apiType == glu::ApiType::es(2, 0))
    {
        if (drawMethod != gls::DrawTestSpec::DRAWMETHOD_DRAWARRAYS &&
            drawMethod != gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS)
            return false;
        if (drawMethod == gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS &&
            (indexType != INDEXTYPE_BYTE && indexType != INDEXTYPE_SHORT))
            return false;
    }

    // Indirect limitations
    if (methodInfo.indirect)
    {
        // Indirect offset alignment
        if (indirectOffset % 4 != 0)
            return false;

        // All attribute arrays must be stored in a buffer
        for (int ndx = 0; ndx < (int)attribs.size(); ++ndx)
            if (!attribs[ndx].useDefaultAttribute && attribs[ndx].storage == gls::DrawTestSpec::STORAGE_USER)
                return false;
    }
    if (drawMethod == DRAWMETHOD_DRAWELEMENTS_INDIRECT)
    {
        // index offset must be convertable to firstIndex
        if (indexPointerOffset % gls::DrawTestSpec::indexTypeSize(indexType) != 0)
            return false;

        // Indices must be in a buffer
        if (indexStorage != STORAGE_BUFFER)
            return false;
    }

    // Do not allow user pointer in GL core
    if (apiType.getProfile() == glu::PROFILE_CORE)
    {
        if (methodInfo.indexed && indexStorage == DrawTestSpec::STORAGE_USER)
            return false;
    }

    return true;
}

DrawTestSpec::CompatibilityTestType DrawTestSpec::isCompatibilityTest(void) const
{
    const MethodInfo methodInfo = getMethodInfo(drawMethod);

    bool bufferAlignmentBad = false;
    bool strideAlignmentBad = false;

    // Attribute buffer alignment
    for (int ndx = 0; ndx < (int)attribs.size(); ++ndx)
        if (!attribs[ndx].isBufferAligned())
            bufferAlignmentBad = true;

    // Attribute stride alignment
    for (int ndx = 0; ndx < (int)attribs.size(); ++ndx)
        if (!attribs[ndx].isBufferStrideAligned())
            strideAlignmentBad = true;

    // Index buffer alignment
    if (methodInfo.indexed)
    {
        if (indexStorage == STORAGE_BUFFER)
        {
            int indexSize = 0;
            if (indexType == INDEXTYPE_BYTE)
                indexSize = 1;
            else if (indexType == INDEXTYPE_SHORT)
                indexSize = 2;
            else if (indexType == INDEXTYPE_INT)
                indexSize = 4;
            else
                DE_ASSERT(false);

            if (indexPointerOffset % indexSize != 0)
                bufferAlignmentBad = true;
        }
    }

    // \note combination bad alignment & stride is treated as bad offset
    if (bufferAlignmentBad)
        return COMPATIBILITY_UNALIGNED_OFFSET;
    else if (strideAlignmentBad)
        return COMPATIBILITY_UNALIGNED_STRIDE;
    else
        return COMPATIBILITY_NONE;
}

enum PrimitiveClass
{
    PRIMITIVECLASS_POINT = 0,
    PRIMITIVECLASS_LINE,
    PRIMITIVECLASS_TRIANGLE,

    PRIMITIVECLASS_LAST
};

static PrimitiveClass getDrawPrimitiveClass(gls::DrawTestSpec::Primitive primitiveType)
{
    switch (primitiveType)
    {
    case gls::DrawTestSpec::PRIMITIVE_POINTS:
        return PRIMITIVECLASS_POINT;

    case gls::DrawTestSpec::PRIMITIVE_LINES:
    case gls::DrawTestSpec::PRIMITIVE_LINE_STRIP:
    case gls::DrawTestSpec::PRIMITIVE_LINE_LOOP:
    case gls::DrawTestSpec::PRIMITIVE_LINES_ADJACENCY:
    case gls::DrawTestSpec::PRIMITIVE_LINE_STRIP_ADJACENCY:
        return PRIMITIVECLASS_LINE;

    case gls::DrawTestSpec::PRIMITIVE_TRIANGLES:
    case gls::DrawTestSpec::PRIMITIVE_TRIANGLE_FAN:
    case gls::DrawTestSpec::PRIMITIVE_TRIANGLE_STRIP:
    case gls::DrawTestSpec::PRIMITIVE_TRIANGLES_ADJACENCY:
    case gls::DrawTestSpec::PRIMITIVE_TRIANGLE_STRIP_ADJACENCY:
        return PRIMITIVECLASS_TRIANGLE;

    default:
        DE_ASSERT(false);
        return PRIMITIVECLASS_LAST;
    }
}

static bool containsLineCases(const std::vector<DrawTestSpec> &m_specs)
{
    for (int ndx = 0; ndx < (int)m_specs.size(); ++ndx)
    {
        if (getDrawPrimitiveClass(m_specs[ndx].primitive) == PRIMITIVECLASS_LINE)
            return true;
    }
    return false;
}

// DrawTest

DrawTest::DrawTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const DrawTestSpec &spec, const char *name,
                   const char *desc)
    : TestCase(testCtx, name, desc)
    , m_renderCtx(renderCtx)
    , m_contextInfo(nullptr)
    , m_refBuffers(nullptr)
    , m_refContext(nullptr)
    , m_glesContext(nullptr)
    , m_glArrayPack(nullptr)
    , m_rrArrayPack(nullptr)
    , m_maxDiffRed(-1)
    , m_maxDiffGreen(-1)
    , m_maxDiffBlue(-1)
    , m_iteration(0)
    , m_result() // \note no per-iteration result logging (only one iteration)
{
    addIteration(spec);
}

DrawTest::DrawTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name, const char *desc)
    : TestCase(testCtx, name, desc)
    , m_renderCtx(renderCtx)
    , m_contextInfo(nullptr)
    , m_refBuffers(nullptr)
    , m_refContext(nullptr)
    , m_glesContext(nullptr)
    , m_glArrayPack(nullptr)
    , m_rrArrayPack(nullptr)
    , m_maxDiffRed(-1)
    , m_maxDiffGreen(-1)
    , m_maxDiffBlue(-1)
    , m_iteration(0)
    , m_result(testCtx.getLog(), "Iteration result: ")
{
}

DrawTest::~DrawTest(void)
{
    deinit();
}

void DrawTest::addIteration(const DrawTestSpec &spec, const char *description)
{
    // Validate spec
    const bool validSpec = spec.valid();
    DE_ASSERT(validSpec);

    if (!validSpec)
        return;

    // Check the context type is the same with other iterations
    if (!m_specs.empty())
    {
        const bool validContext = m_specs[0].apiType == spec.apiType;
        DE_ASSERT(validContext);

        if (!validContext)
            return;
    }

    m_specs.push_back(spec);

    if (description)
        m_iteration_descriptions.push_back(std::string(description));
    else
        m_iteration_descriptions.push_back(std::string());
}

void DrawTest::init(void)
{
    DE_ASSERT(!m_specs.empty());
    DE_ASSERT(contextSupports(m_renderCtx.getType(), m_specs[0].apiType));

    const int renderTargetWidth  = de::min(MAX_RENDER_TARGET_SIZE, m_renderCtx.getRenderTarget().getWidth());
    const int renderTargetHeight = de::min(MAX_RENDER_TARGET_SIZE, m_renderCtx.getRenderTarget().getHeight());

    // lines have significantly different rasterization in MSAA mode
    const bool isLineCase         = containsLineCases(m_specs);
    const bool isMSAACase         = m_renderCtx.getRenderTarget().getNumSamples() > 1;
    const int renderTargetSamples = (isMSAACase && isLineCase) ? (4) : (1);

    sglr::ReferenceContextLimits limits(m_renderCtx);
    bool useVao = false;

    m_glesContext =
        new sglr::GLContext(m_renderCtx, m_testCtx.getLog(), sglr::GLCONTEXT_LOG_CALLS | sglr::GLCONTEXT_LOG_PROGRAMS,
                            tcu::IVec4(0, 0, renderTargetWidth, renderTargetHeight));

    if (m_renderCtx.getType().getAPI() == glu::ApiType::es(2, 0) ||
        m_renderCtx.getType().getAPI() == glu::ApiType::es(3, 0))
        useVao = false;
    else if (contextSupports(m_renderCtx.getType(), glu::ApiType::es(3, 1)) ||
             glu::isContextTypeGLCore(m_renderCtx.getType()))
        useVao = true;
    else
        DE_FATAL("Unknown context type");

    m_refBuffers = new sglr::ReferenceContextBuffers(m_renderCtx.getRenderTarget().getPixelFormat(), 0, 0,
                                                     renderTargetWidth, renderTargetHeight, renderTargetSamples);
    m_refContext = new sglr::ReferenceContext(limits, m_refBuffers->getColorbuffer(), m_refBuffers->getDepthbuffer(),
                                              m_refBuffers->getStencilbuffer());

    m_glArrayPack = new AttributePack(m_testCtx, m_renderCtx, *m_glesContext,
                                      tcu::UVec2(renderTargetWidth, renderTargetHeight), useVao, true);
    m_rrArrayPack = new AttributePack(m_testCtx, m_renderCtx, *m_refContext,
                                      tcu::UVec2(renderTargetWidth, renderTargetHeight), useVao, false);

    m_maxDiffRed =
        deCeilFloatToInt32(256.0f * (6.0f / (float)(1 << m_renderCtx.getRenderTarget().getPixelFormat().redBits)));
    m_maxDiffGreen =
        deCeilFloatToInt32(256.0f * (6.0f / (float)(1 << m_renderCtx.getRenderTarget().getPixelFormat().greenBits)));
    m_maxDiffBlue =
        deCeilFloatToInt32(256.0f * (6.0f / (float)(1 << m_renderCtx.getRenderTarget().getPixelFormat().blueBits)));
    m_contextInfo = glu::ContextInfo::create(m_renderCtx);
}

void DrawTest::deinit(void)
{
    delete m_glArrayPack;
    delete m_rrArrayPack;
    delete m_refBuffers;
    delete m_refContext;
    delete m_glesContext;
    delete m_contextInfo;

    m_glArrayPack = nullptr;
    m_rrArrayPack = nullptr;
    m_refBuffers  = nullptr;
    m_refContext  = nullptr;
    m_glesContext = nullptr;
    m_contextInfo = nullptr;
}

DrawTest::IterateResult DrawTest::iterate(void)
{
    const int specNdx        = (m_iteration / 2);
    const DrawTestSpec &spec = m_specs[specNdx];

    if (spec.drawMethod == DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_BASEVERTEX ||
        spec.drawMethod == DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_INSTANCED_BASEVERTEX ||
        spec.drawMethod == DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_RANGED_BASEVERTEX)
    {
        const bool supportsES32orGL45 = contextSupports(m_renderCtx.getType(), glu::ApiType::es(3, 2)) ||
                                        contextSupports(m_renderCtx.getType(), glu::ApiType::core(4, 5));
        TCU_CHECK_AND_THROW(NotSupportedError,
                            supportsES32orGL45 ||
                                m_contextInfo->isExtensionSupported("GL_EXT_draw_elements_base_vertex"),
                            "GL_EXT_draw_elements_base_vertex is not supported.");
    }

    const bool drawStep               = (m_iteration % 2) == 0;
    const bool compareStep            = (m_iteration % 2) == 1;
    const IterateResult iterateResult = ((size_t)m_iteration + 1 == m_specs.size() * 2) ? (STOP) : (CONTINUE);
    const bool updateProgram =
        (m_iteration == 0) ||
        (drawStep && !checkSpecsShaderCompatible(m_specs[specNdx],
                                                 m_specs[specNdx - 1])); // try to use the same shader in all iterations
    IterationLogSectionEmitter sectionEmitter(m_testCtx.getLog(), specNdx, m_specs.size(),
                                              m_iteration_descriptions[specNdx], drawStep && m_specs.size() != 1);

    if (drawStep)
    {
        const MethodInfo methodInfo = getMethodInfo(spec.drawMethod);
        const bool indexed          = methodInfo.indexed;
        const bool instanced        = methodInfo.instanced;
        const bool ranged           = methodInfo.ranged;
        const bool hasFirst         = methodInfo.first;
        const bool hasBaseVtx       = methodInfo.baseVertex;

        const size_t primitiveElementCount =
            getElementCount(spec.primitive, spec.primitiveCount); // !< elements to be drawn
        const int indexMin           = (ranged) ? (spec.indexMin) : (0);
        const int firstAddition      = (hasFirst) ? (spec.first) : (0);
        const int baseVertexAddition = (hasBaseVtx && spec.baseVertex > 0) ?
                                           (spec.baseVertex) :
                                           (0); // spec.baseVertex > 0 => Create bigger attribute buffer
        const int indexBase          = (hasBaseVtx && spec.baseVertex < 0) ? (-spec.baseVertex) :
                                                                             (0); // spec.baseVertex < 0 => Create bigger indices
        const size_t elementCount =
            primitiveElementCount + indexMin + firstAddition +
            baseVertexAddition; // !< elements in buffer (buffer should have at least primitiveElementCount ACCESSIBLE (index range, first) elements)
        const int maxElementIndex = (int)primitiveElementCount + indexMin + firstAddition - 1;
        const int indexMax =
            de::max(0, (ranged) ? (de::clamp<int>(spec.indexMax, 0, maxElementIndex)) : (maxElementIndex));
        float coordScale = getCoordScale(spec);
        float colorScale = getColorScale(spec);

        rr::GenericVec4 nullAttribValue;

        // Log info
        m_testCtx.getLog() << TestLog::Message << spec.getMultilineDesc() << TestLog::EndMessage;
        m_testCtx.getLog() << TestLog::Message << TestLog::EndMessage; // extra line for clarity

        // Data

        m_glArrayPack->clearArrays();
        m_rrArrayPack->clearArrays();

        for (int attribNdx = 0; attribNdx < (int)spec.attribs.size(); attribNdx++)
        {
            DrawTestSpec::AttributeSpec attribSpec = spec.attribs[attribNdx];
            const bool isPositionAttr              = (attribNdx == 0) || (attribSpec.additionalPositionAttribute);

            if (attribSpec.useDefaultAttribute)
            {
                const int seed              = 10 * attribSpec.hash() + 100 * spec.hash() + attribNdx;
                rr::GenericVec4 attribValue = RandomArrayGenerator::generateAttributeValue(seed, attribSpec.inputType);

                m_glArrayPack->newArray(DrawTestSpec::STORAGE_USER);
                m_rrArrayPack->newArray(DrawTestSpec::STORAGE_USER);

                m_glArrayPack->getArray(attribNdx)->setupArray(false, 0, attribSpec.componentCount,
                                                               attribSpec.inputType, attribSpec.outputType, false, 0, 0,
                                                               attribValue, isPositionAttr, false);
                m_rrArrayPack->getArray(attribNdx)->setupArray(false, 0, attribSpec.componentCount,
                                                               attribSpec.inputType, attribSpec.outputType, false, 0, 0,
                                                               attribValue, isPositionAttr, false);
            }
            else
            {
                const int seed = attribSpec.hash() + 100 * spec.hash() + attribNdx;
                const size_t elementSize =
                    attribSpec.componentCount * DrawTestSpec::inputTypeSize(attribSpec.inputType);
                const size_t stride                = (attribSpec.stride == 0) ? (elementSize) : (attribSpec.stride);
                const size_t evaluatedElementCount = (instanced && attribSpec.instanceDivisor > 0) ?
                                                         (spec.instanceCount / attribSpec.instanceDivisor + 1) :
                                                         (elementCount);
                const size_t referencedElementCount =
                    (ranged) ? (de::max<size_t>(evaluatedElementCount, spec.indexMax + 1)) : (evaluatedElementCount);
                const size_t bufferSize = attribSpec.offset + stride * (referencedElementCount - 1) + elementSize;
                const char *data =
                    RandomArrayGenerator::generateArray(seed, (int)referencedElementCount, attribSpec.componentCount,
                                                        attribSpec.offset, (int)stride, attribSpec.inputType);

                try
                {
                    m_glArrayPack->newArray(attribSpec.storage);
                    m_rrArrayPack->newArray(attribSpec.storage);

                    m_glArrayPack->getArray(attribNdx)->data(DrawTestSpec::TARGET_ARRAY, bufferSize, data,
                                                             attribSpec.usage);
                    m_rrArrayPack->getArray(attribNdx)->data(DrawTestSpec::TARGET_ARRAY, bufferSize, data,
                                                             attribSpec.usage);

                    m_glArrayPack->getArray(attribNdx)->setupArray(
                        true, attribSpec.offset, attribSpec.componentCount, attribSpec.inputType, attribSpec.outputType,
                        attribSpec.normalize, attribSpec.stride, attribSpec.instanceDivisor, nullAttribValue,
                        isPositionAttr, attribSpec.bgraComponentOrder);
                    m_rrArrayPack->getArray(attribNdx)->setupArray(
                        true, attribSpec.offset, attribSpec.componentCount, attribSpec.inputType, attribSpec.outputType,
                        attribSpec.normalize, attribSpec.stride, attribSpec.instanceDivisor, nullAttribValue,
                        isPositionAttr, attribSpec.bgraComponentOrder);

                    delete[] data;
                    data = NULL;
                }
                catch (...)
                {
                    delete[] data;
                    throw;
                }
            }
        }

        // Shader program
        if (updateProgram)
        {
            m_glArrayPack->updateProgram();
            m_rrArrayPack->updateProgram();
        }

        // Draw
        try
        {
            // indices
            if (indexed)
            {
                const int seed                = spec.hash();
                const size_t indexElementSize = DrawTestSpec::indexTypeSize(spec.indexType);
                const size_t indexArraySize   = spec.indexPointerOffset + indexElementSize * elementCount;
                const char *indexArray        = RandomArrayGenerator::generateIndices(
                    seed, (int)elementCount, spec.indexType, spec.indexPointerOffset, indexMin, indexMax, indexBase);
                const char *indexPointerBase =
                    (spec.indexStorage == DrawTestSpec::STORAGE_USER) ? (indexArray) : (nullptr);
                const char *indexPointer = indexPointerBase ? indexPointerBase + spec.indexPointerOffset :
                                                              reinterpret_cast<const char *>(spec.indexPointerOffset);

                de::UniquePtr<AttributeArray> glArray(new AttributeArray(spec.indexStorage, *m_glesContext));
                de::UniquePtr<AttributeArray> rrArray(new AttributeArray(spec.indexStorage, *m_refContext));

                try
                {
                    glArray->data(DrawTestSpec::TARGET_ELEMENT_ARRAY, indexArraySize, indexArray,
                                  DrawTestSpec::USAGE_STATIC_DRAW);
                    rrArray->data(DrawTestSpec::TARGET_ELEMENT_ARRAY, indexArraySize, indexArray,
                                  DrawTestSpec::USAGE_STATIC_DRAW);

                    m_glArrayPack->render(spec.primitive, spec.drawMethod, 0, (int)primitiveElementCount,
                                          spec.indexType, indexPointer, spec.indexMin, spec.indexMax,
                                          spec.instanceCount, spec.indirectOffset, spec.baseVertex, coordScale,
                                          colorScale, glArray.get());
                    m_rrArrayPack->render(spec.primitive, spec.drawMethod, 0, (int)primitiveElementCount,
                                          spec.indexType, indexPointer, spec.indexMin, spec.indexMax,
                                          spec.instanceCount, spec.indirectOffset, spec.baseVertex, coordScale,
                                          colorScale, rrArray.get());

                    delete[] indexArray;
                    indexArray = NULL;
                }
                catch (...)
                {
                    delete[] indexArray;
                    throw;
                }
            }
            else
            {
                m_glArrayPack->render(spec.primitive, spec.drawMethod, spec.first, (int)primitiveElementCount,
                                      DrawTestSpec::INDEXTYPE_LAST, nullptr, 0, 0, spec.instanceCount,
                                      spec.indirectOffset, 0, coordScale, colorScale, nullptr);
                m_testCtx.touchWatchdog();
                m_rrArrayPack->render(spec.primitive, spec.drawMethod, spec.first, (int)primitiveElementCount,
                                      DrawTestSpec::INDEXTYPE_LAST, nullptr, 0, 0, spec.instanceCount,
                                      spec.indirectOffset, 0, coordScale, colorScale, nullptr);
            }
        }
        catch (glu::Error &err)
        {
            // GL Errors are ok if the mode is not properly aligned

            const DrawTestSpec::CompatibilityTestType ctype = spec.isCompatibilityTest();

            m_testCtx.getLog() << TestLog::Message << "Got error: " << err.what() << TestLog::EndMessage;

            if (ctype == DrawTestSpec::COMPATIBILITY_UNALIGNED_OFFSET)
                m_result.addResult(QP_TEST_RESULT_COMPATIBILITY_WARNING, "Failed to draw with unaligned buffers.");
            else if (ctype == DrawTestSpec::COMPATIBILITY_UNALIGNED_STRIDE)
                m_result.addResult(QP_TEST_RESULT_COMPATIBILITY_WARNING, "Failed to draw with unaligned stride.");
            else
                throw;
        }
    }
    else if (compareStep)
    {
        if (!compare(spec.primitive))
        {
            const DrawTestSpec::CompatibilityTestType ctype = spec.isCompatibilityTest();

            if (ctype == DrawTestSpec::COMPATIBILITY_UNALIGNED_OFFSET)
                m_result.addResult(QP_TEST_RESULT_COMPATIBILITY_WARNING, "Failed to draw with unaligned buffers.");
            else if (ctype == DrawTestSpec::COMPATIBILITY_UNALIGNED_STRIDE)
                m_result.addResult(QP_TEST_RESULT_COMPATIBILITY_WARNING, "Failed to draw with unaligned stride.");
            else
                m_result.addResult(QP_TEST_RESULT_FAIL, "Image comparison failed.");
        }
    }
    else
    {
        DE_ASSERT(false);
        return STOP;
    }

    m_result.setTestContextResult(m_testCtx);

    m_iteration++;
    return iterateResult;
}

static bool isBlack(const tcu::RGBA &c)
{
    // ignore alpha channel
    return c.getRed() == 0 && c.getGreen() == 0 && c.getBlue() == 0;
}

static bool isEdgeTripletComponent(int c1, int c2, int c3, int renderTargetDifference)
{
    const int roundingDifference = 2 * renderTargetDifference; // src and dst pixels rounded to different directions
    const int d1                 = c2 - c1;
    const int d2                 = c3 - c2;
    const int rampDiff           = de::abs(d2 - d1);

    return rampDiff > roundingDifference;
}

static bool isEdgeTriplet(const tcu::RGBA &c1, const tcu::RGBA &c2, const tcu::RGBA &c3,
                          const tcu::IVec3 &renderTargetThreshold)
{
    // black (background color) and non-black is always an edge
    {
        const bool b1 = isBlack(c1);
        const bool b2 = isBlack(c2);
        const bool b3 = isBlack(c3);

        // both pixels with coverage and pixels without coverage
        if ((b1 && b2 && b3) == false && (b1 || b2 || b3) == true)
            return true;
        // all black
        if (b1 && b2 && b3)
            return false;
        // all with coverage
        DE_ASSERT(!b1 && !b2 && !b3);
    }

    // Color is always linearly interpolated => component values change nearly linearly
    // in any constant direction on triangle hull. (df/dx ~= C).

    // Edge detection (this function) is run against the reference image
    // => no dithering to worry about

    return isEdgeTripletComponent(c1.getRed(), c2.getRed(), c3.getRed(), renderTargetThreshold.x()) ||
           isEdgeTripletComponent(c1.getGreen(), c2.getGreen(), c3.getGreen(), renderTargetThreshold.y()) ||
           isEdgeTripletComponent(c1.getBlue(), c2.getBlue(), c3.getBlue(), renderTargetThreshold.z());
}

static bool pixelNearEdge(int x, int y, const tcu::Surface &ref, const tcu::IVec3 &renderTargetThreshold)
{
    // should not be called for edge pixels
    DE_ASSERT(x >= 1 && x <= ref.getWidth() - 2);
    DE_ASSERT(y >= 1 && y <= ref.getHeight() - 2);

    // horizontal

    for (int dy = -1; dy < 2; ++dy)
    {
        const tcu::RGBA c1 = ref.getPixel(x - 1, y + dy);
        const tcu::RGBA c2 = ref.getPixel(x, y + dy);
        const tcu::RGBA c3 = ref.getPixel(x + 1, y + dy);
        if (isEdgeTriplet(c1, c2, c3, renderTargetThreshold))
            return true;
    }

    // vertical

    for (int dx = -1; dx < 2; ++dx)
    {
        const tcu::RGBA c1 = ref.getPixel(x + dx, y - 1);
        const tcu::RGBA c2 = ref.getPixel(x + dx, y);
        const tcu::RGBA c3 = ref.getPixel(x + dx, y + 1);
        if (isEdgeTriplet(c1, c2, c3, renderTargetThreshold))
            return true;
    }

    return false;
}

static uint32_t getVisualizationGrayscaleColor(const tcu::RGBA &c)
{
    // make triangle coverage and error pixels obvious by converting coverage to grayscale
    if (isBlack(c))
        return 0;
    else
        return 50u + (uint32_t)(c.getRed() + c.getBlue() + c.getGreen()) / 8u;
}

static bool pixelNearLineIntersection(int x, int y, const tcu::Surface &target)
{
    // should not be called for edge pixels
    DE_ASSERT(x >= 1 && x <= target.getWidth() - 2);
    DE_ASSERT(y >= 1 && y <= target.getHeight() - 2);

    int coveredPixels = 0;

    for (int dy = -1; dy < 2; dy++)
        for (int dx = -1; dx < 2; dx++)
        {
            const bool targetCoverage = !isBlack(target.getPixel(x + dx, y + dy));
            if (targetCoverage)
            {
                ++coveredPixels;

                // A single thin line cannot have more than 3 covered pixels in a 3x3 area
                if (coveredPixels >= 4)
                    return true;
            }
        }

    return false;
}

static inline bool colorsEqual(const tcu::RGBA &colorA, const tcu::RGBA &colorB, const tcu::IVec3 &compareThreshold)
{
    enum
    {
        TCU_RGBA_RGB_MASK = tcu::RGBA::RED_MASK | tcu::RGBA::GREEN_MASK | tcu::RGBA::BLUE_MASK
    };

    return tcu::compareThresholdMasked(colorA, colorB,
                                       tcu::RGBA(compareThreshold.x(), compareThreshold.y(), compareThreshold.z(), 0),
                                       TCU_RGBA_RGB_MASK);
}

// search 3x3 are for matching color
static bool pixelNeighborhoodContainsColor(const tcu::Surface &target, int x, int y, const tcu::RGBA &color,
                                           const tcu::IVec3 &compareThreshold)
{
    // should not be called for edge pixels
    DE_ASSERT(x >= 1 && x <= target.getWidth() - 2);
    DE_ASSERT(y >= 1 && y <= target.getHeight() - 2);

    for (int dy = -1; dy < 2; dy++)
        for (int dx = -1; dx < 2; dx++)
        {
            const tcu::RGBA targetCmpPixel = target.getPixel(x + dx, y + dy);
            if (colorsEqual(color, targetCmpPixel, compareThreshold))
                return true;
        }

    return false;
}

// search 3x3 are for matching coverage (coverage == (color != background color))
static bool pixelNeighborhoodContainsCoverage(const tcu::Surface &target, int x, int y, bool coverage)
{
    // should not be called for edge pixels
    DE_ASSERT(x >= 1 && x <= target.getWidth() - 2);
    DE_ASSERT(y >= 1 && y <= target.getHeight() - 2);

    for (int dy = -1; dy < 2; dy++)
        for (int dx = -1; dx < 2; dx++)
        {
            const bool targetCmpCoverage = !isBlack(target.getPixel(x + dx, y + dy));
            if (targetCmpCoverage == coverage)
                return true;
        }

    return false;
}

static bool edgeRelaxedImageCompare(tcu::TestLog &log, const char *imageSetName, const char *imageSetDesc,
                                    const tcu::Surface &reference, const tcu::Surface &result,
                                    const tcu::IVec3 &compareThreshold, const tcu::IVec3 &renderTargetThreshold,
                                    int maxAllowedInvalidPixels)
{
    DE_ASSERT(result.getWidth() == reference.getWidth() && result.getHeight() == reference.getHeight());

    const tcu::IVec4 green(0, 255, 0, 255);
    const tcu::IVec4 red(255, 0, 0, 255);
    const int width  = reference.getWidth();
    const int height = reference.getHeight();
    tcu::TextureLevel errorMask(tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8), width,
                                height);
    const tcu::PixelBufferAccess errorAccess = errorMask.getAccess();
    int numFailingPixels                     = 0;

    // clear errormask edges which would otherwise be transparent

    tcu::clear(tcu::getSubregion(errorAccess, 0, 0, width, 1), green);
    tcu::clear(tcu::getSubregion(errorAccess, 0, height - 1, width, 1), green);
    tcu::clear(tcu::getSubregion(errorAccess, 0, 0, 1, height), green);
    tcu::clear(tcu::getSubregion(errorAccess, width - 1, 0, 1, height), green);

    // skip edge pixels since coverage on edge cannot be verified

    for (int y = 1; y < height - 1; ++y)
        for (int x = 1; x < width - 1; ++x)
        {
            const tcu::RGBA refPixel    = reference.getPixel(x, y);
            const tcu::RGBA screenPixel = result.getPixel(x, y);
            const bool directMatch      = colorsEqual(refPixel, screenPixel, compareThreshold);
            const bool isOkReferencePixel =
                directMatch ||
                pixelNeighborhoodContainsColor(
                    result, x, y, refPixel,
                    compareThreshold); // screen image has a matching pixel nearby (~= If something is drawn on reference, it must be drawn to screen too.)
            const bool isOkScreenPixel =
                directMatch ||
                pixelNeighborhoodContainsColor(
                    reference, x, y, screenPixel,
                    compareThreshold); // reference image has a matching pixel nearby (~= If something is drawn on screen, it must be drawn to reference too.)

            if (isOkScreenPixel && isOkReferencePixel)
            {
                // pixel valid, write greenish pixels to make the result image easier to read
                const uint32_t grayscaleValue = getVisualizationGrayscaleColor(screenPixel);
                errorAccess.setPixel(tcu::UVec4(grayscaleValue, 255, grayscaleValue, 255), x, y);
            }
            else if (!pixelNearEdge(x, y, reference, renderTargetThreshold))
            {
                // non-edge pixel values must be within threshold of the reference values
                errorAccess.setPixel(red, x, y);
                ++numFailingPixels;
            }
            else
            {
                // we are on/near an edge, verify only coverage (coverage == not background colored)
                const bool referenceCoverage     = !isBlack(refPixel);
                const bool screenCoverage        = !isBlack(screenPixel);
                const bool isOkReferenceCoverage = pixelNeighborhoodContainsCoverage(
                    result, x, y, referenceCoverage); // Check reference pixel against screen pixel
                const bool isOkScreenCoverage = pixelNeighborhoodContainsCoverage(
                    reference, x, y, screenCoverage); // Check screen pixels against reference pixel

                if (isOkScreenCoverage && isOkReferenceCoverage)
                {
                    // pixel valid, write greenish pixels to make the result image easier to read
                    const uint32_t grayscaleValue = getVisualizationGrayscaleColor(screenPixel);
                    errorAccess.setPixel(tcu::UVec4(grayscaleValue, 255, grayscaleValue, 255), x, y);
                }
                else
                {
                    // coverage does not match
                    errorAccess.setPixel(red, x, y);
                    ++numFailingPixels;
                }
            }
        }

    log << TestLog::Message << "Comparing images:\n"
        << "\tallowed deviation in pixel positions = 1\n"
        << "\tnumber of allowed invalid pixels = " << maxAllowedInvalidPixels << "\n"
        << "\tnumber of invalid pixels = " << numFailingPixels << TestLog::EndMessage;

    if (numFailingPixels > maxAllowedInvalidPixels)
    {
        log << TestLog::Message << "Image comparison failed. Color threshold = (" << compareThreshold.x() << ", "
            << compareThreshold.y() << ", " << compareThreshold.z() << ")" << TestLog::EndMessage
            << TestLog::ImageSet(imageSetName, imageSetDesc) << TestLog::Image("Result", "Result", result)
            << TestLog::Image("Reference", "Reference", reference)
            << TestLog::Image("ErrorMask", "Error mask", errorMask) << TestLog::EndImageSet;

        return false;
    }
    else
    {
        log << TestLog::ImageSet(imageSetName, imageSetDesc) << TestLog::Image("Result", "Result", result)
            << TestLog::EndImageSet;

        return true;
    }
}

static bool intersectionRelaxedLineImageCompare(tcu::TestLog &log, const char *imageSetName, const char *imageSetDesc,
                                                const tcu::Surface &reference, const tcu::Surface &result,
                                                const tcu::IVec3 &compareThreshold, int maxAllowedInvalidPixels)
{
    DE_ASSERT(result.getWidth() == reference.getWidth() && result.getHeight() == reference.getHeight());

    const tcu::IVec4 green(0, 255, 0, 255);
    const tcu::IVec4 red(255, 0, 0, 255);
    const int width  = reference.getWidth();
    const int height = reference.getHeight();
    tcu::TextureLevel errorMask(tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8), width,
                                height);
    const tcu::PixelBufferAccess errorAccess = errorMask.getAccess();
    int numFailingPixels                     = 0;

    // clear errormask edges which would otherwise be transparent

    tcu::clear(tcu::getSubregion(errorAccess, 0, 0, width, 1), green);
    tcu::clear(tcu::getSubregion(errorAccess, 0, height - 1, width, 1), green);
    tcu::clear(tcu::getSubregion(errorAccess, 0, 0, 1, height), green);
    tcu::clear(tcu::getSubregion(errorAccess, width - 1, 0, 1, height), green);

    // skip edge pixels since coverage on edge cannot be verified

    for (int y = 1; y < height - 1; ++y)
        for (int x = 1; x < width - 1; ++x)
        {
            const tcu::RGBA refPixel    = reference.getPixel(x, y);
            const tcu::RGBA screenPixel = result.getPixel(x, y);
            const bool directMatch      = colorsEqual(refPixel, screenPixel, compareThreshold);
            const bool isOkScreenPixel =
                directMatch ||
                pixelNeighborhoodContainsColor(
                    reference, x, y, screenPixel,
                    compareThreshold); // reference image has a matching pixel nearby (~= If something is drawn on screen, it must be drawn to reference too.)
            const bool isOkReferencePixel =
                directMatch ||
                pixelNeighborhoodContainsColor(
                    result, x, y, refPixel,
                    compareThreshold); // screen image has a matching pixel nearby (~= If something is drawn on reference, it must be drawn to screen too.)

            if (isOkScreenPixel && isOkReferencePixel)
            {
                // pixel valid, write greenish pixels to make the result image easier to read
                const uint32_t grayscaleValue = getVisualizationGrayscaleColor(screenPixel);
                errorAccess.setPixel(tcu::UVec4(grayscaleValue, 255, grayscaleValue, 255), x, y);
            }
            else if (!pixelNearLineIntersection(x, y, reference) && !pixelNearLineIntersection(x, y, result))
            {
                // non-intersection pixel values must be within threshold of the reference values
                errorAccess.setPixel(red, x, y);
                ++numFailingPixels;
            }
            else
            {
                // pixel is near a line intersection
                // we are on/near an edge, verify only coverage (coverage == not background colored)
                const bool referenceCoverage  = !isBlack(refPixel);
                const bool screenCoverage     = !isBlack(screenPixel);
                const bool isOkScreenCoverage = pixelNeighborhoodContainsCoverage(
                    reference, x, y, screenCoverage); // Check screen pixels against reference pixel
                const bool isOkReferenceCoverage = pixelNeighborhoodContainsCoverage(
                    result, x, y, referenceCoverage); // Check reference pixel against screen pixel

                if (isOkScreenCoverage && isOkReferenceCoverage)
                {
                    // pixel valid, write greenish pixels to make the result image easier to read
                    const uint32_t grayscaleValue = getVisualizationGrayscaleColor(screenPixel);
                    errorAccess.setPixel(tcu::UVec4(grayscaleValue, 255, grayscaleValue, 255), x, y);
                }
                else
                {
                    // coverage does not match
                    errorAccess.setPixel(red, x, y);
                    ++numFailingPixels;
                }
            }
        }

    log << TestLog::Message << "Comparing images:\n"
        << "\tallowed deviation in pixel positions = 1\n"
        << "\tnumber of allowed invalid pixels = " << maxAllowedInvalidPixels << "\n"
        << "\tnumber of invalid pixels = " << numFailingPixels << TestLog::EndMessage;

    if (numFailingPixels > maxAllowedInvalidPixels)
    {
        log << TestLog::Message << "Image comparison failed. Color threshold = (" << compareThreshold.x() << ", "
            << compareThreshold.y() << ", " << compareThreshold.z() << ")" << TestLog::EndMessage
            << TestLog::ImageSet(imageSetName, imageSetDesc) << TestLog::Image("Result", "Result", result)
            << TestLog::Image("Reference", "Reference", reference)
            << TestLog::Image("ErrorMask", "Error mask", errorMask) << TestLog::EndImageSet;

        return false;
    }
    else
    {
        log << TestLog::ImageSet(imageSetName, imageSetDesc) << TestLog::Image("Result", "Result", result)
            << TestLog::EndImageSet;

        return true;
    }
}

bool DrawTest::compare(gls::DrawTestSpec::Primitive primitiveType)
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
        return tcu::fuzzyCompare(m_testCtx.getLog(), "Compare Results", "Compare Results", ref.getAccess(),
                                 screen.getAccess(), 0.3f, tcu::COMPARE_LOG_RESULT);
    }
    else
    {
        const PrimitiveClass primitiveClass            = getDrawPrimitiveClass(primitiveType);
        const int maxAllowedInvalidPixelsWithPoints    = 0; //!< points are unlikely to have overlapping fragments
        const int maxAllowedInvalidPixelsWithLines     = 5; //!< line are allowed to have a few bad pixels
        const int maxAllowedInvalidPixelsWithTriangles = 10;

        switch (primitiveClass)
        {
        case PRIMITIVECLASS_POINT:
        {
            // Point are extremely unlikely to have overlapping regions, don't allow any no extra / missing pixels
            return tcu::intThresholdPositionDeviationErrorThresholdCompare(
                m_testCtx.getLog(), "CompareResult", "Result of rendering", ref.getAccess(), screen.getAccess(),
                tcu::UVec4(m_maxDiffRed, m_maxDiffGreen, m_maxDiffBlue, 256),
                tcu::IVec3(1, 1, 0),               //!< 3x3 search kernel
                true,                              //!< relax comparison on the image boundary
                maxAllowedInvalidPixelsWithPoints, //!< error threshold
                tcu::COMPARE_LOG_RESULT);
        }

        case PRIMITIVECLASS_LINE:
        {
            // Lines can potentially have a large number of overlapping pixels. Pixel comparison may potentially produce
            // false negatives in such pixels if for example the pixel in question is overdrawn by another line in the
            // reference image but not in the resultin image. Relax comparison near line intersection points (areas) and
            // compare only coverage, not color, in such pixels
            return intersectionRelaxedLineImageCompare(m_testCtx.getLog(), "CompareResult", "Result of rendering", ref,
                                                       screen, tcu::IVec3(m_maxDiffRed, m_maxDiffGreen, m_maxDiffBlue),
                                                       maxAllowedInvalidPixelsWithLines);
        }

        case PRIMITIVECLASS_TRIANGLE:
        {
            // Triangles are likely to partially or fully overlap. Pixel difference comparison is fragile in pixels
            // where there could be potential overlapping since the  pixels might be covered by one triangle in the
            // reference image and by the other in the result image. Relax comparsion near primitive edges and
            // compare only coverage, not color, in such pixels.
            const tcu::IVec3 renderTargetThreshold =
                m_renderCtx.getRenderTarget().getPixelFormat().getColorThreshold().toIVec().xyz();

            return edgeRelaxedImageCompare(m_testCtx.getLog(), "CompareResult", "Result of rendering", ref, screen,
                                           tcu::IVec3(m_maxDiffRed, m_maxDiffGreen, m_maxDiffBlue),
                                           renderTargetThreshold, maxAllowedInvalidPixelsWithTriangles);
        }

        default:
            DE_ASSERT(false);
            return false;
        }
    }
}

float DrawTest::getCoordScale(const DrawTestSpec &spec) const
{
    float maxValue = 1.0f;

    for (int arrayNdx = 0; arrayNdx < (int)spec.attribs.size(); arrayNdx++)
    {
        DrawTestSpec::AttributeSpec attribSpec = spec.attribs[arrayNdx];
        const bool isPositionAttr              = (arrayNdx == 0) || (attribSpec.additionalPositionAttribute);
        float attrMaxValue                     = 0;

        if (!isPositionAttr)
            continue;

        if (attribSpec.inputType == DrawTestSpec::INPUTTYPE_UNSIGNED_INT_2_10_10_10)
        {
            if (attribSpec.normalize)
                attrMaxValue += 1.0f;
            else
                attrMaxValue += 1024.0f;
        }
        else if (attribSpec.inputType == DrawTestSpec::INPUTTYPE_INT_2_10_10_10)
        {
            if (attribSpec.normalize)
                attrMaxValue += 1.0f;
            else
                attrMaxValue += 512.0f;
        }
        else
        {
            const float max = GLValue::getMaxValue(attribSpec.inputType).toFloat();

            attrMaxValue +=
                (attribSpec.normalize && !inputTypeIsFloatType(attribSpec.inputType)) ? (1.0f) : (max * 1.1f);
        }

        if (attribSpec.outputType == DrawTestSpec::OUTPUTTYPE_VEC3 ||
            attribSpec.outputType == DrawTestSpec::OUTPUTTYPE_VEC4 ||
            attribSpec.outputType == DrawTestSpec::OUTPUTTYPE_IVEC3 ||
            attribSpec.outputType == DrawTestSpec::OUTPUTTYPE_IVEC4 ||
            attribSpec.outputType == DrawTestSpec::OUTPUTTYPE_UVEC3 ||
            attribSpec.outputType == DrawTestSpec::OUTPUTTYPE_UVEC4)
            attrMaxValue *= 2;

        maxValue += attrMaxValue;
    }

    return 1.0f / maxValue;
}

float DrawTest::getColorScale(const DrawTestSpec &spec) const
{
    float colorScale = 1.0f;

    for (int arrayNdx = 1; arrayNdx < (int)spec.attribs.size(); arrayNdx++)
    {
        DrawTestSpec::AttributeSpec attribSpec = spec.attribs[arrayNdx];
        const bool isPositionAttr              = (arrayNdx == 0) || (attribSpec.additionalPositionAttribute);

        if (isPositionAttr)
            continue;

        if (attribSpec.inputType == DrawTestSpec::INPUTTYPE_UNSIGNED_INT_2_10_10_10)
        {
            if (!attribSpec.normalize)
                colorScale *= 1.0f / 1024.0f;
        }
        else if (attribSpec.inputType == DrawTestSpec::INPUTTYPE_INT_2_10_10_10)
        {
            if (!attribSpec.normalize)
                colorScale *= 1.0f / 512.0f;
        }
        else
        {
            const float max = GLValue::getMaxValue(attribSpec.inputType).toFloat();

            colorScale *=
                (attribSpec.normalize && !inputTypeIsFloatType(attribSpec.inputType) ? 1.0f : float(1.0 / double(max)));
            if (attribSpec.outputType == DrawTestSpec::OUTPUTTYPE_VEC4 ||
                attribSpec.outputType == DrawTestSpec::OUTPUTTYPE_UVEC4 ||
                attribSpec.outputType == DrawTestSpec::OUTPUTTYPE_IVEC4)
                colorScale *=
                    (attribSpec.normalize && !inputTypeIsFloatType(attribSpec.inputType) ? 1.0f :
                                                                                           float(1.0 / double(max)));
        }
    }

    return colorScale;
}

} // namespace gls
} // namespace deqp
