/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
 * -------------------------------------------------
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
 * \brief Shader operators tests.
 *//*--------------------------------------------------------------------*/

#include "es2fShaderOperatorTests.hpp"
#include "glsShaderRenderCase.hpp"
#include "gluShaderUtil.hpp"
#include "tcuVectorUtil.hpp"

#include "deStringUtil.hpp"
#include "deInt32.h"
#include "deMemory.h"

#include <map>

using namespace tcu;
using namespace glu;
using namespace deqp::gls;

using std::map;
using std::ostringstream;
using std::pair;
using std::string;
using std::vector;

namespace deqp
{
namespace gles2
{
namespace Functional
{

#if defined(abs)
#undef abs
#endif

using de::clamp;
using de::max;
using de::min;

// \note VS2013 gets confused without these
using tcu::exp2;
using tcu::log2;

inline float abs(float v)
{
    return deFloatAbs(v);
}

inline bool logicalAnd(bool a, bool b)
{
    return (a && b);
}
inline bool logicalOr(bool a, bool b)
{
    return (a || b);
}
inline bool logicalXor(bool a, bool b)
{
    return (a != b);
}

#define DEFINE_VEC_FLOAT_FUNCTION(FUNC_NAME, SCALAR_OP_NAME)                    \
    template <int Size>                                                         \
    inline Vector<float, Size> FUNC_NAME(const Vector<float, Size> &v, float s) \
    {                                                                           \
        Vector<float, Size> res;                                                \
        for (int i = 0; i < Size; i++)                                          \
            res[i] = SCALAR_OP_NAME(v[i], s);                                   \
        return res;                                                             \
    }

#define DEFINE_FLOAT_VEC_FUNCTION(FUNC_NAME, SCALAR_OP_NAME)                    \
    template <int Size>                                                         \
    inline Vector<float, Size> FUNC_NAME(float s, const Vector<float, Size> &v) \
    {                                                                           \
        Vector<float, Size> res;                                                \
        for (int i = 0; i < Size; i++)                                          \
            res[i] = SCALAR_OP_NAME(s, v[i]);                                   \
        return res;                                                             \
    }

#define DEFINE_VEC_VEC_FLOAT_FUNCTION(FUNC_NAME, SCALAR_OP_NAME)                                                \
    template <int Size>                                                                                         \
    inline Vector<float, Size> FUNC_NAME(const Vector<float, Size> &v0, const Vector<float, Size> &v1, float s) \
    {                                                                                                           \
        Vector<float, Size> res;                                                                                \
        for (int i = 0; i < Size; i++)                                                                          \
            res[i] = SCALAR_OP_NAME(v0[i], v1[i], s);                                                           \
        return res;                                                                                             \
    }

#define DEFINE_VEC_FLOAT_FLOAT_FUNCTION(FUNC_NAME, SCALAR_OP_NAME)                         \
    template <int Size>                                                                    \
    inline Vector<float, Size> FUNC_NAME(const Vector<float, Size> &v, float s0, float s1) \
    {                                                                                      \
        Vector<float, Size> res;                                                           \
        for (int i = 0; i < Size; i++)                                                     \
            res[i] = SCALAR_OP_NAME(v[i], s0, s1);                                         \
        return res;                                                                        \
    }

#define DEFINE_FLOAT_FLOAT_VEC_FUNCTION(FUNC_NAME, SCALAR_OP_NAME)                         \
    template <int Size>                                                                    \
    inline Vector<float, Size> FUNC_NAME(float s0, float s1, const Vector<float, Size> &v) \
    {                                                                                      \
        Vector<float, Size> res;                                                           \
        for (int i = 0; i < Size; i++)                                                     \
            res[i] = SCALAR_OP_NAME(s0, s1, v[i]);                                         \
        return res;                                                                        \
    }

DEFINE_VEC_FLOAT_FUNCTION(modVecFloat, mod)
DEFINE_VEC_FLOAT_FUNCTION(minVecFloat, min)
DEFINE_VEC_FLOAT_FUNCTION(maxVecFloat, max)
DEFINE_VEC_FLOAT_FLOAT_FUNCTION(clampVecFloatFloat, clamp)
DEFINE_VEC_VEC_FLOAT_FUNCTION(mixVecVecFloat, mix)
DEFINE_FLOAT_VEC_FUNCTION(stepFloatVec, step)
DEFINE_FLOAT_FLOAT_VEC_FUNCTION(smoothStepFloatFloatVec, smoothStep)

#undef DEFINE_VEC_FLOAT_FUNCTION
#undef DEFINE_VEC_VEC_FLOAT_FUNCTION
#undef DEFINE_VEC_FLOAT_FLOAT_FUNCTION
#undef DEFINE_FLOAT_FLOAT_VEC_FUNCTION

inline float addOne(float v)
{
    return v + 1.0f;
}
inline float subOne(float v)
{
    return v - 1.0f;
}
inline int addOne(int v)
{
    return v + 1;
}
inline int subOne(int v)
{
    return v - 1;
}

template <int Size>
inline Vector<float, Size> addOne(const Vector<float, Size> &v)
{
    return v + 1.0f;
}
template <int Size>
inline Vector<float, Size> subOne(const Vector<float, Size> &v)
{
    return v - 1.0f;
}
template <int Size>
inline Vector<int, Size> addOne(const Vector<int, Size> &v)
{
    return v + 1;
}
template <int Size>
inline Vector<int, Size> subOne(const Vector<int, Size> &v)
{
    return v - 1;
}

template <typename T>
inline T selection(bool cond, T a, T b)
{
    return cond ? a : b;
}

template <typename T, int Size>
inline Vector<T, Size> addVecScalar(const Vector<T, Size> &v, T s)
{
    return v + s;
}
template <typename T, int Size>
inline Vector<T, Size> subVecScalar(const Vector<T, Size> &v, T s)
{
    return v - s;
}
template <typename T, int Size>
inline Vector<T, Size> mulVecScalar(const Vector<T, Size> &v, T s)
{
    return v * s;
}
template <typename T, int Size>
inline Vector<T, Size> divVecScalar(const Vector<T, Size> &v, T s)
{
    return v / s;
}

template <typename T, int Size>
inline Vector<T, Size> addScalarVec(T s, const Vector<T, Size> &v)
{
    return s + v;
}
template <typename T, int Size>
inline Vector<T, Size> subScalarVec(T s, const Vector<T, Size> &v)
{
    return s - v;
}
template <typename T, int Size>
inline Vector<T, Size> mulScalarVec(T s, const Vector<T, Size> &v)
{
    return s * v;
}
template <typename T, int Size>
inline Vector<T, Size> divScalarVec(T s, const Vector<T, Size> &v)
{
    return s / v;
}

// Reference functions for specific sequence operations for the sequence operator tests.

// Reference for expression "in0, in2 + in1, in1 + in0"
inline Vec4 sequenceNoSideEffCase0(const Vec4 &in0, const Vec4 &in1, const Vec4 &in2)
{
    DE_UNREF(in2);
    return in1 + in0;
}
// Reference for expression "in0, in2 + in1, in1 + in0"
inline int sequenceNoSideEffCase1(float in0, int in1, float in2)
{
    DE_UNREF(in0);
    DE_UNREF(in2);
    return in1 + in1;
}
// Reference for expression "in0 && in1, in0, ivec2(vec2(in0) + in2)"
inline IVec2 sequenceNoSideEffCase2(bool in0, bool in1, const Vec2 &in2)
{
    DE_UNREF(in1);
    return IVec2((int)((float)in0 + in2.x()), (int)((float)in0 + in2.y()));
}
// Reference for expression "in0 + vec4(in1), in2, in1"
inline IVec4 sequenceNoSideEffCase3(const Vec4 &in0, const IVec4 &in1, const BVec4 &in2)
{
    DE_UNREF(in0);
    DE_UNREF(in2);
    return in1;
}
// Reference for expression "in0++, in1 = in0 + in2, in2 = in1"
inline Vec4 sequenceSideEffCase0(const Vec4 &in0, const Vec4 &in1, const Vec4 &in2)
{
    DE_UNREF(in1);
    return in0 + 1.0f + in2;
}
// Reference for expression "in1++, in0 = float(in1), in1 = int(in0 + in2)"
inline int sequenceSideEffCase1(float in0, int in1, float in2)
{
    DE_UNREF(in0);
    return (int)(float(in1) + 1.0f + in2);
}
// Reference for expression "in1 = in0, in2++, in2 = in2 + vec2(in1), ivec2(in2)"
inline IVec2 sequenceSideEffCase2(bool in0, bool in1, const Vec2 &in2)
{
    DE_UNREF(in1);
    return (in2 + Vec2(1.0f) + Vec2((float)in0)).asInt();
}
// Reference for expression "in0 = in0 + vec4(in2), in1 = in1 + ivec4(in0), in1++"
inline IVec4 sequenceSideEffCase3(const Vec4 &in0, const IVec4 &in1, const BVec4 &in2)
{
    return in1 + (in0 + Vec4((float)in2.x(), (float)in2.y(), (float)in2.z(), (float)in2.w())).asInt();
}

// ShaderEvalFunc-type wrappers for the above functions.
void evalSequenceNoSideEffCase0(ShaderEvalContext &ctx)
{
    ctx.color = sequenceNoSideEffCase0(ctx.in[0].swizzle(1, 2, 3, 0), ctx.in[1].swizzle(3, 2, 1, 0),
                                       ctx.in[2].swizzle(0, 3, 2, 1));
}
void evalSequenceNoSideEffCase1(ShaderEvalContext &ctx)
{
    ctx.color.x() = (float)sequenceNoSideEffCase1(ctx.in[0].z(), (int)ctx.in[1].x(), ctx.in[2].y());
}
void evalSequenceNoSideEffCase2(ShaderEvalContext &ctx)
{
    ctx.color.yz() =
        sequenceNoSideEffCase2(ctx.in[0].z() > 0.0f, ctx.in[1].x() > 0.0f, ctx.in[2].swizzle(2, 1)).asFloat();
}
void evalSequenceNoSideEffCase3(ShaderEvalContext &ctx)
{
    ctx.color = sequenceNoSideEffCase3(ctx.in[0].swizzle(1, 2, 3, 0), ctx.in[1].swizzle(3, 2, 1, 0).asInt(),
                                       greaterThan(ctx.in[2].swizzle(0, 3, 2, 1), Vec4(0.0f, 0.0f, 0.0f, 0.0f)))
                    .asFloat();
}
void evalSequenceSideEffCase0(ShaderEvalContext &ctx)
{
    ctx.color = sequenceSideEffCase0(ctx.in[0].swizzle(1, 2, 3, 0), ctx.in[1].swizzle(3, 2, 1, 0),
                                     ctx.in[2].swizzle(0, 3, 2, 1));
}
void evalSequenceSideEffCase1(ShaderEvalContext &ctx)
{
    ctx.color.x() = (float)sequenceSideEffCase1(ctx.in[0].z(), (int)ctx.in[1].x(), ctx.in[2].y());
}
void evalSequenceSideEffCase2(ShaderEvalContext &ctx)
{
    ctx.color.yz() =
        sequenceSideEffCase2(ctx.in[0].z() > 0.0f, ctx.in[1].x() > 0.0f, ctx.in[2].swizzle(2, 1)).asFloat();
}
void evalSequenceSideEffCase3(ShaderEvalContext &ctx)
{
    ctx.color = sequenceSideEffCase3(ctx.in[0].swizzle(1, 2, 3, 0), ctx.in[1].swizzle(3, 2, 1, 0).asInt(),
                                     greaterThan(ctx.in[2].swizzle(0, 3, 2, 1), Vec4(0.0f, 0.0f, 0.0f, 0.0f)))
                    .asFloat();
}

enum
{
    MAX_INPUTS = 3
};

enum PrecisionMask
{
    PRECMASK_NA      = 0, //!< Precision not applicable (booleans)
    PRECMASK_LOWP    = (1 << PRECISION_LOWP),
    PRECMASK_MEDIUMP = (1 << PRECISION_MEDIUMP),
    PRECMASK_HIGHP   = (1 << PRECISION_HIGHP),

    PRECMASK_MEDIUMP_HIGHP = (1 << PRECISION_MEDIUMP) | (1 << PRECISION_HIGHP),
    PRECMASK_ALL           = (1 << PRECISION_LOWP) | (1 << PRECISION_MEDIUMP) | (1 << PRECISION_HIGHP)
};

enum ValueType
{
    VALUE_NONE          = 0,
    VALUE_FLOAT         = (1 << 0),  // float scalar
    VALUE_FLOAT_VEC     = (1 << 1),  // float vector
    VALUE_FLOAT_GENTYPE = (1 << 2),  // float scalar/vector
    VALUE_VEC3          = (1 << 3),  // vec3 only
    VALUE_MATRIX        = (1 << 4),  // matrix
    VALUE_BOOL          = (1 << 5),  // boolean scalar
    VALUE_BOOL_VEC      = (1 << 6),  // boolean vector
    VALUE_BOOL_GENTYPE  = (1 << 7),  // boolean scalar/vector
    VALUE_INT           = (1 << 8),  // int scalar
    VALUE_INT_VEC       = (1 << 9),  // int vector
    VALUE_INT_GENTYPE   = (1 << 10), // int scalar/vector

    // Shorthands.
    F   = VALUE_FLOAT,
    FV  = VALUE_FLOAT_VEC,
    GT  = VALUE_FLOAT_GENTYPE,
    V3  = VALUE_VEC3,
    M   = VALUE_MATRIX,
    B   = VALUE_BOOL,
    BV  = VALUE_BOOL_VEC,
    BGT = VALUE_BOOL_GENTYPE,
    I   = VALUE_INT,
    IV  = VALUE_INT_VEC,
    IGT = VALUE_INT_GENTYPE
};

static inline bool isScalarType(ValueType type)
{
    return type == VALUE_FLOAT || type == VALUE_BOOL || type == VALUE_INT;
}

struct Value
{
    Value(ValueType valueType_, float rangeMin_, float rangeMax_)
        : valueType(valueType_)
        , rangeMin(rangeMin_)
        , rangeMax(rangeMax_)
    {
    }

    ValueType valueType;
    float rangeMin;
    float rangeMax;
};

enum OperationType
{
    FUNCTION = 0,
    OPERATOR,
    SIDE_EFFECT_OPERATOR // Test the side-effect (as opposed to the result) of a side-effect operator.
};

struct BuiltinFuncInfo
{
    BuiltinFuncInfo(const char *caseName_, const char *shaderFuncName_, ValueType outValue_, Value input0_,
                    Value input1_, Value input2_, float resultScale_, float resultBias_, uint32_t precisionMask_,
                    ShaderEvalFunc evalFuncScalar_, ShaderEvalFunc evalFuncVec2_, ShaderEvalFunc evalFuncVec3_,
                    ShaderEvalFunc evalFuncVec4_, OperationType type_ = FUNCTION, bool isUnaryPrefix_ = true)
        : caseName(caseName_)
        , shaderFuncName(shaderFuncName_)
        , outValue(outValue_)
        , input0(input0_)
        , input1(input1_)
        , input2(input2_)
        , resultScale(resultScale_)
        , resultBias(resultBias_)
        , precisionMask(precisionMask_)
        , evalFuncScalar(evalFuncScalar_)
        , evalFuncVec2(evalFuncVec2_)
        , evalFuncVec3(evalFuncVec3_)
        , evalFuncVec4(evalFuncVec4_)
        , type(type_)
        , isUnaryPrefix(isUnaryPrefix_)
    {
    }

