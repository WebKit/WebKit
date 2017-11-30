/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
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
 * \brief Shader matrix arithmetic tests.
 *
 * Variables:
 *  + operation
 *    - mat OP mat
 *    - mat OP vec
 *    - vec OP mat
 *    - mat OP scalar
 *    - OP ( mat )
 *    - vec OP vec
 *    - OP mat
 *  + matrix source
 *    - constant (ctor)
 *    - uniform
 *    - vertex input
 *    - fragment input
 *  + other operand: always dynamic data?
 *  + how to reduce to vec3?
 *//*--------------------------------------------------------------------*/

#include "es3fShaderMatrixTests.hpp"
#include "glsShaderRenderCase.hpp"
#include "gluShaderUtil.hpp"
#include "tcuVector.hpp"
#include "tcuMatrix.hpp"
#include "tcuMatrixUtil.hpp"
#include "deStringUtil.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

using std::string;
using std::vector;
using namespace glu;
using namespace deqp::gls;

using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;
using tcu::Mat2;
using tcu::Mat2x3;
using tcu::Mat2x4;
using tcu::Mat3x2;
using tcu::Mat3;
using tcu::Mat3x4;
using tcu::Mat4x2;
using tcu::Mat4x3;
using tcu::Mat4;

// Uniform / constant values for tests.
// \note Input1 should not contain 0 components as it is used as divisor in div cases.
// \todo [2012-02-14 pyry] Make these dynamic.
static const float  s_constInFloat[2]   = { 0.5f, -0.2f };
static const Vec2   s_constInVec2[2]    = { Vec2(1.2f, 0.5f), Vec2(0.5f, 1.0f) };
static const Vec3   s_constInVec3[2]    = { Vec3(1.1f, 0.1f, 0.5f), Vec3(-0.2f, 0.5f, 0.8f) };
static const Vec4   s_constInVec4[2]    = { Vec4(1.4f, 0.2f, -0.5f, 0.7f), Vec4(0.2f, -1.0f, 0.5f, 0.8f) };

static const float s_constInMat2x2[2][4] =
{
    {
        -0.1f,  1.0f,
        -0.2f,  0.0f,
    },
    {
         0.8f,  0.1f,
         0.5f, -0.9f,
    },
};
static const float s_constInMat3x2[2][6] =
{
    {
         0.8f, -0.3f,  0.3f,
         1.0f,  1.2f, -1.2f,
    },
    {
         1.2f, -1.0f,  0.5f,
        -0.8f,  1.1f,  0.3f,
    },
};
static const float s_constInMat4x2[2][8] =
{
    {
        -0.2f,  0.5f, 0.0f, -1.0f,
         1.2f, -0.5f, 0.3f, -0.9f,
    },
    {
        1.0f,  0.1f, -1.1f,  0.6f,
        0.8f, -1.2f, -1.1f,  0.7f,
    },
};
static const float s_constInMat2x3[2][6] =
{
    {
        -0.6f, -0.1f,
        -0.7f, -1.2f,
        -0.2f,  0.0f,
    },
    {
         1.1f,  0.6f,
         0.8f,  1.0f,
         0.7f,  0.1f,
    },
};
static const float s_constInMat3x3[2][9] =
{
    {
        -0.2f,  1.1f, 1.2f,
        -1.0f,  1.2f, 0.5f,
         0.7f, -0.2f, 1.0f,
    },
    {
        -0.1f, -0.1f,  0.1f,
        -0.1f, -0.2f,  1.0f,
        -0.5f,  0.1f, -0.4f,
    },
};
static const float s_constInMat4x3[2][12] =
{
    {
        -0.9f,  0.0f,  0.6f,  0.2f,
         0.9f, -0.1f, -0.3f, -0.7f,
        -0.1f,  0.1f,  1.0f,  0.0f,
    },
    {
         0.5f,  0.7f,  0.7f,  1.2f,
         1.1f,  0.1f,  1.0f, -1.0f,
        -0.2f, -0.2f, -0.3f, -0.5f,
    },
};
static const float s_constInMat2x4[2][8] =
{
    {
        -0.6f, -1.1f,
        -0.6f, -0.6f,
        -0.2f, -0.6f,
        -0.1f, -0.1f,
    },
    {
        -1.2f, -1.0f,
         0.7f, -1.0f,
         0.7f,  0.7f,
        -0.4f, -0.3f,
    },
};
static const float s_constInMat3x4[2][12] =
{
    {
         0.6f, -0.4f,  1.2f,
         0.9f,  0.8f,  0.4f,
         1.1f,  0.3f,  0.5f,
        -0.2f,  0.0f,  1.1f,
    },
    {
        -0.8f,  1.2f, -0.2f,
        -1.1f, -0.9f, -0.5f,
        -1.2f,  1.0f,  1.2f,
         0.1f, -0.7f, -0.5f,
    },
};
static const float s_constInMat4x4[2][16] =
{
    {
         0.3f,  0.9f, -0.2f,  1.0f,
        -0.4f, -0.6f,  0.6f, -1.0f,
        -0.9f, -0.1f,  0.3f, -0.2f,
        -0.3f, -0.9f,  1.0f,  0.1f,
    },
    {
         0.4f, -0.7f, -0.8f,  0.7f,
        -0.4f, -0.8f,  0.6f, -0.3f,
         0.7f, -1.0f,  0.1f, -0.3f,
         0.2f,  0.6f,  0.4f, -1.0f,
    },
};

namespace MatrixCaseUtils
{

enum InputType
{
    INPUTTYPE_CONST = 0,
    INPUTTYPE_UNIFORM,
    INPUTTYPE_DYNAMIC,

    INPUTTYPE_LAST
};

struct ShaderInput
{
    ShaderInput (InputType inputType_, DataType dataType_, Precision precision_)
        : inputType (inputType_)
        , dataType  (dataType_)
        , precision (precision_)
    {
    }

    InputType       inputType;
    DataType        dataType;
    Precision       precision;
};

enum MatrixOp
{
    OP_ADD = 0,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_COMP_MUL,
    OP_OUTER_PRODUCT,
    OP_TRANSPOSE,
    OP_INVERSE,
    OP_DETERMINANT,
    OP_UNARY_PLUS,
    OP_NEGATION,
    OP_PRE_INCREMENT,
    OP_PRE_DECREMENT,
    OP_POST_INCREMENT,
    OP_POST_DECREMENT,
    OP_ADD_INTO,
    OP_SUBTRACT_FROM,
    OP_MULTIPLY_INTO,
    OP_DIVIDE_INTO,
    OP_LAST
};

// Type traits.

template <int DataT>
struct TypeTraits;

#define DECLARE_TYPE_TRAIT(DATATYPE, TYPE)  \
template<>                                  \
struct TypeTraits<DATATYPE> {               \
    typedef TYPE Type;                      \
}

DECLARE_TYPE_TRAIT(TYPE_FLOAT,          float);
DECLARE_TYPE_TRAIT(TYPE_FLOAT_VEC2,     tcu::Vec2);
DECLARE_TYPE_TRAIT(TYPE_FLOAT_VEC3,     tcu::Vec3);
DECLARE_TYPE_TRAIT(TYPE_FLOAT_VEC4,     tcu::Vec4);
DECLARE_TYPE_TRAIT(TYPE_FLOAT_MAT2,     tcu::Mat2);
DECLARE_TYPE_TRAIT(TYPE_FLOAT_MAT2X3,   tcu::Mat2x3);
DECLARE_TYPE_TRAIT(TYPE_FLOAT_MAT2X4,   tcu::Mat2x4);
DECLARE_TYPE_TRAIT(TYPE_FLOAT_MAT3X2,   tcu::Mat3x2);
DECLARE_TYPE_TRAIT(TYPE_FLOAT_MAT3,     tcu::Mat3);
DECLARE_TYPE_TRAIT(TYPE_FLOAT_MAT3X4,   tcu::Mat3x4);
DECLARE_TYPE_TRAIT(TYPE_FLOAT_MAT4X2,   tcu::Mat4x2);
DECLARE_TYPE_TRAIT(TYPE_FLOAT_MAT4X3,   tcu::Mat4x3);
DECLARE_TYPE_TRAIT(TYPE_FLOAT_MAT4,     tcu::Mat4);

// Operation info

enum OperationType
{
    OPERATIONTYPE_BINARY_OPERATOR = 0,
    OPERATIONTYPE_BINARY_FUNCTION,
    OPERATIONTYPE_UNARY_PREFIX_OPERATOR,
    OPERATIONTYPE_UNARY_POSTFIX_OPERATOR,
    OPERATIONTYPE_UNARY_FUNCTION,
    OPERATIONTYPE_ASSIGNMENT,

    OPERATIONTYPE_LAST
};

static const char* getOperationName (MatrixOp op)
{
    switch (op)
    {
        case OP_ADD:            return "+";
        case OP_SUB:            return "-";
        case OP_MUL:            return "*";
        case OP_DIV:            return "/";
        case OP_COMP_MUL:       return "matrixCompMult";
        case OP_OUTER_PRODUCT:  return "outerProduct";
        case OP_TRANSPOSE:      return "transpose";
        case OP_INVERSE:        return "inverse";
        case OP_DETERMINANT:    return "determinant";
        case OP_UNARY_PLUS:     return "+";
        case OP_NEGATION:       return "-";
        case OP_PRE_INCREMENT:  return "++";
        case OP_PRE_DECREMENT:  return "--";
        case OP_POST_INCREMENT: return "++";
        case OP_POST_DECREMENT: return "--";
        case OP_ADD_INTO:       return "+=";
        case OP_SUBTRACT_FROM:  return "-=";
        case OP_MULTIPLY_INTO:  return "*=";
        case OP_DIVIDE_INTO:    return "/=";

        default:
            DE_ASSERT(DE_FALSE);
            return "";
    }
}

static OperationType getOperationType (MatrixOp op)
{
    switch (op)
    {
        case OP_ADD:            return OPERATIONTYPE_BINARY_OPERATOR;
        case OP_SUB:            return OPERATIONTYPE_BINARY_OPERATOR;
        case OP_MUL:            return OPERATIONTYPE_BINARY_OPERATOR;
        case OP_DIV:            return OPERATIONTYPE_BINARY_OPERATOR;
        case OP_COMP_MUL:       return OPERATIONTYPE_BINARY_FUNCTION;
        case OP_OUTER_PRODUCT:  return OPERATIONTYPE_BINARY_FUNCTION;
        case OP_TRANSPOSE:      return OPERATIONTYPE_UNARY_FUNCTION;
        case OP_INVERSE:        return OPERATIONTYPE_UNARY_FUNCTION;
        case OP_DETERMINANT:    return OPERATIONTYPE_UNARY_FUNCTION;
        case OP_UNARY_PLUS:     return OPERATIONTYPE_UNARY_PREFIX_OPERATOR;
        case OP_NEGATION:       return OPERATIONTYPE_UNARY_PREFIX_OPERATOR;
        case OP_PRE_INCREMENT:  return OPERATIONTYPE_UNARY_PREFIX_OPERATOR;
        case OP_PRE_DECREMENT:  return OPERATIONTYPE_UNARY_PREFIX_OPERATOR;
        case OP_POST_INCREMENT: return OPERATIONTYPE_UNARY_POSTFIX_OPERATOR;
        case OP_POST_DECREMENT: return OPERATIONTYPE_UNARY_POSTFIX_OPERATOR;
        case OP_ADD_INTO:       return OPERATIONTYPE_ASSIGNMENT;
        case OP_SUBTRACT_FROM:  return OPERATIONTYPE_ASSIGNMENT;
        case OP_MULTIPLY_INTO:  return OPERATIONTYPE_ASSIGNMENT;
        case OP_DIVIDE_INTO:    return OPERATIONTYPE_ASSIGNMENT;
        default:
            DE_ASSERT(DE_FALSE);
            return OPERATIONTYPE_LAST;
    }
}

enum TestMatrixType
{
    TESTMATRIXTYPE_DEFAULT = 0,
    TESTMATRIXTYPE_NEGATED,
    TESTMATRIXTYPE_INCREMENTED,
    TESTMATRIXTYPE_DECREMENTED,
    TESTMATRIXTYPE_NEGATED_INCREMENTED,
    TESTMATRIXTYPE_INCREMENTED_LESS,

