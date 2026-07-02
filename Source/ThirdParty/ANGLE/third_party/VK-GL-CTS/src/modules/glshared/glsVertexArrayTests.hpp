#ifndef _GLSVERTEXARRAYTESTS_HPP
#define _GLSVERTEXARRAYTESTS_HPP
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

#include "tcuTestCase.hpp"
#include "tcuVector.hpp"
#include "tcuSurface.hpp"
#include "gluRenderContext.hpp"
#include "gluCallLogWrapper.hpp"
#include "tcuTestLog.hpp"
#include "gluShaderProgram.hpp"
#include "deFloat16.h"
#include "deMath.h"
#include "tcuFloat.hpp"
#include "tcuPixelFormat.hpp"
#include "sglrContext.hpp"

namespace sglr
{

class ReferenceContextBuffers;
class ReferenceContext;
class Context;

} // namespace sglr

namespace deqp
{
namespace gls
{

class Array
{
public:
    enum Target
    {
        // \note [mika] Are these actualy used somewhere?
        TARGET_ELEMENT_ARRAY = 0,
        TARGET_ARRAY,

        TARGET_LAST
    };

    enum InputType
    {
        INPUTTYPE_FLOAT = 0,
        INPUTTYPE_FIXED,
        INPUTTYPE_DOUBLE,

        INPUTTYPE_BYTE,
        INPUTTYPE_SHORT,

        INPUTTYPE_UNSIGNED_BYTE,
        INPUTTYPE_UNSIGNED_SHORT,

        INPUTTYPE_INT,
        INPUTTYPE_UNSIGNED_INT,
        INPUTTYPE_HALF,
        INPUTTYPE_UNSIGNED_INT_2_10_10_10,
        INPUTTYPE_INT_2_10_10_10,

        INPUTTYPE_LAST
    };

    enum OutputType
    {
        OUTPUTTYPE_FLOAT = 0,
        OUTPUTTYPE_VEC2,
        OUTPUTTYPE_VEC3,
        OUTPUTTYPE_VEC4,

        OUTPUTTYPE_INT,
        OUTPUTTYPE_UINT,

        OUTPUTTYPE_IVEC2,
        OUTPUTTYPE_IVEC3,
        OUTPUTTYPE_IVEC4,

        OUTPUTTYPE_UVEC2,
        OUTPUTTYPE_UVEC3,
        OUTPUTTYPE_UVEC4,

        OUTPUTTYPE_LAST
    };

    enum Usage
    {
        USAGE_DYNAMIC_DRAW = 0,
        USAGE_STATIC_DRAW,
        USAGE_STREAM_DRAW,

        USAGE_STREAM_READ,
        USAGE_STREAM_COPY,

        USAGE_STATIC_READ,
        USAGE_STATIC_COPY,

        USAGE_DYNAMIC_READ,
        USAGE_DYNAMIC_COPY,

        USAGE_LAST
    };

    enum Storage
    {
        STORAGE_USER = 0,
        STORAGE_BUFFER,

        STORAGE_LAST
    };

    enum Primitive
    {
        PRIMITIVE_POINTS = 0,
        PRIMITIVE_TRIANGLES,
        PRIMITIVE_TRIANGLE_FAN,
        PRIMITIVE_TRIANGLE_STRIP,

        PRIMITIVE_LAST
    };

    static std::string targetToString(Target target);
    static std::string inputTypeToString(InputType type);
    static std::string outputTypeToString(OutputType type);
    static std::string usageTypeToString(Usage usage);
    static std::string storageToString(Storage storage);
    static std::string primitiveToString(Primitive primitive);
    static int inputTypeSize(InputType type);

    virtual ~Array(void)
    {
    }
    virtual void data(Target target, int size, const char *data, Usage usage)   = 0;
    virtual void subdata(Target target, int offset, int size, const char *data) = 0;
    virtual void bind(int attribNdx, int offset, int size, InputType inType, OutputType outType, bool normalized,
                      int stride)                                               = 0;
    virtual void unBind(void)                                                   = 0;

    virtual bool isBound(void) const             = 0;
    virtual int getComponentCount(void) const    = 0;
    virtual Target getTarget(void) const         = 0;
    virtual InputType getInputType(void) const   = 0;
    virtual OutputType getOutputType(void) const = 0;
    virtual Storage getStorageType(void) const   = 0;
    virtual bool getNormalized(void) const       = 0;
    virtual int getStride(void) const            = 0;
    virtual int getAttribNdx(void) const         = 0;
    virtual void setAttribNdx(int attribNdx)     = 0;
};

class ContextArray : public Array
{
public:
    ContextArray(Storage storage, sglr::Context &context);
    virtual ~ContextArray(void);
    virtual void data(Target target, int size, const char *data, Usage usage);
    virtual void subdata(Target target, int offset, int size, const char *data);
    virtual void bind(int attribNdx, int offset, int size, InputType inType, OutputType outType, bool normalized,
                      int stride);
    virtual void bindIndexArray(Array::Target storage);
    virtual void unBind(void)
    {
        m_bound = false;
    }
    virtual bool isBound(void) const
    {
        return m_bound;
    }