    const char *caseName;       //!< Name of case.
    const char *shaderFuncName; //!< Name in shading language.
    ValueType outValue;
    Value input0;
    Value input1;
    Value input2;
    float resultScale;
    float resultBias;
    uint32_t precisionMask;
    ShaderEvalFunc evalFuncScalar;
    ShaderEvalFunc evalFuncVec2;
    ShaderEvalFunc evalFuncVec3;
    ShaderEvalFunc evalFuncVec4;
    OperationType type;
    bool isUnaryPrefix; //!< Whether a unary operator is a prefix operator; redundant unless unary.
};

static inline BuiltinFuncInfo BuiltinOperInfo(const char *caseName_, const char *shaderFuncName_, ValueType outValue_,
                                              Value input0_, Value input1_, Value input2_, float resultScale_,
                                              float resultBias_, uint32_t precisionMask_,
                                              ShaderEvalFunc evalFuncScalar_, ShaderEvalFunc evalFuncVec2_,
                                              ShaderEvalFunc evalFuncVec3_, ShaderEvalFunc evalFuncVec4_)
{
    return BuiltinFuncInfo(caseName_, shaderFuncName_, outValue_, input0_, input1_, input2_, resultScale_, resultBias_,
                           precisionMask_, evalFuncScalar_, evalFuncVec2_, evalFuncVec3_, evalFuncVec4_, OPERATOR);
}

// For postfix (unary) operators.
static inline BuiltinFuncInfo BuiltinPostOperInfo(const char *caseName_, const char *shaderFuncName_,
                                                  ValueType outValue_, Value input0_, Value input1_, Value input2_,
                                                  float resultScale_, float resultBias_, uint32_t precisionMask_,
                                                  ShaderEvalFunc evalFuncScalar_, ShaderEvalFunc evalFuncVec2_,
                                                  ShaderEvalFunc evalFuncVec3_, ShaderEvalFunc evalFuncVec4_)
{
    return BuiltinFuncInfo(caseName_, shaderFuncName_, outValue_, input0_, input1_, input2_, resultScale_, resultBias_,
                           precisionMask_, evalFuncScalar_, evalFuncVec2_, evalFuncVec3_, evalFuncVec4_, OPERATOR,
                           false);
}

static inline BuiltinFuncInfo BuiltinSideEffOperInfo(const char *caseName_, const char *shaderFuncName_,
                                                     ValueType outValue_, Value input0_, Value input1_, Value input2_,
                                                     float resultScale_, float resultBias_, uint32_t precisionMask_,
                                                     ShaderEvalFunc evalFuncScalar_, ShaderEvalFunc evalFuncVec2_,
                                                     ShaderEvalFunc evalFuncVec3_, ShaderEvalFunc evalFuncVec4_)
{
    return BuiltinFuncInfo(caseName_, shaderFuncName_, outValue_, input0_, input1_, input2_, resultScale_, resultBias_,
                           precisionMask_, evalFuncScalar_, evalFuncVec2_, evalFuncVec3_, evalFuncVec4_,
                           SIDE_EFFECT_OPERATOR);
}

// For postfix (unary) operators, testing side-effect.
static inline BuiltinFuncInfo BuiltinPostSideEffOperInfo(const char *caseName_, const char *shaderFuncName_,
                                                         ValueType outValue_, Value input0_, Value input1_,
                                                         Value input2_, float resultScale_, float resultBias_,
                                                         uint32_t precisionMask_, ShaderEvalFunc evalFuncScalar_,
                                                         ShaderEvalFunc evalFuncVec2_, ShaderEvalFunc evalFuncVec3_,
                                                         ShaderEvalFunc evalFuncVec4_)
{
    return BuiltinFuncInfo(caseName_, shaderFuncName_, outValue_, input0_, input1_, input2_, resultScale_, resultBias_,
                           precisionMask_, evalFuncScalar_, evalFuncVec2_, evalFuncVec3_, evalFuncVec4_,
                           SIDE_EFFECT_OPERATOR, false);
}

// BuiltinFuncGroup

struct BuiltinFuncGroup
{
    BuiltinFuncGroup(const char *name_, const char *description_) : name(name_), description(description_)
    {
    }
    BuiltinFuncGroup &operator<<(const BuiltinFuncInfo &info)
    {
        funcInfos.push_back(info);
        return *this;
    }

    const char *name;
    const char *description;
    std::vector<BuiltinFuncInfo> funcInfos;
};

static const char *s_inSwizzles[MAX_INPUTS][4] = {{"z", "wy", "zxy", "yzwx"},
                                                  {"x", "yx", "yzx", "wzyx"},
                                                  {"y", "zy", "wyz", "xwzy"}};

static const char *s_outSwizzles[] = {"x", "yz", "xyz", "xyzw"};

// OperatorShaderEvaluator

class OperatorShaderEvaluator : public ShaderEvaluator
{
public:
    OperatorShaderEvaluator(ShaderEvalFunc evalFunc, float scale, float bias)
    {
        m_evalFunc = evalFunc;
        m_scale    = scale;
        m_bias     = bias;
    }

    virtual ~OperatorShaderEvaluator(void)
    {
    }

    virtual void evaluate(ShaderEvalContext &ctx)
    {
        m_evalFunc(ctx);
        ctx.color = ctx.color * m_scale + m_bias;
    }

private:
    ShaderEvalFunc m_evalFunc;
    float m_scale;
    float m_bias;
};

// Concrete value.

struct ShaderValue
{
    ShaderValue(DataType type_, float rangeMin_, float rangeMax_)
        : type(type_)
        , rangeMin(rangeMin_)
        , rangeMax(rangeMax_)
    {
    }

    ShaderValue(void) : type(TYPE_LAST), rangeMin(0.0f), rangeMax(0.0f)
    {
    }

    DataType type;
    float rangeMin;
    float rangeMax;
};

struct ShaderDataSpec
{
    ShaderDataSpec(void)
        : resultScale(1.0f)
        , resultBias(0.0f)
        , precision(PRECISION_LAST)
        , output(TYPE_LAST)
        , numInputs(0)
    {
    }

    float resultScale;
    float resultBias;
    Precision precision;
    DataType output;
    int numInputs;
    ShaderValue inputs[MAX_INPUTS];
};

// ShaderOperatorCase

class ShaderOperatorCase : public ShaderRenderCase
{
public:
    ShaderOperatorCase(Context &context, const char *caseName, const char *description, bool isVertexCase,
                       ShaderEvalFunc evalFunc, const char *shaderOp, const ShaderDataSpec &spec);
    virtual ~ShaderOperatorCase(void);

private:
    ShaderOperatorCase(const ShaderOperatorCase &);            // not allowed!
    ShaderOperatorCase &operator=(const ShaderOperatorCase &); // not allowed!

    OperatorShaderEvaluator m_evaluator;
};

ShaderOperatorCase::ShaderOperatorCase(Context &context, const char *caseName, const char *description,
                                       bool isVertexCase, ShaderEvalFunc evalFunc, const char *shaderOp,
                                       const ShaderDataSpec &spec)
    : ShaderRenderCase(context.getTestContext(), context.getRenderContext(), context.getContextInfo(), caseName,
                       description, isVertexCase, m_evaluator)
    , m_evaluator(evalFunc, spec.resultScale, spec.resultBias)
{
    const char *precision = spec.precision != PRECISION_LAST ? getPrecisionName(spec.precision) : nullptr;
    const char *inputPrecision[MAX_INPUTS];

    ostringstream vtx;
    ostringstream frag;
    ostringstream &op = isVertexCase ? vtx : frag;

    // Compute precision for inputs.
    for (int i = 0; i < spec.numInputs; i++)
    {
        bool isBoolVal = de::inRange<int>(spec.inputs[i].type, TYPE_BOOL, TYPE_BOOL_VEC4);
        bool isIntVal  = de::inRange<int>(spec.inputs[i].type, TYPE_INT, TYPE_INT_VEC4);
        // \note Mediump interpolators are used for booleans and lowp ints.
        Precision prec =
            isBoolVal || (isIntVal && spec.precision == PRECISION_LOWP) ? PRECISION_MEDIUMP : spec.precision;
        inputPrecision[i] = getPrecisionName(prec);
    }

    // Attributes.
    vtx << "attribute highp vec4 a_position;\n";
    for (int i = 0; i < spec.numInputs; i++)
        vtx << "attribute " << inputPrecision[i] << " vec4 a_in" << i << ";\n";

    if (isVertexCase)
    {
        vtx << "varying mediump vec4 v_color;\n";
        frag << "varying mediump vec4 v_color;\n";
    }
    else
    {
        for (int i = 0; i < spec.numInputs; i++)
        {
            vtx << "varying " << inputPrecision[i] << " vec4 v_in" << i << ";\n";
            frag << "varying " << inputPrecision[i] << " vec4 v_in" << i << ";\n";
        }
    }

    vtx << "\n";
    vtx << "void main()\n";
    vtx << "{\n";
    vtx << "    gl_Position = a_position;\n";

    frag << "\n";
    frag << "void main()\n";
    frag << "{\n";

    // Expression inputs.
    string prefix = isVertexCase ? "a_" : "v_";
    for (int i = 0; i < spec.numInputs; i++)
    {
        DataType inType      = spec.inputs[i].type;
        int inSize           = getDataTypeScalarSize(inType);
        bool isInt           = de::inRange<int>(inType, TYPE_INT, TYPE_INT_VEC4);
        bool isBool          = de::inRange<int>(inType, TYPE_BOOL, TYPE_BOOL_VEC4);
        const char *typeName = getDataTypeName(inType);
        const char *swizzle  = s_inSwizzles[i][inSize - 1];

        op << "\t";
        if (precision && !isBool)
            op << precision << " ";

        op << typeName << " in" << i << " = ";

        if (isBool)
        {
            if (inSize == 1)
                op << "(";
            else
                op << "greaterThan(";
        }
        else if (isInt)
            op << typeName << "(";

        op << prefix << "in" << i << "." << swizzle;

        if (isBool)
        {
            if (inSize == 1)
                op << " > 0.0)";
            else
                op << ", vec" << inSize << "(0.0))";
        }
        else if (isInt)
            op << ")";

        op << ";\n";
    }

    // Result variable.
    {
        const char *outTypeName = getDataTypeName(spec.output);
        bool isBoolOut          = de::inRange<int>(spec.output, TYPE_BOOL, TYPE_BOOL_VEC4);

        op << "\t";
        if (precision && !isBoolOut)
            op << precision << " ";
        op << outTypeName << " res = " << outTypeName << "(0.0);\n\n";
    }

    // Expression.
    op << "\t" << shaderOp << "\n\n";

    // Convert to color.
    bool isResFloatVec = de::inRange<int>(spec.output, TYPE_FLOAT, TYPE_FLOAT_VEC4);
    int outScalarSize  = getDataTypeScalarSize(spec.output);

    op << "\tmediump vec4 color = vec4(0.0, 0.0, 0.0, 1.0);\n";
    op << "\tcolor." << s_outSwizzles[outScalarSize - 1] << " = ";

    if (!isResFloatVec && outScalarSize == 1)
        op << "float(res)";
    else if (!isResFloatVec)
        op << "vec" << outScalarSize << "(res)";
    else
        op << "res";

    op << ";\n";

    // Scale & bias.
    float resultScale = spec.resultScale;
    float resultBias  = spec.resultBias;
    if ((resultScale != 1.0f) || (resultBias != 0.0f))
    {
        op << "\tcolor = color";
        if (resultScale != 1.0f)
            op << " * " << de::floatToString(resultScale, 2);
        if (resultBias != 0.0f)
            op << " + " << de::floatToString(resultBias, 2);
        op << ";\n";
    }

    // ..
    if (isVertexCase)
    {
        vtx << "    v_color = color;\n";
        frag << "    gl_FragColor = v_color;\n";
    }
    else
    {
        for (int i = 0; i < spec.numInputs; i++)
            vtx << "    v_in" << i << " = a_in" << i << ";\n";
        frag << "    gl_FragColor = color;\n";
    }

    vtx << "}\n";
    frag << "}\n";

    m_vertShaderSource = vtx.str();
    m_fragShaderSource = frag.str();

    // Setup the user attributes.
    m_userAttribTransforms.resize(spec.numInputs);
    for (int inputNdx = 0; inputNdx < spec.numInputs; inputNdx++)
    {
        const ShaderValue &v = spec.inputs[inputNdx];
        DE_ASSERT(v.type != TYPE_LAST);

        float scale   = (v.rangeMax - v.rangeMin);
        float minBias = v.rangeMin;
        float maxBias = v.rangeMax;
        Mat4 attribMatrix;

        for (int rowNdx = 0; rowNdx < 4; rowNdx++)
        {
            Vec4 row;

            switch ((rowNdx + inputNdx) % 4)
            {
            case 0:
                row = Vec4(scale, 0.0f, 0.0f, minBias);
                break;
            case 1:
                row = Vec4(0.0f, scale, 0.0f, minBias);
                break;
            case 2:
                row = Vec4(-scale, 0.0f, 0.0f, maxBias);
                break;
            case 3:
                row = Vec4(0.0f, -scale, 0.0f, maxBias);
                break;
            default:
                DE_ASSERT(false);
            }

            attribMatrix.setRow(rowNdx, row);
        }

        m_userAttribTransforms[inputNdx] = attribMatrix;
    }
}

ShaderOperatorCase::~ShaderOperatorCase(void)
{
}

// ShaderOperatorTests.

ShaderOperatorTests::ShaderOperatorTests(Context &context) : TestCaseGroup(context, "operator", "Operator tests.")
{
}

ShaderOperatorTests::~ShaderOperatorTests(void)
{
}

// Vector math functions.
template <typename T>
inline T nop(T f)
{
    return f;
}

template <typename T, int Size>
Vector<T, Size> nop(const Vector<T, Size> &v)
{
    return v;
}

#define DECLARE_UNARY_GENTYPE_FUNCS(FUNC_NAME)               \
    void eval_##FUNC_NAME##_float(ShaderEvalContext &c)      \
    {                                                        \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(2)).x();     \
    }                                                        \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)       \
    {                                                        \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1));     \
    }                                                        \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)       \
    {                                                        \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1)); \
    }                                                        \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)       \
    {                                                        \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0));    \
    }

#define DECLARE_BINARY_GENTYPE_FUNCS(FUNC_NAME)                                        \
    void eval_##FUNC_NAME##_float(ShaderEvalContext &c)                                \
    {                                                                                  \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(2), c.in[1].swizzle(0)).x();           \
    }                                                                                  \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1), c.in[1].swizzle(1, 0));        \
    }                                                                                  \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1), c.in[1].swizzle(1, 2, 0)); \
    }                                                                                  \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0), c.in[1].swizzle(3, 2, 1, 0)); \
    }

#define DECLARE_TERNARY_GENTYPE_FUNCS(FUNC_NAME)                                                                    \
    void eval_##FUNC_NAME##_float(ShaderEvalContext &c)                                                             \
    {                                                                                                               \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(2), c.in[1].swizzle(0), c.in[2].swizzle(1)).x();                    \
    }                                                                                                               \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                                                              \
    {                                                                                                               \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1), c.in[1].swizzle(1, 0), c.in[2].swizzle(2, 1));              \
    }                                                                                                               \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                                                              \
    {                                                                                                               \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1), c.in[1].swizzle(1, 2, 0), c.in[2].swizzle(3, 1, 2));    \
    }                                                                                                               \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                                                              \
    {                                                                                                               \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0), c.in[1].swizzle(3, 2, 1, 0), c.in[2].swizzle(0, 3, 2, 1)); \
    }