    TESTMATRIXTYPE_LAST
};

static TestMatrixType getOperationTestMatrixType (MatrixOp op)
{
    switch(op)
    {
        case OP_ADD:            return TESTMATRIXTYPE_DEFAULT;
        case OP_SUB:            return TESTMATRIXTYPE_DEFAULT;
        case OP_MUL:            return TESTMATRIXTYPE_DEFAULT;
        case OP_DIV:            return TESTMATRIXTYPE_DEFAULT;
        case OP_COMP_MUL:       return TESTMATRIXTYPE_DEFAULT;
        case OP_OUTER_PRODUCT:  return TESTMATRIXTYPE_DEFAULT;
        case OP_TRANSPOSE:      return TESTMATRIXTYPE_DEFAULT;
        case OP_INVERSE:        return TESTMATRIXTYPE_DEFAULT;
        case OP_DETERMINANT:    return TESTMATRIXTYPE_DEFAULT;
        case OP_UNARY_PLUS:     return TESTMATRIXTYPE_DECREMENTED;
        case OP_NEGATION:       return TESTMATRIXTYPE_NEGATED_INCREMENTED;
        case OP_PRE_INCREMENT:  return TESTMATRIXTYPE_NEGATED;
        case OP_PRE_DECREMENT:  return TESTMATRIXTYPE_INCREMENTED;
        case OP_POST_INCREMENT: return TESTMATRIXTYPE_NEGATED;
        case OP_POST_DECREMENT: return TESTMATRIXTYPE_DEFAULT;
        case OP_ADD_INTO:       return TESTMATRIXTYPE_DEFAULT;
        case OP_SUBTRACT_FROM:  return TESTMATRIXTYPE_INCREMENTED_LESS;
        case OP_MULTIPLY_INTO:  return TESTMATRIXTYPE_NEGATED;
        case OP_DIVIDE_INTO:    return TESTMATRIXTYPE_DECREMENTED;

        default:
            DE_ASSERT(DE_FALSE);
            return TESTMATRIXTYPE_LAST;
    }
}

static bool isOperationBinary (MatrixOp op)
{
    return getOperationType(op) == OPERATIONTYPE_BINARY_OPERATOR ||
           getOperationType(op) == OPERATIONTYPE_BINARY_FUNCTION ||
           getOperationType(op) == OPERATIONTYPE_ASSIGNMENT;
}

static bool isOperationMatrixScalar (MatrixOp op)
{
    return op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_DIV;
}

static bool isOperationMatrixVector (MatrixOp op)
{
    return op == OP_MUL;
}

static bool isOperationArithmeticMatrixMatrix (MatrixOp op)
{
    return op == OP_MUL;
}

static bool isOperationComponentwiseMatrixMatrix (MatrixOp op)
{
    return op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_DIV || op == OP_COMP_MUL;
}

static bool isOperationVectorVector (MatrixOp op)
{
    return op == OP_OUTER_PRODUCT;
}

static bool isOperationUnaryAnyMatrix (MatrixOp op)
{
    return  op == OP_TRANSPOSE           ||
            op == OP_UNARY_PLUS          ||
            op == OP_NEGATION            ||
            op == OP_PRE_INCREMENT       ||
            op == OP_PRE_DECREMENT       ||
            op == OP_POST_INCREMENT      ||
            op == OP_POST_DECREMENT;
}

static bool isOperationUnarySymmetricMatrix (MatrixOp op)
{
    return op == OP_INVERSE || op == OP_DETERMINANT;
}

static bool isOperationValueModifying (MatrixOp op)
{
    return  op == OP_PRE_INCREMENT       ||
            op == OP_PRE_DECREMENT       ||
            op == OP_POST_INCREMENT      ||
            op == OP_POST_DECREMENT;
}

static bool isOperationAssignment (MatrixOp op)
{
    return  op == OP_ADD_INTO        ||
            op == OP_SUBTRACT_FROM   ||
            op == OP_MULTIPLY_INTO   ||
            op == OP_DIVIDE_INTO;
}

static bool isOperationAssignmentAnyMatrix (MatrixOp op)
{
    return  op == OP_ADD_INTO        ||
            op == OP_SUBTRACT_FROM   ||
            op == OP_DIVIDE_INTO;
}

static bool isOperationAssignmentSymmetricMatrix (MatrixOp op)
{
    return op == OP_MULTIPLY_INTO;
}

// Operation nature

enum OperationNature
{
    OPERATIONNATURE_PURE = 0,
    OPERATIONNATURE_MUTATING,
    OPERATIONNATURE_ASSIGNMENT,