    virtual int getComponentCount(void) const
    {
        return m_componentCount;
    }
    virtual Array::Target getTarget(void) const
    {
        return m_target;
    }
    virtual Array::InputType getInputType(void) const
    {
        return m_inputType;
    }
    virtual Array::OutputType getOutputType(void) const
    {
        return m_outputType;
    }
    virtual Array::Storage getStorageType(void) const
    {
        return m_storage;
    }
    virtual bool getNormalized(void) const
    {
        return m_normalize;
    }
    virtual int getStride(void) const
    {
        return m_stride;
    }
    virtual int getAttribNdx(void) const
    {
        return m_attribNdx;
    }
    virtual void setAttribNdx(int attribNdx)
    {
        m_attribNdx = attribNdx;
    }

    void glBind(uint32_t loc);
    static uint32_t targetToGL(Array::Target target);
    static uint32_t usageToGL(Array::Usage usage);
    static uint32_t inputTypeToGL(Array::InputType type);
    static std::string outputTypeToGLType(Array::OutputType type);
    static uint32_t primitiveToGL(Array::Primitive primitive);

private:
    Storage m_storage;
    sglr::Context &m_ctx;
    uint32_t m_glBuffer;

    bool m_bound;
    int m_attribNdx;
    int m_size;
    char *m_data;
    int m_componentCount;
    Array::Target m_target;
    Array::InputType m_inputType;
    Array::OutputType m_outputType;
    bool m_normalize;
    int m_stride;
    int m_offset;
};

class ContextArrayPack
{
public:
    ContextArrayPack(glu::RenderContext &renderCtx, sglr::Context &drawContext);
    virtual ~ContextArrayPack(void);
    virtual Array *getArray(int i);
    virtual int getArrayCount(void);
    virtual void newArray(Array::Storage storage);
    virtual void render(Array::Primitive primitive, int firstVertex, int vertexCount, bool useVao, float coordScale,
                        float colorScale);

    const tcu::Surface &getSurface(void) const
    {
        return m_screen;
    }

private:
    void updateProgram(void);
    glu::RenderContext &m_renderCtx;
    sglr::Context &m_ctx;