#define DECLARE_UNARY_SCALAR_GENTYPE_FUNCS(FUNC_NAME)         \
    void eval_##FUNC_NAME##_float(ShaderEvalContext &c)       \
    {                                                         \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(2));          \
    }                                                         \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)        \
    {                                                         \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(3, 1));       \
    }                                                         \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)        \
    {                                                         \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(2, 0, 1));    \
    }                                                         \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)        \
    {                                                         \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0)); \
    }

#define DECLARE_BINARY_SCALAR_GENTYPE_FUNCS(FUNC_NAME)                                     \
    void eval_##FUNC_NAME##_float(ShaderEvalContext &c)                                    \
    {                                                                                      \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(2), c.in[1].swizzle(0));                   \
    }                                                                                      \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                                     \
    {                                                                                      \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(3, 1), c.in[1].swizzle(1, 0));             \
    }                                                                                      \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                                     \
    {                                                                                      \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(2, 0, 1), c.in[1].swizzle(1, 2, 0));       \
    }                                                                                      \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                                     \
    {                                                                                      \
        c.color.x() = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0), c.in[1].swizzle(3, 2, 1, 0)); \
    }

#define DECLARE_BINARY_BOOL_FUNCS(FUNC_NAME)                                    \
    void eval_##FUNC_NAME##_bool(ShaderEvalContext &c)                          \
    {                                                                           \
        c.color.x() = (float)FUNC_NAME(c.in[0].z() > 0.0f, c.in[1].x() > 0.0f); \
    }

#define DECLARE_UNARY_BOOL_GENTYPE_FUNCS(FUNC_NAME)                                             \
    void eval_##FUNC_NAME##_bool(ShaderEvalContext &c)                                          \
    {                                                                                           \
        c.color.x() = (float)FUNC_NAME(c.in[0].z() > 0.0f);                                     \
    }                                                                                           \
    void eval_##FUNC_NAME##_bvec2(ShaderEvalContext &c)                                         \
    {                                                                                           \
        c.color.yz() = FUNC_NAME(greaterThan(c.in[0].swizzle(3, 1), Vec2(0.0f))).asFloat();     \
    }                                                                                           \
    void eval_##FUNC_NAME##_bvec3(ShaderEvalContext &c)                                         \
    {                                                                                           \
        c.color.xyz() = FUNC_NAME(greaterThan(c.in[0].swizzle(2, 0, 1), Vec3(0.0f))).asFloat(); \
    }                                                                                           \
    void eval_##FUNC_NAME##_bvec4(ShaderEvalContext &c)                                         \
    {                                                                                           \
        c.color = FUNC_NAME(greaterThan(c.in[0].swizzle(1, 2, 3, 0), Vec4(0.0f))).asFloat();    \
    }

#define DECLARE_TERNARY_BOOL_GENTYPE_FUNCS(FUNC_NAME)                                                                 \
    void eval_##FUNC_NAME##_bool(ShaderEvalContext &c)                                                                \
    {                                                                                                                 \
        c.color.x() = (float)FUNC_NAME(c.in[0].z() > 0.0f, c.in[1].x() > 0.0f, c.in[2].y() > 0.0f);                   \
    }                                                                                                                 \
    void eval_##FUNC_NAME##_bvec2(ShaderEvalContext &c)                                                               \
    {                                                                                                                 \
        c.color.yz() =                                                                                                \
            FUNC_NAME(greaterThan(c.in[0].swizzle(3, 1), Vec2(0.0f)), greaterThan(c.in[1].swizzle(1, 0), Vec2(0.0f)), \
                      greaterThan(c.in[2].swizzle(2, 1), Vec2(0.0f)))                                                 \
                .asFloat();                                                                                           \
    }                                                                                                                 \
    void eval_##FUNC_NAME##_bvec3(ShaderEvalContext &c)                                                               \
    {                                                                                                                 \
        c.color.xyz() = FUNC_NAME(greaterThan(c.in[0].swizzle(2, 0, 1), Vec3(0.0f)),                                  \
                                  greaterThan(c.in[1].swizzle(1, 2, 0), Vec3(0.0f)),                                  \
                                  greaterThan(c.in[2].swizzle(3, 1, 2), Vec3(0.0f)))                                  \
                            .asFloat();                                                                               \
    }                                                                                                                 \
    void eval_##FUNC_NAME##_bvec4(ShaderEvalContext &c)                                                               \
    {                                                                                                                 \
        c.color = FUNC_NAME(greaterThan(c.in[0].swizzle(1, 2, 3, 0), Vec4(0.0f)),                                     \
                            greaterThan(c.in[1].swizzle(3, 2, 1, 0), Vec4(0.0f)),                                     \
                            greaterThan(c.in[2].swizzle(0, 3, 2, 1), Vec4(0.0f)))                                     \
                      .asFloat();                                                                                     \
    }

#define DECLARE_UNARY_INT_GENTYPE_FUNCS(FUNC_NAME)                             \
    void eval_##FUNC_NAME##_int(ShaderEvalContext &c)                          \
    {                                                                          \
        c.color.x() = (float)FUNC_NAME((int)c.in[0].z());                      \
    }                                                                          \
    void eval_##FUNC_NAME##_ivec2(ShaderEvalContext &c)                        \
    {                                                                          \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1).asInt()).asFloat();     \
    }                                                                          \
    void eval_##FUNC_NAME##_ivec3(ShaderEvalContext &c)                        \
    {                                                                          \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asInt()).asFloat(); \
    }                                                                          \
    void eval_##FUNC_NAME##_ivec4(ShaderEvalContext &c)                        \
    {                                                                          \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asInt()).asFloat();    \
    }

#define DECLARE_BINARY_INT_GENTYPE_FUNCS(FUNC_NAME)                                                              \
    void eval_##FUNC_NAME##_int(ShaderEvalContext &c)                                                            \
    {                                                                                                            \
        c.color.x() = (float)tcu::FUNC_NAME((int)c.in[0].z(), (int)c.in[1].x());                                 \
    }                                                                                                            \
    void eval_##FUNC_NAME##_ivec2(ShaderEvalContext &c)                                                          \
    {                                                                                                            \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1).asInt(), c.in[1].swizzle(1, 0).asInt()).asFloat();        \
    }                                                                                                            \
    void eval_##FUNC_NAME##_ivec3(ShaderEvalContext &c)                                                          \
    {                                                                                                            \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asInt(), c.in[1].swizzle(1, 2, 0).asInt()).asFloat(); \
    }                                                                                                            \
    void eval_##FUNC_NAME##_ivec4(ShaderEvalContext &c)                                                          \
    {                                                                                                            \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asInt(), c.in[1].swizzle(3, 2, 1, 0).asInt()).asFloat(); \
    }

#define DECLARE_TERNARY_INT_GENTYPE_FUNCS(FUNC_NAME)                                                               \
    void eval_##FUNC_NAME##_int(ShaderEvalContext &c)                                                              \
    {                                                                                                              \
        c.color.x() = (float)FUNC_NAME((int)c.in[0].z(), (int)c.in[1].x(), (int)c.in[2].y());                      \
    }                                                                                                              \
    void eval_##FUNC_NAME##_ivec2(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color.yz() =                                                                                             \
            FUNC_NAME(c.in[0].swizzle(3, 1).asInt(), c.in[1].swizzle(1, 0).asInt(), c.in[2].swizzle(2, 1).asInt()) \
                .asFloat();                                                                                        \
    }                                                                                                              \
    void eval_##FUNC_NAME##_ivec3(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asInt(), c.in[1].swizzle(1, 2, 0).asInt(),              \
                                  c.in[2].swizzle(3, 1, 2).asInt())                                                \
                            .asFloat();                                                                            \
    }                                                                                                              \
    void eval_##FUNC_NAME##_ivec4(ShaderEvalContext &c)                                                            \
    {                                                                                                              \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asInt(), c.in[1].swizzle(3, 2, 1, 0).asInt(),              \
                            c.in[2].swizzle(0, 3, 2, 1).asInt())                                                   \
                      .asFloat();                                                                                  \
    }

#define DECLARE_VEC_FLOAT_FUNCS(FUNC_NAME)                                \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                    \
    {                                                                     \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1), c.in[1].x());     \
    }                                                                     \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                    \
    {                                                                     \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1), c.in[1].x()); \
    }                                                                     \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                    \
    {                                                                     \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0), c.in[1].x());    \
    }

#define DECLARE_VEC_FLOAT_FLOAT_FUNCS(FUNC_NAME)                                       \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1), c.in[1].x(), c.in[2].y());     \
    }                                                                                  \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1), c.in[1].x(), c.in[2].y()); \
    }                                                                                  \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0), c.in[1].x(), c.in[2].y());    \
    }

#define DECLARE_VEC_VEC_FLOAT_FUNCS(FUNC_NAME)                                                      \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                                              \
    {                                                                                               \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1), c.in[1].swizzle(1, 0), c.in[2].y());        \
    }                                                                                               \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                                              \
    {                                                                                               \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1), c.in[1].swizzle(1, 2, 0), c.in[2].y()); \
    }                                                                                               \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                                              \
    {                                                                                               \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0), c.in[1].swizzle(3, 2, 1, 0), c.in[2].y()); \
    }

#define DECLARE_FLOAT_FLOAT_VEC_FUNCS(FUNC_NAME)                                       \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color.yz() = FUNC_NAME(c.in[0].z(), c.in[1].x(), c.in[2].swizzle(2, 1));     \
    }                                                                                  \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color.xyz() = FUNC_NAME(c.in[0].z(), c.in[1].x(), c.in[2].swizzle(3, 1, 2)); \
    }                                                                                  \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                                 \
    {                                                                                  \
        c.color = FUNC_NAME(c.in[0].z(), c.in[1].x(), c.in[2].swizzle(0, 3, 2, 1));    \
    }

#define DECLARE_FLOAT_VEC_FUNCS(FUNC_NAME)                                \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                    \
    {                                                                     \
        c.color.yz() = FUNC_NAME(c.in[0].z(), c.in[1].swizzle(1, 0));     \
    }                                                                     \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                    \
    {                                                                     \
        c.color.xyz() = FUNC_NAME(c.in[0].z(), c.in[1].swizzle(1, 2, 0)); \
    }                                                                     \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                    \
    {                                                                     \
        c.color = FUNC_NAME(c.in[0].z(), c.in[1].swizzle(3, 2, 1, 0));    \
    }

#define DECLARE_IVEC_INT_FUNCS(FUNC_NAME)                                                        \
    void eval_##FUNC_NAME##_ivec2(ShaderEvalContext &c)                                          \
    {                                                                                            \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1).asInt(), (int)c.in[1].x()).asFloat();     \
    }                                                                                            \
    void eval_##FUNC_NAME##_ivec3(ShaderEvalContext &c)                                          \
    {                                                                                            \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1).asInt(), (int)c.in[1].x()).asFloat(); \
    }                                                                                            \
    void eval_##FUNC_NAME##_ivec4(ShaderEvalContext &c)                                          \
    {                                                                                            \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0).asInt(), (int)c.in[1].x()).asFloat();    \
    }

#define DECLARE_INT_IVEC_FUNCS(FUNC_NAME)                                                        \
    void eval_##FUNC_NAME##_ivec2(ShaderEvalContext &c)                                          \
    {                                                                                            \
        c.color.yz() = FUNC_NAME((int)c.in[0].z(), c.in[1].swizzle(1, 0).asInt()).asFloat();     \
    }                                                                                            \
    void eval_##FUNC_NAME##_ivec3(ShaderEvalContext &c)                                          \
    {                                                                                            \
        c.color.xyz() = FUNC_NAME((int)c.in[0].z(), c.in[1].swizzle(1, 2, 0).asInt()).asFloat(); \
    }                                                                                            \
    void eval_##FUNC_NAME##_ivec4(ShaderEvalContext &c)                                          \
    {                                                                                            \
        c.color = FUNC_NAME((int)c.in[0].z(), c.in[1].swizzle(3, 2, 1, 0).asInt()).asFloat();    \
    }

// Operators.

DECLARE_UNARY_GENTYPE_FUNCS(nop)
DECLARE_UNARY_GENTYPE_FUNCS(negate)
DECLARE_UNARY_GENTYPE_FUNCS(addOne)
DECLARE_UNARY_GENTYPE_FUNCS(subOne)
DECLARE_BINARY_GENTYPE_FUNCS(add)
DECLARE_BINARY_GENTYPE_FUNCS(sub)
DECLARE_BINARY_GENTYPE_FUNCS(mul)
DECLARE_BINARY_GENTYPE_FUNCS(div)

void eval_selection_float(ShaderEvalContext &c)
{
    c.color.x() = selection(c.in[0].z() > 0.0f, c.in[1].x(), c.in[2].y());
}
void eval_selection_vec2(ShaderEvalContext &c)
{
    c.color.yz() = selection(c.in[0].z() > 0.0f, c.in[1].swizzle(1, 0), c.in[2].swizzle(2, 1));
}
void eval_selection_vec3(ShaderEvalContext &c)
{
    c.color.xyz() = selection(c.in[0].z() > 0.0f, c.in[1].swizzle(1, 2, 0), c.in[2].swizzle(3, 1, 2));
}
void eval_selection_vec4(ShaderEvalContext &c)
{
    c.color = selection(c.in[0].z() > 0.0f, c.in[1].swizzle(3, 2, 1, 0), c.in[2].swizzle(0, 3, 2, 1));
}

DECLARE_UNARY_INT_GENTYPE_FUNCS(nop)
DECLARE_UNARY_INT_GENTYPE_FUNCS(negate)
DECLARE_UNARY_INT_GENTYPE_FUNCS(addOne)
DECLARE_UNARY_INT_GENTYPE_FUNCS(subOne)
DECLARE_BINARY_INT_GENTYPE_FUNCS(add)
DECLARE_BINARY_INT_GENTYPE_FUNCS(sub)
DECLARE_BINARY_INT_GENTYPE_FUNCS(mul)
DECLARE_BINARY_INT_GENTYPE_FUNCS(div)

void eval_selection_int(ShaderEvalContext &c)
{
    c.color.x() = (float)selection(c.in[0].z() > 0.0f, (int)c.in[1].x(), (int)c.in[2].y());
}
void eval_selection_ivec2(ShaderEvalContext &c)
{
    c.color.yz() =
        selection(c.in[0].z() > 0.0f, c.in[1].swizzle(1, 0).asInt(), c.in[2].swizzle(2, 1).asInt()).asFloat();
}
void eval_selection_ivec3(ShaderEvalContext &c)
{
    c.color.xyz() =
        selection(c.in[0].z() > 0.0f, c.in[1].swizzle(1, 2, 0).asInt(), c.in[2].swizzle(3, 1, 2).asInt()).asFloat();
}
void eval_selection_ivec4(ShaderEvalContext &c)
{
    c.color = selection(c.in[0].z() > 0.0f, c.in[1].swizzle(3, 2, 1, 0).asInt(), c.in[2].swizzle(0, 3, 2, 1).asInt())
                  .asFloat();
}

DECLARE_UNARY_BOOL_GENTYPE_FUNCS(boolNot)
DECLARE_BINARY_BOOL_FUNCS(logicalAnd)
DECLARE_BINARY_BOOL_FUNCS(logicalOr)
DECLARE_BINARY_BOOL_FUNCS(logicalXor)