    OPERATIONNATURE_LAST
};

static OperationNature getOperationNature (MatrixOp op)
{
    if (isOperationAssignment(op))
        return OPERATIONNATURE_ASSIGNMENT;

    if (isOperationValueModifying(op))
        return OPERATIONNATURE_MUTATING;

    return OPERATIONNATURE_PURE;
}

// Input value loader.

template <int InputT, int DataT>
typename TypeTraits<DataT>::Type getInputValue (const ShaderEvalContext& evalCtx, int inputNdx);

template <> inline float        getInputValue<INPUTTYPE_CONST,      TYPE_FLOAT>         (const ShaderEvalContext& evalCtx, int inputNdx) { DE_UNREF(evalCtx); return s_constInFloat[inputNdx];  }
template <> inline tcu::Vec2    getInputValue<INPUTTYPE_CONST,      TYPE_FLOAT_VEC2>    (const ShaderEvalContext& evalCtx, int inputNdx) { DE_UNREF(evalCtx); return s_constInVec2[inputNdx];   }
template <> inline tcu::Vec3    getInputValue<INPUTTYPE_CONST,      TYPE_FLOAT_VEC3>    (const ShaderEvalContext& evalCtx, int inputNdx) { DE_UNREF(evalCtx); return s_constInVec3[inputNdx];   }
template <> inline tcu::Vec4    getInputValue<INPUTTYPE_CONST,      TYPE_FLOAT_VEC4>    (const ShaderEvalContext& evalCtx, int inputNdx) { DE_UNREF(evalCtx); return s_constInVec4[inputNdx];   }

template <> inline tcu::Mat2    getInputValue<INPUTTYPE_CONST,      TYPE_FLOAT_MAT2>    (const ShaderEvalContext& evalCtx, int inputNdx) { DE_UNREF(evalCtx); return tcu::Mat2(s_constInMat2x2[inputNdx]);      }
template <> inline tcu::Mat2x3  getInputValue<INPUTTYPE_CONST,      TYPE_FLOAT_MAT2X3>  (const ShaderEvalContext& evalCtx, int inputNdx) { DE_UNREF(evalCtx); return tcu::Mat2x3(s_constInMat2x3[inputNdx]);    }
template <> inline tcu::Mat2x4  getInputValue<INPUTTYPE_CONST,      TYPE_FLOAT_MAT2X4>  (const ShaderEvalContext& evalCtx, int inputNdx) { DE_UNREF(evalCtx); return tcu::Mat2x4(s_constInMat2x4[inputNdx]);    }
template <> inline tcu::Mat3x2  getInputValue<INPUTTYPE_CONST,      TYPE_FLOAT_MAT3X2>  (const ShaderEvalContext& evalCtx, int inputNdx) { DE_UNREF(evalCtx); return tcu::Mat3x2(s_constInMat3x2[inputNdx]);    }
template <> inline tcu::Mat3    getInputValue<INPUTTYPE_CONST,      TYPE_FLOAT_MAT3>    (const ShaderEvalContext& evalCtx, int inputNdx) { DE_UNREF(evalCtx); return tcu::Mat3(s_constInMat3x3[inputNdx]);      }
template <> inline tcu::Mat3x4  getInputValue<INPUTTYPE_CONST,      TYPE_FLOAT_MAT3X4>  (const ShaderEvalContext& evalCtx, int inputNdx) { DE_UNREF(evalCtx); return tcu::Mat3x4(s_constInMat3x4[inputNdx]);    }
template <> inline tcu::Mat4x2  getInputValue<INPUTTYPE_CONST,      TYPE_FLOAT_MAT4X2>  (const ShaderEvalContext& evalCtx, int inputNdx) { DE_UNREF(evalCtx); return tcu::Mat4x2(s_constInMat4x2[inputNdx]);    }
template <> inline tcu::Mat4x3  getInputValue<INPUTTYPE_CONST,      TYPE_FLOAT_MAT4X3>  (const ShaderEvalContext& evalCtx, int inputNdx) { DE_UNREF(evalCtx); return tcu::Mat4x3(s_constInMat4x3[inputNdx]);    }
template <> inline tcu::Mat4    getInputValue<INPUTTYPE_CONST,      TYPE_FLOAT_MAT4>    (const ShaderEvalContext& evalCtx, int inputNdx) { DE_UNREF(evalCtx); return tcu::Mat4(s_constInMat4x4[inputNdx]);      }

template <> inline float        getInputValue<INPUTTYPE_DYNAMIC,    TYPE_FLOAT>         (const ShaderEvalContext& evalCtx, int inputNdx) { DE_UNREF(inputNdx); return evalCtx.coords.x();                   }
template <> inline tcu::Vec2    getInputValue<INPUTTYPE_DYNAMIC,    TYPE_FLOAT_VEC2>    (const ShaderEvalContext& evalCtx, int inputNdx) { DE_UNREF(inputNdx); return evalCtx.coords.swizzle(0, 1);         }
template <> inline tcu::Vec3    getInputValue<INPUTTYPE_DYNAMIC,    TYPE_FLOAT_VEC3>    (const ShaderEvalContext& evalCtx, int inputNdx) { DE_UNREF(inputNdx); return evalCtx.coords.swizzle(0, 1, 2);      }
template <> inline tcu::Vec4    getInputValue<INPUTTYPE_DYNAMIC,    TYPE_FLOAT_VEC4>    (const ShaderEvalContext& evalCtx, int inputNdx) { DE_UNREF(inputNdx); return evalCtx.coords.swizzle(0, 1, 2, 3);   }

template <> inline tcu::Mat2 getInputValue<INPUTTYPE_DYNAMIC, TYPE_FLOAT_MAT2> (const ShaderEvalContext& evalCtx, int inputNdx)
{
    DE_UNREF(inputNdx); // Not used.
    tcu::Mat2 m;
    m.setColumn(0, evalCtx.in[0].swizzle(0,1));
    m.setColumn(1, evalCtx.in[1].swizzle(0,1));
    return m;
}

template <> inline tcu::Mat2x3 getInputValue<INPUTTYPE_DYNAMIC, TYPE_FLOAT_MAT2X3> (const ShaderEvalContext& evalCtx, int inputNdx)
{
    DE_UNREF(inputNdx); // Not used.
    tcu::Mat2x3 m;
    m.setColumn(0, evalCtx.in[0].swizzle(0,1,2));
    m.setColumn(1, evalCtx.in[1].swizzle(0,1,2));
    return m;
}

template <> inline tcu::Mat2x4 getInputValue<INPUTTYPE_DYNAMIC, TYPE_FLOAT_MAT2X4> (const ShaderEvalContext& evalCtx, int inputNdx)
{
    DE_UNREF(inputNdx); // Not used.
    tcu::Mat2x4 m;
    m.setColumn(0, evalCtx.in[0]);
    m.setColumn(1, evalCtx.in[1]);
    return m;
}

template <> inline tcu::Mat3x2 getInputValue<INPUTTYPE_DYNAMIC, TYPE_FLOAT_MAT3X2> (const ShaderEvalContext& evalCtx, int inputNdx)
{
    DE_UNREF(inputNdx); // Not used.
    tcu::Mat3x2 m;
    m.setColumn(0, evalCtx.in[0].swizzle(0,1));
    m.setColumn(1, evalCtx.in[1].swizzle(0,1));
    m.setColumn(2, evalCtx.in[2].swizzle(0,1));
    return m;
}

template <> inline tcu::Mat3 getInputValue<INPUTTYPE_DYNAMIC, TYPE_FLOAT_MAT3> (const ShaderEvalContext& evalCtx, int inputNdx)
{
    DE_UNREF(inputNdx); // Not used.
    tcu::Mat3 m;
    m.setColumn(0, evalCtx.in[0].swizzle(0,1,2));
    m.setColumn(1, evalCtx.in[1].swizzle(0,1,2));
    m.setColumn(2, evalCtx.in[2].swizzle(0,1,2));
    return m;
}

template <> inline tcu::Mat3x4 getInputValue<INPUTTYPE_DYNAMIC, TYPE_FLOAT_MAT3X4> (const ShaderEvalContext& evalCtx, int inputNdx)
{
    DE_UNREF(inputNdx); // Not used.
    tcu::Mat3x4 m;
    m.setColumn(0, evalCtx.in[0]);
    m.setColumn(1, evalCtx.in[1]);
    m.setColumn(2, evalCtx.in[2]);
    return m;
}

template <> inline tcu::Mat4x2 getInputValue<INPUTTYPE_DYNAMIC, TYPE_FLOAT_MAT4X2> (const ShaderEvalContext& evalCtx, int inputNdx)
{
    DE_UNREF(inputNdx); // Not used.
    tcu::Mat4x2 m;
    m.setColumn(0, evalCtx.in[0].swizzle(0,1));
    m.setColumn(1, evalCtx.in[1].swizzle(0,1));
    m.setColumn(2, evalCtx.in[2].swizzle(0,1));
    m.setColumn(3, evalCtx.in[3].swizzle(0,1));
    return m;
}

template <> inline tcu::Mat4x3 getInputValue<INPUTTYPE_DYNAMIC, TYPE_FLOAT_MAT4X3> (const ShaderEvalContext& evalCtx, int inputNdx)
{
    DE_UNREF(inputNdx); // Not used.
    tcu::Mat4x3 m;
    m.setColumn(0, evalCtx.in[0].swizzle(0,1,2));
    m.setColumn(1, evalCtx.in[1].swizzle(0,1,2));
    m.setColumn(2, evalCtx.in[2].swizzle(0,1,2));
    m.setColumn(3, evalCtx.in[3].swizzle(0,1,2));
    return m;
}

template <> inline tcu::Mat4 getInputValue<INPUTTYPE_DYNAMIC, TYPE_FLOAT_MAT4> (const ShaderEvalContext& evalCtx, int inputNdx)
{
    DE_UNREF(inputNdx); // Not used.
    tcu::Mat4 m;
    m.setColumn(0, evalCtx.in[0]);
    m.setColumn(1, evalCtx.in[1]);
    m.setColumn(2, evalCtx.in[2]);
    m.setColumn(3, evalCtx.in[3]);
    return m;
}

// Reduction from expression result to vec3.

inline tcu::Vec3 reduceToVec3 (const tcu::Vec2& value)      { return value.swizzle(0,1,0); }
inline tcu::Vec3 reduceToVec3 (const tcu::Vec3& value)      { return value; }
inline tcu::Vec3 reduceToVec3 (const tcu::Vec4& value)      { return tcu::Vec3(value.x(), value.y(), value.z()+value.w()); }
inline tcu::Vec3 reduceToVec3 (const tcu::Mat2& value)      { return tcu::Vec3(value(0, 0), value(0, 1), value(1, 0)+value(1, 1)); }
inline tcu::Vec3 reduceToVec3 (const tcu::Mat2x3& value)    { return value.getColumn(0) + value.getColumn(1); }
inline tcu::Vec3 reduceToVec3 (const tcu::Mat2x4& value)    { return value.getColumn(0).swizzle(0,1,2) + value.getColumn(1).swizzle(1,2,3); }
inline tcu::Vec3 reduceToVec3 (const tcu::Mat3x2& value)    { return tcu::Vec3(value(0,0)+value(1,0), value(0,1)+value(1,1), value(0,2)+value(1,2)); }
inline tcu::Vec3 reduceToVec3 (const tcu::Mat3& value)      { return value.getColumn(0) + value.getColumn(1) + value.getColumn(2); }
inline tcu::Vec3 reduceToVec3 (const tcu::Mat3x4& value)    { return value.getColumn(0).swizzle(0,1,2) + value.getColumn(1).swizzle(1,2,3) + value.getColumn(2).swizzle(2,3,0); }
inline tcu::Vec3 reduceToVec3 (const tcu::Mat4x2& value)    { return tcu::Vec3(value(0,0)+value(1,0)+value(0,3), value(0,1)+value(1,1)+value(1,3), value(0,2)+value(1,2)); }
inline tcu::Vec3 reduceToVec3 (const tcu::Mat4x3& value)    { return value.getColumn(0) + value.getColumn(1) + value.getColumn(2) + value.getColumn(3); }
inline tcu::Vec3 reduceToVec3 (const tcu::Mat4& value)      { return value.getColumn(0).swizzle(0,1,2) + value.getColumn(1).swizzle(1,2,3) + value.getColumn(2).swizzle(2,3,0) + value.getColumn(3).swizzle(3,0,1); }

// matrixCompMult

template <typename T, int Rows, int Cols>
tcu::Matrix<T, Rows, Cols> matrixCompMult (const tcu::Matrix<T, Rows, Cols>& a, const tcu::Matrix<T, Rows, Cols>& b)
{
    tcu::Matrix<T, Rows, Cols> retVal;

    for (int r = 0; r < Rows; ++r)
        for (int c = 0; c < Cols; ++c)
            retVal(r,c) = a(r,c) * b(r, c);

    return retVal;
}

// transpose

template <typename T, int Rows, int Cols>
tcu::Matrix<T, Cols, Rows> transpose (const tcu::Matrix<T, Rows, Cols>& mat)
{
    tcu::Matrix<T, Cols, Rows> retVal;

    for (int r = 0; r < Rows; ++r)
        for (int c = 0; c < Cols; ++c)
            retVal(c, r) = mat(r, c);

    return retVal;
}

// outerProduct

template <typename T, int Rows, int Cols>
tcu::Matrix<T, Cols, Rows> outerProduct (const tcu::Vector<T, Cols>& a, const tcu::Vector<T, Rows>& b)
{
    tcu::Matrix<T, Rows, Cols> retVal;

    for (int r = 0; r < Rows; ++r)
        for (int c = 0; c < Cols; ++c)
            retVal(r,c) = a[c] * b[r];

    return transpose(retVal); // to gl-form (column-major)
}

// Determinant

template <int Size>
float determinant (const tcu::Matrix<float, Size, Size>& mat);

template <>
float determinant<2> (const tcu::Matrix<float, 2, 2>& mat)
{
    return mat(0,0) * mat(1,1) - mat(1,0) * mat(0,1);
}

template <>
float determinant<3> (const tcu::Matrix<float, 3, 3>& mat)
{
    return  + mat(0,0) * mat(1,1) * mat(2,2)
            + mat(0,1) * mat(1,2) * mat(2,0)
            + mat(0,2) * mat(1,0) * mat(2,1)
            - mat(0,0) * mat(1,2) * mat(2,1)
            - mat(0,1) * mat(1,0) * mat(2,2)
            - mat(0,2) * mat(1,1) * mat(2,0);
}

template <>
float determinant<4> (const tcu::Matrix<float, 4, 4>& mat)
{
    const float minorMatrices[4][3*3] =
    {
        {
            mat(1,1),   mat(2,1),   mat(3,1),
            mat(1,2),   mat(2,2),   mat(3,2),
            mat(1,3),   mat(2,3),   mat(3,3),
        },
        {
            mat(1,0),   mat(2,0),   mat(3,0),
            mat(1,2),   mat(2,2),   mat(3,2),
            mat(1,3),   mat(2,3),   mat(3,3),
        },
        {
            mat(1,0),   mat(2,0),   mat(3,0),
            mat(1,1),   mat(2,1),   mat(3,1),
            mat(1,3),   mat(2,3),   mat(3,3),
        },
        {
            mat(1,0),   mat(2,0),   mat(3,0),
            mat(1,1),   mat(2,1),   mat(3,1),
            mat(1,2),   mat(2,2),   mat(3,2),
        }
    };

    return  + mat(0,0) * determinant(tcu::Mat3(minorMatrices[0]))
            - mat(0,1) * determinant(tcu::Mat3(minorMatrices[1]))
            + mat(0,2) * determinant(tcu::Mat3(minorMatrices[2]))
            - mat(0,3) * determinant(tcu::Mat3(minorMatrices[3]));
}

// Inverse

template <int Size>
tcu::Matrix<float, Size, Size> inverse (const tcu::Matrix<float, Size, Size>& mat);

template <>
tcu::Matrix<float, 2, 2> inverse<2> (const tcu::Matrix<float, 2, 2>& mat)
{
    const float                 det     = determinant(mat);
    tcu::Matrix<float, 2, 2>    retVal;

    DE_ASSERT(det != 0.0f);

    retVal(0, 0) =  mat(1, 1) / det;
    retVal(0, 1) = -mat(0, 1) / det;
    retVal(1, 0) = -mat(1, 0) / det;
    retVal(1, 1) =  mat(0, 0) / det;

    return retVal;
}

template <>
tcu::Matrix<float, 3, 3> inverse<3> (const tcu::Matrix<float, 3, 3>& mat)
{
    // Blockwise inversion

    DE_ASSERT(determinant(mat) != 0.0f);

    const float areaA[2*2] =
    {
        mat(0,0),   mat(0,1),
        mat(1,0),   mat(1,1)
    };
    const float areaB[2] =
    {
        mat(0,2),
        mat(1,2),
    };
    const float areaC[2] =
    {
        mat(2,0),   mat(2,1),
    };
    const float areaD[1] =
    {
        mat(2,2)
    };
    const float nullField[4] = { 0.0f };

    const tcu::Matrix<float, 2, 2>  invA = inverse(tcu::Matrix<float, 2, 2>(areaA));
    const tcu::Matrix<float, 2, 1>  matB =         tcu::Matrix<float, 2, 1>(areaB);
    const tcu::Matrix<float, 1, 2>  matC =         tcu::Matrix<float, 1, 2>(areaC);
    const tcu::Matrix<float, 1, 1>  matD =         tcu::Matrix<float, 1, 1>(areaD);

    const float                     schurComplement = 1.0f / (matD - matC*invA*matB)(0,0);
    const tcu::Matrix<float, 2, 2>  zeroMat         = Mat2(nullField);

    const tcu::Matrix<float, 2, 2>  blockA = invA + invA*matB*schurComplement*matC*invA;
    const tcu::Matrix<float, 2, 1>  blockB = (zeroMat-invA)*matB*schurComplement;
    const tcu::Matrix<float, 1, 2>  blockC = matC*invA*(-schurComplement);
    const float                     blockD = schurComplement;

    const float result[3*3] =
    {
        blockA(0,0),    blockA(0,1),    blockB(0,0),
        blockA(1,0),    blockA(1,1),    blockB(1,0),
        blockC(0,0),    blockC(0,1),    blockD,
    };

    return Mat3(result);
}

template <>
tcu::Matrix<float, 4, 4> inverse<4> (const tcu::Matrix<float, 4, 4>& mat)
{
    // Blockwise inversion

    DE_ASSERT(determinant(mat) != 0.0f);

    const float areaA[2*2] =
    {
        mat(0,0),   mat(0,1),
        mat(1,0),   mat(1,1)
    };
    const float areaB[2*2] =
    {
        mat(0,2),   mat(0,3),
        mat(1,2),   mat(1,3)
    };
    const float areaC[2*2] =
    {
        mat(2,0),   mat(2,1),
        mat(3,0),   mat(3,1)
    };
    const float areaD[2*2] =
    {
        mat(2,2),   mat(2,3),
        mat(3,2),   mat(3,3)
    };
    const float nullField[4] = { 0.0f };

    const tcu::Matrix<float, 2, 2> invA = inverse(Mat2(areaA));
    const tcu::Matrix<float, 2, 2> matB =         Mat2(areaB);
    const tcu::Matrix<float, 2, 2> matC =         Mat2(areaC);
    const tcu::Matrix<float, 2, 2> matD =         Mat2(areaD);

    const tcu::Matrix<float, 2, 2> schurComplement = inverse(matD - matC*invA*matB);
    const tcu::Matrix<float, 2, 2> zeroMat         = Mat2(nullField);

    const tcu::Matrix<float, 2, 2> blockA = invA + invA*matB*schurComplement*matC*invA;
    const tcu::Matrix<float, 2, 2> blockB = (zeroMat-invA)*matB*schurComplement;
    const tcu::Matrix<float, 2, 2> blockC = (zeroMat-schurComplement)*matC*invA;
    const tcu::Matrix<float, 2, 2> blockD = schurComplement;

    const float result[4*4] =
    {
        blockA(0,0),    blockA(0,1),    blockB(0,0),    blockB(0,1),
        blockA(1,0),    blockA(1,1),    blockB(1,0),    blockB(1,1),
        blockC(0,0),    blockC(0,1),    blockD(0,0),    blockD(0,1),
        blockC(1,0),    blockC(1,1),    blockD(1,0),    blockD(1,1),
    };

    return Mat4(result);
}

// negate

template <typename T, int Rows, int Cols>
tcu::Matrix<T, Rows, Cols> negate (const tcu::Matrix<T, Rows, Cols>& mat)
{
    tcu::Matrix<T, Rows, Cols> retVal;

    for (int r = 0; r < Rows; ++r)
        for (int c = 0; c < Cols; ++c)
            retVal(r,c) = -mat(r, c);

    return retVal;
}

// increment/decrement

template <typename T, int Rows, int Cols>
tcu::Matrix<T, Rows, Cols> increment (const tcu::Matrix<T, Rows, Cols>& mat)
{
    tcu::Matrix<T, Rows, Cols> retVal;

    for (int r = 0; r < Rows; ++r)
        for (int c = 0; c < Cols; ++c)
            retVal(r,c) = mat(r, c) + 1.0f;

    return retVal;
}

template <typename T, int Rows, int Cols>
tcu::Matrix<T, Rows, Cols> decrement (const tcu::Matrix<T, Rows, Cols>& mat)
{
    tcu::Matrix<T, Rows, Cols> retVal;

    for (int r = 0; r < Rows; ++r)
        for (int c = 0; c < Cols; ++c)
            retVal(r,c) = mat(r, c) - 1.0f;

    return retVal;
}

// Evaluator template.

typedef void (*MatrixShaderEvalFunc) (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type);

template <int Op, int In0DataType, int In1DataType>
struct Evaluator;

template <int In0DataType, int In1DataType>
struct Evaluator<OP_ADD, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);
        typename TypeTraits<In1DataType>::Type  in1 = (in1Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In1DataType>(evalCtx, 1)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In1DataType>(evalCtx, 1);
        evalCtx.color.xyz() = reduceToVec3(in0 + in1);
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_SUB, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);
        typename TypeTraits<In1DataType>::Type  in1 = (in1Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In1DataType>(evalCtx, 1)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In1DataType>(evalCtx, 1);
        evalCtx.color.xyz() = reduceToVec3(in0 - in1);
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_MUL, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);
        typename TypeTraits<In1DataType>::Type  in1 = (in1Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In1DataType>(evalCtx, 1)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In1DataType>(evalCtx, 1);
        evalCtx.color.xyz() = reduceToVec3(in0 * in1);
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_DIV, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);
        typename TypeTraits<In1DataType>::Type  in1 = (in1Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In1DataType>(evalCtx, 1)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In1DataType>(evalCtx, 1);
        evalCtx.color.xyz() = reduceToVec3(in0 / in1);
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_COMP_MUL, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);
        typename TypeTraits<In1DataType>::Type  in1 = (in1Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In1DataType>(evalCtx, 1)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In1DataType>(evalCtx, 1);
        evalCtx.color.xyz() = reduceToVec3(matrixCompMult(in0, in1));
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_OUTER_PRODUCT, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);
        typename TypeTraits<In1DataType>::Type  in1 = (in1Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In1DataType>(evalCtx, 1)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In1DataType>(evalCtx, 1);
        evalCtx.color.xyz() = reduceToVec3(outerProduct(in0, in1));
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_TRANSPOSE, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        DE_UNREF(in1Type);
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);
        evalCtx.color.xyz() = reduceToVec3(transpose(in0));
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_INVERSE, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        DE_UNREF(in1Type);
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);
        evalCtx.color.xyz() = reduceToVec3(inverse(in0));
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_DETERMINANT, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        DE_UNREF(in1Type);
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);
        evalCtx.color.xyz() = Vec3(determinant(in0));
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_UNARY_PLUS, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        DE_UNREF(in1Type);
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);
        evalCtx.color.xyz() = reduceToVec3(in0);
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_NEGATION, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        DE_UNREF(in1Type);
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);
        evalCtx.color.xyz() = reduceToVec3(negate(in0));
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_PRE_INCREMENT, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        DE_UNREF(in1Type);
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);

        // modifying reduction: sum modified value too
        evalCtx.color.xyz() = reduceToVec3(increment(in0)) + reduceToVec3(increment(in0));
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_PRE_DECREMENT, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        DE_UNREF(in1Type);
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);

        // modifying reduction: sum modified value too
        evalCtx.color.xyz() = reduceToVec3(decrement(in0)) + reduceToVec3(decrement(in0));
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_POST_INCREMENT, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        DE_UNREF(in1Type);
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);

        // modifying reduction: sum modified value too
        evalCtx.color.xyz() = reduceToVec3(in0) + reduceToVec3(increment(in0));
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_POST_DECREMENT, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        DE_UNREF(in1Type);
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);

        // modifying reduction: sum modified value too
        evalCtx.color.xyz() = reduceToVec3(in0) + reduceToVec3(decrement(in0));
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_ADD_INTO, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);
        typename TypeTraits<In1DataType>::Type  in1 = (in1Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In1DataType>(evalCtx, 1)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In1DataType>(evalCtx, 1);
        evalCtx.color.xyz() = reduceToVec3(in0 + in1);
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_SUBTRACT_FROM, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);
        typename TypeTraits<In1DataType>::Type  in1 = (in1Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In1DataType>(evalCtx, 1)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In1DataType>(evalCtx, 1);
        evalCtx.color.xyz() = reduceToVec3(in0 - in1);
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_MULTIPLY_INTO, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);
        typename TypeTraits<In1DataType>::Type  in1 = (in1Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In1DataType>(evalCtx, 1)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In1DataType>(evalCtx, 1);
        evalCtx.color.xyz() = reduceToVec3(in0 * in1);
    }
};