    std::vector<ContextArray *> m_arrays;
    sglr::ShaderProgram *m_program;
    tcu::Surface m_screen;
};

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
        static WrappedType<Type> fromFloat(float value)
        {
            WrappedType<Type> v;
            v.m_value = (Type)value;
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
        inline WrappedType<Type> operator%(const WrappedType<Type> &other) const
        {
            return WrappedType<Type>::create((Type)(m_value % other.getValue()));
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

    template <class Type>
    class WrappedFloatType
    {
    public:
        static WrappedFloatType<Type> create(Type value)
        {
            WrappedFloatType<Type> v;
            v.m_value = value;
            return v;
        }
        static WrappedFloatType<Type> fromFloat(float value)
        {
            WrappedFloatType<Type> v;
            v.m_value = (Type)value;
            return v;
        }
        inline Type getValue(void) const
        {
            return m_value;
        }

        inline WrappedFloatType<Type> operator+(const WrappedFloatType<Type> &other) const
        {
            return WrappedFloatType<Type>::create((Type)(m_value + other.getValue()));
        }
        inline WrappedFloatType<Type> operator*(const WrappedFloatType<Type> &other) const
        {
            return WrappedFloatType<Type>::create((Type)(m_value * other.getValue()));
        }
        inline WrappedFloatType<Type> operator/(const WrappedFloatType<Type> &other) const
        {
            return WrappedFloatType<Type>::create((Type)(m_value / other.getValue()));
        }
        inline WrappedFloatType<Type> operator%(const WrappedFloatType<Type> &other) const
        {
            return WrappedFloatType<Type>::create((Type)(deMod(m_value, other.getValue())));
        }
        inline WrappedFloatType<Type> operator-(const WrappedFloatType<Type> &other) const
        {
            return WrappedFloatType<Type>::create((Type)(m_value - other.getValue()));
        }

        inline WrappedFloatType<Type> &operator+=(const WrappedFloatType<Type> &other)
        {
            m_value += other.getValue();
            return *this;
        }
        inline WrappedFloatType<Type> &operator*=(const WrappedFloatType<Type> &other)
        {
            m_value *= other.getValue();
            return *this;
        }
        inline WrappedFloatType<Type> &operator/=(const WrappedFloatType<Type> &other)
        {
            m_value /= other.getValue();
            return *this;
        }
        inline WrappedFloatType<Type> &operator-=(const WrappedFloatType<Type> &other)
        {
            m_value -= other.getValue();
            return *this;
        }

        inline bool operator==(const WrappedFloatType<Type> &other) const
        {
            return m_value == other.m_value;
        }
        inline bool operator!=(const WrappedFloatType<Type> &other) const
        {
            return m_value != other.m_value;
        }
        inline bool operator<(const WrappedFloatType<Type> &other) const
        {
            return m_value < other.m_value;
        }
        inline bool operator>(const WrappedFloatType<Type> &other) const
        {
            return m_value > other.m_value;
        }
        inline bool operator<=(const WrappedFloatType<Type> &other) const
        {
            return m_value <= other.m_value;
        }
        inline bool operator>=(const WrappedFloatType<Type> &other) const
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

    typedef WrappedFloatType<float> Float;
    typedef WrappedFloatType<double> Double;

    typedef WrappedType<int32_t> Int;
    typedef WrappedType<uint32_t> Uint;

    class Half
    {
    public:
        static Half create(float value)
        {
            Half h;
            h.m_value = floatToHalf(value);
            return h;
        }
        static Half fromFloat(float value)
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
        inline Half operator%(const Half &other) const
        {
            return create(deFloatMod(halfToFloat(m_value), halfToFloat(other.getValue())));
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
        static Fixed fromFloat(float value)
        {
            Fixed v;
            v.m_value = (int32_t)value;
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
        inline Fixed operator%(const Fixed &other) const
        {
            return create(m_value % other.getValue());
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
    GLValue(void) : type(Array::INPUTTYPE_LAST)
    {
    }
    explicit GLValue(Float value) : type(Array::INPUTTYPE_FLOAT), fl(value)
    {
    }
    explicit GLValue(Fixed value) : type(Array::INPUTTYPE_FIXED), fi(value)
    {
    }
    explicit GLValue(Byte value) : type(Array::INPUTTYPE_BYTE), b(value)
    {
    }
    explicit GLValue(Ubyte value) : type(Array::INPUTTYPE_UNSIGNED_BYTE), ub(value)
    {
    }
    explicit GLValue(Short value) : type(Array::INPUTTYPE_SHORT), s(value)
    {
    }
    explicit GLValue(Ushort value) : type(Array::INPUTTYPE_UNSIGNED_SHORT), us(value)
    {
    }
    explicit GLValue(Int value) : type(Array::INPUTTYPE_INT), i(value)
    {
    }
    explicit GLValue(Uint value) : type(Array::INPUTTYPE_UNSIGNED_INT), ui(value)
    {
    }
    explicit GLValue(Half value) : type(Array::INPUTTYPE_HALF), h(value)
    {
    }
    explicit GLValue(Double value) : type(Array::INPUTTYPE_DOUBLE), d(value)
    {
    }

    float toFloat(void) const;

    static GLValue getMaxValue(Array::InputType type);
    static GLValue getMinValue(Array::InputType type);

    Array::InputType type;

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

class VertexArrayTest : public tcu::TestCase
{
public:
    VertexArrayTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name, const char *desc);
    virtual ~VertexArrayTest(void);
    virtual void init(void);
    virtual void deinit(void);

protected:
    VertexArrayTest(const VertexArrayTest &other);
    VertexArrayTest &operator=(const VertexArrayTest &other);

    void compare(void);

    glu::RenderContext &m_renderCtx;

    sglr::ReferenceContextBuffers *m_refBuffers;
    sglr::ReferenceContext *m_refContext;
    sglr::Context *m_glesContext;

    ContextArrayPack *m_glArrayPack;
    ContextArrayPack *m_rrArrayPack;
    bool m_isOk;

    int m_maxDiffRed;
    int m_maxDiffGreen;
    int m_maxDiffBlue;
};

class MultiVertexArrayTest : public VertexArrayTest
{
public:
    class Spec
    {
    public:
        class ArraySpec
        {
        public:
            ArraySpec(Array::InputType inputType, Array::OutputType outputType, Array::Storage storage,
                      Array::Usage usage, int componetCount, int offset, int stride, bool normalize, GLValue min,
                      GLValue max);

            Array::InputType inputType;
            Array::OutputType outputType;
            Array::Storage storage;
            Array::Usage usage;
            int componentCount;
            int offset;
            int stride;
            bool normalize;
            GLValue min;
            GLValue max;
        };

        std::string getName(void) const;
        std::string getDesc(void) const;

        Array::Primitive primitive;
        int drawCount; //!<Number of primitives to draw
        int first;

        std::vector<ArraySpec> arrays;
    };

    MultiVertexArrayTest(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const Spec &spec, const char *name,
                         const char *desc);
    virtual ~MultiVertexArrayTest(void);
    virtual IterateResult iterate(void);

private:
    bool isUnalignedBufferOffsetTest(void) const;
    bool isUnalignedBufferStrideTest(void) const;

    Spec m_spec;
    int m_iteration;
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

} // namespace gls
} // namespace deqp

#endif // _GLSVERTEXARRAYTESTS_HPP