void eval_selection_bool(ShaderEvalContext &c)
{
    c.color.x() = (float)selection(c.in[0].z() > 0.0f, c.in[1].x() > 0.0f, c.in[2].y() > 0.0f);
}
void eval_selection_bvec2(ShaderEvalContext &c)
{
    c.color.yz() = selection(c.in[0].z() > 0.0f, greaterThan(c.in[1].swizzle(1, 0), Vec2(0.0f, 0.0f)),
                             greaterThan(c.in[2].swizzle(2, 1), Vec2(0.0f, 0.0f)))
                       .asFloat();
}
void eval_selection_bvec3(ShaderEvalContext &c)
{
    c.color.xyz() = selection(c.in[0].z() > 0.0f, greaterThan(c.in[1].swizzle(1, 2, 0), Vec3(0.0f, 0.0f, 0.0f)),
                              greaterThan(c.in[2].swizzle(3, 1, 2), Vec3(0.0f, 0.0f, 0.0f)))
                        .asFloat();
}
void eval_selection_bvec4(ShaderEvalContext &c)
{
    c.color = selection(c.in[0].z() > 0.0f, greaterThan(c.in[1].swizzle(3, 2, 1, 0), Vec4(0.0f, 0.0f, 0.0f, 0.0f)),
                        greaterThan(c.in[2].swizzle(0, 3, 2, 1), Vec4(0.0f, 0.0f, 0.0f, 0.0f)))
                  .asFloat();
}

DECLARE_VEC_FLOAT_FUNCS(addVecScalar)
DECLARE_VEC_FLOAT_FUNCS(subVecScalar)
DECLARE_VEC_FLOAT_FUNCS(mulVecScalar)
DECLARE_VEC_FLOAT_FUNCS(divVecScalar)

DECLARE_FLOAT_VEC_FUNCS(addScalarVec)
DECLARE_FLOAT_VEC_FUNCS(subScalarVec)
DECLARE_FLOAT_VEC_FUNCS(mulScalarVec)
DECLARE_FLOAT_VEC_FUNCS(divScalarVec)

DECLARE_IVEC_INT_FUNCS(addVecScalar)
DECLARE_IVEC_INT_FUNCS(subVecScalar)
DECLARE_IVEC_INT_FUNCS(mulVecScalar)
DECLARE_IVEC_INT_FUNCS(divVecScalar)

DECLARE_INT_IVEC_FUNCS(addScalarVec)
DECLARE_INT_IVEC_FUNCS(subScalarVec)
DECLARE_INT_IVEC_FUNCS(mulScalarVec)
DECLARE_INT_IVEC_FUNCS(divScalarVec)

// Built-in functions.

DECLARE_UNARY_GENTYPE_FUNCS(radians)
DECLARE_UNARY_GENTYPE_FUNCS(degrees)
DECLARE_UNARY_GENTYPE_FUNCS(sin)
DECLARE_UNARY_GENTYPE_FUNCS(cos)
DECLARE_UNARY_GENTYPE_FUNCS(tan)
DECLARE_UNARY_GENTYPE_FUNCS(asin)
DECLARE_UNARY_GENTYPE_FUNCS(acos)
DECLARE_UNARY_GENTYPE_FUNCS(atan)
DECLARE_BINARY_GENTYPE_FUNCS(atan2)

DECLARE_BINARY_GENTYPE_FUNCS(pow)
DECLARE_UNARY_GENTYPE_FUNCS(exp)
DECLARE_UNARY_GENTYPE_FUNCS(log)
DECLARE_UNARY_GENTYPE_FUNCS(exp2)
DECLARE_UNARY_GENTYPE_FUNCS(log2)
DECLARE_UNARY_GENTYPE_FUNCS(sqrt)
DECLARE_UNARY_GENTYPE_FUNCS(inverseSqrt)

DECLARE_UNARY_GENTYPE_FUNCS(abs)
DECLARE_UNARY_GENTYPE_FUNCS(sign)
DECLARE_UNARY_GENTYPE_FUNCS(floor)
DECLARE_UNARY_GENTYPE_FUNCS(ceil)
DECLARE_UNARY_GENTYPE_FUNCS(fract)
DECLARE_BINARY_GENTYPE_FUNCS(mod)
DECLARE_VEC_FLOAT_FUNCS(modVecFloat)
DECLARE_BINARY_GENTYPE_FUNCS(min)
DECLARE_VEC_FLOAT_FUNCS(minVecFloat)
DECLARE_BINARY_GENTYPE_FUNCS(max)
DECLARE_VEC_FLOAT_FUNCS(maxVecFloat)
DECLARE_TERNARY_GENTYPE_FUNCS(clamp)
DECLARE_VEC_FLOAT_FLOAT_FUNCS(clampVecFloatFloat)
DECLARE_TERNARY_GENTYPE_FUNCS(mix)
DECLARE_VEC_VEC_FLOAT_FUNCS(mixVecVecFloat)
DECLARE_BINARY_GENTYPE_FUNCS(step)
DECLARE_FLOAT_VEC_FUNCS(stepFloatVec)
DECLARE_TERNARY_GENTYPE_FUNCS(smoothStep)
DECLARE_FLOAT_FLOAT_VEC_FUNCS(smoothStepFloatFloatVec)

DECLARE_UNARY_SCALAR_GENTYPE_FUNCS(length)
DECLARE_BINARY_SCALAR_GENTYPE_FUNCS(distance)
DECLARE_BINARY_SCALAR_GENTYPE_FUNCS(dot)
void eval_cross_vec3(ShaderEvalContext &c)
{
    c.color.xyz() = cross(c.in[0].swizzle(2, 0, 1), c.in[1].swizzle(1, 2, 0));
}

DECLARE_UNARY_GENTYPE_FUNCS(normalize)
DECLARE_TERNARY_GENTYPE_FUNCS(faceForward)
DECLARE_BINARY_GENTYPE_FUNCS(reflect)

void eval_refract_float(ShaderEvalContext &c)
{
    c.color.x() = refract(c.in[0].z(), c.in[1].x(), c.in[2].y());
}
void eval_refract_vec2(ShaderEvalContext &c)
{
    c.color.yz() = refract(c.in[0].swizzle(3, 1), c.in[1].swizzle(1, 0), c.in[2].y());
}
void eval_refract_vec3(ShaderEvalContext &c)
{
    c.color.xyz() = refract(c.in[0].swizzle(2, 0, 1), c.in[1].swizzle(1, 2, 0), c.in[2].y());
}
void eval_refract_vec4(ShaderEvalContext &c)
{
    c.color = refract(c.in[0].swizzle(1, 2, 3, 0), c.in[1].swizzle(3, 2, 1, 0), c.in[2].y());
}

// Compare functions.

#define DECLARE_FLOAT_COMPARE_FUNCS(FUNC_NAME)                                                    \
    void eval_##FUNC_NAME##_float(ShaderEvalContext &c)                                           \
    {                                                                                             \
        c.color.x() = (float)FUNC_NAME(c.in[0].z(), c.in[1].x());                                 \
    }                                                                                             \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                                            \
    {                                                                                             \
        c.color.x() = (float)FUNC_NAME(c.in[0].swizzle(3, 1), c.in[1].swizzle(1, 0));             \
    }                                                                                             \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                                            \
    {                                                                                             \
        c.color.x() = (float)FUNC_NAME(c.in[0].swizzle(2, 0, 1), c.in[1].swizzle(1, 2, 0));       \
    }                                                                                             \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                                            \
    {                                                                                             \
        c.color.x() = (float)FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0), c.in[1].swizzle(3, 2, 1, 0)); \
    }

#define DECLARE_FLOAT_CWISE_COMPARE_FUNCS(FUNC_NAME)                                             \
    void eval_##FUNC_NAME##_float(ShaderEvalContext &c)                                          \
    {                                                                                            \
        c.color.x() = (float)FUNC_NAME(c.in[0].z(), c.in[1].x());                                \
    }                                                                                            \
    void eval_##FUNC_NAME##_vec2(ShaderEvalContext &c)                                           \
    {                                                                                            \
        c.color.yz() = FUNC_NAME(c.in[0].swizzle(3, 1), c.in[1].swizzle(1, 0)).asFloat();        \
    }                                                                                            \
    void eval_##FUNC_NAME##_vec3(ShaderEvalContext &c)                                           \
    {                                                                                            \
        c.color.xyz() = FUNC_NAME(c.in[0].swizzle(2, 0, 1), c.in[1].swizzle(1, 2, 0)).asFloat(); \
    }                                                                                            \
    void eval_##FUNC_NAME##_vec4(ShaderEvalContext &c)                                           \
    {                                                                                            \
        c.color = FUNC_NAME(c.in[0].swizzle(1, 2, 3, 0), c.in[1].swizzle(3, 2, 1, 0)).asFloat(); \
    }

#define DECLARE_INT_COMPARE_FUNCS(FUNC_NAME)                                                                      \
    void eval_##FUNC_NAME##_int(ShaderEvalContext &c)                                                             \
    {                                                                                                             \
        c.color.x() = (float)FUNC_NAME(chopToInt(c.in[0].z()), chopToInt(c.in[1].x()));                           \
    }                                                                                                             \
    void eval_##FUNC_NAME##_ivec2(ShaderEvalContext &c)                                                           \
    {                                                                                                             \
        c.color.x() = (float)FUNC_NAME(chopToInt(c.in[0].swizzle(3, 1)), chopToInt(c.in[1].swizzle(1, 0)));       \
    }                                                                                                             \
    void eval_##FUNC_NAME##_ivec3(ShaderEvalContext &c)                                                           \
    {                                                                                                             \
        c.color.x() = (float)FUNC_NAME(chopToInt(c.in[0].swizzle(2, 0, 1)), chopToInt(c.in[1].swizzle(1, 2, 0))); \
    }                                                                                                             \
    void eval_##FUNC_NAME##_ivec4(ShaderEvalContext &c)                                                           \
    {                                                                                                             \
        c.color.x() =                                                                                             \
            (float)FUNC_NAME(chopToInt(c.in[0].swizzle(1, 2, 3, 0)), chopToInt(c.in[1].swizzle(3, 2, 1, 0)));     \
    }

#define DECLARE_INT_CWISE_COMPARE_FUNCS(FUNC_NAME)                                                                     \
    void eval_##FUNC_NAME##_int(ShaderEvalContext &c)                                                                  \
    {                                                                                                                  \
        c.color.x() = (float)FUNC_NAME(chopToInt(c.in[0].z()), chopToInt(c.in[1].x()));                                \
    }                                                                                                                  \
    void eval_##FUNC_NAME##_ivec2(ShaderEvalContext &c)                                                                \
    {                                                                                                                  \
        c.color.yz() = FUNC_NAME(chopToInt(c.in[0].swizzle(3, 1)), chopToInt(c.in[1].swizzle(1, 0))).asFloat();        \
    }                                                                                                                  \
    void eval_##FUNC_NAME##_ivec3(ShaderEvalContext &c)                                                                \
    {                                                                                                                  \
        c.color.xyz() = FUNC_NAME(chopToInt(c.in[0].swizzle(2, 0, 1)), chopToInt(c.in[1].swizzle(1, 2, 0))).asFloat(); \
    }                                                                                                                  \
    void eval_##FUNC_NAME##_ivec4(ShaderEvalContext &c)                                                                \
    {                                                                                                                  \
        c.color = FUNC_NAME(chopToInt(c.in[0].swizzle(1, 2, 3, 0)), chopToInt(c.in[1].swizzle(3, 2, 1, 0))).asFloat(); \
    }

#define DECLARE_BOOL_COMPARE_FUNCS(FUNC_NAME)                                                 \
    void eval_##FUNC_NAME##_bool(ShaderEvalContext &c)                                        \
    {                                                                                         \
        c.color.x() = (float)FUNC_NAME(c.in[0].z() > 0.0f, c.in[1].x() > 0.0f);               \
    }                                                                                         \
    void eval_##FUNC_NAME##_bvec2(ShaderEvalContext &c)                                       \
    {                                                                                         \
        c.color.x() = (float)FUNC_NAME(greaterThan(c.in[0].swizzle(3, 1), Vec2(0.0f)),        \
                                       greaterThan(c.in[1].swizzle(1, 0), Vec2(0.0f)));       \
    }                                                                                         \
    void eval_##FUNC_NAME##_bvec3(ShaderEvalContext &c)                                       \
    {                                                                                         \
        c.color.x() = (float)FUNC_NAME(greaterThan(c.in[0].swizzle(2, 0, 1), Vec3(0.0f)),     \
                                       greaterThan(c.in[1].swizzle(1, 2, 0), Vec3(0.0f)));    \
    }                                                                                         \
    void eval_##FUNC_NAME##_bvec4(ShaderEvalContext &c)                                       \
    {                                                                                         \
        c.color.x() = (float)FUNC_NAME(greaterThan(c.in[0].swizzle(1, 2, 3, 0), Vec4(0.0f)),  \
                                       greaterThan(c.in[1].swizzle(3, 2, 1, 0), Vec4(0.0f))); \
    }

#define DECLARE_BOOL_CWISE_COMPARE_FUNCS(FUNC_NAME)                                                                   \
    void eval_##FUNC_NAME##_bool(ShaderEvalContext &c)                                                                \
    {                                                                                                                 \
        c.color.x() = (float)FUNC_NAME(c.in[0].z() > 0.0f, c.in[1].x() > 0.0f);                                       \
    }                                                                                                                 \
    void eval_##FUNC_NAME##_bvec2(ShaderEvalContext &c)                                                               \
    {                                                                                                                 \
        c.color.yz() =                                                                                                \
            FUNC_NAME(greaterThan(c.in[0].swizzle(3, 1), Vec2(0.0f)), greaterThan(c.in[1].swizzle(1, 0), Vec2(0.0f))) \
                .asFloat();                                                                                           \
    }                                                                                                                 \
    void eval_##FUNC_NAME##_bvec3(ShaderEvalContext &c)                                                               \
    {                                                                                                                 \
        c.color.xyz() = FUNC_NAME(greaterThan(c.in[0].swizzle(2, 0, 1), Vec3(0.0f)),                                  \
                                  greaterThan(c.in[1].swizzle(1, 2, 0), Vec3(0.0f)))                                  \
                            .asFloat();                                                                               \
    }                                                                                                                 \
    void eval_##FUNC_NAME##_bvec4(ShaderEvalContext &c)                                                               \
    {                                                                                                                 \
        c.color = FUNC_NAME(greaterThan(c.in[0].swizzle(1, 2, 3, 0), Vec4(0.0f)),                                     \
                            greaterThan(c.in[1].swizzle(3, 2, 1, 0), Vec4(0.0f)))                                     \
                      .asFloat();                                                                                     \
    }

DECLARE_FLOAT_COMPARE_FUNCS(allEqual)
DECLARE_FLOAT_COMPARE_FUNCS(anyNotEqual)
DECLARE_FLOAT_CWISE_COMPARE_FUNCS(lessThan)
DECLARE_FLOAT_CWISE_COMPARE_FUNCS(lessThanEqual)
DECLARE_FLOAT_CWISE_COMPARE_FUNCS(greaterThan)
DECLARE_FLOAT_CWISE_COMPARE_FUNCS(greaterThanEqual)
DECLARE_FLOAT_CWISE_COMPARE_FUNCS(equal)
DECLARE_FLOAT_CWISE_COMPARE_FUNCS(notEqual)

DECLARE_INT_COMPARE_FUNCS(allEqual)
DECLARE_INT_COMPARE_FUNCS(anyNotEqual)
DECLARE_INT_CWISE_COMPARE_FUNCS(lessThan)
DECLARE_INT_CWISE_COMPARE_FUNCS(lessThanEqual)
DECLARE_INT_CWISE_COMPARE_FUNCS(greaterThan)
DECLARE_INT_CWISE_COMPARE_FUNCS(greaterThanEqual)
DECLARE_INT_CWISE_COMPARE_FUNCS(equal)
DECLARE_INT_CWISE_COMPARE_FUNCS(notEqual)