template <int In0DataType, int In1DataType>
struct Evaluator<OP_DIVIDE_INTO, In0DataType, In1DataType>
{
    static void evaluate (ShaderEvalContext& evalCtx, InputType in0Type, InputType in1Type)
    {
        typename TypeTraits<In0DataType>::Type  in0 = (in0Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In0DataType>(evalCtx, 0)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In0DataType>(evalCtx, 0);
        typename TypeTraits<In1DataType>::Type  in1 = (in1Type == INPUTTYPE_DYNAMIC) ? getInputValue<INPUTTYPE_DYNAMIC, In1DataType>(evalCtx, 1)
                                                                                     : getInputValue<INPUTTYPE_CONST,   In1DataType>(evalCtx, 1);
        evalCtx.color.xyz() = reduceToVec3(in0 / in1);
    }
};

MatrixShaderEvalFunc getEvalFunc (const ShaderInput& in0, const ShaderInput& in1, MatrixOp op)
{
    // Evaluator is selected based on op and input data types.
    // For efficient lookup the types and op enums are packed together to form a 19-bit key:
    // [18..14 OP] [13..7 TYPE0] [6..0 TYPE1]

    DE_STATIC_ASSERT(TYPE_LAST  <= (1<<7));
    DE_STATIC_ASSERT(OP_LAST    <= (1<<5));

#define PACK_EVAL_CASE(OP, IN0DATATYPE, IN1DATATYPE)    (((OP) << 14) | ((IN0DATATYPE) << 7) | (IN1DATATYPE))

#define MAKE_EVAL_CASE(OP, IN0DATATYPE, IN1DATATYPE)    \
    case PACK_EVAL_CASE(OP, IN0DATATYPE, IN1DATATYPE):  \
        return Evaluator<OP, IN0DATATYPE, IN1DATATYPE>::evaluate

#define MAKE_SCALAR_OPS(IN0DATATYPE, IN1DATATYPE)       \
    MAKE_EVAL_CASE(OP_ADD, IN0DATATYPE, IN1DATATYPE);   \
    MAKE_EVAL_CASE(OP_SUB, IN0DATATYPE, IN1DATATYPE);   \
    MAKE_EVAL_CASE(OP_MUL, IN0DATATYPE, IN1DATATYPE);   \
    MAKE_EVAL_CASE(OP_DIV, IN0DATATYPE, IN1DATATYPE)

#define MAKE_CWISE_OPS(IN0DATATYPE, IN1DATATYPE)            \
    MAKE_EVAL_CASE(OP_ADD,      IN0DATATYPE, IN1DATATYPE);  \
    MAKE_EVAL_CASE(OP_SUB,      IN0DATATYPE, IN1DATATYPE);  \
    MAKE_EVAL_CASE(OP_DIV,      IN0DATATYPE, IN1DATATYPE);  \
    MAKE_EVAL_CASE(OP_COMP_MUL, IN0DATATYPE, IN1DATATYPE)

#define MAKE_MUL_OP(IN0DATATYPE, IN1DATATYPE)           \
    MAKE_EVAL_CASE(OP_MUL, IN0DATATYPE, IN1DATATYPE)

#define MAKE_VECVEC_OP(IN0DATATYPE, IN1DATATYPE)            \
    MAKE_EVAL_CASE(OP_OUTER_PRODUCT, IN0DATATYPE, IN1DATATYPE)

#define MAKE_UNARY_OP(IN0DATATYPE)                              \
    MAKE_EVAL_CASE(OP_TRANSPOSE,        IN0DATATYPE, TYPE_LAST);    \
    MAKE_EVAL_CASE(OP_UNARY_PLUS,       IN0DATATYPE, TYPE_LAST);    \
    MAKE_EVAL_CASE(OP_NEGATION,         IN0DATATYPE, TYPE_LAST);    \
    MAKE_EVAL_CASE(OP_PRE_INCREMENT,    IN0DATATYPE, TYPE_LAST);    \
    MAKE_EVAL_CASE(OP_PRE_DECREMENT,    IN0DATATYPE, TYPE_LAST);    \
    MAKE_EVAL_CASE(OP_POST_INCREMENT,   IN0DATATYPE, TYPE_LAST);    \
    MAKE_EVAL_CASE(OP_POST_DECREMENT,   IN0DATATYPE, TYPE_LAST)

#define MAKE_UNARY_SYMMETRIC_OP(IN0DATATYPE)                    \
    MAKE_UNARY_OP(IN0DATATYPE);                                 \
    MAKE_EVAL_CASE(OP_DETERMINANT,  IN0DATATYPE, TYPE_LAST);    \
    MAKE_EVAL_CASE(OP_INVERSE,      IN0DATATYPE, TYPE_LAST)

#define MAKE_ASSIGNMENT_OP(IN0DATATYPE)                             \
    MAKE_EVAL_CASE(OP_ADD_INTO,         IN0DATATYPE, IN0DATATYPE);  \
    MAKE_EVAL_CASE(OP_SUBTRACT_FROM,    IN0DATATYPE, IN0DATATYPE);  \
    MAKE_EVAL_CASE(OP_DIVIDE_INTO,      IN0DATATYPE, IN0DATATYPE)

#define MAKE_ASSIGNMENT_SYMMETRIC_OP(IN0DATATYPE)                   \
    MAKE_ASSIGNMENT_OP(IN0DATATYPE);                                \
    MAKE_EVAL_CASE(OP_MULTIPLY_INTO,    IN0DATATYPE, IN0DATATYPE)

    switch (PACK_EVAL_CASE(op, in0.dataType, in1.dataType))
    {
        // Matrix-scalar.
        MAKE_SCALAR_OPS(TYPE_FLOAT_MAT2,    TYPE_FLOAT);
        MAKE_SCALAR_OPS(TYPE_FLOAT_MAT2X3,  TYPE_FLOAT);
        MAKE_SCALAR_OPS(TYPE_FLOAT_MAT2X4,  TYPE_FLOAT);
        MAKE_SCALAR_OPS(TYPE_FLOAT_MAT3X2,  TYPE_FLOAT);
        MAKE_SCALAR_OPS(TYPE_FLOAT_MAT3,    TYPE_FLOAT);
        MAKE_SCALAR_OPS(TYPE_FLOAT_MAT3X4,  TYPE_FLOAT);
        MAKE_SCALAR_OPS(TYPE_FLOAT_MAT4X2,  TYPE_FLOAT);
        MAKE_SCALAR_OPS(TYPE_FLOAT_MAT4X3,  TYPE_FLOAT);
        MAKE_SCALAR_OPS(TYPE_FLOAT_MAT4,    TYPE_FLOAT);

        // Matrix-vector.
        MAKE_MUL_OP(TYPE_FLOAT_MAT2,    TYPE_FLOAT_VEC2);
        MAKE_MUL_OP(TYPE_FLOAT_MAT2X3,  TYPE_FLOAT_VEC2);
        MAKE_MUL_OP(TYPE_FLOAT_MAT2X4,  TYPE_FLOAT_VEC2);
        MAKE_MUL_OP(TYPE_FLOAT_MAT3X2,  TYPE_FLOAT_VEC3);
        MAKE_MUL_OP(TYPE_FLOAT_MAT3,    TYPE_FLOAT_VEC3);
        MAKE_MUL_OP(TYPE_FLOAT_MAT3X4,  TYPE_FLOAT_VEC3);
        MAKE_MUL_OP(TYPE_FLOAT_MAT4X2,  TYPE_FLOAT_VEC4);
        MAKE_MUL_OP(TYPE_FLOAT_MAT4X3,  TYPE_FLOAT_VEC4);
        MAKE_MUL_OP(TYPE_FLOAT_MAT4,    TYPE_FLOAT_VEC4);

        // Vector-matrix.
        MAKE_MUL_OP(TYPE_FLOAT_VEC2, TYPE_FLOAT_MAT2);
        MAKE_MUL_OP(TYPE_FLOAT_VEC3, TYPE_FLOAT_MAT2X3);
        MAKE_MUL_OP(TYPE_FLOAT_VEC4, TYPE_FLOAT_MAT2X4);
        MAKE_MUL_OP(TYPE_FLOAT_VEC2, TYPE_FLOAT_MAT3X2);
        MAKE_MUL_OP(TYPE_FLOAT_VEC3, TYPE_FLOAT_MAT3);
        MAKE_MUL_OP(TYPE_FLOAT_VEC4, TYPE_FLOAT_MAT3X4);
        MAKE_MUL_OP(TYPE_FLOAT_VEC2, TYPE_FLOAT_MAT4X2);
        MAKE_MUL_OP(TYPE_FLOAT_VEC3, TYPE_FLOAT_MAT4X3);
        MAKE_MUL_OP(TYPE_FLOAT_VEC4, TYPE_FLOAT_MAT4);

        // Matrix-matrix.
        MAKE_CWISE_OPS(TYPE_FLOAT_MAT2,     TYPE_FLOAT_MAT2);
        MAKE_MUL_OP(TYPE_FLOAT_MAT2,        TYPE_FLOAT_MAT2);
        MAKE_MUL_OP(TYPE_FLOAT_MAT2,        TYPE_FLOAT_MAT3X2);
        MAKE_MUL_OP(TYPE_FLOAT_MAT2,        TYPE_FLOAT_MAT4X2);

        MAKE_CWISE_OPS(TYPE_FLOAT_MAT2X3,   TYPE_FLOAT_MAT2X3);
        MAKE_MUL_OP(TYPE_FLOAT_MAT2X3,      TYPE_FLOAT_MAT2);
        MAKE_MUL_OP(TYPE_FLOAT_MAT2X3,      TYPE_FLOAT_MAT3X2);
        MAKE_MUL_OP(TYPE_FLOAT_MAT2X3,      TYPE_FLOAT_MAT4X2);

        MAKE_CWISE_OPS(TYPE_FLOAT_MAT2X4,   TYPE_FLOAT_MAT2X4);
        MAKE_MUL_OP(TYPE_FLOAT_MAT2X4,      TYPE_FLOAT_MAT2);
        MAKE_MUL_OP(TYPE_FLOAT_MAT2X4,      TYPE_FLOAT_MAT3X2);
        MAKE_MUL_OP(TYPE_FLOAT_MAT2X4,      TYPE_FLOAT_MAT4X2);

        MAKE_CWISE_OPS(TYPE_FLOAT_MAT3X2,   TYPE_FLOAT_MAT3X2);
        MAKE_MUL_OP(TYPE_FLOAT_MAT3X2,      TYPE_FLOAT_MAT2X3);
        MAKE_MUL_OP(TYPE_FLOAT_MAT3X2,      TYPE_FLOAT_MAT3);
        MAKE_MUL_OP(TYPE_FLOAT_MAT3X2,      TYPE_FLOAT_MAT4X3);

        MAKE_CWISE_OPS(TYPE_FLOAT_MAT3,     TYPE_FLOAT_MAT3);
        MAKE_MUL_OP(TYPE_FLOAT_MAT3,        TYPE_FLOAT_MAT2X3);
        MAKE_MUL_OP(TYPE_FLOAT_MAT3,        TYPE_FLOAT_MAT3);
        MAKE_MUL_OP(TYPE_FLOAT_MAT3,        TYPE_FLOAT_MAT4X3);

        MAKE_CWISE_OPS(TYPE_FLOAT_MAT3X4,   TYPE_FLOAT_MAT3X4);
        MAKE_MUL_OP(TYPE_FLOAT_MAT3X4,      TYPE_FLOAT_MAT2X3);
        MAKE_MUL_OP(TYPE_FLOAT_MAT3X4,      TYPE_FLOAT_MAT3);
        MAKE_MUL_OP(TYPE_FLOAT_MAT3X4,      TYPE_FLOAT_MAT4X3);

        MAKE_CWISE_OPS(TYPE_FLOAT_MAT4X2,   TYPE_FLOAT_MAT4X2);
        MAKE_MUL_OP(TYPE_FLOAT_MAT4X2,      TYPE_FLOAT_MAT2X4);
        MAKE_MUL_OP(TYPE_FLOAT_MAT4X2,      TYPE_FLOAT_MAT3X4);
        MAKE_MUL_OP(TYPE_FLOAT_MAT4X2,      TYPE_FLOAT_MAT4);

        MAKE_CWISE_OPS(TYPE_FLOAT_MAT4X3,   TYPE_FLOAT_MAT4X3);
        MAKE_MUL_OP(TYPE_FLOAT_MAT4X3,      TYPE_FLOAT_MAT2X4);
        MAKE_MUL_OP(TYPE_FLOAT_MAT4X3,      TYPE_FLOAT_MAT3X4);
        MAKE_MUL_OP(TYPE_FLOAT_MAT4X3,      TYPE_FLOAT_MAT4);

        MAKE_CWISE_OPS(TYPE_FLOAT_MAT4,     TYPE_FLOAT_MAT4);
        MAKE_MUL_OP(TYPE_FLOAT_MAT4,        TYPE_FLOAT_MAT2X4);
        MAKE_MUL_OP(TYPE_FLOAT_MAT4,        TYPE_FLOAT_MAT3X4);
        MAKE_MUL_OP(TYPE_FLOAT_MAT4,        TYPE_FLOAT_MAT4);

        // Vector-vector.
        MAKE_VECVEC_OP(TYPE_FLOAT_VEC2,     TYPE_FLOAT_VEC2);
        MAKE_VECVEC_OP(TYPE_FLOAT_VEC2,     TYPE_FLOAT_VEC3);
        MAKE_VECVEC_OP(TYPE_FLOAT_VEC2,     TYPE_FLOAT_VEC4);
        MAKE_VECVEC_OP(TYPE_FLOAT_VEC3,     TYPE_FLOAT_VEC2);
        MAKE_VECVEC_OP(TYPE_FLOAT_VEC3,     TYPE_FLOAT_VEC3);
        MAKE_VECVEC_OP(TYPE_FLOAT_VEC3,     TYPE_FLOAT_VEC4);
        MAKE_VECVEC_OP(TYPE_FLOAT_VEC4,     TYPE_FLOAT_VEC2);
        MAKE_VECVEC_OP(TYPE_FLOAT_VEC4,     TYPE_FLOAT_VEC3);
        MAKE_VECVEC_OP(TYPE_FLOAT_VEC4,     TYPE_FLOAT_VEC4);

        // Unary Matrix.
        MAKE_UNARY_SYMMETRIC_OP(TYPE_FLOAT_MAT2);
        MAKE_UNARY_OP(TYPE_FLOAT_MAT2X3);
        MAKE_UNARY_OP(TYPE_FLOAT_MAT2X4);
        MAKE_UNARY_OP(TYPE_FLOAT_MAT3X2);
        MAKE_UNARY_SYMMETRIC_OP(TYPE_FLOAT_MAT3);
        MAKE_UNARY_OP(TYPE_FLOAT_MAT3X4);
        MAKE_UNARY_OP(TYPE_FLOAT_MAT4X2);
        MAKE_UNARY_OP(TYPE_FLOAT_MAT4X3);
        MAKE_UNARY_SYMMETRIC_OP(TYPE_FLOAT_MAT4);

        // Assignments
        MAKE_ASSIGNMENT_SYMMETRIC_OP(TYPE_FLOAT_MAT2);
        MAKE_ASSIGNMENT_OP(TYPE_FLOAT_MAT2X3);
        MAKE_ASSIGNMENT_OP(TYPE_FLOAT_MAT2X4);
        MAKE_ASSIGNMENT_OP(TYPE_FLOAT_MAT3X2);
        MAKE_ASSIGNMENT_SYMMETRIC_OP(TYPE_FLOAT_MAT3);
        MAKE_ASSIGNMENT_OP(TYPE_FLOAT_MAT3X4);
        MAKE_ASSIGNMENT_OP(TYPE_FLOAT_MAT4X2);
        MAKE_ASSIGNMENT_OP(TYPE_FLOAT_MAT4X3);
        MAKE_ASSIGNMENT_SYMMETRIC_OP(TYPE_FLOAT_MAT4);

        default:
            DE_ASSERT(DE_FALSE);
            return DE_NULL;
    }

#undef PACK_EVAL_CASE
#undef MAKE_EVAL_CASE
#undef MUL_OP
#undef ALL_OPS
#undef MAKE_MAT_SCALAR_VEC_CASES
#undef MAKE_MAT_MAT_CASES
}

// Shader source format utilities.

template <int Size>
void writeVectorConstructor (std::ostream& str, const tcu::Vector<float, Size>& v)
{
    str << "vec" << Size << "(";
    for (int ndx = 0; ndx < Size; ndx++)
    {
        if (ndx != 0)
            str << ", ";
        str << de::floatToString(v[ndx], 1);
    }
    str << ")";
}

template <int Cols, int Rows>
void writeMatrixConstructor (std::ostream& str, const tcu::Matrix<float, Rows, Cols>& m)
{
    if (Rows == Cols)
        str << "mat" << Cols;
    else
        str << "mat" << Cols << "x" << Rows;

    str << "(";
    for (int colNdx = 0; colNdx < Cols; colNdx++)
    {
        for (int rowNdx = 0; rowNdx < Rows; rowNdx++)
        {
            if (rowNdx > 0 || colNdx > 0)
                str << ", ";
            str << de::floatToString(m(rowNdx, colNdx), 1);
        }
    }
    str << ")";
}

} // MatrixCaseUtils

using namespace MatrixCaseUtils;

class MatrixShaderEvaluator : public ShaderEvaluator
{
public:
                            MatrixShaderEvaluator   (MatrixShaderEvalFunc evalFunc, InputType inType0, InputType inType1);

    virtual void            evaluate                (ShaderEvalContext& evalCtx);

private:
    MatrixShaderEvalFunc    m_matEvalFunc;
    InputType               m_inType0;
    InputType               m_inType1;
};

MatrixShaderEvaluator::MatrixShaderEvaluator (MatrixShaderEvalFunc evalFunc, InputType inType0, InputType inType1)
    : m_matEvalFunc (evalFunc)
    , m_inType0     (inType0)
    , m_inType1     (inType1)
{
}

void MatrixShaderEvaluator::evaluate (ShaderEvalContext& evalCtx)
{
    m_matEvalFunc(evalCtx, m_inType0, m_inType1);
}

class ShaderMatrixCase : public ShaderRenderCase
{
public:
                            ShaderMatrixCase            (Context& context, const char* name, const char* desc, const ShaderInput& in0, const ShaderInput& in1, MatrixOp op, bool isVertexCase);
                            ~ShaderMatrixCase           (void);

    void                    init                        (void);

protected:
    std::string             genGLSLMatToVec3Reduction   (const glu::DataType& matType, const char* varName);
    void                    setupUniforms               (int programID, const tcu::Vec4& constCoords);

private:
    ShaderInput             m_in0;
    ShaderInput             m_in1;
    MatrixOp                m_op;
    MatrixShaderEvaluator   m_matEvaluator;
};