DECLARE_BOOL_COMPARE_FUNCS(allEqual)
DECLARE_BOOL_COMPARE_FUNCS(anyNotEqual)
DECLARE_BOOL_CWISE_COMPARE_FUNCS(equal)
DECLARE_BOOL_CWISE_COMPARE_FUNCS(notEqual)

// Boolean functions.

#define DECLARE_UNARY_SCALAR_BVEC_FUNCS(GLSL_NAME, FUNC_NAME)                                 \
    void eval_##GLSL_NAME##_bvec2(ShaderEvalContext &c)                                       \
    {                                                                                         \
        c.color.x() = float(FUNC_NAME(greaterThan(c.in[0].swizzle(3, 1), Vec2(0.0f))));       \
    }                                                                                         \
    void eval_##GLSL_NAME##_bvec3(ShaderEvalContext &c)                                       \
    {                                                                                         \
        c.color.x() = float(FUNC_NAME(greaterThan(c.in[0].swizzle(2, 0, 1), Vec3(0.0f))));    \
    }                                                                                         \
    void eval_##GLSL_NAME##_bvec4(ShaderEvalContext &c)                                       \
    {                                                                                         \
        c.color.x() = float(FUNC_NAME(greaterThan(c.in[0].swizzle(1, 2, 3, 0), Vec4(0.0f)))); \
    }

#define DECLARE_UNARY_BVEC_BVEC_FUNCS(GLSL_NAME, FUNC_NAME)                                         \
    void eval_##GLSL_NAME##_bvec2(ShaderEvalContext &c)                                             \
    {                                                                                               \
        c.color.yz() = FUNC_NAME(greaterThan(c.in[0].swizzle(3, 1), Vec2(0.0f))).asFloat();         \
    }                                                                                               \
    void eval_##GLSL_NAME##_bvec3(ShaderEvalContext &c)                                             \
    {                                                                                               \
        c.color.xyz() = FUNC_NAME(greaterThan(c.in[0].swizzle(2, 0, 1), Vec3(0.0f))).asFloat();     \
    }                                                                                               \
    void eval_##GLSL_NAME##_bvec4(ShaderEvalContext &c)                                             \
    {                                                                                               \
        c.color.xyzw() = FUNC_NAME(greaterThan(c.in[0].swizzle(1, 2, 3, 0), Vec4(0.0f))).asFloat(); \
    }

DECLARE_UNARY_SCALAR_BVEC_FUNCS(any, boolAny)
DECLARE_UNARY_SCALAR_BVEC_FUNCS(all, boolAll)