ShaderMatrixCase::ShaderMatrixCase (Context& context, const char* name, const char* desc, const ShaderInput& in0, const ShaderInput& in1, MatrixOp op, bool isVertexCase)
    : ShaderRenderCase  (context.getTestContext(), context.getRenderContext(), context.getContextInfo(), name, desc, isVertexCase, m_matEvaluator)
    , m_in0             (in0)
    , m_in1             (in1)
    , m_op              (op)
    , m_matEvaluator    (getEvalFunc(in0, in1, op), in0.inputType, in1.inputType)
{
}

ShaderMatrixCase::~ShaderMatrixCase (void)
{
}

void ShaderMatrixCase::init (void)
{
    std::ostringstream  vtx;
    std::ostringstream  frag;
    std::ostringstream& op              = m_isVertexCase ? vtx : frag;

    bool                isInDynMat0     = isDataTypeMatrix(m_in0.dataType) && m_in0.inputType == INPUTTYPE_DYNAMIC;
    bool                isInDynMat1     = isDataTypeMatrix(m_in1.dataType) && m_in1.inputType == INPUTTYPE_DYNAMIC;
    string              inValue0;
    string              inValue1;
    DataType            resultType      = TYPE_LAST;
    Precision           resultPrec      = m_in0.precision;
    vector<string>      passVars;
    int                 numInputs       = (isOperationBinary(m_op)) ? (2) : (1);

    std::string         operationValue0;
    std::string         operationValue1;

    DE_ASSERT(!isInDynMat0 || !isInDynMat1); // Only single dynamic matrix input is allowed.
    DE_UNREF(isInDynMat0 && isInDynMat1);

    // Compute result type.
    if (m_op == OP_MUL && isDataTypeMatrix(m_in0.dataType) && isDataTypeMatrix(m_in1.dataType))
    {
        resultType = getDataTypeMatrix(getDataTypeMatrixNumColumns(m_in1.dataType), getDataTypeMatrixNumRows(m_in0.dataType));
    }
    else if (m_op == OP_OUTER_PRODUCT)
    {
        resultType = getDataTypeMatrix(getDataTypeScalarSize(m_in1.dataType), getDataTypeScalarSize(m_in0.dataType));
    }
    else if (m_op == OP_TRANSPOSE)
    {
        resultType = getDataTypeMatrix(getDataTypeMatrixNumRows(m_in0.dataType), getDataTypeMatrixNumColumns(m_in0.dataType));
    }
    else if (m_op == OP_INVERSE)
    {
        resultType = m_in0.dataType;
    }
    else if (m_op == OP_DETERMINANT)
    {
        resultType = TYPE_FLOAT;
    }
    else if (getOperationType(m_op) == OPERATIONTYPE_UNARY_PREFIX_OPERATOR ||
             getOperationType(m_op) == OPERATIONTYPE_UNARY_POSTFIX_OPERATOR)
    {
        resultType = m_in0.dataType;
    }
    else if (isDataTypeMatrix(m_in0.dataType) && isDataTypeMatrix(m_in1.dataType))
    {
        DE_ASSERT(m_in0.dataType == m_in1.dataType);
        resultType = m_in0.dataType;
    }
    else if (isDataTypeMatrix(m_in0.dataType) || isDataTypeMatrix(m_in1.dataType))
    {
        int         matNdx      = isDataTypeMatrix(m_in0.dataType) ? 0 : 1;
        DataType    matrixType  = matNdx == 0 ? m_in0.dataType : m_in1.dataType;
        DataType    otherType   = matNdx == 0 ? m_in1.dataType : m_in0.dataType;

        if (otherType == TYPE_FLOAT)
            resultType = matrixType;
        else
        {
            DE_ASSERT(isDataTypeVector(otherType));
            resultType = getDataTypeFloatVec(matNdx == 0 ? getDataTypeMatrixNumRows(matrixType) : getDataTypeMatrixNumColumns(matrixType));
        }
    }
    else
    {
        DE_ASSERT(DE_FALSE);
    }

    vtx << "#version 300 es\n";
    frag << "#version 300 es\n";

    vtx << "in highp vec4 a_position;\n";
    frag << "layout(location = 0) out mediump vec4 dEQP_FragColor;\n";
    if (m_isVertexCase)
    {
        vtx << "out mediump vec4 v_color;\n";
        frag << "in mediump vec4 v_color;\n";
    }

    // Input declarations.
    for (int inNdx = 0; inNdx < numInputs; inNdx++)
    {
        const ShaderInput&  in          = inNdx > 0 ? m_in1 : m_in0;
        const char*         precName    = getPrecisionName(in.precision);
        const char*         typeName    = getDataTypeName(in.dataType);
        string&             inValue     = inNdx > 0 ? inValue1 : inValue0;

        if (in.inputType == INPUTTYPE_DYNAMIC)
        {
            vtx << "in " << precName << " " << typeName << " a_";

            if (isDataTypeMatrix(in.dataType))
            {
                // a_matN, v_matN
                vtx << typeName << ";\n";
                if (!m_isVertexCase)
                {
                    vtx << "out " << precName << " " << typeName << " v_" << typeName << ";\n";
                    frag << "in " << precName << " " << typeName << " v_" << typeName << ";\n";
                    passVars.push_back(typeName);
                }

                inValue = string(m_isVertexCase ? "a_" : "v_") + getDataTypeName(in.dataType);
            }
            else
            {
                // a_coords, v_coords
                vtx << "coords;\n";
                if (!m_isVertexCase)
                {
                    vtx << "out " << precName << " " << typeName << " v_coords;\n";
                    frag << "in " << precName << " " << typeName << " v_coords;\n";
                    passVars.push_back("coords");
                }

                inValue = m_isVertexCase ? "a_coords" : "v_coords";
            }
        }
        else if (in.inputType == INPUTTYPE_UNIFORM)
        {
            op << "uniform " << precName << " " << typeName << " u_in" << inNdx << ";\n";
            inValue = string("u_in") + de::toString(inNdx);
        }
        else if (in.inputType == INPUTTYPE_CONST)
        {
            op << "const " << precName << " " << typeName << " in" << inNdx << " = ";

            // Generate declaration.
            switch (in.dataType)
            {
                case TYPE_FLOAT:        op << de::floatToString(s_constInFloat[inNdx], 1);                  break;
                case TYPE_FLOAT_VEC2:   writeVectorConstructor<2>(op, s_constInVec2[inNdx]);                break;
                case TYPE_FLOAT_VEC3:   writeVectorConstructor<3>(op, s_constInVec3[inNdx]);                break;
                case TYPE_FLOAT_VEC4:   writeVectorConstructor<4>(op, s_constInVec4[inNdx]);                break;
                case TYPE_FLOAT_MAT2:   writeMatrixConstructor<2, 2>(op, Mat2(s_constInMat2x2[inNdx]));     break;
                case TYPE_FLOAT_MAT2X3: writeMatrixConstructor<2, 3>(op, Mat2x3(s_constInMat2x3[inNdx]));   break;
                case TYPE_FLOAT_MAT2X4: writeMatrixConstructor<2, 4>(op, Mat2x4(s_constInMat2x4[inNdx]));   break;
                case TYPE_FLOAT_MAT3X2: writeMatrixConstructor<3, 2>(op, Mat3x2(s_constInMat3x2[inNdx]));   break;
                case TYPE_FLOAT_MAT3:   writeMatrixConstructor<3, 3>(op, Mat3(s_constInMat3x3[inNdx]));     break;
                case TYPE_FLOAT_MAT3X4: writeMatrixConstructor<3, 4>(op, Mat3x4(s_constInMat3x4[inNdx]));   break;
                case TYPE_FLOAT_MAT4X2: writeMatrixConstructor<4, 2>(op, Mat4x2(s_constInMat4x2[inNdx]));   break;
                case TYPE_FLOAT_MAT4X3: writeMatrixConstructor<4, 3>(op, Mat4x3(s_constInMat4x3[inNdx]));   break;
                case TYPE_FLOAT_MAT4:   writeMatrixConstructor<4, 4>(op, Mat4(s_constInMat4x4[inNdx]));     break;

                default:
                    DE_ASSERT(DE_FALSE);
            }

            op << ";\n";

            inValue = string("in") + de::toString(inNdx);
        }
    }

    vtx << "\n"
        << "void main (void)\n"
        << "{\n"
        << "    gl_Position = a_position;\n";
    frag << "\n"
         << "void main (void)\n"
         << "{\n";

    if (m_isVertexCase)
        frag << "   dEQP_FragColor = v_color;\n";
    else
    {
        for (vector<string>::const_iterator copyIter = passVars.begin(); copyIter != passVars.end(); copyIter++)
            vtx << "    v_" << *copyIter << " = " << "a_" << *copyIter << ";\n";
    }

    // Operation.

    switch (getOperationNature(m_op))
    {
        case OPERATIONNATURE_PURE:
            DE_ASSERT(getOperationType(m_op) != OPERATIONTYPE_ASSIGNMENT);

            operationValue0 = inValue0;
            operationValue1 = inValue1;
            break;

        case OPERATIONNATURE_MUTATING:
            DE_ASSERT(getOperationType(m_op) != OPERATIONTYPE_ASSIGNMENT);

            op << " " << getPrecisionName(resultPrec) << " " << getDataTypeName(resultType) << " tmpValue = " << inValue0 << ";\n";

            operationValue0 = "tmpValue";
            operationValue1 = inValue1;
            break;

        case OPERATIONNATURE_ASSIGNMENT:
            DE_ASSERT(getOperationType(m_op) == OPERATIONTYPE_ASSIGNMENT);

            operationValue0 = inValue0;
            operationValue1 = inValue1;
            break;

        default:
            DE_ASSERT(DE_FALSE);
    }

    switch (getOperationType(m_op))
    {
        case OPERATIONTYPE_BINARY_OPERATOR:
            op << " " << getPrecisionName(resultPrec) << " " << getDataTypeName(resultType) << " res = " << operationValue0 << " " << getOperationName(m_op) << " " << operationValue1 << ";\n";
            break;

        case OPERATIONTYPE_UNARY_PREFIX_OPERATOR:
            op << " " << getPrecisionName(resultPrec) << " " << getDataTypeName(resultType) << " res = " << getOperationName(m_op) << operationValue0 << ";\n";
            break;

        case OPERATIONTYPE_UNARY_POSTFIX_OPERATOR:
            op << " " << getPrecisionName(resultPrec) << " " << getDataTypeName(resultType) << " res = " << operationValue0 << getOperationName(m_op) << ";\n";
            break;

        case OPERATIONTYPE_BINARY_FUNCTION:
            op << " " << getPrecisionName(resultPrec) << " " << getDataTypeName(resultType) << " res = " << getOperationName(m_op) << "(" << operationValue0 << ", " << operationValue1 << ");\n";
            break;

        case OPERATIONTYPE_UNARY_FUNCTION:
            op << " " << getPrecisionName(resultPrec) << " " << getDataTypeName(resultType) << " res = " << getOperationName(m_op) << "(" << operationValue0 << ");\n";
            break;

        case OPERATIONTYPE_ASSIGNMENT:
            op << " " << getPrecisionName(resultPrec) << " " << getDataTypeName(resultType) << " res = " << operationValue0 << ";\n";
            op << " res " << getOperationName(m_op) << " " << operationValue1 << ";\n";
            break;

        default:
            DE_ASSERT(DE_FALSE);
    }

    // Reduction to vec3 (rgb). Check the used value too if it was modified
    op << " " << (m_isVertexCase ? "v_color" : "dEQP_FragColor") << " = ";

    if (isOperationValueModifying(m_op))
        op << "vec4(" << genGLSLMatToVec3Reduction(resultType, "res") << ", 1.0) + vec4(" << genGLSLMatToVec3Reduction(resultType, "tmpValue") << ", 0.0);\n";
    else
        op << "vec4(" << genGLSLMatToVec3Reduction(resultType, "res") << ", 1.0);\n";

    vtx << "}\n";
    frag << "}\n";

    m_vertShaderSource  = vtx.str();
    m_fragShaderSource  = frag.str();

    // \todo [2012-02-14 pyry] Compute better values for matrix tests.
    m_userAttribTransforms.resize(4);
    for (int attribNdx = 0; attribNdx < 4; attribNdx++)
    {
        m_userAttribTransforms[attribNdx] = Mat4(0.0f);
        m_userAttribTransforms[attribNdx](                  0, 3) = 0.2f;                               // !< prevent matrix*vec from going into zero (assuming vec.w != 0)
        m_userAttribTransforms[attribNdx](                  1, 3) = 0.1f;                               // !<
        m_userAttribTransforms[attribNdx](                  2, 3) = 0.4f + 0.15f * float(attribNdx);    // !<
        m_userAttribTransforms[attribNdx](                  3, 3) = 0.7f;                               // !<
        m_userAttribTransforms[attribNdx]((0 + attribNdx) % 4, 0) = 1.0f;
        m_userAttribTransforms[attribNdx]((1 + attribNdx) % 4, 1) = 1.0f;
        m_userAttribTransforms[attribNdx]((2 + attribNdx) % 4, 2) = 1.0f;
        m_userAttribTransforms[attribNdx]((3 + attribNdx) % 4, 3) = 1.0f;
    }

    // prevent bad reference cases such as black result images by fine-tuning used matrices
    if (getOperationTestMatrixType(m_op) != TESTMATRIXTYPE_DEFAULT)
    {
        for (int attribNdx = 0; attribNdx < 4; attribNdx++)
        {
            for (int row = 0; row < 4; row++)
            for (int col = 0; col < 4; col++)
            {
                switch (getOperationTestMatrixType(m_op))
                {
                    case TESTMATRIXTYPE_NEGATED:
                        m_userAttribTransforms[attribNdx](row, col) = -m_userAttribTransforms[attribNdx](row, col);
                        break;
                    case TESTMATRIXTYPE_INCREMENTED:
                        m_userAttribTransforms[attribNdx](row, col) += 0.3f;
                        break;
                    case TESTMATRIXTYPE_DECREMENTED:
                        m_userAttribTransforms[attribNdx](row, col) -= 0.3f;
                        break;
                    case TESTMATRIXTYPE_NEGATED_INCREMENTED:
                        m_userAttribTransforms[attribNdx](row, col) = -m_userAttribTransforms[attribNdx](row, col) + 0.3f;
                        break;
                    case TESTMATRIXTYPE_INCREMENTED_LESS:
                        m_userAttribTransforms[attribNdx](row, col) -= 0.1f;
                        break;

                    default:
                        DE_ASSERT(DE_FALSE);
                        break;
                }
            }
        }
    }

    ShaderRenderCase::init();
}