void ShaderOperatorTests::init(void)
{
    // Requisites:
    // - input types (const, uniform, dynamic, mixture)
    // - data types (bool, int, float, vecs, mats)
    // -
    // - complex expressions (\todo [petri] move to expressions?)
    //   * early-exit from side effects
    //   * precedence

    // unary plus, minus
    // add, sub
    // mul (larger range)
    // div (div-by-zero)
    // incr, decr (int only)
    // relational
    // equality
    // logical
    // selection
    // assignment
    // arithmetic assignment

    // parenthesis
    // sequence
    // subscript, function call, field selector/swizzler

    // precedence
    // data types (float, int, bool, vecs, matrices)

    // TestCaseGroup* group = new TestCaseGroup(m_testCtx, "additive", "Additive operator tests.");
    // addChild(group);

    // * * *

    // Built-in functions
    // - precision, data types

#define BOOL_FUNCS(FUNC_NAME) eval_##FUNC_NAME##_bool, nullptr, nullptr, nullptr

#define FLOAT_VEC_FUNCS(FUNC_NAME) nullptr, eval_##FUNC_NAME##_vec2, eval_##FUNC_NAME##_vec3, eval_##FUNC_NAME##_vec4
#define INT_VEC_FUNCS(FUNC_NAME) nullptr, eval_##FUNC_NAME##_ivec2, eval_##FUNC_NAME##_ivec3, eval_##FUNC_NAME##_ivec4
#define BOOL_VEC_FUNCS(FUNC_NAME) nullptr, eval_##FUNC_NAME##_bvec2, eval_##FUNC_NAME##_bvec3, eval_##FUNC_NAME##_bvec4

#define FLOAT_GENTYPE_FUNCS(FUNC_NAME) \
    eval_##FUNC_NAME##_float, eval_##FUNC_NAME##_vec2, eval_##FUNC_NAME##_vec3, eval_##FUNC_NAME##_vec4
#define INT_GENTYPE_FUNCS(FUNC_NAME) \
    eval_##FUNC_NAME##_int, eval_##FUNC_NAME##_ivec2, eval_##FUNC_NAME##_ivec3, eval_##FUNC_NAME##_ivec4
#define BOOL_GENTYPE_FUNCS(FUNC_NAME) \
    eval_##FUNC_NAME##_bool, eval_##FUNC_NAME##_bvec2, eval_##FUNC_NAME##_bvec3, eval_##FUNC_NAME##_bvec4

    Value notUsed = Value(VALUE_NONE, 0.0f, 0.0f);

    std::vector<BuiltinFuncGroup> funcInfoGroups;

    // Unary operators.
    funcInfoGroups.push_back(
        BuiltinFuncGroup("unary_operator", "Unary operator tests")
        << BuiltinOperInfo("plus", "+", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f, 0.5f, PRECMASK_ALL,
                           FLOAT_GENTYPE_FUNCS(nop))
        << BuiltinOperInfo("plus", "+", IGT, Value(IGT, -5.0f, 5.0f), notUsed, notUsed, 0.1f, 0.5f, PRECMASK_ALL,
                           INT_GENTYPE_FUNCS(nop))
        << BuiltinOperInfo("minus", "-", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f, 0.5f, PRECMASK_ALL,
                           FLOAT_GENTYPE_FUNCS(negate))
        << BuiltinOperInfo("minus", "-", IGT, Value(IGT, -5.0f, 5.0f), notUsed, notUsed, 0.1f, 0.5f, PRECMASK_ALL,
                           INT_GENTYPE_FUNCS(negate))
        << BuiltinOperInfo("not", "!", B, Value(B, -1.0f, 1.0f), notUsed, notUsed, 1.0f, 0.0f, PRECMASK_NA,
                           eval_boolNot_bool, nullptr, nullptr, nullptr)

        // Pre/post incr/decr side effect cases.
        << BuiltinSideEffOperInfo("pre_increment_effect", "++", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f,
                                  0.0f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(addOne))
        << BuiltinSideEffOperInfo("pre_increment_effect", "++", IGT, Value(IGT, -6.0f, 4.0f), notUsed, notUsed, 0.1f,
                                  0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(addOne))
        << BuiltinSideEffOperInfo("pre_decrement_effect", "--", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f,
                                  1.0f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(subOne))
        << BuiltinSideEffOperInfo("pre_decrement_effect", "--", IGT, Value(IGT, -4.0f, 6.0f), notUsed, notUsed, 0.1f,
                                  0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(subOne))
        << BuiltinPostSideEffOperInfo("post_increment_effect", "++", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f,
                                      0.0f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(addOne))
        << BuiltinPostSideEffOperInfo("post_increment_effect", "++", IGT, Value(IGT, -6.0f, 4.0f), notUsed, notUsed,
                                      0.1f, 0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(addOne))
        << BuiltinPostSideEffOperInfo("post_decrement_effect", "--", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f,
                                      1.0f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(subOne))
        << BuiltinPostSideEffOperInfo("post_decrement_effect", "--", IGT, Value(IGT, -4.0f, 6.0f), notUsed, notUsed,
                                      0.1f, 0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(subOne))

        // Pre/post incr/decr result cases.
        << BuiltinOperInfo("pre_increment_result", "++", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f, 0.0f,
                           PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(addOne))
        << BuiltinOperInfo("pre_increment_result", "++", IGT, Value(IGT, -6.0f, 4.0f), notUsed, notUsed, 0.1f, 0.5f,
                           PRECMASK_ALL, INT_GENTYPE_FUNCS(addOne))
        << BuiltinOperInfo("pre_decrement_result", "--", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f, 1.0f,
                           PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(subOne))
        << BuiltinOperInfo("pre_decrement_result", "--", IGT, Value(IGT, -4.0f, 6.0f), notUsed, notUsed, 0.1f, 0.5f,
                           PRECMASK_ALL, INT_GENTYPE_FUNCS(subOne))
        << BuiltinPostOperInfo("post_increment_result", "++", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f, 0.5f,
                               PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(nop))
        << BuiltinPostOperInfo("post_increment_result", "++", IGT, Value(IGT, -5.0f, 5.0f), notUsed, notUsed, 0.1f,
                               0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(nop))
        << BuiltinPostOperInfo("post_decrement_result", "--", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 0.5f, 0.5f,
                               PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(nop))
        << BuiltinPostOperInfo("post_decrement_result", "--", IGT, Value(IGT, -5.0f, 5.0f), notUsed, notUsed, 0.1f,
                               0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(nop)));

    // Binary operators.
    funcInfoGroups.push_back(
        BuiltinFuncGroup("binary_operator", "Binary operator tests")
        // Arithmetic operators.
        << BuiltinOperInfo("add", "+", GT, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(add))
        << BuiltinOperInfo("add", "+", IGT, Value(IGT, -4.0f, 6.0f), Value(IGT, -6.0f, 5.0f), notUsed, 0.1f, 0.5f,
                           PRECMASK_ALL, INT_GENTYPE_FUNCS(add))
        << BuiltinOperInfo("add", "+", FV, Value(FV, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_VEC_FUNCS(addVecScalar))
        << BuiltinOperInfo("add", "+", IV, Value(IV, -4.0f, 6.0f), Value(I, -6.0f, 5.0f), notUsed, 0.1f, 0.5f,
                           PRECMASK_ALL, INT_VEC_FUNCS(addVecScalar))
        << BuiltinOperInfo("add", "+", FV, Value(F, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_VEC_FUNCS(addScalarVec))
        << BuiltinOperInfo("add", "+", IV, Value(I, -4.0f, 6.0f), Value(IV, -6.0f, 5.0f), notUsed, 0.1f, 0.5f,
                           PRECMASK_ALL, INT_VEC_FUNCS(addScalarVec))
        << BuiltinOperInfo("sub", "-", GT, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(sub))
        << BuiltinOperInfo("sub", "-", IGT, Value(IGT, -4.0f, 6.0f), Value(IGT, -6.0f, 5.0f), notUsed, 0.1f, 0.5f,
                           PRECMASK_ALL, INT_GENTYPE_FUNCS(sub))
        << BuiltinOperInfo("sub", "-", FV, Value(FV, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_VEC_FUNCS(subVecScalar))
        << BuiltinOperInfo("sub", "-", IV, Value(IV, -4.0f, 6.0f), Value(I, -6.0f, 5.0f), notUsed, 0.1f, 0.5f,
                           PRECMASK_ALL, INT_VEC_FUNCS(subVecScalar))
        << BuiltinOperInfo("sub", "-", FV, Value(F, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_VEC_FUNCS(subScalarVec))
        << BuiltinOperInfo("sub", "-", IV, Value(I, -4.0f, 6.0f), Value(IV, -6.0f, 5.0f), notUsed, 0.1f, 0.5f,
                           PRECMASK_ALL, INT_VEC_FUNCS(subScalarVec))
        << BuiltinOperInfo("mul", "*", GT, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(mul))
        << BuiltinOperInfo("mul", "*", IGT, Value(IGT, -4.0f, 6.0f), Value(IGT, -6.0f, 5.0f), notUsed, 0.1f, 0.5f,
                           PRECMASK_ALL, INT_GENTYPE_FUNCS(mul))
        << BuiltinOperInfo("mul", "*", FV, Value(FV, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_VEC_FUNCS(mulVecScalar))
        << BuiltinOperInfo("mul", "*", IV, Value(IV, -4.0f, 6.0f), Value(I, -6.0f, 5.0f), notUsed, 0.1f, 0.5f,
                           PRECMASK_ALL, INT_VEC_FUNCS(mulVecScalar))
        << BuiltinOperInfo("mul", "*", FV, Value(F, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_VEC_FUNCS(mulScalarVec))
        << BuiltinOperInfo("mul", "*", IV, Value(I, -4.0f, 6.0f), Value(IV, -6.0f, 5.0f), notUsed, 0.1f, 0.5f,
                           PRECMASK_ALL, INT_VEC_FUNCS(mulScalarVec))
        << BuiltinOperInfo("div", "/", GT, Value(GT, -1.0f, 1.0f), Value(GT, -2.0f, -0.5f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(div))
        << BuiltinOperInfo("div", "/", IGT, Value(IGT, 24.0f, 24.0f), Value(IGT, -4.0f, -1.0f), notUsed, 0.04f, 1.0f,
                           PRECMASK_ALL, INT_GENTYPE_FUNCS(div))
        << BuiltinOperInfo("div", "/", FV, Value(FV, -1.0f, 1.0f), Value(F, -2.0f, -0.5f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_VEC_FUNCS(divVecScalar))
        << BuiltinOperInfo("div", "/", IV, Value(IV, 24.0f, 24.0f), Value(I, -4.0f, -1.0f), notUsed, 0.04f, 1.0f,
                           PRECMASK_ALL, INT_VEC_FUNCS(divVecScalar))
        << BuiltinOperInfo("div", "/", FV, Value(F, -1.0f, 1.0f), Value(FV, -2.0f, -0.5f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_VEC_FUNCS(divScalarVec))
        << BuiltinOperInfo("div", "/", IV, Value(I, 24.0f, 24.0f), Value(IV, -4.0f, -1.0f), notUsed, 0.04f, 1.0f,
                           PRECMASK_ALL, INT_VEC_FUNCS(divScalarVec))

        // Arithmetic assignment side effect cases.
        << BuiltinSideEffOperInfo("add_assign_effect", "+=", GT, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f),
                                  notUsed, 0.25f, 0.5f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(add))
        << BuiltinSideEffOperInfo("add_assign_effect", "+=", IGT, Value(IGT, -5.0f, 5.0f), Value(IGT, -5.0f, 5.0f),
                                  notUsed, 0.05f, 0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(add))
        << BuiltinSideEffOperInfo("add_assign_effect", "+=", FV, Value(FV, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed,
                                  0.25f, 0.5f, PRECMASK_ALL, FLOAT_VEC_FUNCS(addVecScalar))
        << BuiltinSideEffOperInfo("add_assign_effect", "+=", IV, Value(IV, -5.0f, 5.0f), Value(I, -5.0f, 5.0f), notUsed,
                                  0.05f, 0.5f, PRECMASK_ALL, INT_VEC_FUNCS(addVecScalar))
        << BuiltinSideEffOperInfo("sub_assign_effect", "-=", GT, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f),
                                  notUsed, 0.25f, 0.5f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(sub))
        << BuiltinSideEffOperInfo("sub_assign_effect", "-=", IGT, Value(IGT, -5.0f, 5.0f), Value(IGT, -5.0f, 5.0f),
                                  notUsed, 0.05f, 0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(sub))
        << BuiltinSideEffOperInfo("sub_assign_effect", "-=", FV, Value(FV, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed,
                                  0.25f, 0.5f, PRECMASK_ALL, FLOAT_VEC_FUNCS(subVecScalar))
        << BuiltinSideEffOperInfo("sub_assign_effect", "-=", IV, Value(IV, -5.0f, 5.0f), Value(I, -5.0f, 5.0f), notUsed,
                                  0.05f, 0.5f, PRECMASK_ALL, INT_VEC_FUNCS(subVecScalar))
        << BuiltinSideEffOperInfo("mul_assign_effect", "*=", GT, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f),
                                  notUsed, 0.5f, 0.5f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(mul))
        << BuiltinSideEffOperInfo("mul_assign_effect", "*=", IGT, Value(IGT, -4.0f, 4.0f), Value(IGT, -4.0f, 4.0f),
                                  notUsed, 0.03f, 0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(mul))
        << BuiltinSideEffOperInfo("mul_assign_effect", "*=", FV, Value(FV, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed,
                                  0.5f, 0.5f, PRECMASK_ALL, FLOAT_VEC_FUNCS(mulVecScalar))
        << BuiltinSideEffOperInfo("mul_assign_effect", "*=", IV, Value(IV, -4.0f, 4.0f), Value(I, -4.0f, 4.0f), notUsed,
                                  0.03f, 0.5f, PRECMASK_ALL, INT_VEC_FUNCS(mulVecScalar))
        << BuiltinSideEffOperInfo("div_assign_effect", "/=", GT, Value(GT, -1.0f, 1.0f), Value(GT, -2.0f, -0.5f),
                                  notUsed, 0.25f, 0.5f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(div))
        << BuiltinSideEffOperInfo("div_assign_effect", "/=", IGT, Value(IGT, 24.0f, 24.0f), Value(IGT, -4.0f, -1.0f),
                                  notUsed, 0.04f, 1.0f, PRECMASK_ALL, INT_GENTYPE_FUNCS(div))
        << BuiltinSideEffOperInfo("div_assign_effect", "/=", FV, Value(FV, -1.0f, 1.0f), Value(F, -2.0f, -0.5f),
                                  notUsed, 0.25f, 0.5f, PRECMASK_ALL, FLOAT_VEC_FUNCS(divVecScalar))
        << BuiltinSideEffOperInfo("div_assign_effect", "/=", IV, Value(IV, 24.0f, 24.0f), Value(I, -4.0f, -1.0f),
                                  notUsed, 0.04f, 1.0f, PRECMASK_ALL, INT_VEC_FUNCS(divVecScalar))

        // Arithmetic assignment result cases.
        << BuiltinOperInfo("add_assign_result", "+=", GT, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f), notUsed,
                           0.25f, 0.5f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(add))
        << BuiltinOperInfo("add_assign_result", "+=", IGT, Value(IGT, -5.0f, 5.0f), Value(IGT, -5.0f, 5.0f), notUsed,
                           0.05f, 0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(add))
        << BuiltinOperInfo("add_assign_result", "+=", FV, Value(FV, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 0.25f,
                           0.5f, PRECMASK_ALL, FLOAT_VEC_FUNCS(addVecScalar))
        << BuiltinOperInfo("add_assign_result", "+=", IV, Value(IV, -5.0f, 5.0f), Value(I, -5.0f, 5.0f), notUsed, 0.05f,
                           0.5f, PRECMASK_ALL, INT_VEC_FUNCS(addVecScalar))
        << BuiltinOperInfo("sub_assign_result", "-=", GT, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f), notUsed,
                           0.25f, 0.5f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(sub))
        << BuiltinOperInfo("sub_assign_result", "-=", IGT, Value(IGT, -5.0f, 5.0f), Value(IGT, -5.0f, 5.0f), notUsed,
                           0.05f, 0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(sub))
        << BuiltinOperInfo("sub_assign_result", "-=", FV, Value(FV, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 0.25f,
                           0.5f, PRECMASK_ALL, FLOAT_VEC_FUNCS(subVecScalar))
        << BuiltinOperInfo("sub_assign_result", "-=", IV, Value(IV, -5.0f, 5.0f), Value(I, -5.0f, 5.0f), notUsed, 0.05f,
                           0.5f, PRECMASK_ALL, INT_VEC_FUNCS(subVecScalar))
        << BuiltinOperInfo("mul_assign_result", "*=", GT, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f), notUsed, 0.5f,
                           0.5f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(mul))
        << BuiltinOperInfo("mul_assign_result", "*=", IGT, Value(IGT, -4.0f, 4.0f), Value(IGT, -4.0f, 4.0f), notUsed,
                           0.03f, 0.5f, PRECMASK_ALL, INT_GENTYPE_FUNCS(mul))
        << BuiltinOperInfo("mul_assign_result", "*=", FV, Value(FV, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 0.5f,
                           0.5f, PRECMASK_ALL, FLOAT_VEC_FUNCS(mulVecScalar))
        << BuiltinOperInfo("mul_assign_result", "*=", IV, Value(IV, -4.0f, 4.0f), Value(I, -4.0f, 4.0f), notUsed, 0.03f,
                           0.5f, PRECMASK_ALL, INT_VEC_FUNCS(mulVecScalar))
        << BuiltinOperInfo("div_assign_result", "/=", GT, Value(GT, -1.0f, 1.0f), Value(GT, -2.0f, -0.5f), notUsed,
                           0.25f, 0.5f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(div))
        << BuiltinOperInfo("div_assign_result", "/=", IGT, Value(IGT, 24.0f, 24.0f), Value(IGT, -4.0f, -1.0f), notUsed,
                           0.04f, 1.0f, PRECMASK_ALL, INT_GENTYPE_FUNCS(div))
        << BuiltinOperInfo("div_assign_result", "/=", FV, Value(FV, -1.0f, 1.0f), Value(F, -2.0f, -0.5f), notUsed,
                           0.25f, 0.5f, PRECMASK_ALL, FLOAT_VEC_FUNCS(divVecScalar))
        << BuiltinOperInfo("div_assign_result", "/=", IV, Value(IV, 24.0f, 24.0f), Value(I, -4.0f, -1.0f), notUsed,
                           0.04f, 1.0f, PRECMASK_ALL, INT_VEC_FUNCS(divVecScalar))

        // Scalar relational operators.
        << BuiltinOperInfo("less", "<", B, Value(F, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, eval_lessThan_float, nullptr, nullptr, nullptr)
        << BuiltinOperInfo("less", "<", B, Value(I, -5.0f, 5.0f), Value(I, -5.0f, 5.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, eval_lessThan_int, nullptr, nullptr, nullptr)
        << BuiltinOperInfo("less_or_equal", "<=", B, Value(F, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, eval_lessThanEqual_float, nullptr, nullptr, nullptr)
        << BuiltinOperInfo("less_or_equal", "<=", B, Value(I, -5.0f, 5.0f), Value(I, -5.0f, 5.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, eval_lessThanEqual_int, nullptr, nullptr, nullptr)
        << BuiltinOperInfo("greater", ">", B, Value(F, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, eval_greaterThan_float, nullptr, nullptr, nullptr)
        << BuiltinOperInfo("greater", ">", B, Value(I, -5.0f, 5.0f), Value(I, -5.0f, 5.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, eval_greaterThan_int, nullptr, nullptr, nullptr)
        << BuiltinOperInfo("greater_or_equal", ">=", B, Value(F, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 1.0f,
                           0.0f, PRECMASK_ALL, eval_greaterThanEqual_float, nullptr, nullptr, nullptr)
        << BuiltinOperInfo("greater_or_equal", ">=", B, Value(I, -5.0f, 5.0f), Value(I, -5.0f, 5.0f), notUsed, 1.0f,
                           0.0f, PRECMASK_ALL, eval_greaterThanEqual_int, nullptr, nullptr, nullptr)

        // Equality comparison operators.
        << BuiltinOperInfo("equal", "==", B, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(allEqual))
        << BuiltinOperInfo("equal", "==", B, Value(IGT, -5.5f, 4.7f), Value(IGT, -4.9f, 5.8f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, INT_GENTYPE_FUNCS(allEqual))
        << BuiltinOperInfo("equal", "==", B, Value(BGT, -2.1f, 2.1f), Value(BGT, -1.1f, 3.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_NA, BOOL_GENTYPE_FUNCS(allEqual))
        << BuiltinOperInfo("not_equal", "!=", B, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(anyNotEqual))
        << BuiltinOperInfo("not_equal", "!=", B, Value(IGT, -5.5f, 4.7f), Value(IGT, -4.9f, 5.8f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, INT_GENTYPE_FUNCS(anyNotEqual))
        << BuiltinOperInfo("not_equal", "!=", B, Value(BGT, -2.1f, 2.1f), Value(BGT, -1.1f, 3.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_NA, BOOL_GENTYPE_FUNCS(anyNotEqual))

        // Logical operators.
        << BuiltinOperInfo("logical_and", "&&", B, Value(B, -1.0f, 1.0f), Value(B, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_NA, BOOL_FUNCS(logicalAnd))
        << BuiltinOperInfo("logical_or", "||", B, Value(B, -1.0f, 1.0f), Value(B, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_NA, BOOL_FUNCS(logicalOr))
        << BuiltinOperInfo("logical_xor", "^^", B, Value(B, -1.0f, 1.0f), Value(B, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_NA, BOOL_FUNCS(logicalXor)));

    // 8.1 Angle and Trigonometry Functions.
    funcInfoGroups.push_back(BuiltinFuncGroup("angle_and_trigonometry", "Angle and trigonometry function tests.")
                             << BuiltinFuncInfo("radians", "radians", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed,
                                                25.0f, 0.5f, PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(radians))
                             << BuiltinFuncInfo("degrees", "degrees", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed,
                                                0.04f, 0.5f, PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(degrees))
                             << BuiltinFuncInfo("sin", "sin", GT, Value(GT, -5.0f, 5.0f), notUsed, notUsed, 0.5f, 0.5f,
                                                PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(sin))
                             << BuiltinFuncInfo("sin", "sin", GT, Value(GT, -1.5f, 1.5f), notUsed, notUsed, 0.5f, 0.5f,
                                                PRECMASK_LOWP, FLOAT_GENTYPE_FUNCS(sin))
                             << BuiltinFuncInfo("cos", "cos", GT, Value(GT, -5.0f, 5.0f), notUsed, notUsed, 0.5f, 0.5f,
                                                PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(cos))
                             << BuiltinFuncInfo("cos", "cos", GT, Value(GT, -1.5f, 1.5f), notUsed, notUsed, 0.5f, 0.5f,
                                                PRECMASK_LOWP, FLOAT_GENTYPE_FUNCS(cos))
                             << BuiltinFuncInfo("tan", "tan", GT, Value(GT, -5.0f, 5.0f), notUsed, notUsed, 0.5f, 0.5f,
                                                PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(tan))
                             << BuiltinFuncInfo("tan", "tan", GT, Value(GT, -1.5f, 5.5f), notUsed, notUsed, 0.5f, 0.5f,
                                                PRECMASK_LOWP, FLOAT_GENTYPE_FUNCS(tan))
                             << BuiltinFuncInfo("asin", "asin", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 1.0f,
                                                0.0f, PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(asin))
                             << BuiltinFuncInfo("acos", "acos", GT, Value(GT, -1.0f, 1.0f), notUsed, notUsed, 1.0f,
                                                0.0f, PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(acos))
                             << BuiltinFuncInfo("atan", "atan", GT, Value(GT, -4.0f, 4.0f), notUsed, notUsed, 0.5f,
                                                0.5f, PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(atan))
                             << BuiltinFuncInfo("atan2", "atan", GT, Value(GT, -4.0f, 4.0f), Value(GT, 0.5f, 2.0f),
                                                notUsed, 0.5f, 0.5f, PRECMASK_MEDIUMP_HIGHP,
                                                FLOAT_GENTYPE_FUNCS(atan2)));

    // 8.2 Exponential Functions.
    funcInfoGroups.push_back(BuiltinFuncGroup("exponential", "Exponential function tests")
                             << BuiltinFuncInfo("pow", "pow", GT, Value(GT, 0.1f, 8.0f), Value(GT, -4.0f, 2.0f),
                                                notUsed, 1.0f, 0.0f, PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(pow))
                             << BuiltinFuncInfo("exp", "exp", GT, Value(GT, -6.0f, 3.0f), notUsed, notUsed, 0.5f, 0.0f,
                                                PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(exp))
                             << BuiltinFuncInfo("log", "log", GT, Value(GT, 0.1f, 10.0f), notUsed, notUsed, 0.5f, 0.3f,
                                                PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(log))
                             << BuiltinFuncInfo("exp2", "exp2", GT, Value(GT, -7.0f, 2.0f), notUsed, notUsed, 1.0f,
                                                0.0f, PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(exp2))
                             << BuiltinFuncInfo("log2", "log2", GT, Value(GT, 0.1f, 10.0f), notUsed, notUsed, 1.0f,
                                                0.0f, PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(log2))
                             << BuiltinFuncInfo("sqrt", "sqrt", GT, Value(GT, 0.0f, 10.0f), notUsed, notUsed, 0.3f,
                                                0.0f, PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(sqrt))
                             << BuiltinFuncInfo("inversesqrt", "inversesqrt", GT, Value(GT, 0.5f, 10.0f), notUsed,
                                                notUsed, 1.0f, 0.0f, PRECMASK_MEDIUMP_HIGHP,
                                                FLOAT_GENTYPE_FUNCS(inverseSqrt)));

    // 8.3 Common Functions.
    funcInfoGroups.push_back(
        BuiltinFuncGroup("common_functions", "Common function tests.")
        << BuiltinFuncInfo("abs", "abs", GT, Value(GT, -2.0f, 2.0f), notUsed, notUsed, 0.5f, 0.5f, PRECMASK_ALL,
                           FLOAT_GENTYPE_FUNCS(abs))
        << BuiltinFuncInfo("sign", "sign", GT, Value(GT, -1.5f, 1.5f), notUsed, notUsed, 0.3f, 0.5f, PRECMASK_ALL,
                           FLOAT_GENTYPE_FUNCS(sign))
        << BuiltinFuncInfo("floor", "floor", GT, Value(GT, -2.5f, 2.5f), notUsed, notUsed, 0.2f, 0.7f, PRECMASK_ALL,
                           FLOAT_GENTYPE_FUNCS(floor))
        << BuiltinFuncInfo("ceil", "ceil", GT, Value(GT, -2.5f, 2.5f), notUsed, notUsed, 0.2f, 0.5f, PRECMASK_ALL,
                           FLOAT_GENTYPE_FUNCS(ceil))
        << BuiltinFuncInfo("fract", "fract", GT, Value(GT, -1.5f, 1.5f), notUsed, notUsed, 0.8f, 0.1f, PRECMASK_ALL,
                           FLOAT_GENTYPE_FUNCS(fract))
        << BuiltinFuncInfo("mod", "mod", GT, Value(GT, -2.0f, 2.0f), Value(GT, 0.9f, 6.0f), notUsed, 0.5f, 0.5f,
                           PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(mod))
        << BuiltinFuncInfo("mod", "mod", GT, Value(FV, -2.0f, 2.0f), Value(F, 0.9f, 6.0f), notUsed, 0.5f, 0.5f,
                           PRECMASK_MEDIUMP_HIGHP, FLOAT_VEC_FUNCS(modVecFloat))
        << BuiltinFuncInfo("min", "min", GT, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f), notUsed, 0.5f, 0.5f,
                           PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(min))
        << BuiltinFuncInfo("min", "min", GT, Value(FV, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 0.5f, 0.5f,
                           PRECMASK_ALL, FLOAT_VEC_FUNCS(minVecFloat))
        << BuiltinFuncInfo("max", "max", GT, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f), notUsed, 0.5f, 0.5f,
                           PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(max))
        << BuiltinFuncInfo("max", "max", GT, Value(FV, -1.0f, 1.0f), Value(F, -1.0f, 1.0f), notUsed, 0.5f, 0.5f,
                           PRECMASK_ALL, FLOAT_VEC_FUNCS(maxVecFloat))
        << BuiltinFuncInfo("clamp", "clamp", GT, Value(GT, -1.0f, 1.0f), Value(GT, -0.5f, 0.5f), Value(GT, 0.5f, 1.0f),
                           0.5f, 0.5f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(clamp))
        << BuiltinFuncInfo("clamp", "clamp", GT, Value(FV, -1.0f, 1.0f), Value(F, -0.5f, 0.5f), Value(F, 0.5f, 1.0f),
                           0.5f, 0.5f, PRECMASK_ALL, FLOAT_VEC_FUNCS(clampVecFloatFloat))
        << BuiltinFuncInfo("mix", "mix", GT, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 1.0f), Value(GT, 0.0f, 1.0f),
                           0.5f, 0.5f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(mix))
        << BuiltinFuncInfo("mix", "mix", GT, Value(FV, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f), Value(F, 0.0f, 1.0f), 0.5f,
                           0.5f, PRECMASK_ALL, FLOAT_VEC_FUNCS(mixVecVecFloat))
        << BuiltinFuncInfo("step", "step", GT, Value(GT, -1.0f, 1.0f), Value(GT, -1.0f, 0.0f), notUsed, 0.5f, 0.25f,
                           PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(step))
        << BuiltinFuncInfo("step", "step", GT, Value(F, -1.0f, 1.0f), Value(FV, -1.0f, 0.0f), notUsed, 0.5f, 0.25f,
                           PRECMASK_ALL, FLOAT_VEC_FUNCS(stepFloatVec))
        << BuiltinFuncInfo("smoothstep", "smoothstep", GT, Value(GT, -0.5f, 0.0f), Value(GT, 0.1f, 1.0f),
                           Value(GT, -1.0f, 1.0f), 0.5f, 0.5f, PRECMASK_ALL, FLOAT_GENTYPE_FUNCS(smoothStep))
        << BuiltinFuncInfo("smoothstep", "smoothstep", GT, Value(F, -0.5f, 0.0f), Value(F, 0.1f, 1.0f),
                           Value(FV, -1.0f, 1.0f), 0.5f, 0.5f, PRECMASK_ALL, FLOAT_VEC_FUNCS(smoothStepFloatFloatVec)));

    // 8.4 Geometric Functions.
    funcInfoGroups.push_back(
        BuiltinFuncGroup("geometric", "Geometric function tests.")
        << BuiltinFuncInfo("length", "length", F, Value(GT, -5.0f, 5.0f), notUsed, notUsed, 0.1f, 0.5f,
                           PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(length))
        << BuiltinFuncInfo("distance", "distance", F, Value(GT, -5.0f, 5.0f), Value(GT, -5.0f, 5.0f), notUsed, 0.1f,
                           0.5f, PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(distance))
        << BuiltinFuncInfo("dot", "dot", F, Value(GT, -5.0f, 5.0f), Value(GT, -5.0f, 5.0f), notUsed, 0.1f, 0.5f,
                           PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(dot))
        << BuiltinFuncInfo("cross", "cross", V3, Value(GT, -5.0f, 5.0f), Value(GT, -5.0f, 5.0f), notUsed, 0.1f, 0.5f,
                           PRECMASK_MEDIUMP_HIGHP, nullptr, nullptr, eval_cross_vec3, nullptr)
        << BuiltinFuncInfo("normalize", "normalize", GT, Value(GT, 0.1f, 4.0f), notUsed, notUsed, 0.5f, 0.5f,
                           PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(normalize))
        << BuiltinFuncInfo("faceforward", "faceforward", GT, Value(GT, -5.0f, 5.0f), Value(GT, -5.0f, 5.0f),
                           Value(GT, -1.0f, 1.0f), 0.5f, 0.5f, PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(faceForward))
        << BuiltinFuncInfo("reflect", "reflect", GT, Value(GT, -0.8f, -0.5f), Value(GT, 0.5f, 0.8f), notUsed, 0.5f,
                           0.5f, PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(reflect))
        << BuiltinFuncInfo("refract", "refract", GT, Value(GT, -0.8f, 1.2f), Value(GT, -1.1f, 0.5f),
                           Value(F, 0.2f, 1.5f), 0.5f, 0.5f, PRECMASK_MEDIUMP_HIGHP, FLOAT_GENTYPE_FUNCS(refract)));

    // 8.5 Matrix Functions.
    // separate matrix tests?
    //    funcInfoGroups.push_back(
    //        BuiltinFuncGroup("matrix", "Matrix function tests.")
    //        << BuiltinFuncInfo("matrixCompMult", "matrixCompMult", M, ... )
    // );

    // 8.6 Vector Relational Functions.
    funcInfoGroups.push_back(
        BuiltinFuncGroup("float_compare", "Floating point comparison tests.")
        << BuiltinFuncInfo("lessThan", "lessThan", BV, Value(FV, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f), notUsed, 1.0f,
                           0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(lessThan))
        << BuiltinFuncInfo("lessThanEqual", "lessThanEqual", BV, Value(FV, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f),
                           notUsed, 1.0f, 0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(lessThanEqual))
        << BuiltinFuncInfo("greaterThan", "greaterThan", BV, Value(FV, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f), notUsed,
                           1.0f, 0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(greaterThan))
        << BuiltinFuncInfo("greaterThanEqual", "greaterThanEqual", BV, Value(FV, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f),
                           notUsed, 1.0f, 0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(greaterThanEqual))
        << BuiltinFuncInfo("equal", "equal", BV, Value(FV, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, FLOAT_VEC_FUNCS(equal))
        << BuiltinFuncInfo("notEqual", "notEqual", BV, Value(FV, -1.0f, 1.0f), Value(FV, -1.0f, 1.0f), notUsed, 1.0f,
                           0.0f, PRECMASK_ALL, FLOAT_VEC_FUNCS(notEqual)));

    funcInfoGroups.push_back(
        BuiltinFuncGroup("int_compare", "Integer comparison tests.")
        << BuiltinFuncInfo("lessThan", "lessThan", BV, Value(IV, -5.2f, 4.9f), Value(IV, -5.0f, 5.0f), notUsed, 1.0f,
                           0.0f, PRECMASK_ALL, INT_VEC_FUNCS(lessThan))
        << BuiltinFuncInfo("lessThanEqual", "lessThanEqual", BV, Value(IV, -5.2f, 4.9f), Value(IV, -5.0f, 5.0f),
                           notUsed, 1.0f, 0.0f, PRECMASK_ALL, INT_VEC_FUNCS(lessThanEqual))
        << BuiltinFuncInfo("greaterThan", "greaterThan", BV, Value(IV, -5.2f, 4.9f), Value(IV, -5.0f, 5.0f), notUsed,
                           1.0f, 0.0f, PRECMASK_ALL, INT_VEC_FUNCS(greaterThan))
        << BuiltinFuncInfo("greaterThanEqual", "greaterThanEqual", BV, Value(IV, -5.2f, 4.9f), Value(IV, -5.0f, 5.0f),
                           notUsed, 1.0f, 0.0f, PRECMASK_ALL, INT_VEC_FUNCS(greaterThanEqual))
        << BuiltinFuncInfo("equal", "equal", BV, Value(IV, -5.2f, 4.9f), Value(IV, -5.0f, 5.0f), notUsed, 1.0f, 0.0f,
                           PRECMASK_ALL, INT_VEC_FUNCS(equal))
        << BuiltinFuncInfo("notEqual", "notEqual", BV, Value(IV, -5.2f, 4.9f), Value(IV, -5.0f, 5.0f), notUsed, 1.0f,
                           0.0f, PRECMASK_ALL, INT_VEC_FUNCS(notEqual)));

    funcInfoGroups.push_back(BuiltinFuncGroup("bool_compare", "Boolean comparison tests.")
                             << BuiltinFuncInfo("equal", "equal", BV, Value(BV, -5.2f, 4.9f), Value(BV, -5.0f, 5.0f),
                                                notUsed, 1.0f, 0.0f, PRECMASK_NA, BOOL_VEC_FUNCS(equal))
                             << BuiltinFuncInfo("notEqual", "notEqual", BV, Value(BV, -5.2f, 4.9f),
                                                Value(BV, -5.0f, 5.0f), notUsed, 1.0f, 0.0f, PRECMASK_NA,
                                                BOOL_VEC_FUNCS(notEqual))
                             << BuiltinFuncInfo("any", "any", B, Value(BV, -1.0f, 0.3f), notUsed, notUsed, 1.0f, 0.0f,
                                                PRECMASK_NA, BOOL_VEC_FUNCS(any))
                             << BuiltinFuncInfo("all", "all", B, Value(BV, -0.3f, 1.0f), notUsed, notUsed, 1.0f, 0.0f,
                                                PRECMASK_NA, BOOL_VEC_FUNCS(all))
                             << BuiltinFuncInfo("not", "not", BV, Value(BV, -1.0f, 1.0f), notUsed, notUsed, 1.0f, 0.0f,
                                                PRECMASK_NA, BOOL_VEC_FUNCS(boolNot)));

    // 8.7 Texture Lookup Functions
    // texture2D (sampler, vec2)
    // texture2D (sampler, vec2, bias)
    // texture2DProj (sampler, vec3)
    // texture2DProj (sampler, vec3, bias)
    // texture2DProj (sampler, vec4)
    // texture2DProj (sampler, vec4, bias)
    // texture2DLod (sampler, vec2, lod)
    // texture2DProjLod (sampler, vec3, lod)
    // texture2DProjLod (sampler, vec4, lod)
    // textureCube (sampler, vec3)
    // textureCube (sampler, vec3, bias)
    // textureCubeLod (sampler, vec3, lod)

    static const ShaderType s_shaderTypes[] = {SHADERTYPE_VERTEX, SHADERTYPE_FRAGMENT};

    static const DataType s_floatTypes[] = {TYPE_FLOAT, TYPE_FLOAT_VEC2, TYPE_FLOAT_VEC3, TYPE_FLOAT_VEC4};

    static const DataType s_intTypes[] = {TYPE_INT, TYPE_INT_VEC2, TYPE_INT_VEC3, TYPE_INT_VEC4};

    static const DataType s_boolTypes[] = {TYPE_BOOL, TYPE_BOOL_VEC2, TYPE_BOOL_VEC3, TYPE_BOOL_VEC4};

    for (int outerGroupNdx = 0; outerGroupNdx < (int)funcInfoGroups.size(); outerGroupNdx++)
    {
        // Create outer group.
        const BuiltinFuncGroup &outerGroupInfo = funcInfoGroups[outerGroupNdx];
        TestCaseGroup *outerGroup = new TestCaseGroup(m_context, outerGroupInfo.name, outerGroupInfo.description);
        addChild(outerGroup);

        // Only create new group if name differs from previous one.
        TestCaseGroup *innerGroup = nullptr;

        for (int funcInfoNdx = 0; funcInfoNdx < (int)outerGroupInfo.funcInfos.size(); funcInfoNdx++)
        {
            const BuiltinFuncInfo &funcInfo = outerGroupInfo.funcInfos[funcInfoNdx];
            const char *shaderFuncName      = funcInfo.shaderFuncName;
            bool isBoolCase                 = (funcInfo.precisionMask == PRECMASK_NA);
            bool isIntCase   = (funcInfo.input0.valueType & (VALUE_INT | VALUE_INT_VEC | VALUE_INT_GENTYPE)) != 0;
            bool isFloatCase = !isBoolCase && !isIntCase; // \todo [petri] Better check.
            bool isBoolOut   = (funcInfo.outValue & (VALUE_BOOL | VALUE_BOOL_VEC | VALUE_BOOL_GENTYPE)) != 0;
            bool isIntOut    = (funcInfo.outValue & (VALUE_INT | VALUE_INT_VEC | VALUE_INT_GENTYPE)) != 0;
            bool isFloatOut  = !isBoolOut && !isIntOut;

            if (!innerGroup || (string(innerGroup->getName()) != funcInfo.caseName))
            {
                string groupDesc = string("Built-in function ") + shaderFuncName + "() tests.";
                innerGroup       = new TestCaseGroup(m_context, funcInfo.caseName, groupDesc.c_str());
                outerGroup->addChild(innerGroup);
            }

            for (int inScalarSize = 1; inScalarSize <= 4; inScalarSize++)
            {
                int outScalarSize    = ((funcInfo.outValue == VALUE_FLOAT) || (funcInfo.outValue == VALUE_BOOL)) ?
                                           1 :
                                           inScalarSize; // \todo [petri] Int.
                DataType outDataType = isFloatOut ? s_floatTypes[outScalarSize - 1] :
                                       isIntOut   ? s_intTypes[outScalarSize - 1] :
                                       isBoolOut  ? s_boolTypes[outScalarSize - 1] :
                                                    TYPE_LAST;

                ShaderEvalFunc evalFunc = nullptr;
                if (inScalarSize == 1)
                    evalFunc = funcInfo.evalFuncScalar;
                else if (inScalarSize == 2)
                    evalFunc = funcInfo.evalFuncVec2;
                else if (inScalarSize == 3)
                    evalFunc = funcInfo.evalFuncVec3;
                else if (inScalarSize == 4)
                    evalFunc = funcInfo.evalFuncVec4;
                else
                    DE_ASSERT(false);

                // Skip if no valid eval func.
                // \todo [petri] Better check for V3 only etc. cases?
                if (evalFunc == nullptr)
                    continue;

                for (int precision = 0; precision < PRECISION_LAST; precision++)
                {
                    if ((funcInfo.precisionMask & (1 << precision)) ||
                        (funcInfo.precisionMask == PRECMASK_NA &&
                         precision == PRECISION_MEDIUMP)) // use mediump interpolators for booleans
                    {
                        const char *precisionStr = getPrecisionName((Precision)precision);
                        string precisionPrefix   = isBoolCase ? "" : (string(precisionStr) + "_");

                        for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(s_shaderTypes); shaderTypeNdx++)
                        {
                            ShaderType shaderType = s_shaderTypes[shaderTypeNdx];
                            ShaderDataSpec shaderSpec;
                            const char *shaderTypeName = getShaderTypeName(shaderType);
                            bool isVertexCase          = (ShaderType)shaderType == SHADERTYPE_VERTEX;
                            bool isUnaryOp             = (funcInfo.input1.valueType == VALUE_NONE);

                            // \note Data type names will be added to description and name in a following loop.
                            string desc = string("Built-in function ") + shaderFuncName + "(";
                            string name = precisionPrefix;

                            // Generate shader op.
                            string shaderOp = string("res = ");

                            // Setup shader data info.
                            shaderSpec.numInputs   = 0;
                            shaderSpec.precision   = isBoolCase ? PRECISION_LAST : (Precision)precision;
                            shaderSpec.output      = outDataType;
                            shaderSpec.resultScale = funcInfo.resultScale;
                            shaderSpec.resultBias  = funcInfo.resultBias;

                            if (funcInfo.type == OPERATOR)
                            {
                                if (isUnaryOp && funcInfo.isUnaryPrefix)
                                    shaderOp += shaderFuncName;
                            }
                            else if (funcInfo.type == FUNCTION)
                                shaderOp += string(shaderFuncName) + "(";
                            else // SIDE_EFFECT_OPERATOR
                                shaderOp += "in0;\n\t";

                            for (int inputNdx = 0; inputNdx < MAX_INPUTS; inputNdx++)
                            {
                                const Value &v     = (inputNdx == 0) ? funcInfo.input0 :
                                                     (inputNdx == 1) ? funcInfo.input1 :
                                                                       funcInfo.input2;
                                const Value &prevV = (inputNdx == 1) ? funcInfo.input0 :
                                                     (inputNdx == 2) ? funcInfo.input1 :
                                                                       funcInfo.input2;

                                if (v.valueType == VALUE_NONE)
                                    continue; // Skip unused input.

                                int curInScalarSize    = isScalarType(v.valueType) ? 1 : inScalarSize;
                                DataType curInDataType = isFloatCase ? s_floatTypes[curInScalarSize - 1] :
                                                         isIntCase   ? s_intTypes[curInScalarSize - 1] :
                                                         isBoolCase  ? s_boolTypes[curInScalarSize - 1] :
                                                                       TYPE_LAST;

                                // Write input type(s) to case description and name.

                                if (inputNdx > 0)
                                    desc += ", ";

                                desc += getDataTypeName(curInDataType);

                                if (inputNdx == 0 ||
                                    isScalarType(prevV.valueType) !=
                                        isScalarType(
                                            v.valueType)) // \note Only write input type to case name if different from previous input type (avoid overly long names).
                                    name += string("") + getDataTypeName(curInDataType) + "_";

                                // Generate op input source.

                                if (funcInfo.type == OPERATOR || funcInfo.type == FUNCTION)
                                {
                                    if (inputNdx != 0)
                                    {
                                        if (funcInfo.type == OPERATOR && !isUnaryOp)
                                            shaderOp += " " + string(shaderFuncName) + " ";
                                        else
                                            shaderOp += ", ";
                                    }

                                    shaderOp += "in" + de::toString(inputNdx);

                                    if (funcInfo.type == OPERATOR && isUnaryOp && !funcInfo.isUnaryPrefix)
                                        shaderOp += string(shaderFuncName);
                                }
                                else
                                {
                                    DE_ASSERT(funcInfo.type == SIDE_EFFECT_OPERATOR);

                                    if (inputNdx != 0 || (isUnaryOp && funcInfo.isUnaryPrefix))
                                        shaderOp += string("") + (isUnaryOp ? "" : " ") + shaderFuncName +
                                                    (isUnaryOp ? "" : " ");

                                    shaderOp +=
                                        inputNdx == 0 ?
                                            "res" :
                                            "in" +
                                                de::toString(
                                                    inputNdx); // \note in0 has already been assigned to res, so start from in1.

                                    if (isUnaryOp && !funcInfo.isUnaryPrefix)
                                        shaderOp += shaderFuncName;
                                }

                                // Fill in shader info.
                                shaderSpec.inputs[shaderSpec.numInputs++] =
                                    ShaderValue(curInDataType, v.rangeMin, v.rangeMax);
                            }

                            if (funcInfo.type == FUNCTION)
                                shaderOp += ")";

                            shaderOp += ";";

                            desc += ").";
                            name += shaderTypeName;

                            // Create the test case.
                            innerGroup->addChild(new ShaderOperatorCase(m_context, name.c_str(), desc.c_str(),
                                                                        isVertexCase, evalFunc, shaderOp.c_str(),
                                                                        shaderSpec));
                        }
                    }
                }
            }
        }
    }

    // The ?: selection operator.

    static const struct
    {
        DataType type; // The type of "Y" and "Z" operands in "X ? Y : Z" (X is always bool).
        ShaderEvalFunc evalFunc;
    } s_selectionInfo[] = {{TYPE_FLOAT, eval_selection_float},     {TYPE_FLOAT_VEC2, eval_selection_vec2},
                           {TYPE_FLOAT_VEC3, eval_selection_vec3}, {TYPE_FLOAT_VEC4, eval_selection_vec4},
                           {TYPE_INT, eval_selection_int},         {TYPE_INT_VEC2, eval_selection_ivec2},
                           {TYPE_INT_VEC3, eval_selection_ivec3},  {TYPE_INT_VEC4, eval_selection_ivec4},
                           {TYPE_BOOL, eval_selection_bool},       {TYPE_BOOL_VEC2, eval_selection_bvec2},
                           {TYPE_BOOL_VEC3, eval_selection_bvec3}, {TYPE_BOOL_VEC4, eval_selection_bvec4}};

    TestCaseGroup *selectionGroup = new TestCaseGroup(m_context, "selection", "Selection operator tests");
    addChild(selectionGroup);

    for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(s_selectionInfo); typeNdx++)
    {
        DataType curType        = s_selectionInfo[typeNdx].type;
        ShaderEvalFunc evalFunc = s_selectionInfo[typeNdx].evalFunc;
        bool isBoolCase         = isDataTypeBoolOrBVec(curType);
        bool isFloatCase        = isDataTypeFloatOrVec(curType);
        bool isIntCase          = isDataTypeIntOrIVec(curType);
        const char *dataTypeStr = getDataTypeName(curType);

        DE_ASSERT(isBoolCase || isFloatCase || isIntCase);
        DE_UNREF(isIntCase);

        for (int precision = 0; precision < (int)PRECISION_LAST; precision++)
        {
            if (isBoolCase && precision != PRECISION_MEDIUMP) // Use mediump interpolators for booleans.
                continue;

            const char *precisionStr = getPrecisionName((Precision)precision);
            string precisionPrefix   = isBoolCase ? "" : (string(precisionStr) + "_");

            for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(s_shaderTypes); shaderTypeNdx++)
            {
                ShaderType shaderType = s_shaderTypes[shaderTypeNdx];
                ShaderDataSpec shaderSpec;
                const char *shaderTypeName = getShaderTypeName(shaderType);
                bool isVertexCase          = (ShaderType)shaderType == SHADERTYPE_VERTEX;

                string name = precisionPrefix + dataTypeStr + "_" + shaderTypeName;

                shaderSpec.numInputs   = 3;
                shaderSpec.precision   = isBoolCase ? PRECISION_LAST : (Precision)precision;
                shaderSpec.output      = curType;
                shaderSpec.resultScale = isBoolCase ? 1.0f : isFloatCase ? 0.5f : 0.1f;
                shaderSpec.resultBias  = isBoolCase ? 0.0f : isFloatCase ? 0.5f : 0.5f;

                float rangeMin = isBoolCase ? -1.0f : isFloatCase ? -1.0f : -5.0f;
                float rangeMax = isBoolCase ? 1.0f : isFloatCase ? 1.0f : 5.0f;

                shaderSpec.inputs[0] = ShaderValue(TYPE_BOOL, -1.0f, 1.0f);
                shaderSpec.inputs[1] = ShaderValue(curType, rangeMin, rangeMax);
                shaderSpec.inputs[2] = ShaderValue(curType, rangeMin, rangeMax);

                selectionGroup->addChild(new ShaderOperatorCase(m_context, name.c_str(), "", isVertexCase, evalFunc,
                                                                "res = in0 ? in1 : in2;", shaderSpec));
            }
        }
    }

    // The sequence operator (comma).

    TestCaseGroup *sequenceGroup = new TestCaseGroup(m_context, "sequence", "Sequence operator tests");
    addChild(sequenceGroup);

    TestCaseGroup *sequenceNoSideEffGroup =
        new TestCaseGroup(m_context, "no_side_effects", "Sequence tests without side-effects");
    TestCaseGroup *sequenceSideEffGroup =
        new TestCaseGroup(m_context, "side_effects", "Sequence tests with side-effects");
    sequenceGroup->addChild(sequenceNoSideEffGroup);
    sequenceGroup->addChild(sequenceSideEffGroup);

    static const struct
    {
        bool containsSideEffects;
        const char *caseName;
        const char *expressionStr;
        int numInputs;
        DataType inputTypes[MAX_INPUTS];
        DataType resultType;
        ShaderEvalFunc evalFunc;
    } s_sequenceCases[] = {{false,
                            "vec4",
                            "in0, in2 + in1, in1 + in0",
                            3,
                            {TYPE_FLOAT_VEC4, TYPE_FLOAT_VEC4, TYPE_FLOAT_VEC4},
                            TYPE_FLOAT_VEC4,
                            evalSequenceNoSideEffCase0},
                           {false,
                            "float_int",
                            "in0 + in2, in1 + in1",
                            3,
                            {TYPE_FLOAT, TYPE_INT, TYPE_FLOAT},
                            TYPE_INT,
                            evalSequenceNoSideEffCase1},
                           {false,
                            "bool_vec2",
                            "in0 && in1, in0, ivec2(vec2(in0) + in2)",
                            3,
                            {TYPE_BOOL, TYPE_BOOL, TYPE_FLOAT_VEC2},
                            TYPE_INT_VEC2,
                            evalSequenceNoSideEffCase2},
                           {false,
                            "vec4_ivec4_bvec4",
                            "in0 + vec4(in1), in2, in1",
                            3,
                            {TYPE_FLOAT_VEC4, TYPE_INT_VEC4, TYPE_BOOL_VEC4},
                            TYPE_INT_VEC4,
                            evalSequenceNoSideEffCase3},

                           {true,
                            "vec4",
                            "in0++, in1 = in0 + in2, in2 = in1",
                            3,
                            {TYPE_FLOAT_VEC4, TYPE_FLOAT_VEC4, TYPE_FLOAT_VEC4},
                            TYPE_FLOAT_VEC4,
                            evalSequenceSideEffCase0},
                           {true,
                            "float_int",
                            "in1++, in0 = float(in1), in1 = int(in0 + in2)",
                            3,
                            {TYPE_FLOAT, TYPE_INT, TYPE_FLOAT},
                            TYPE_INT,
                            evalSequenceSideEffCase1},
                           {true,
                            "bool_vec2",
                            "in1 = in0, in2++, in2 = in2 + vec2(in1), ivec2(in2)",
                            3,
                            {TYPE_BOOL, TYPE_BOOL, TYPE_FLOAT_VEC2},
                            TYPE_INT_VEC2,
                            evalSequenceSideEffCase2},
                           {true,
                            "vec4_ivec4_bvec4",
                            "in0 = in0 + vec4(in2), in1 = in1 + ivec4(in0), in1++",
                            3,
                            {TYPE_FLOAT_VEC4, TYPE_INT_VEC4, TYPE_BOOL_VEC4},
                            TYPE_INT_VEC4,
                            evalSequenceSideEffCase3}};

    for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(s_sequenceCases); caseNdx++)
    {
        for (int precision = 0; precision < (int)PRECISION_LAST; precision++)
        {
            for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(s_shaderTypes); shaderTypeNdx++)
            {
                ShaderType shaderType = s_shaderTypes[shaderTypeNdx];
                ShaderDataSpec shaderSpec;
                const char *shaderTypeName = getShaderTypeName(shaderType);
                bool isVertexCase          = (ShaderType)shaderType == SHADERTYPE_VERTEX;

                string name = string("") + getPrecisionName((Precision)precision) + "_" +
                              s_sequenceCases[caseNdx].caseName + "_" + shaderTypeName;

                shaderSpec.numInputs   = s_sequenceCases[caseNdx].numInputs;
                shaderSpec.precision   = (Precision)precision;
                shaderSpec.output      = s_sequenceCases[caseNdx].resultType;
                shaderSpec.resultScale = 0.5f;
                shaderSpec.resultBias  = 0.0f;

                for (int inputNdx = 0; inputNdx < s_sequenceCases[caseNdx].numInputs; inputNdx++)
                {
                    DataType type  = s_sequenceCases[caseNdx].inputTypes[inputNdx];
                    float rangeMin = isDataTypeFloatOrVec(type) ? -0.5f : isDataTypeIntOrIVec(type) ? -2.0f : -1.0f;
                    float rangeMax = isDataTypeFloatOrVec(type) ? 0.5f : isDataTypeIntOrIVec(type) ? 2.0f : 1.0f;

                    shaderSpec.inputs[inputNdx] = ShaderValue(type, rangeMin, rangeMax);
                }

                string expression = string("") + "res = (" + s_sequenceCases[caseNdx].expressionStr + ");";

                TestCaseGroup *group =
                    s_sequenceCases[caseNdx].containsSideEffects ? sequenceSideEffGroup : sequenceNoSideEffGroup;
                group->addChild(new ShaderOperatorCase(m_context, name.c_str(), "", isVertexCase,
                                                       s_sequenceCases[caseNdx].evalFunc, expression.c_str(),
                                                       shaderSpec));
            }
        }
    }

    // Regression tests for sequence operator.
    // http://khronos.org/registry/webgl/sdk/tests/conformance/glsl/bugs/sequence-operator-evaluation-order.html
    {
        class Case : public ShaderRenderCase
        {
            static void evalFunc(ShaderEvalContext &c)
            {
                c.color = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f); // green
            }

        public:
            Case(Context &context, const char *name, const char *description, const char *fragShaderSource)
                : ShaderRenderCase(context.getTestContext(), context.getRenderContext(), context.getContextInfo(), name,
                                   description, false, &evalFunc)
            {
                m_vertShaderSource = "attribute vec4 a_position;\n"
                                     "void main()\n"
                                     "{\n"
                                     "    gl_Position = a_position;\n"
                                     "}\n";
                m_fragShaderSource = fragShaderSource;
            }
        };

        // ESSL 1.00 section 5.9, about sequence operator:
        // "All expressions are evaluated, in order, from left to right"
        // Also use a ternary operator where the third operand has side effects to make sure
        // only the second operand is evaluated.
        sequenceSideEffGroup->addChild(new Case(m_context, "affect_ternary",
                                                "Expression where first operand of a sequence operator has side "
                                                "effects which affect the second operand that is a ternary operator",
                                                ("precision mediump float;\n"
                                                 "bool correct = true;\n"
                                                 "uniform float u_zero;\n"
                                                 "float wrong() {\n"
                                                 "    correct = false;\n"
                                                 "    return 0.0;\n"
                                                 "}\n"
                                                 "void main() {\n"
                                                 "    float a = u_zero - 0.5; // Result should be -0.5.\n"
                                                 "    float green = (a++, a > 0.0 ? 1.0 : wrong());\n"
                                                 "    gl_FragColor = vec4(0.0, correct ? green : 0.0, 0.0, 1.0);\n"
                                                 "}\n")));

        sequenceSideEffGroup->addChild(
            new Case(m_context, "affect_and",
                     "Expression where first operand of a sequence operator has side effects which affect the second "
                     "operand that is an and operator",
                     ("precision mediump float;\n"
                      "uniform bool u_false;\n"
                      "bool sideEffectA = false;\n"
                      "bool funcA() {\n"
                      "    sideEffectA = true;\n"
                      "    return true;\n"
                      "}\n"
                      "bool sideEffectB = false;\n"
                      "bool funcB() {\n"
                      "    sideEffectB = true;\n"
                      "    return true;\n"
                      "}\n"
                      "void main() {\n"
                      "    bool b = (funcA(), u_false == sideEffectA && funcB());\n"
                      "    gl_FragColor = (!b && sideEffectA && !sideEffectB) ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);\n"
                      "}\n")));

        sequenceSideEffGroup->addChild(
            new Case(m_context, "affect_or",
                     "Expression where first operand of a sequence operator has side effects which affect the second "
                     "operand that is an or operator",
                     ("precision mediump float;\n"
                      "uniform bool u_false;\n"
                      "bool sideEffectA = false;\n"
                      "bool funcA() {\n"
                      "    sideEffectA = true;\n"
                      "    return false;\n"
                      "}\n"
                      "bool sideEffectB = false;\n"
                      "bool funcB() {\n"
                      "    sideEffectB = true;\n"
                      "    return false;\n"
                      "}\n"
                      "void main() {\n"
                      "    bool b = (funcA(), (u_false == !sideEffectA) || funcB());\n"
                      "    gl_FragColor = (b && sideEffectA && !sideEffectB) ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);\n"
                      "}\n")));
    }
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