std::string ShaderMatrixCase::genGLSLMatToVec3Reduction (const glu::DataType& matType, const char* varName)
{
    std::ostringstream op;

    switch (matType)
    {
        case TYPE_FLOAT:        op << varName << ", "           << varName << ", "          << varName << "";                                                                                                                                           break;
        case TYPE_FLOAT_VEC2:   op << varName << ".x, "         << varName << ".y, "        << varName << ".x";                                                                                                                                         break;
        case TYPE_FLOAT_VEC3:   op << varName << "";                                                                                                                                                                                                    break;
        case TYPE_FLOAT_VEC4:   op << varName << ".x, "         << varName << ".y, "        << varName << ".z+"         << varName << ".w";                                                                                                             break;
        case TYPE_FLOAT_MAT2:   op << varName << "[0][0], "     << varName << "[1][0], "    << varName << "[0][1]+"     << varName << "[1][1]";                                                                                                         break;
        case TYPE_FLOAT_MAT2X3: op << varName << "[0] + "       << varName << "[1]";                                                                                                                                                                    break;
        case TYPE_FLOAT_MAT2X4: op << varName << "[0].xyz + "   << varName << "[1].yzw";                                                                                                                                                                break;
        case TYPE_FLOAT_MAT3X2: op << varName << "[0][0]+"      << varName << "[0][1], "    << varName << "[1][0]+"     << varName << "[1][1], "    << varName << "[2][0]+" << varName << "[2][1]";                                                     break;
        case TYPE_FLOAT_MAT3:   op << varName << "[0] + "       << varName << "[1] + "      << varName << "[2]";                                                                                                                                        break;
        case TYPE_FLOAT_MAT3X4: op << varName << "[0].xyz + "   << varName << "[1].yzw + "  << varName << "[2].zwx";                                                                                                                                    break;
        case TYPE_FLOAT_MAT4X2: op << varName << "[0][0]+"      << varName << "[0][1]+"     << varName << "[3][0], "    << varName << "[1][0]+"     << varName << "[1][1]+" << varName << "[3][1], " << varName << "[2][0]+" << varName << "[2][1]";    break;
        case TYPE_FLOAT_MAT4X3: op << varName << "[0] + "       << varName << "[1] + "      << varName << "[2] + "      << varName << "[3]";                                                                                                            break;
        case TYPE_FLOAT_MAT4:   op << varName << "[0].xyz+"     << varName << "[1].yzw+"    << varName << "[2].zwx+"    << varName << "[3].wxy";                                                                                                        break;

        default:
            DE_ASSERT(DE_FALSE);
    }

    return op.str();
}

void ShaderMatrixCase::setupUniforms (int programID, const tcu::Vec4& constCoords)
{
    const glw::Functions& gl = m_renderCtx.getFunctions();

    DE_UNREF(constCoords);

    for (int inNdx = 0; inNdx < 2; inNdx++)
    {
        const ShaderInput& in = inNdx > 0 ? m_in1 : m_in0;

        if (in.inputType == INPUTTYPE_UNIFORM)
        {
            int loc = gl.getUniformLocation(programID, (string("u_in") + de::toString(inNdx)).c_str());

            if (loc < 0)
                continue;

            switch (in.dataType)
            {
                case TYPE_FLOAT:        gl.uniform1f(loc, s_constInFloat[inNdx]);                       break;
                case TYPE_FLOAT_VEC2:   gl.uniform2fv(loc, 1, s_constInVec2[inNdx].getPtr());           break;
                case TYPE_FLOAT_VEC3:   gl.uniform3fv(loc, 1, s_constInVec3[inNdx].getPtr());           break;
                case TYPE_FLOAT_VEC4:   gl.uniform4fv(loc, 1, s_constInVec4[inNdx].getPtr());           break;
                // \note GLES3 supports transpose in matrix upload.
                case TYPE_FLOAT_MAT2:   gl.uniformMatrix2fv (loc, 1, GL_TRUE, s_constInMat2x2[inNdx]);  break;
                case TYPE_FLOAT_MAT2X3: gl.uniformMatrix2x3fv(loc, 1, GL_TRUE, s_constInMat2x3[inNdx]); break;
                case TYPE_FLOAT_MAT2X4: gl.uniformMatrix2x4fv(loc, 1, GL_TRUE, s_constInMat2x4[inNdx]); break;
                case TYPE_FLOAT_MAT3X2: gl.uniformMatrix3x2fv(loc, 1, GL_TRUE, s_constInMat3x2[inNdx]); break;
                case TYPE_FLOAT_MAT3:   gl.uniformMatrix3fv (loc, 1, GL_TRUE, s_constInMat3x3[inNdx]);  break;
                case TYPE_FLOAT_MAT3X4: gl.uniformMatrix3x4fv(loc, 1, GL_TRUE, s_constInMat3x4[inNdx]); break;
                case TYPE_FLOAT_MAT4X2: gl.uniformMatrix4x2fv(loc, 1, GL_TRUE, s_constInMat4x2[inNdx]); break;
                case TYPE_FLOAT_MAT4X3: gl.uniformMatrix4x3fv(loc, 1, GL_TRUE, s_constInMat4x3[inNdx]); break;
                case TYPE_FLOAT_MAT4:   gl.uniformMatrix4fv (loc, 1, GL_TRUE, s_constInMat4x4[inNdx]);  break;
                default:
                    DE_ASSERT(false);
            }
        }
    }
}

ShaderMatrixTests::ShaderMatrixTests (Context& context)
    : TestCaseGroup(context, "matrix", "Matrix Tests")
{
}

ShaderMatrixTests::~ShaderMatrixTests (void)
{
}

void ShaderMatrixTests::init (void)
{
    static const struct
    {
        const char*     name;
        const char*     desc;
        MatrixOp        op;
        bool            extendedInputTypeCases; // !< test with const and uniform types too
        bool            createInputTypeGroup;   // !< create group for input types
    } ops[] =
    {
        { "add",            "Matrix addition tests",                        OP_ADD,             true,   true    },
        { "sub",            "Matrix subtraction tests",                     OP_SUB,             true,   true    },
        { "mul",            "Matrix multiplication tests",                  OP_MUL,             true,   true    },
        { "div",            "Matrix division tests",                        OP_DIV,             true,   true    },
        { "matrixcompmult", "Matrix component-wise multiplication tests",   OP_COMP_MUL,        false,  true    },
        { "outerproduct",   "Matrix outerProduct() tests",                  OP_OUTER_PRODUCT,   false,  true    },
        { "transpose",      "Matrix transpose() tests",                     OP_TRANSPOSE,       false,  true    },
        { "determinant",    "Matrix determinant() tests",                   OP_DETERMINANT,     false,  true    },
        { "inverse",        "Matrix inverse() tests",                       OP_INVERSE,         false,  true    },
        { "unary_addition", "Matrix unary addition tests",                  OP_UNARY_PLUS,      false,  false   },
        { "negation",       "Matrix negation tests",                        OP_NEGATION,        false,  false   },
        { "pre_increment",  "Matrix prefix increment tests",                OP_PRE_INCREMENT,   false,  false   },
        { "pre_decrement",  "Matrix prefix decrement tests",                OP_PRE_DECREMENT,   false,  false   },
        { "post_increment", "Matrix postfix increment tests",               OP_POST_INCREMENT,  false,  false   },
        { "post_decrement", "Matrix postfix decrement tests",               OP_POST_DECREMENT,  false,  false   },
        { "add_assign",     "Matrix add into tests",                        OP_ADD_INTO,        false,  false   },
        { "sub_assign",     "Matrix subtract from tests",                   OP_SUBTRACT_FROM,   false,  false   },
        { "mul_assign",     "Matrix multiply into tests",                   OP_MULTIPLY_INTO,   false,  false   },
        { "div_assign",     "Matrix divide into tests",                     OP_DIVIDE_INTO,     false,  false   },
    };

    struct InputTypeSpec
    {
        const char*     name;
        const char*     desc;
        InputType       type;
    };
    static const InputTypeSpec extendedInputTypes[] =
    {
        { "const",      "Constant matrix input",    INPUTTYPE_CONST     },
        { "uniform",    "Uniform matrix input",     INPUTTYPE_UNIFORM   },
        { "dynamic",    "Dynamic matrix input",     INPUTTYPE_DYNAMIC   }
    };
    static const InputTypeSpec reducedInputTypes[] =
    {
        { "dynamic",    "Dynamic matrix input",     INPUTTYPE_DYNAMIC   }
    };

    static const DataType matrixTypes[] =
    {
        TYPE_FLOAT_MAT2,
        TYPE_FLOAT_MAT2X3,
        TYPE_FLOAT_MAT2X4,
        TYPE_FLOAT_MAT3X2,
        TYPE_FLOAT_MAT3,
        TYPE_FLOAT_MAT3X4,
        TYPE_FLOAT_MAT4X2,
        TYPE_FLOAT_MAT4X3,
        TYPE_FLOAT_MAT4
    };

    static const Precision precisions[] =
    {
        PRECISION_LOWP,
        PRECISION_MEDIUMP,
        PRECISION_HIGHP
    };

    for (int opNdx = 0; opNdx < DE_LENGTH_OF_ARRAY(ops); opNdx++)
    {
        const InputTypeSpec*    inTypeList      = (ops[opNdx].extendedInputTypeCases) ? (extendedInputTypes) : (reducedInputTypes);
        const int               inTypeListSize  = (ops[opNdx].extendedInputTypeCases) ? (DE_LENGTH_OF_ARRAY(extendedInputTypes)) : (DE_LENGTH_OF_ARRAY(reducedInputTypes));
        const MatrixOp          op              = ops[opNdx].op;
        tcu::TestCaseGroup*     opGroup         = new tcu::TestCaseGroup(m_testCtx, ops[opNdx].name, ops[opNdx].desc);

        addChild(opGroup);

        for (int inTypeNdx = 0; inTypeNdx < inTypeListSize; inTypeNdx++)
        {
            const InputType     inputType   = inTypeList[inTypeNdx].type;
            tcu::TestCaseGroup* inGroup;

            if (ops[opNdx].createInputTypeGroup)
            {
                inGroup = new tcu::TestCaseGroup(m_testCtx, inTypeList[inTypeNdx].name, inTypeList[inTypeNdx].desc);
                opGroup->addChild(inGroup);
            }
            else
                inGroup = opGroup;

            for (int matTypeNdx = 0; matTypeNdx < DE_LENGTH_OF_ARRAY(matrixTypes); matTypeNdx++)
            {
                DataType    matType     = matrixTypes[matTypeNdx];
                int         numCols     = getDataTypeMatrixNumColumns(matType);
                int         numRows     = getDataTypeMatrixNumRows(matType);
                const char* matTypeName = getDataTypeName(matType);

                for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisions); precNdx++)
                {
                    Precision   precision   = precisions[precNdx];
                    const char* precName    = getPrecisionName(precision);
                    string      baseName    = string(precName) + "_" + matTypeName + "_";
                    ShaderInput matIn       (inputType, matType, precision);

                    if (isOperationMatrixScalar(op))
                    {
                        // Matrix-scalar \note For div cases we use uniform input.
                        ShaderInput scalarIn(op == OP_DIV ? INPUTTYPE_UNIFORM : INPUTTYPE_DYNAMIC, TYPE_FLOAT, precision);
                        inGroup->addChild(new ShaderMatrixCase(m_context, (baseName + "float_vertex").c_str(),      "Matrix-scalar case", matIn, scalarIn, op, true));
                        inGroup->addChild(new ShaderMatrixCase(m_context, (baseName + "float_fragment").c_str(),    "Matrix-scalar case", matIn, scalarIn, op, false));
                    }

                    if (isOperationMatrixVector(op))
                    {
                        // Matrix-vector.
                        DataType    colVecType  = getDataTypeFloatVec(numCols);
                        ShaderInput colVecIn    (op == OP_DIV ? INPUTTYPE_UNIFORM : INPUTTYPE_DYNAMIC, colVecType, precision);

                        inGroup->addChild(new ShaderMatrixCase(m_context, (baseName + getDataTypeName(colVecType) + "_vertex").c_str(),     "Matrix-vector case", matIn, colVecIn, op, true));
                        inGroup->addChild(new ShaderMatrixCase(m_context, (baseName + getDataTypeName(colVecType) + "_fragment").c_str(),   "Matrix-vector case", matIn, colVecIn, op, false));

                        // Vector-matrix.
                        DataType    rowVecType  = getDataTypeFloatVec(numRows);
                        ShaderInput rowVecIn    (op == OP_DIV ? INPUTTYPE_UNIFORM : INPUTTYPE_DYNAMIC, rowVecType, precision);
                        string      vecMatName  = string(precName) + "_" + getDataTypeName(rowVecType) + "_" + matTypeName;

                        inGroup->addChild(new ShaderMatrixCase(m_context, (vecMatName + "_vertex").c_str(),     "Vector-matrix case", rowVecIn, matIn, op, true));
                        inGroup->addChild(new ShaderMatrixCase(m_context, (vecMatName + "_fragment").c_str(),   "Vector-matrix case", rowVecIn, matIn, op, false));
                    }

                    if (isOperationArithmeticMatrixMatrix(op))
                    {
                        // Arithmetic matrix-matrix multiplication.
                        for (int otherCols = 2; otherCols <= 4; otherCols++)
                        {
                            ShaderInput otherMatIn(inputType == INPUTTYPE_DYNAMIC ? INPUTTYPE_UNIFORM : inputType, getDataTypeMatrix(otherCols, numCols /* rows */), precision);
                            inGroup->addChild(new ShaderMatrixCase(m_context, (baseName + getDataTypeName(otherMatIn.dataType) + "_vertex").c_str(),    "Matrix-matrix case", matIn, otherMatIn, op, true));
                            inGroup->addChild(new ShaderMatrixCase(m_context, (baseName + getDataTypeName(otherMatIn.dataType) + "_fragment").c_str(),  "Matrix-matrix case", matIn, otherMatIn, op, false));
                        }
                    }
                    else if (isOperationComponentwiseMatrixMatrix(op))
                    {
                        // Component-wise.
                        ShaderInput otherMatIn(inputType == INPUTTYPE_DYNAMIC ? INPUTTYPE_UNIFORM : inputType, matType, precision);
                        inGroup->addChild(new ShaderMatrixCase(m_context, (baseName + matTypeName + "_vertex").c_str(),     "Matrix-matrix case", matIn, otherMatIn, op, true));
                        inGroup->addChild(new ShaderMatrixCase(m_context, (baseName + matTypeName + "_fragment").c_str(),   "Matrix-matrix case", matIn, otherMatIn, op, false));
                    }

                    if (isOperationVectorVector(op))
                    {
                        ShaderInput vec1In(inputType,                                                               getDataTypeFloatVec(numRows), precision);
                        ShaderInput vec2In((inputType == INPUTTYPE_DYNAMIC) ? (INPUTTYPE_UNIFORM) : (inputType),    getDataTypeFloatVec(numCols), precision);

                        inGroup->addChild(new ShaderMatrixCase(m_context, (baseName + "float_vertex").c_str(),      "Vector-vector case", vec1In, vec2In, op, true));
                        inGroup->addChild(new ShaderMatrixCase(m_context, (baseName + "float_fragment").c_str(),    "Vector-vector case", vec1In, vec2In, op, false));
                    }

                    if ((isOperationUnaryAnyMatrix(op)) ||
                        (isOperationUnarySymmetricMatrix(op) && numCols == numRows))
                    {
                        ShaderInput voidInput(INPUTTYPE_LAST, TYPE_LAST, PRECISION_LAST);
                        inGroup->addChild(new ShaderMatrixCase(m_context, (baseName + "float_vertex").c_str(),      "Matrix case", matIn, voidInput, op, true));
                        inGroup->addChild(new ShaderMatrixCase(m_context, (baseName + "float_fragment").c_str(),    "Matrix case", matIn, voidInput, op, false));
                    }

                    if ((isOperationAssignmentAnyMatrix(op)) ||
                        (isOperationAssignmentSymmetricMatrix(op) && numCols == numRows))
                    {
                        ShaderInput otherMatIn(inputType == INPUTTYPE_DYNAMIC ? INPUTTYPE_UNIFORM : inputType, matType, precision);
                        inGroup->addChild(new ShaderMatrixCase(m_context, (baseName + "float_vertex").c_str(),      "Matrix assignment case", matIn, otherMatIn, op, true));
                        inGroup->addChild(new ShaderMatrixCase(m_context, (baseName + "float_fragment").c_str(),    "Matrix assignment case", matIn, otherMatIn, op, false));
                    }
                }
            }
        }
    }
}

} // Functional
} // gles3
} // deqp

#if defined(_MSC_VER) && _MSC_FULL_VER == 191125507
// Work around crbug.com/759402 which is a code-gen bug in VC++ 2017, version
// 15.3.2.
#pragma optimize("", off)
#endif
