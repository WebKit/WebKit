//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <cctype>

#include "compiler/translator/InfoSink.h"
#include "compiler/translator/Symbol.h"
#include "compiler/translator/TranslatorMetalDirect/AstHelpers.h"
#include "compiler/translator/TranslatorMetalDirect/Name.h"
#include "compiler/translator/TranslatorMetalDirect/ProgramPrelude.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

using namespace sh;

////////////////////////////////////////////////////////////////////////////////

namespace
{

class ProgramPrelude : public TIntermTraverser
{
    using LineTag       = unsigned;
    using FuncEmitter   = void (*)(ProgramPrelude &, const TFunction &);
    using FuncToEmitter = std::map<Name, FuncEmitter>;

  public:
    ProgramPrelude(TInfoSinkBase &out, const ProgramPreludeConfig &ppc)
        : TIntermTraverser(true, false, false), mOut(out)
    {
        include_metal_stdlib();
        ALWAYS_INLINE();
        int_clamp();
        if (ppc.hasStructEq)
        {
            equalVector();
            equalMatrix();
        }

        switch (ppc.shaderType)
        {
            case MetalShaderType::None:
                ASSERT(0 && "ppc.shaderType should not be ShaderTypeNone");
                break;
            case MetalShaderType::Vertex:
                transform_feedback_guard();
                break;
            case MetalShaderType::Fragment:
                writeSampleMask();
                break;
            case MetalShaderType::Compute:
                ASSERT(0 && "compute shaders not currently supported");
                break;
            default:
                break;
        }

#if 1
        mOut << "#define ANGLE_tensor metal::array\n";
        mOut << "#pragma clang diagnostic ignored \"-Wunused-value\"\n";
#else
        tensor();
#endif
    }

  private:
    bool emitGuard(LineTag lineTag)
    {
        if (mEmitted.find(lineTag) != mEmitted.end())
        {
            return false;
        }
        mEmitted.insert(lineTag);
        return true;
    }

    static FuncToEmitter BuildFuncToEmitter();

    void visitOperator(TOperator op, const TFunction *func, const TType *argType0);

    void visitOperator(TOperator op,
                       const TFunction *func,
                       const TType *argType0,
                       const TType *argType1);

    void visitOperator(TOperator op,
                       const TFunction *func,
                       const TType *argType0,
                       const TType *argType1,
                       const TType *argType2);

    void visitVariable(const Name &name, const TType &type);
    void visitVariable(const TVariable &var);
    void visitStructure(const TStructure &s);

    bool visitBinary(Visit, TIntermBinary *node) override;
    bool visitUnary(Visit, TIntermUnary *node) override;
    bool visitAggregate(Visit, TIntermAggregate *node) override;
    bool visitDeclaration(Visit, TIntermDeclaration *node) override;
    void visitSymbol(TIntermSymbol *node) override;

  private:
    void ALWAYS_INLINE();

    void include_metal_stdlib();
    void include_metal_atomic();
    void include_metal_common();
    void include_metal_geometric();
    void include_metal_graphics();
    void include_metal_math();
    void include_metal_matrix();
    void include_metal_pack();
    void include_metal_relational();

    void transform_feedback_guard();

    void enable_if();
    void scalar_of();
    void is_scalar();
    void is_vector();
    void is_matrix();
    void addressof();
    void distance();
    void length();
    void dot();
    void normalize();
    void faceforward();
    void reflect();
    void refract();
    void degrees();
    void radians();
    void mod();
    void mixBool();
    void postIncrementMatrix();
    void preIncrementMatrix();
    void postDecrementMatrix();
    void preDecrementMatrix();
    void negateMatrix();
    void matmulAssign();
    void atan();
    void int_clamp();
    void addMatrixScalarAssign();
    void subMatrixScalarAssign();
    void addMatrixScalar();
    void subMatrixScalar();
    void divMatrixScalar();
    void divMatrixScalarFast();
    void divMatrixScalarAssign();
    void divMatrixScalarAssignFast();
    void tensor();
    void componentWiseDivide();
    void componentWiseDivideAssign();
    void componentWiseMultiply();
    void outerProduct();
    void inverse2();
    void inverse3();
    void inverse4();
    void equalScalar();
    void equalVector();
    void equalMatrix();
    void notEqualVector();
    void notEqualStruct();
    void notEqualStructArray();
    void notEqualMatrix();
    void equalArray();
    void equalStructArray();
    void notEqualArray();
    void sign();
    void pack_half_2x16();
    void unpack_half_2x16();
    void vectorElemRef();
    void swizzleRef();
    void out();
    void inout();
    void flattenArray();
    void castVector();
    void castMatrix();
    void functionConstants();
    void gradient();
    void writeSampleMask();
    void textureEnv();
    void texelFetch();
    void texelFetchOffset();
    void texture();
    void texture_generic_float2();
    void texture_generic_float2_float();
    void texture_generic_float3();
    void texture_generic_float3_float();
    void texture_depth2d_float3();
    void texture_depth2d_float3_float();
    void texture_depth2darray_float4();
    void texture_depth2darray_float4_float();
    void texture_depthcube_float4();
    void texture_depthcube_float4_float();
    void texture_texture2darray_float3();
    void texture_texture2darray_float3_float();
    void texture_texture2darray_float4();
    void texture_texture2darray_float4_float();
    void texture1DLod();
    void texture1DProj();
    void texture1DProjLod();
    void texture2D();
    void texture2DGradEXT();
    void texture2DLod();
    void texture2DLodEXT();
    void texture2DProj();
    void texture2DProjGradEXT();
    void texture2DProjLod();
    void texture2DProjLodEXT();
    void texture2DRect();
    void texture2DRectProj();
    void texture3DLod();
    void texture3DProj();
    void texture3DProjLod();
    void textureCube();
    void textureCubeGradEXT();
    void textureCubeLod();
    void textureCubeLodEXT();
    void textureCubeProj();
    void textureCubeProjLod();
    void textureGrad();
    void textureGrad_generic_floatN_floatN_floatN();
    void textureGrad_generic_float3_float2_float2();
    void textureGrad_generic_float4_float2_float2();
    void textureGrad_depth2d_float3_float2_float2();
    void textureGrad_depth2darray_float4_float2_float2();
    void textureGrad_depthcube_float4_float3_float3();
    void textureGrad_texturecube_float3_float3_float3();
    void textureGradOffset();
    void textureGradOffset_generic_floatN_floatN_floatN_intN();
    void textureGradOffset_generic_float3_float2_float2_int2();
    void textureGradOffset_generic_float4_float2_float2_int2();
    void textureGradOffset_depth2d_float3_float2_float2_int2();
    void textureGradOffset_depth2darray_float4_float2_float2_int2();
    void textureGradOffset_depthcube_float4_float3_float3_int3();
    void textureGradOffset_texturecube_float3_float3_float3_int3();
    void textureLod();
    void textureLod_generic_float2();
    void textureLod_generic_float3();
    void textureLod_depth2d_float3();
    void textureLod_texture2darray_float3();
    void textureLod_texture2darray_float4();
    void textureLodOffset();
    void textureOffset();
    void textureProj();
    void textureProjGrad();
    void textureProjGrad_generic_float3_float2_float2();
    void textureProjGrad_generic_float4_float2_float2();
    void textureProjGrad_depth2d_float4_float2_float2();
    void textureProjGrad_texture3d_float4_float3_float3();
    void textureProjGradOffset();
    void textureProjGradOffset_generic_float3_float2_float2_int2();
    void textureProjGradOffset_generic_float4_float2_float2_int2();
    void textureProjGradOffset_depth2d_float4_float2_float2_int2();
    void textureProjGradOffset_texture3d_float4_float3_float3_int3();
    void textureProjLod();
    void textureProjLod_generic_float3();
    void textureProjLod_generic_float4();
    void textureProjLod_depth2d_float4();
    void textureProjLod_texture3d_float4();
    void textureProjLodOffset();
    void textureProjOffset();
    void textureSize();

  private:
    TInfoSinkBase &mOut;
    std::unordered_set<LineTag> mEmitted;
    std::unordered_set<const TSymbol *> mHandled;
    const FuncToEmitter mFuncToEmitter = BuildFuncToEmitter();
};

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////

#define PROGRAM_PRELUDE_INCLUDE(header)             \
    void ProgramPrelude::include_##header()         \
    {                                               \
        if (emitGuard(__LINE__))                    \
        {                                           \
            mOut << ("#include <" #header ">\n\n"); \
        }                                           \
    }

#define PROGRAM_PRELUDE_DECLARE(name, code, ...)                \
    void ProgramPrelude::name()                                 \
    {                                                           \
        ASSERT(code[0] == '\n');                                \
        if (emitGuard(__LINE__))                                \
        {                                                       \
            __VA_ARGS__; /* dependencies */                     \
            mOut << (static_cast<const char *>(code "\n") + 1); \
        }                                                       \
    }

////////////////////////////////////////////////////////////////////////////////

PROGRAM_PRELUDE_INCLUDE(metal_stdlib)
PROGRAM_PRELUDE_INCLUDE(metal_atomic)
PROGRAM_PRELUDE_INCLUDE(metal_common)
PROGRAM_PRELUDE_INCLUDE(metal_geometric)
PROGRAM_PRELUDE_INCLUDE(metal_graphics)
PROGRAM_PRELUDE_INCLUDE(metal_math)
PROGRAM_PRELUDE_INCLUDE(metal_matrix)
PROGRAM_PRELUDE_INCLUDE(metal_pack)
PROGRAM_PRELUDE_INCLUDE(metal_relational)

PROGRAM_PRELUDE_DECLARE(transform_feedback_guard, R"(
#if TRANSFORM_FEEDBACK_ENABLED
    #define __VERTEX_OUT(args) void
#else
    #define __VERTEX_OUT(args) args
#endif
)")

PROGRAM_PRELUDE_DECLARE(ALWAYS_INLINE, R"(
#define ANGLE_ALWAYS_INLINE __attribute__((always_inline))
)")

PROGRAM_PRELUDE_DECLARE(enable_if, R"(
template <bool B, typename T = void>
struct ANGLE_enable_if {};
template <typename T>
struct ANGLE_enable_if<true, T>
{
    using type = T;
};
template <bool B>
using ANGLE_enable_if_t = typename ANGLE_enable_if<B>::type;
)")

PROGRAM_PRELUDE_DECLARE(scalar_of, R"(
template <typename T>
struct ANGLE_scalar_of
{
    using type = T;
};
template <typename T>
using ANGLE_scalar_of_t = typename ANGLE_scalar_of<T>::type;
)")

PROGRAM_PRELUDE_DECLARE(is_scalar, R"(
template <typename T>
struct ANGLE_is_scalar {};
#define ANGLE_DEFINE_SCALAR(scalar) \
    template <> struct ANGLE_is_scalar<scalar> { enum { value = true }; }
ANGLE_DEFINE_SCALAR(bool);
ANGLE_DEFINE_SCALAR(char);
ANGLE_DEFINE_SCALAR(short);
ANGLE_DEFINE_SCALAR(int);
ANGLE_DEFINE_SCALAR(uchar);
ANGLE_DEFINE_SCALAR(ushort);
ANGLE_DEFINE_SCALAR(uint);
ANGLE_DEFINE_SCALAR(half);
ANGLE_DEFINE_SCALAR(float);
)")

PROGRAM_PRELUDE_DECLARE(is_vector,
                        R"(
template <typename T>
struct ANGLE_is_vector
{
    enum { value = false };
};
#define ANGLE_DEFINE_VECTOR(scalar) \
    template <> struct ANGLE_is_vector<metal::scalar ## 2> { enum { value = true }; }; \
    template <> struct ANGLE_is_vector<metal::scalar ## 3> { enum { value = true }; }; \
    template <> struct ANGLE_is_vector<metal::scalar ## 4> { enum { value = true }; }; \
    template <> struct ANGLE_scalar_of<metal::scalar ## 2> { using type = scalar; }; \
    template <> struct ANGLE_scalar_of<metal::scalar ## 3> { using type = scalar; }; \
    template <> struct ANGLE_scalar_of<metal::scalar ## 4> { using type = scalar; }
ANGLE_DEFINE_VECTOR(bool);
ANGLE_DEFINE_VECTOR(char);
ANGLE_DEFINE_VECTOR(short);
ANGLE_DEFINE_VECTOR(int);
ANGLE_DEFINE_VECTOR(uchar);
ANGLE_DEFINE_VECTOR(ushort);
ANGLE_DEFINE_VECTOR(uint);
ANGLE_DEFINE_VECTOR(half);
ANGLE_DEFINE_VECTOR(float);
)",
                        scalar_of())

PROGRAM_PRELUDE_DECLARE(is_matrix,
                        R"(
template <typename T>
struct ANGLE_is_matrix
{
    enum { value = false };
};
#define ANGLE_DEFINE_MATRIX(scalar) \
    template <> struct ANGLE_is_matrix<metal::scalar ## 2x2> { enum { value = true }; }; \
    template <> struct ANGLE_is_matrix<metal::scalar ## 2x3> { enum { value = true }; }; \
    template <> struct ANGLE_is_matrix<metal::scalar ## 2x4> { enum { value = true }; }; \
    template <> struct ANGLE_is_matrix<metal::scalar ## 3x2> { enum { value = true }; }; \
    template <> struct ANGLE_is_matrix<metal::scalar ## 3x3> { enum { value = true }; }; \
    template <> struct ANGLE_is_matrix<metal::scalar ## 3x4> { enum { value = true }; }; \
    template <> struct ANGLE_is_matrix<metal::scalar ## 4x2> { enum { value = true }; }; \
    template <> struct ANGLE_is_matrix<metal::scalar ## 4x3> { enum { value = true }; }; \
    template <> struct ANGLE_is_matrix<metal::scalar ## 4x4> { enum { value = true }; }; \
    template <> struct ANGLE_scalar_of<metal::scalar ## 2x2> { using type = scalar; }; \
    template <> struct ANGLE_scalar_of<metal::scalar ## 2x3> { using type = scalar; }; \
    template <> struct ANGLE_scalar_of<metal::scalar ## 2x4> { using type = scalar; }; \
    template <> struct ANGLE_scalar_of<metal::scalar ## 3x2> { using type = scalar; }; \
    template <> struct ANGLE_scalar_of<metal::scalar ## 3x3> { using type = scalar; }; \
    template <> struct ANGLE_scalar_of<metal::scalar ## 3x4> { using type = scalar; }; \
    template <> struct ANGLE_scalar_of<metal::scalar ## 4x2> { using type = scalar; }; \
    template <> struct ANGLE_scalar_of<metal::scalar ## 4x3> { using type = scalar; }; \
    template <> struct ANGLE_scalar_of<metal::scalar ## 4x4> { using type = scalar; }
ANGLE_DEFINE_MATRIX(half);
ANGLE_DEFINE_MATRIX(float);
)",
                        scalar_of())

PROGRAM_PRELUDE_DECLARE(addressof,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE thread T * ANGLE_addressof(thread T &ref)
{
    return &ref;
}
)")

PROGRAM_PRELUDE_DECLARE(distance,
                        R"(
template <typename T, typename Enable = void>
struct ANGLE_distance_impl
{
    static ANGLE_ALWAYS_INLINE ANGLE_scalar_of_t<T> exec(T x, T y)
    {
        return metal::distance(x, y);
    }
};
template <typename T>
struct ANGLE_distance_impl<T, ANGLE_enable_if_t<ANGLE_is_scalar<T>::value>>
{
    static ANGLE_ALWAYS_INLINE T exec(T x, T y)
    {
        return metal::abs(x - y);
    }
};
template <typename T>
ANGLE_ALWAYS_INLINE ANGLE_scalar_of_t<T> ANGLE_distance(T x, T y)
{
    return ANGLE_distance_impl<T>::exec(x, y);
};
)",
                        include_metal_geometric(),
                        include_metal_math(),
                        enable_if(),
                        is_scalar(),
                        is_vector(),
                        is_matrix())

PROGRAM_PRELUDE_DECLARE(length,
                        R"(
template <typename T, typename Enable = void>
struct ANGLE_length_impl
{
    static ANGLE_ALWAYS_INLINE ANGLE_scalar_of_t<T> exec(T x)
    {
        return metal::length(x);
    }
};
template <typename T>
struct ANGLE_length_impl<T, ANGLE_enable_if_t<ANGLE_is_scalar<T>::value>>
{
    static ANGLE_ALWAYS_INLINE T exec(T x)
    {
        return metal::abs(x);
    }
};
template <typename T>
ANGLE_ALWAYS_INLINE ANGLE_scalar_of_t<T> ANGLE_length(T x)
{
    return ANGLE_length_impl<T>::exec(x);
};
)",
                        include_metal_geometric(),
                        include_metal_math(),
                        enable_if(),
                        is_scalar(),
                        is_vector(),
                        is_matrix())

PROGRAM_PRELUDE_DECLARE(dot,
                        R"(
template <typename T, typename Enable = void>
struct ANGLE_dot_impl
{
    static ANGLE_ALWAYS_INLINE ANGLE_scalar_of_t<T> exec(T x, T y)
    {
        return metal::dot(x, y);
    }
};
template <typename T>
struct ANGLE_dot_impl<T, ANGLE_enable_if_t<ANGLE_is_scalar<T>::value>>
{
    static ANGLE_ALWAYS_INLINE T exec(T x, T y)
    {
        return x * y;
    }
};
template <typename T>
ANGLE_ALWAYS_INLINE ANGLE_scalar_of_t<T> ANGLE_dot(T x, T y)
{
    return ANGLE_dot_impl<T>::exec(x, y);
};
)",
                        include_metal_geometric(),
                        enable_if(),
                        is_scalar(),
                        is_vector(),
                        is_matrix())

PROGRAM_PRELUDE_DECLARE(normalize,
                        R"(
template <typename T, typename Enable = void>
struct ANGLE_normalize_impl
{
    static ANGLE_ALWAYS_INLINE T exec(T x)
    {
        return metal::fast::normalize(x);
    }
};
template <typename T>
struct ANGLE_normalize_impl<T, ANGLE_enable_if_t<ANGLE_is_scalar<T>::value>>
{
    static ANGLE_ALWAYS_INLINE T exec(T x)
    {
        return ANGLE_sign(x);
    }
};
template <typename T>
ANGLE_ALWAYS_INLINE T ANGLE_normalize(T x)
{
    return ANGLE_normalize_impl<T>::exec(x);
};
)",
                        include_metal_common(),
                        include_metal_geometric(),
                        enable_if(),
                        is_scalar(),
                        is_vector(),
                        is_matrix(),
                        sign())

PROGRAM_PRELUDE_DECLARE(faceforward,
                        R"(
template <typename T, typename Enable = void>
struct ANGLE_faceforward_impl
{
    static ANGLE_ALWAYS_INLINE T exec(T n, T i, T nref)
    {
        return metal::faceforward(n, i, nref);
    }
};
template <typename T>
struct ANGLE_faceforward_impl<T, ANGLE_enable_if_t<ANGLE_is_scalar<T>::value>>
{
    static ANGLE_ALWAYS_INLINE T exec(T n, T i, T nref)
    {
        return ANGLE_dot(nref, i) < T(0) ? n : -n;
    }
};
template <typename T>
ANGLE_ALWAYS_INLINE T ANGLE_faceforward(T n, T i, T nref)
{
    return ANGLE_faceforward_impl<T>::exec(n, i, nref);
};
)",
                        include_metal_geometric(),
                        enable_if(),
                        is_scalar(),
                        is_vector(),
                        is_matrix(),
                        dot())

PROGRAM_PRELUDE_DECLARE(reflect,
                        R"(
template <typename T, typename Enable = void>
struct ANGLE_reflect_impl
{
    static ANGLE_ALWAYS_INLINE T exec(T i, T n)
    {
        return metal::reflect(i, n);
    }
};
template <typename T>
struct ANGLE_reflect_impl<T, ANGLE_enable_if_t<ANGLE_is_scalar<T>::value>>
{
    static ANGLE_ALWAYS_INLINE T exec(T i, T n)
    {
        return i - T(2) * ANGLE_dot(n, i) * n;
    }
};
template <typename T>
ANGLE_ALWAYS_INLINE T ANGLE_reflect(T i, T n)
{
    return ANGLE_reflect_impl<T>::exec(i, n);
};
)",
                        include_metal_geometric(),
                        enable_if(),
                        is_scalar(),
                        is_vector(),
                        is_matrix(),
                        dot())

PROGRAM_PRELUDE_DECLARE(refract,
                        R"(
template <typename T, typename Enable = void>
struct ANGLE_refract_impl
{
    static ANGLE_ALWAYS_INLINE T exec(T i, T n, ANGLE_scalar_of_t<T> eta)
    {
        return metal::refract(i, n, eta);
    }
};
template <typename T>
struct ANGLE_refract_impl<T, ANGLE_enable_if_t<ANGLE_is_scalar<T>::value>>
{
    static ANGLE_ALWAYS_INLINE T exec(T i, T n, T eta)
    {
        auto dotNI = n * i;
        auto k = T(1) - eta * eta * (T(1) - dotNI * dotNI);
        if (k < T(0))
        {
            return T(0);
        }
        else
        {
            return eta * i - (eta * dotNI + metal::sqrt(k)) * n;
        }
    }
};
template <typename T>
ANGLE_ALWAYS_INLINE T ANGLE_refract(T i, T n, ANGLE_scalar_of_t<T> eta)
{
    return ANGLE_refract_impl<T>::exec(i, n, eta);
};
)",
                        include_metal_math(),
                        include_metal_geometric(),
                        enable_if(),
                        is_scalar(),
                        is_vector(),
                        is_matrix())

PROGRAM_PRELUDE_DECLARE(sign,
                        R"(
template <typename T, typename Enable = void>
struct ANGLE_sign_impl
{
    static ANGLE_ALWAYS_INLINE T exec(T x)
    {
        return metal::sign(x);
    }
};
template <>
struct ANGLE_sign_impl<int>
{
    static ANGLE_ALWAYS_INLINE int exec(int x)
    {
        return (0 < x) - (x < 0);
    }
};
template <int N>
struct ANGLE_sign_impl<metal::vec<int, N>>
{
    static ANGLE_ALWAYS_INLINE metal::vec<int, N> exec(metal::vec<int, N> x)
    {
        metal::vec<int, N> s;
        for (int i = 0; i < N; ++i)
        {
            s[i] = ANGLE_sign_impl<int>::exec(x[i]);
        }
        return s;
    }
};
template <typename T>
ANGLE_ALWAYS_INLINE T ANGLE_sign(T x)
{
    return ANGLE_sign_impl<T>::exec(x);
};
)",
                        include_metal_common())

PROGRAM_PRELUDE_DECLARE(int_clamp,
                        R"(
ANGLE_ALWAYS_INLINE int ANGLE_int_clamp(int value, int minValue, int maxValue)
{
    return ((value < minValue) ?  minValue : ((value > maxValue) ? maxValue : value));
};
)")

PROGRAM_PRELUDE_DECLARE(atan,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE T ANGLE_atan(T yOverX)
{
    return metal::atan(yOverX);
}
template <typename T>
ANGLE_ALWAYS_INLINE T ANGLE_atan(T y, T x)
{
    return metal::atan2(y, x);
}
)",
                        include_metal_math())

PROGRAM_PRELUDE_DECLARE(degrees, R"(
template <typename T>
ANGLE_ALWAYS_INLINE T ANGLE_degrees(T x)
{
    return static_cast<T>(57.29577951308232) * x;
}
)")

PROGRAM_PRELUDE_DECLARE(radians, R"(
template <typename T>
ANGLE_ALWAYS_INLINE T ANGLE_radians(T x)
{
    return static_cast<T>(1.7453292519943295e-2) * x;
}
)")

PROGRAM_PRELUDE_DECLARE(mod,
                        R"(
template <typename X, typename Y>
ANGLE_ALWAYS_INLINE X ANGLE_mod(X x, Y y)
{
    return x - y * metal::floor(x / y);
}
)",
                        include_metal_math())

PROGRAM_PRELUDE_DECLARE(mixBool,
                        R"(
template <typename T, int N>
ANGLE_ALWAYS_INLINE metal::vec<T,N> ANGLE_mix_bool(metal::vec<T, N> a, metal::vec<T, N> b, metal::vec<bool, N> c)
{
    return metal::mix(a, b, static_cast<metal::vec<T,N>>(c));
}
)",
                        include_metal_common())

PROGRAM_PRELUDE_DECLARE(pack_half_2x16,
                        R"(
ANGLE_ALWAYS_INLINE uint32_t ANGLE_pack_half_2x16(float2 v)
{
    return as_type<uint32_t>(half2(v));
}
)", )

PROGRAM_PRELUDE_DECLARE(unpack_half_2x16,
                        R"(
ANGLE_ALWAYS_INLINE float2 ANGLE_unpack_half_2x16(uint32_t x)
{
    return float2(as_type<half2>(x));
}
)", )

PROGRAM_PRELUDE_DECLARE(matmulAssign, R"(
template <typename T, int Cols, int Rows>
ANGLE_ALWAYS_INLINE thread metal::matrix<T, Cols, Rows> &operator*=(thread metal::matrix<T, Cols, Rows> &a, metal::matrix<T, Cols, Cols> b)
{
    a = a * b;
    return a;
}
)")

PROGRAM_PRELUDE_DECLARE(postIncrementMatrix,
                        R"(
template <typename T, int Cols, int Rows>
ANGLE_ALWAYS_INLINE metal::matrix<T, Cols, Rows> operator++(thread metal::matrix<T, Cols, Rows> &a, int)
{
    auto b = a;
    a += T(1);
    return b;
}
)",
                        addMatrixScalarAssign())

PROGRAM_PRELUDE_DECLARE(preIncrementMatrix,
                        R"(
template <typename T, int Cols, int Rows>
ANGLE_ALWAYS_INLINE thread metal::matrix<T, Cols, Rows> &operator++(thread metal::matrix<T, Cols, Rows> &a)
{
    a += T(1);
    return a;
}
)",
                        addMatrixScalarAssign())

PROGRAM_PRELUDE_DECLARE(postDecrementMatrix,
                        R"(
template <typename T, int Cols, int Rows>
ANGLE_ALWAYS_INLINE metal::matrix<T, Cols, Rows> operator--(thread metal::matrix<T, Cols, Rows> &a, int)
{
    auto b = a;
    a -= T(1);
    return b;
}
)",
                        subMatrixScalarAssign())

PROGRAM_PRELUDE_DECLARE(preDecrementMatrix,
                        R"(
template <typename T, int Cols, int Rows>
ANGLE_ALWAYS_INLINE thread metal::matrix<T, Cols, Rows> &operator--(thread metal::matrix<T, Cols, Rows> &a)
{
    a -= T(1);
    return a;
}
)",
                        subMatrixScalarAssign())

PROGRAM_PRELUDE_DECLARE(negateMatrix,
                        R"(
template <typename T, int Cols, int Rows>
ANGLE_ALWAYS_INLINE metal::matrix<T, Cols, Rows> operator-(metal::matrix<T, Cols, Rows> m)
{
    for (size_t col = 0; col < Cols; ++col)
    {
        thread auto &mCol = m[col];
        mCol = -mCol;
    }
    return m;
}
)", )

PROGRAM_PRELUDE_DECLARE(addMatrixScalarAssign, R"(
template <typename T, int Cols, int Rows>
ANGLE_ALWAYS_INLINE thread metal::matrix<T, Cols, Rows> &operator+=(thread metal::matrix<T, Cols, Rows> &m, T x)
{
    for (size_t col = 0; col < Cols; ++col)
    {
        m[col] += x;
    }
    return m;
}
)")

PROGRAM_PRELUDE_DECLARE(addMatrixScalar,
                        R"(
template <typename T, int Cols, int Rows>
ANGLE_ALWAYS_INLINE metal::matrix<T, Cols, Rows> operator+(metal::matrix<T, Cols, Rows> m, T x)
{
    m += x;
    return m;
}
)",
                        addMatrixScalarAssign())

PROGRAM_PRELUDE_DECLARE(subMatrixScalarAssign,
                        R"(
template <typename T, int Cols, int Rows>
ANGLE_ALWAYS_INLINE thread metal::matrix<T, Cols, Rows> &operator-=(thread metal::matrix<T, Cols, Rows> &m, T x)
{
    for (size_t col = 0; col < Cols; ++col)
    {
        m[col] -= x;
    }
    return m;
}
)", )

PROGRAM_PRELUDE_DECLARE(subMatrixScalar,
                        R"(
template <typename T, int Cols, int Rows>
ANGLE_ALWAYS_INLINE metal::matrix<T, Cols, Rows> operator-(metal::matrix<T, Cols, Rows> m, T x)
{
    m -= x;
    return m;
}
)",
                        subMatrixScalarAssign())

PROGRAM_PRELUDE_DECLARE(divMatrixScalarAssignFast,
                        R"(
template <typename T, int Cols, int Rows>
ANGLE_ALWAYS_INLINE thread metal::matrix<T, Cols, Rows> &operator/=(thread metal::matrix<T, Cols, Rows> &m, T x)
{
    x = T(1) / x;
    for (size_t col = 0; col < Cols; ++col)
    {
        m[col] *= x;
    }
    return m;
}
)", )

PROGRAM_PRELUDE_DECLARE(divMatrixScalarAssign,
                        R"(
template <typename T, int Cols, int Rows>
ANGLE_ALWAYS_INLINE thread metal::matrix<T, Cols, Rows> &operator/=(thread metal::matrix<T, Cols, Rows> &m, T x)
{
    for (size_t col = 0; col < Cols; ++col)
    {
        m[col] /= x;
    }
    return m;
}
)", )

PROGRAM_PRELUDE_DECLARE(divMatrixScalarFast,
                        R"(
#if __METAL_VERSION__ <= 220
template <typename T, int Cols, int Rows>
ANGLE_ALWAYS_INLINE metal::matrix<T, Cols, Rows> operator/(metal::matrix<T, Cols, Rows> m, T x)
{
    m /= x;
    return m;
}
#endif
)",
                        divMatrixScalarAssignFast())

PROGRAM_PRELUDE_DECLARE(divMatrixScalar,
                        R"(
#if __METAL_VERSION__ <= 220
template <typename T, int Cols, int Rows>
ANGLE_ALWAYS_INLINE metal::matrix<T, Cols, Rows> operator/(metal::matrix<T, Cols, Rows> m, T x)
{
    m /= x;
    return m;
}
#endif
)",
                        divMatrixScalarAssign())

PROGRAM_PRELUDE_DECLARE(componentWiseDivide, R"(
template <typename T, int Cols, int Rows>
ANGLE_ALWAYS_INLINE metal::matrix<T, Cols, Rows> operator/(metal::matrix<T, Cols, Rows> a, metal::matrix<T, Cols, Rows> b)
{
    for (size_t col = 0; col < Cols; ++col)
    {
        a[col] /= b[col];
    }
    return a;
}
)")

PROGRAM_PRELUDE_DECLARE(componentWiseDivideAssign,
                        R"(
template <typename T, int Cols, int Rows>
ANGLE_ALWAYS_INLINE thread metal::matrix<T, Cols, Rows> &operator/=(thread metal::matrix<T, Cols, Rows> &a, metal::matrix<T, Cols, Rows> b)
{
    a = a / b;
    return a;
}
)",
                        componentWiseDivide())

PROGRAM_PRELUDE_DECLARE(componentWiseMultiply, R"(
template <typename T, int Cols, int Rows>
ANGLE_ALWAYS_INLINE metal::matrix<T, Cols, Rows> ANGLE_componentWiseMultiply(metal::matrix<T, Cols, Rows> a, metal::matrix<T, Cols, Rows> b)
{
    for (size_t col = 0; col < Cols; ++col)
    {
        a[col] *= b[col];
    }
    return a;
}
)")

PROGRAM_PRELUDE_DECLARE(outerProduct, R"(
template <typename T, int M, int N>
ANGLE_ALWAYS_INLINE metal::matrix<T, N, M> ANGLE_outerProduct(metal::vec<T, M> u, metal::vec<T, N> v)
{
    metal::matrix<T, N, M> o;
    for (size_t n = 0; n < N; ++n)
    {
        o[n] = u * v[n];
    }
    return o;
}
)")

PROGRAM_PRELUDE_DECLARE(inverse2, R"(
template <typename T>
ANGLE_ALWAYS_INLINE metal::matrix<T, 2, 2> ANGLE_inverse(metal::matrix<T, 2, 2> m)
{
    metal::matrix<T, 2, 2> adj;
    adj[0][0] =  m[1][1];
    adj[0][1] = -m[0][1];
    adj[1][0] = -m[1][0];
    adj[1][1] =  m[0][0];
    T det = (adj[0][0] * m[0][0]) + (adj[0][1] * m[1][0]);
    return adj * (T(1) / det);
}
)")

PROGRAM_PRELUDE_DECLARE(inverse3, R"(
template <typename T>
ANGLE_ALWAYS_INLINE metal::matrix<T, 3, 3> ANGLE_inverse(metal::matrix<T, 3, 3> m)
{
    T a = m[1][1] * m[2][2] - m[2][1] * m[1][2];
    T b = m[1][0] * m[2][2];
    T c = m[1][2] * m[2][0];
    T d = m[1][0] * m[2][1];
    T det = m[0][0] * (a) -
            m[0][1] * (b - c) +
            m[0][2] * (d - m[1][1] * m[2][0]);
    det = T(1) / det;
    metal::matrix<T, 3, 3> minv;
    minv[0][0] = (a) * det;
    minv[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * det;
    minv[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * det;
    minv[1][0] = (c - b) * det;
    minv[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * det;
    minv[1][2] = (m[1][0] * m[0][2] - m[0][0] * m[1][2]) * det;
    minv[2][0] = (d - m[2][0] * m[1][1]) * det;
    minv[2][1] = (m[2][0] * m[0][1] - m[0][0] * m[2][1]) * det;
    minv[2][2] = (m[0][0] * m[1][1] - m[1][0] * m[0][1]) * det;
    return minv;
}
)")

PROGRAM_PRELUDE_DECLARE(inverse4, R"(
template <typename T>
ANGLE_ALWAYS_INLINE metal::matrix<T, 4, 4> ANGLE_inverse(metal::matrix<T, 4, 4> m)
{
    T A2323 = m[2][2] * m[3][3] - m[2][3] * m[3][2];
    T A1323 = m[2][1] * m[3][3] - m[2][3] * m[3][1];
    T A1223 = m[2][1] * m[3][2] - m[2][2] * m[3][1];
    T A0323 = m[2][0] * m[3][3] - m[2][3] * m[3][0];
    T A0223 = m[2][0] * m[3][2] - m[2][2] * m[3][0];
    T A0123 = m[2][0] * m[3][1] - m[2][1] * m[3][0];
    T A2313 = m[1][2] * m[3][3] - m[1][3] * m[3][2];
    T A1313 = m[1][1] * m[3][3] - m[1][3] * m[3][1];
    T A1213 = m[1][1] * m[3][2] - m[1][2] * m[3][1];
    T A2312 = m[1][2] * m[2][3] - m[1][3] * m[2][2];
    T A1312 = m[1][1] * m[2][3] - m[1][3] * m[2][1];
    T A1212 = m[1][1] * m[2][2] - m[1][2] * m[2][1];
    T A0313 = m[1][0] * m[3][3] - m[1][3] * m[3][0];
    T A0213 = m[1][0] * m[3][2] - m[1][2] * m[3][0];
    T A0312 = m[1][0] * m[2][3] - m[1][3] * m[2][0];
    T A0212 = m[1][0] * m[2][2] - m[1][2] * m[2][0];
    T A0113 = m[1][0] * m[3][1] - m[1][1] * m[3][0];
    T A0112 = m[1][0] * m[2][1] - m[1][1] * m[2][0];
    T a = m[1][1] * A2323 - m[1][2] * A1323 + m[1][3] * A1223;
    T b = m[1][0] * A2323 - m[1][2] * A0323 + m[1][3] * A0223;
    T c = m[1][0] * A1323 - m[1][1] * A0323 + m[1][3] * A0123;
    T d = m[1][0] * A1223 - m[1][1] * A0223 + m[1][2] * A0123;
    T det = m[0][0] * ( a )
          - m[0][1] * ( b )
          + m[0][2] * ( c )
          - m[0][3] * ( d );
    det = T(1) / det;
    metal::matrix<T, 4, 4> im;
    im[0][0] = det *   ( a );
    im[0][1] = det * - ( m[0][1] * A2323 - m[0][2] * A1323 + m[0][3] * A1223 );
    im[0][2] = det *   ( m[0][1] * A2313 - m[0][2] * A1313 + m[0][3] * A1213 );
    im[0][3] = det * - ( m[0][1] * A2312 - m[0][2] * A1312 + m[0][3] * A1212 );
    im[1][0] = det * - ( b );
    im[1][1] = det *   ( m[0][0] * A2323 - m[0][2] * A0323 + m[0][3] * A0223 );
    im[1][2] = det * - ( m[0][0] * A2313 - m[0][2] * A0313 + m[0][3] * A0213 );
    im[1][3] = det *   ( m[0][0] * A2312 - m[0][2] * A0312 + m[0][3] * A0212 );
    im[2][0] = det *   ( c );
    im[2][1] = det * - ( m[0][0] * A1323 - m[0][1] * A0323 + m[0][3] * A0123 );
    im[2][2] = det *   ( m[0][0] * A1313 - m[0][1] * A0313 + m[0][3] * A0113 );
    im[2][3] = det * - ( m[0][0] * A1312 - m[0][1] * A0312 + m[0][3] * A0112 );
    im[3][0] = det * - ( d );
    im[3][1] = det *   ( m[0][0] * A1223 - m[0][1] * A0223 + m[0][2] * A0123 );
    im[3][2] = det * - ( m[0][0] * A1213 - m[0][1] * A0213 + m[0][2] * A0113 );
    im[3][3] = det *   ( m[0][0] * A1212 - m[0][1] * A0212 + m[0][2] * A0112 );
    return im;
}
)")

PROGRAM_PRELUDE_DECLARE(equalArray,
                        R"(
template <typename T, size_t N>
ANGLE_ALWAYS_INLINE bool ANGLE_equal(metal::array<T, N> u, metal::array<T, N> v)
{
    for(size_t i = 0; i < N; i++)
        if (!ANGLE_equal(u[i], v[i])) return false;
    return true;
}
)",
                        equalScalar(),
                        equalVector(),
                        equalMatrix())

PROGRAM_PRELUDE_DECLARE(equalStructArray,
                        R"(
template <typename T, size_t N>
ANGLE_ALWAYS_INLINE bool ANGLE_equalStructArray(metal::array<T, N> u, metal::array<T, N> v)
{
    for(size_t i = 0; i < N; i++)
    {
        if (ANGLE_equal(u[i], v[i]) == false) 
            return false;
    }
    return true;
}
)")

PROGRAM_PRELUDE_DECLARE(notEqualArray,
                        R"(
template <typename T, size_t N>
ANGLE_ALWAYS_INLINE bool ANGLE_notEqual(metal::array<T, N> u, metal::array<T, N> v)
{
    return !ANGLE_equal(u,v);
}
)",
                        equalArray())

PROGRAM_PRELUDE_DECLARE(equalScalar,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE bool ANGLE_equal(T u, T v)
{
    return u == v;
}
)",
                        include_metal_math())

PROGRAM_PRELUDE_DECLARE(equalVector,
                        R"(
template <typename T, int N>
ANGLE_ALWAYS_INLINE bool ANGLE_equal(metal::vec<T, N> u, metal::vec<T, N> v)
{
    return metal::all(u == v);
}
)",
                        include_metal_math())

PROGRAM_PRELUDE_DECLARE(equalMatrix,
                        R"(
template <typename T, int C, int R>
ANGLE_ALWAYS_INLINE bool ANGLE_equal(metal::matrix<T, C, R> a, metal::matrix<T, C, R> b)
{
    for (int c = 0; c < C; ++c)
    {
        if (!ANGLE_equal(a[c], b[c]))
        {
            return false;
        }
    }
    return true;
}
)",
                        equalVector())

PROGRAM_PRELUDE_DECLARE(notEqualMatrix,
                        R"(
template <typename T, int C, int R>
ANGLE_ALWAYS_INLINE bool ANGLE_notEqual(metal::matrix<T, C, R> u, metal::matrix<T, C, R> v)
{
    return !ANGLE_equal(u, v);
}
)",
                        equalMatrix())

PROGRAM_PRELUDE_DECLARE(notEqualVector,
                        R"(
template <typename T, int N>
ANGLE_ALWAYS_INLINE bool ANGLE_notEqual(metal::vec<T, N> u, metal::vec<T, N> v)
{
    return !ANGLE_equal(u, v);
}
)",
                        equalVector())

PROGRAM_PRELUDE_DECLARE(notEqualStruct,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE bool ANGLE_notEqualStruct(thread const T &a, thread const T &b)
{
    return !ANGLE_equal(a, b);
}
)",
                        equalVector(),
                        equalMatrix())

PROGRAM_PRELUDE_DECLARE(notEqualStructArray,
                        R"(
template <typename T, size_t N>
ANGLE_ALWAYS_INLINE bool ANGLE_notEqualStructArray(metal::array<T, N> u, metal::array<T, N> v)
{
    for(size_t i = 0; i < N; i++)
    {
        if (ANGLE_notEqualStruct(u[i], v[i]))
            return true;
    }
    return false;
}
)",
                        notEqualStruct())

PROGRAM_PRELUDE_DECLARE(vectorElemRef, R"(
template <typename T, int N>
struct ANGLE_VectorElemRef
{
    thread metal::vec<T, N> &mVec;
    T mRef;
    const int mIndex;
    ~ANGLE_VectorElemRef() { mVec[mIndex] = mRef; }
    ANGLE_VectorElemRef(thread metal::vec<T, N> &vec, int index)
        : mVec(vec), mRef(vec[index]), mIndex(index)
    {}
    operator thread T &() { return mRef; }
};
template <typename T, int N>
ANGLE_ALWAYS_INLINE ANGLE_VectorElemRef<T, N> ANGLE_elem_ref(thread metal::vec<T, N> &vec, int index)
{
    return ANGLE_VectorElemRef<T, N>(vec, index);
}
)")

PROGRAM_PRELUDE_DECLARE(swizzleRef,
                        R"(
template <typename T, int VN, int SN>
struct ANGLE_SwizzleRef
{
    thread metal::vec<T, VN> &mVec;
    metal::vec<T, SN> mRef;
    int mIndices[SN];
    ~ANGLE_SwizzleRef()
    {
        for (int i = 0; i < SN; ++i)
        {
            const int j = mIndices[i];
            mVec[j] = mRef[i];
        }
    }
    ANGLE_SwizzleRef(thread metal::vec<T, VN> &vec, thread const int *indices)
        : mVec(vec)
    {
        for (int i = 0; i < SN; ++i)
        {
            const int j = indices[i];
            mIndices[i] = j;
            mRef[i] = mVec[j];
        }
    }
    operator thread metal::vec<T, SN> &() { return mRef; }
};
template <typename T, int N>
ANGLE_ALWAYS_INLINE ANGLE_VectorElemRef<T, N> ANGLE_swizzle_ref(thread metal::vec<T, N> &vec, int i0)
{
    return ANGLE_VectorElemRef<T, N>(vec, i0);
}
template <typename T, int N>
ANGLE_ALWAYS_INLINE ANGLE_SwizzleRef<T, N, 2> ANGLE_swizzle_ref(thread metal::vec<T, N> &vec, int i0, int i1)
{
    const int is[] = { i0, i1 };
    return ANGLE_SwizzleRef<T, N, 2>(vec, is);
}
template <typename T, int N>
ANGLE_ALWAYS_INLINE ANGLE_SwizzleRef<T, N, 3> ANGLE_swizzle_ref(thread metal::vec<T, N> &vec, int i0, int i1, int i2)
{
    const int is[] = { i0, i1, i2 };
    return ANGLE_SwizzleRef<T, N, 3>(vec, is);
}
template <typename T, int N>
ANGLE_ALWAYS_INLINE ANGLE_SwizzleRef<T, N, 4> ANGLE_swizzle_ref(thread metal::vec<T, N> &vec, int i0, int i1, int i2, int i3)
{
    const int is[] = { i0, i1, i2, i3 };
    return ANGLE_SwizzleRef<T, N, 4>(vec, is);
}
)",
                        vectorElemRef())

PROGRAM_PRELUDE_DECLARE(out, R"(
template <typename T>
struct ANGLE_Out
{
    T mTemp;
    thread T &mDest;
    ~ANGLE_Out() { mDest = mTemp; }
    ANGLE_Out(thread T &dest)
        : mTemp(dest), mDest(dest)
    {}
    operator thread T &() { return mTemp; }
};
template <typename T>
ANGLE_ALWAYS_INLINE ANGLE_Out<T> ANGLE_out(thread T &dest)
{
    return ANGLE_Out<T>(dest);
}
)")

PROGRAM_PRELUDE_DECLARE(inout, R"(
template <typename T>
struct ANGLE_InOut
{
    T mTemp;
    thread T &mDest;
    ~ANGLE_InOut() { mDest = mTemp; }
    ANGLE_InOut(thread T &dest)
        : mTemp(dest), mDest(dest)
    {}
    operator thread T &() { return mTemp; }
};
template <typename T>
ANGLE_ALWAYS_INLINE ANGLE_InOut<T> ANGLE_inout(thread T &dest)
{
    return ANGLE_InOut<T>(dest);
}
)")

PROGRAM_PRELUDE_DECLARE(flattenArray, R"(
template <typename T>
struct ANGLE_flatten_impl
{
    static ANGLE_ALWAYS_INLINE thread T *exec(thread T &x)
    {
        return &x;
    }
};
template <typename T, size_t N>
struct ANGLE_flatten_impl<metal::array<T, N>>
{
    static ANGLE_ALWAYS_INLINE auto exec(thread metal::array<T, N> &arr) -> T
    {
        return ANGLE_flatten_impl<T>::exec(arr[0]);
    }
};
template <typename T, size_t N>
ANGLE_ALWAYS_INLINE auto ANGLE_flatten(thread metal::array<T, N> &arr) -> T
{
    return ANGLE_flatten_impl<T>::exec(arr[0]);
}
)")

PROGRAM_PRELUDE_DECLARE(castVector, R"(
template <typename T, int N1, int N2>
struct ANGLE_castVector {};
template <typename T, int N>
struct ANGLE_castVector<T, N, N>
{
    static ANGLE_ALWAYS_INLINE metal::vec<T, N> exec(metal::vec<T, N> const v)
    {
        return v;
    }
};
template <typename T>
struct ANGLE_castVector<T, 2, 3>
{
    static ANGLE_ALWAYS_INLINE metal::vec<T, 2> exec(metal::vec<T, 3> const v)
    {
        return v.xy;
    }
};
template <typename T>
struct ANGLE_castVector<T, 2, 4>
{
    static ANGLE_ALWAYS_INLINE metal::vec<T, 2> exec(metal::vec<T, 4> const v)
    {
        return v.xy;
    }
};
template <typename T>
struct ANGLE_castVector<T, 3, 4>
{
    static ANGLE_ALWAYS_INLINE metal::vec<T, 3> exec(metal::vec<T, 4> const v)
    {
        return as_type<metal::vec<T, 3>>(v);
    }
};
template <int N1, int N2, typename T>
ANGLE_ALWAYS_INLINE metal::vec<T, N1> ANGLE_cast(metal::vec<T, N2> const v)
{
    return ANGLE_castVector<T, N1, N2>::exec(v);
}
)")

PROGRAM_PRELUDE_DECLARE(castMatrix,
                        R"(
template <typename T, int C1, int R1, int C2, int R2, typename Enable = void>
struct ANGLE_castMatrix
{
    static ANGLE_ALWAYS_INLINE metal::matrix<T, C1, R1> exec(metal::matrix<T, C2, R2> const m2)
    {
        metal::matrix<T, C1, R1> m1;
        const int MinC = C1 <= C2 ? C1 : C2;
        const int MinR = R1 <= R2 ? R1 : R2;
        for (int c = 0; c < MinC; ++c)
        {
            for (int r = 0; r < MinR; ++r)
            {
                m1[c][r] = m2[c][r];
            }
            for (int r = R2; r < R1; ++r)
            {
                m1[c][r] = c == r ? T(1) : T(0);
            }
        }
        for (int c = C2; c < C1; ++c)
        {
            for (int r = 0; r < R1; ++r)
            {
                m1[c][r] = c == r ? T(1) : T(0);
            }
        }
        return m1;
    }
};
template <typename T, int C1, int R1, int C2, int R2>
struct ANGLE_castMatrix<T, C1, R1, C2, R2, ANGLE_enable_if_t<(C1 <= C2 && R1 <= R2)>>
{
    static ANGLE_ALWAYS_INLINE metal::matrix<T, C1, R1> exec(metal::matrix<T, C2, R2> const m2)
    {
        metal::matrix<T, C1, R1> m1;
        for (size_t c = 0; c < C1; ++c)
        {
            m1[c] = ANGLE_cast<R1>(m2[c]);
        }
        return m1;
    }
};
template <int C1, int R1, int C2, int R2, typename T>
ANGLE_ALWAYS_INLINE metal::matrix<T, C1, R1> ANGLE_cast(metal::matrix<T, C2, R2> const m)
{
    return ANGLE_castMatrix<T, C1, R1, C2, R2>::exec(m);
};
)",
                        enable_if(),
                        castVector())

PROGRAM_PRELUDE_DECLARE(tensor, R"(
template <typename T, size_t... DS>
struct ANGLE_tensor_traits;
template <typename T, size_t D>
struct ANGLE_tensor_traits<T, D>
{
    enum : size_t { outer_dim = D };
    using inner_type = T;
    using outer_type = inner_type[D];
};
template <typename T, size_t D, size_t... DS>
struct ANGLE_tensor_traits<T, D, DS...>
{
    enum : size_t { outer_dim = D };
    using inner_type = typename ANGLE_tensor_traits<T, DS...>::outer_type;
    using outer_type = inner_type[D];
};
template <size_t D, typename value_type_, typename inner_type_>
struct ANGLE_tensor_impl
{
    enum : size_t { outer_dim = D };
    using value_type = value_type_;
    using inner_type = inner_type_;
    using outer_type = inner_type[D];
    outer_type _data;
    ANGLE_ALWAYS_INLINE size_t size() const { return outer_dim; }
    ANGLE_ALWAYS_INLINE inner_type &operator[](size_t i) { return _data[i]; }
    ANGLE_ALWAYS_INLINE const inner_type &operator[](size_t i) const { return _data[i]; }
};
template <typename T, size_t... DS>
using ANGLE_tensor = ANGLE_tensor_impl<
    ANGLE_tensor_traits<T, DS...>::outer_dim,
    T,
    typename ANGLE_tensor_traits<T, DS...>::inner_type>;
)")

PROGRAM_PRELUDE_DECLARE(gradient,
                        R"(
template <int N>
struct ANGLE_gradient_traits;
template <>
struct ANGLE_gradient_traits<2> { using type = metal::gradient2d; };
template <>
struct ANGLE_gradient_traits<3> { using type = metal::gradient3d; };

template <int N>
using ANGLE_gradient = typename ANGLE_gradient_traits<N>::type;
)")

PROGRAM_PRELUDE_DECLARE(writeSampleMask,
                        R"(
ANGLE_ALWAYS_INLINE void ANGLE_writeSampleMask(const uint32_t mask,
                                               thread uint& gl_SampleMask)
{
    if (ANGLECoverageMaskEnabled)
    {
        gl_SampleMask = as_type<int>(mask);
    }
}
)",
                        functionConstants())

PROGRAM_PRELUDE_DECLARE(textureEnv,
                        R"(
template <typename T>
struct ANGLE_TextureEnv
{
    thread T *texture;
    thread metal::sampler *sampler;
};
)")

PROGRAM_PRELUDE_DECLARE(functionConstants,
                        R"(
#define ANGLE_SAMPLE_COMPARE_GRADIENT_INDEX 0
#define ANGLE_SAMPLE_COMPARE_LOD_INDEX      1
#define ANGLE_RASTERIZATION_DISCARD_INDEX   2
#define ANGLE_COVERAGE_MASK_ENABLED_INDEX   3
#define ANGLE_DEPTH_WRITE_ENABLED_INDEX     4

constant bool ANGLEUseSampleCompareGradient [[function_constant(ANGLE_SAMPLE_COMPARE_GRADIENT_INDEX)]];
constant bool ANGLEUseSampleCompareLod      [[function_constant(ANGLE_SAMPLE_COMPARE_LOD_INDEX)]];
constant bool ANGLERasterizerDisabled       [[function_constant(ANGLE_RASTERIZATION_DISCARD_INDEX)]];
constant bool ANGLECoverageMaskEnabled      [[function_constant(ANGLE_COVERAGE_MASK_ENABLED_INDEX)]];
constant bool ANGLEDepthWriteEnabled        [[function_constant(ANGLE_DEPTH_WRITE_ENABLED_INDEX)]];
)")

PROGRAM_PRELUDE_DECLARE(texelFetch,
                        R"(
#define ANGLE_texelFetch(env, ...) ANGLE_texelFetch_impl(*env.texture, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texelFetch_impl(
    thread Texture &texture,
    metal::int2 const coord,
    uint32_t level)
{
    return texture.read(uint2(coord), level);
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texelFetch_impl(
    thread Texture &texture,
    metal::int3 const coord,
    uint32_t level)
{
    return texture.read(uint3(coord), level);
}

template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_texelFetch_impl(
    thread metal::texture2d_array<T> &texture,
    metal::int3 const coord,
    uint32_t level)
{
    return texture.read(uint2(coord.xy), uint32_t(coord.z), level);
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(texelFetchOffset,
                        R"(
#define ANGLE_texelFetchOffset(env, ...) ANGLE_texelFetchOffset_impl(*env.texture, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texelFetchOffset_impl(
    thread Texture &texture,
    metal::int2 const coord,
    uint32_t level,
    metal::int2 const offset)
{
    return texture.read(uint2(coord + offset), level);
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texelFetchOffset_impl(
    thread Texture &texture,
    metal::int3 const coord,
    uint32_t level,
    metal::int3 const offset)
{
    return texture.read(uint3(coord + offset), level);
}

template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_texelFetchOffset_impl(
    thread metal::texture2d_array<T> &texture,
    metal::int3 const coord,
    uint32_t level,
    metal::int2 const offset)
{
    return texture.read(uint2(coord.xy + offset), uint32_t(coord.z), level);
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(texture,
                        R"(
#define ANGLE_texture(env, ...) ANGLE_texture_impl(*env.texture, *env.sampler, __VA_ARGS__)
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(texture_generic_float2,
                        R"(
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float2 const coord)
{
    return texture.sample(sampler, coord);
}
)",
                        texture())

PROGRAM_PRELUDE_DECLARE(texture_generic_float2_float,
                        R"(
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float2 const coord,
    float bias)
{
    return texture.sample(sampler, coord, metal::bias(bias));
}
)",
                        texture())

PROGRAM_PRELUDE_DECLARE(texture_generic_float3,
                        R"(
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord)
{
    return texture.sample(sampler, coord);
}
)",
                        texture())

PROGRAM_PRELUDE_DECLARE(texture_generic_float3_float,
                        R"(
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float bias)
{
    return texture.sample(sampler, coord, metal::bias(bias));
}
)",
                        texture())

PROGRAM_PRELUDE_DECLARE(texture_depth2d_float3,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_texture_impl(
    thread metal::depth2d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord)
{
    return texture.sample_compare(sampler, coord.xy, coord.z);
}
)",
                        texture())

PROGRAM_PRELUDE_DECLARE(texture_depth2d_float3_float,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_texture_impl(
    thread metal::depth2d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float bias)
{
    return texture.sample_compare(sampler, coord.xy, coord.z, metal::bias(bias));
}
)",
                        texture())

PROGRAM_PRELUDE_DECLARE(texture_depth2darray_float4,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_texture_impl(
    thread metal::depth2d_array<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord)
{
    return texture.sample_compare(sampler, coord.xy, uint32_t(metal::round(coord.z)), coord.w);
}
)",
                        texture())

PROGRAM_PRELUDE_DECLARE(texture_depth2darray_float4_float,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_texture_impl(
    thread metal::depth2d_array<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float compare)
{
    return texture.sample_compare(sampler, coord.xyz, uint32_t(metal::round(coord.w)), compare);
}
)",
                        texture())

PROGRAM_PRELUDE_DECLARE(texture_depthcube_float4,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_texture_impl(
    thread metal::depthcube<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord)
{
    return texture.sample_compare(sampler, coord.xyz, coord.w);
}
)",
                        texture())

PROGRAM_PRELUDE_DECLARE(texture_depthcube_float4_float,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_texture_impl(
    thread metal::depthcube<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float bias)
{
    return texture.sample_compare(sampler, coord.xyz, coord.w, metal::bias(bias));
}
)",
                        texture())

PROGRAM_PRELUDE_DECLARE(texture_texture2darray_float3,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_texture_impl(
    thread metal::texture2d_array<T> &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord)
{
    return texture.sample(sampler, coord.xy, uint32_t(metal::round(coord.z)));
}
)",
                        texture())

PROGRAM_PRELUDE_DECLARE(texture_texture2darray_float3_float,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_texture_impl(
    thread metal::texture2d_array<T> &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float bias)
{
    return texture.sample(sampler, coord.xy, uint32_t(metal::round(coord.z)), metal::bias(bias));
}
)",
                        texture())

PROGRAM_PRELUDE_DECLARE(texture_texture2darray_float4,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_texture_impl(
    thread metal::texture2d_array<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord)
{
    return texture.sample(sampler, coord.xyz, uint32_t(metal::round(coord.w)));
}
)",
                        texture())

PROGRAM_PRELUDE_DECLARE(texture_texture2darray_float4_float,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_texture_impl(
    thread metal::texture2d_array<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float bias)
{
    return texture.sample(sampler, coord.xyz, uint32_t(metal::round(coord.w)), metal::bias(bias));
}
)",
                        texture())

PROGRAM_PRELUDE_DECLARE(texture1DLod,
                        R"(
#define ANGLE_texture1DLod(env, ...) ANGLE_texture1DLod_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture1DLod_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    float const coord,
    float level)
{
    return texture.sample(sampler, coord, metal::level(level));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(texture1DProj,
                        R"(
#define ANGLE_texture1DProj(env, ...) ANGLE_texture1DProj_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture1DProj_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float2 const coord,
    float bias = 0)
{
    return texture.sample(sampler, coord.x/coord.y, metal::bias(bias));
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture1DProj_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float bias = 0)
{
    return texture.sample(sampler, coord.x/coord.w, metal::bias(bias));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(texture1DProjLod,
                        R"(
#define ANGLE_texture1DProjLod(env, ...) ANGLE_texture1DProjLod_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture1DProjLod_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float2 const coord,
    float level)
{
    return texture.sample(sampler, coord.x/coord.y, metal::level(level));
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture1DProjLod_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float level)
{
    return texture.sample(sampler, coord.x/coord.w, metal::level(level));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(texture2D,
                        R"(
#define ANGLE_texture2D(env, ...) ANGLE_texture2D_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture2D_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float2 const coord)
{
    return texture.sample(sampler, coord);
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture2D_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float2 const coord,
    float bias)
{
    return texture.sample(sampler, coord, metal::bias(bias));
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture2D_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord)
{
    return texture.sample(sampler, coord);
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture2D_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float bias)
{
    return texture.sample(sampler, coord, metal::bias(bias));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(texture2DGradEXT,
                        R"(
#define ANGLE_texture2DGradEXT(env, ...) ANGLE_texture2DGradEXT_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture2DGradEXT_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float2 const coord,
    metal::float2 const dPdx,
    metal::float2 const dPdy)
{
    return texture.sample(sampler, coord, metal::gradient2d(dPdx, dPdy));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(texture2DRect,
                        R"(
#define ANGLE_texture2DRect(env, ...) ANGLE_texture2DRect_impl(*env.texture, *env.sampler, __VA_ARGS__)
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture2DRect_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float2 const coord)
{
    return texture.sample(sampler, coord);
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(texture2DLod,
                        R"(
#define ANGLE_texture2DLod(env, ...) ANGLE_texture2DLod_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture2DLod_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float2 const coord,
    float level)
{
    return texture.sample(sampler, coord, metal::level(level));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(texture2DLodEXT,
                        R"(
#define ANGLE_texture2DLodEXT ANGLE_texture2DLod
)",
                        texture2DLod())

PROGRAM_PRELUDE_DECLARE(texture2DProj,
                        R"(
#define ANGLE_texture2DProj(env, ...) ANGLE_texture2DProj_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture2DProj_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float bias = 0)
{
    return texture.sample(sampler, coord.xy/coord.z, metal::bias(bias));
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture2DProj_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float bias = 0)
{
    return texture.sample(sampler, coord.xy/coord.w, metal::bias(bias));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(texture2DProjGradEXT,
                        R"(
#define ANGLE_texture2DProjGradEXT(env, ...) ANGLE_texture2DProjGradEXT_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture2DProjGradEXT_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    metal::float2 const dPdx,
    metal::float2 const dPdy)
{
    return texture.sample(sampler, coord.xy/coord.z, metal::gradient2d(dPdx, dPdy));
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture2DProjGradEXT_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    metal::float2 const dPdx,
    metal::float2 const dPdy)
{
    return texture.sample(sampler, coord.xy/coord.w, metal::gradient2d(dPdx, dPdy));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(texture2DRectProj,
                        R"(
#define ANGLE_texture2DRectProj(env, ...) ANGLE_texture2DRectProj_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture2DRectProj_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord)
{
    return texture.sample(sampler, coord.xy/coord.z);
}
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture2DRectProj_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord)
{
    return texture.sample(sampler, coord.xy/coord.w);
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(texture2DProjLod,
                        R"(
#define ANGLE_texture2DProjLod(env, ...) ANGLE_texture2DProjLod_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture2DProjLod_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float level)
{
    return texture.sample(sampler, coord.xy/coord.z, metal::level(level));
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture2DProjLod_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float level)
{
    return texture.sample(sampler, coord.xy/coord.w, metal::level(level));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(texture2DProjLodEXT,
                        R"(
#define ANGLE_texture2DProjLodEXT ANGLE_texture2DProjLod
)",
                        texture2DProjLod())

PROGRAM_PRELUDE_DECLARE(texture3DLod,
                        R"(
#define ANGLE_texture3DLod(env, ...) ANGLE_texture3DLod_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture3DLod_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float level)
{
    return texture.sample(sampler, coord, metal::level(level));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(texture3DProj,
                        R"(
#define ANGLE_texture3DProj(env, ...) ANGLE_texture3DProj_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture3DProj_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float bias = 0)
{
    return texture.sample(sampler, coord.xyz/coord.w, metal::bias(bias));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(texture3DProjLod,
                        R"(
#define ANGLE_texture3DProjLod(env, ...) ANGLE_texture3DProjLod_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_texture3DProjLod_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float level)
{
    return texture.sample(sampler, coord.xyz/coord.w, metal::level(level));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(textureCube,
                        R"(
#define ANGLE_textureCube(env, ...) ANGLE_textureCube_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureCube_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord)
{
    return texture.sample(sampler, coord);
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureCube_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float bias)
{
    return texture.sample(sampler, coord, metal::bias(bias));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(textureCubeGradEXT,
                        R"(
#define ANGLE_textureCubeGradEXT(env, ...) ANGLE_textureCubeGradEXT_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureCubeGradEXT_impl(
    thread metal::texturecube<T> &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    metal::float3 const dPdx,
    metal::float3 const dPdy)
{
    return texture.sample(sampler, coord, metal::gradientcube(dPdx, dPdy));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(textureCubeLod,
                        R"(
#define ANGLE_textureCubeLod(env, ...) ANGLE_textureCubeLod_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureCubeLod_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float level)
{
    return texture.sample(sampler, coord, metal::level(level));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(textureCubeLodEXT,
                        R"(
#define ANGLE_textureCubeLodEXT ANGLE_textureCubeLod
)",
                        textureCubeLod())

PROGRAM_PRELUDE_DECLARE(textureCubeProj,
                        R"(
#define ANGLE_textureCubeProj(env, ...) ANGLE_textureCubeProj_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureCubeProj_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float bias = 0)
{
    return texture.sample(sampler, coord.xyz/coord.w, metal::bias(bias));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(textureCubeProjLod,
                        R"(
#define ANGLE_textureCubeProjLod(env, ...) ANGLE_textureCubeProjLod_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureCubeProjLod_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float level)
{
    return texture.sample(sampler, coord.xyz/coord.w, metal::level(level));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(textureGrad,
                        R"(
#define ANGLE_textureGrad(env, ...) ANGLE_textureGrad_impl(*env.texture, *env.sampler, __VA_ARGS__)
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(textureGrad_generic_floatN_floatN_floatN,
                        R"(
template <typename Texture, int N>
ANGLE_ALWAYS_INLINE auto ANGLE_textureGrad_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::vec<float, N> const coord,
    metal::vec<float, N> const dPdx,
    metal::vec<float, N> const dPdy)
{
    return texture.sample(sampler, coord, ANGLE_gradient<N>(dPdx, dPdy));
}
)",
                        gradient(),
                        textureGrad())

PROGRAM_PRELUDE_DECLARE(textureGrad_generic_float3_float2_float2,
                        R"(
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureGrad_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    metal::float2 const dPdx,
    metal::float2 const dPdy)
{
    return texture.sample(sampler, coord.xy, uint32_t(metal::round(coord.z)), metal::gradient2d(dPdx, dPdy));
}
)",
                        textureGrad())

PROGRAM_PRELUDE_DECLARE(textureGrad_generic_float4_float2_float2,
                        R"(
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureGrad_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    metal::float2 const dPdx,
    metal::float2 const dPdy)
{
    return texture.sample(sampler, coord.xy, uint32_t(metal::round(coord.z)), metal::gradient2d(dPdx, dPdy));
}
)",
                        textureGrad())

PROGRAM_PRELUDE_DECLARE(textureGrad_depth2d_float3_float2_float2,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureGrad_impl(
    thread metal::depth2d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    metal::float2 const dPdx,
    metal::float2 const dPdy)
{
    if (ANGLEUseSampleCompareGradient)
    {
        return static_cast<T>(texture.sample_compare(sampler, coord.xy, coord.z, metal::gradient2d(dPdx, dPdy)));
    }
    else
    {
        return static_cast<T>(texture.sample(sampler, coord.xy, metal::gradient2d(dPdx, dPdy)) > coord.z);
    }
}
)",
                        functionConstants(),
                        textureGrad())

PROGRAM_PRELUDE_DECLARE(textureGrad_depth2darray_float4_float2_float2,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureGrad_impl(
    thread metal::depth2d_array<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    metal::float2 const dPdx,
    metal::float2 const dPdy)
{
    if (ANGLEUseSampleCompareGradient)
    {
        return static_cast<T>(texture.sample_compare(sampler, coord.xy, uint32_t(metal::round(coord.z)), coord.w, metal::gradient2d(dPdx, dPdy)));
    }
    else
    {
        return static_cast<T>(texture.sample(sampler, coord.xy, uint32_t(metal::round(coord.z)), metal::gradient2d(dPdx, dPdy)) > coord.w);
    }
}
)",
                        functionConstants(),
                        textureGrad())

PROGRAM_PRELUDE_DECLARE(textureGrad_depthcube_float4_float3_float3,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureGrad_impl(
    thread metal::depthcube<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    metal::float3 const dPdx,
    metal::float3 const dPdy)
{
    if (ANGLEUseSampleCompareGradient)
    {
        return static_cast<T>(texture.sample_compare(sampler, coord.xyz, coord.w, metal::gradientcube(dPdx, dPdy)));
    }
    else
    {
        return static_cast<T>(texture.sample(sampler, coord.xyz, metal::gradientcube(dPdx, dPdy)) > coord.w);
    }
}
)",
                        functionConstants(),
                        textureGrad())

PROGRAM_PRELUDE_DECLARE(textureGrad_texturecube_float3_float3_float3,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureGrad_impl(
    thread metal::texturecube<T> &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    metal::float3 const dPdx,
    metal::float3 const dPdy)
{
    return texture.sample(sampler, coord, metal::gradientcube(dPdx, dPdy));
}
)",
                        textureGrad())

PROGRAM_PRELUDE_DECLARE(textureGradOffset,
                        R"(
#define ANGLE_textureGradOffset(env, ...) ANGLE_textureGradOffset_impl(*env.texture, *env.sampler, __VA_ARGS__)
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(textureGradOffset_generic_floatN_floatN_floatN_intN,
                        R"(
template <typename Texture, int N>
ANGLE_ALWAYS_INLINE auto ANGLE_textureGradOffset_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::vec<float, N> const coord,
    metal::vec<float, N> const dPdx,
    metal::vec<float, N> const dPdy,
    metal::vec<int, N> const offset)
{
    return texture.sample(sampler, coord, ANGLE_gradient<N>(dPdx, dPdy), offset);
}
)",
                        gradient(),
                        textureGradOffset())

PROGRAM_PRELUDE_DECLARE(textureGradOffset_generic_float3_float2_float2_int2,
                        R"(
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureGradOffset_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    metal::float2 const dPdx,
    metal::float2 const dPdy,
    metal::int2 const offset)
{
    return texture.sample(sampler, coord.xy, uint32_t(metal::round(coord.z)), metal::gradient2d(dPdx, dPdy), offset);
}
)",
                        textureGradOffset())

PROGRAM_PRELUDE_DECLARE(textureGradOffset_generic_float4_float2_float2_int2,
                        R"(
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureGradOffset_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    metal::float2 const dPdx,
    metal::float2 const dPdy,
    metal::int2 const offset)
{
    return texture.sample(sampler, coord.xy, uint32_t(metal::round(coord.z)), metal::gradient2d(dPdx, dPdy), offset);
}
)",
                        textureGradOffset())

PROGRAM_PRELUDE_DECLARE(textureGradOffset_depth2d_float3_float2_float2_int2,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureGradOffset_impl(
    thread metal::depth2d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    metal::float2 const dPdx,
    metal::float2 const dPdy,
    metal::int2 const offset)
{
    if (ANGLEUseSampleCompareGradient)
    {
        return static_cast<T>(texture.sample_compare(sampler, coord.xy, coord.z, metal::gradient2d(dPdx, dPdy), offset));
    }
    else
    {
        return static_cast<T>(texture.sample(sampler, coord.xy, metal::gradient2d(dPdx, dPdy), offset) > coord.z);
    }
}
)",
                        functionConstants(),
                        textureGradOffset())

PROGRAM_PRELUDE_DECLARE(textureGradOffset_depth2darray_float4_float2_float2_int2,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureGradOffset_impl(
    thread metal::depth2d_array<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    metal::float2 const dPdx,
    metal::float2 const dPdy,
    metal::int2 const offset)
{
    if (ANGLEUseSampleCompareGradient)
    {
        return static_cast<T>(texture.sample_compare(sampler, coord.xy, uint32_t(metal::round(coord.z)), coord.w, metal::gradient2d(dPdx, dPdy), offset));
    }
    else
    {
        return static_cast<T>(texture.sample(sampler, coord.xy, uint32_t(metal::round(coord.z)), metal::gradient2d(dPdx, dPdy), offset) > coord.w);
    }
}
)",
                        functionConstants(),
                        textureGradOffset())

PROGRAM_PRELUDE_DECLARE(textureGradOffset_depthcube_float4_float3_float3_int3,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureGradOffset_impl(
    thread metal::depthcube<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    metal::float3 const dPdx,
    metal::float3 const dPdy,
    metal::int3 const offset)
{
    return texture.sample_compare(sampler, coord.xyz, coord.w, metal::gradientcube(dPdx, dPdy), offset);
}
)",
                        textureGradOffset())

PROGRAM_PRELUDE_DECLARE(textureGradOffset_texturecube_float3_float3_float3_int3,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureGradOffset_impl(
    thread metal::texturecube<T> &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    metal::float3 const dPdx,
    metal::float3 const dPdy,
    metal::int3 const offset)
{
    return texture.sample(sampler, coord, metal::gradientcube(dPdx, dPdy), offset);
}
)",
                        textureGradOffset())

PROGRAM_PRELUDE_DECLARE(textureLod,
                        R"(
#define ANGLE_textureLod(env, ...) ANGLE_textureLod_impl(*env.texture, *env.sampler, __VA_ARGS__)
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(textureLod_generic_float2,
                        R"(
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureLod_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float2 const coord,
    float level)
{
    return texture.sample(sampler, coord, metal::level(level));
}
)",
                        textureLod())

PROGRAM_PRELUDE_DECLARE(textureLod_generic_float3,
                        R"(
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureLod_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float level)
{
    return texture.sample(sampler, coord, metal::level(level));
}
)",
                        textureLod())

PROGRAM_PRELUDE_DECLARE(textureLod_depth2d_float3,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureLod_impl(
    thread metal::depth2d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float level)
{
    if (ANGLEUseSampleCompareLod)
    {
        return static_cast<T>(texture.sample_compare(sampler, coord.xy, coord.z, metal::level(level)));
    }
    else
    {
        return static_cast<T>(texture.sample(sampler, coord.xy, metal::level(level)) > coord.z);
    }
}
)",
                        functionConstants(),
                        textureLod())

PROGRAM_PRELUDE_DECLARE(textureLod_texture2darray_float3,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureLod_impl(
    thread metal::texture2d_array<T> &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float level)
{
    return texture.sample(sampler, coord.xy, uint32_t(metal::round(coord.z)), metal::level(level));
}
)",
                        textureLod())

PROGRAM_PRELUDE_DECLARE(textureLod_texture2darray_float4,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureLod_impl(
    thread metal::texture2d_array<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float level)
{
    return texture.sample(sampler, coord.xyz, uint32_t(metal::round(coord.w)), metal::level(level));
}
)",
                        textureLod())

PROGRAM_PRELUDE_DECLARE(textureLodOffset,
                        R"(
#define ANGLE_textureLodOffset(env, ...) ANGLE_textureLodOffset_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureLodOffset_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float2 const coord,
    float level,
    metal::int2 const offset)
{
    return texture.sample(sampler, coord, metal::level(level), offset);
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureLodOffset_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float level,
    metal::int3 const offset)
{
    return texture.sample(sampler, coord, metal::level(level), offset);
}

template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureLodOffset_impl(
    thread metal::depth2d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float level,
    int2 const offset)
{
    if (ANGLEUseSampleCompareLod)
    {
        return static_cast<T>(texture.sample_compare(sampler, coord.xy, coord.z, metal::level(level), offset));
    }
    else
    {
        return static_cast<T>(texture.sample(sampler, coord.xy, metal::level(level), offset) > coord.z);
    }
}

template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureLodOffset_impl(
    thread metal::texture2d_array<T> &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float level,
    metal::int2 const offset)
{
    return texture.sample(sampler, coord.xy, uint32_t(metal::round(coord.z)), metal::level(level), offset);
}

template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureLodOffset_impl(
    thread metal::texture2d_array<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float level,
    metal::int3 const offset)
{
    return texture.sample(sampler, coord.xyz, uint32_t(metal::round(coord.w)), metal::level(level), offset);
}
)",
                        functionConstants(),
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(textureOffset,
                        R"(
#define ANGLE_textureOffset(env, ...) ANGLE_textureOffset_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureOffset_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float2 const coord,
    metal::int2 const offset)
{
    return texture.sample(sampler, coord, offset);
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureOffset_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float2 const coord,
    metal::int2 const offset,
    float bias)
{
    return texture.sample(sampler, coord, metal::bias(bias), offset);
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureOffset_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    metal::int2 const offset)
{
    return texture.sample(sampler, coord.xy, uint32_t(metal::round(coord.z)), offset);
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureOffset_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    metal::int2 const offset,
    float bias)
{
    return texture.sample(sampler, coord.xy, uint32_t(metal::round(coord.z)), metal::bias(bias), offset);
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureOffset_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    metal::int3 const offset)
{
    return texture.sample(sampler, coord, offset);
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureOffset_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    metal::int3 const offset,
    float bias)
{
    return texture.sample(sampler, coord, metal::bias(bias), offset);
}

template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureOffset_impl(
    thread metal::depth2d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    metal::int2 const offset)
{
    return texture.sample_compare(sampler, coord.xy, coord.z, offset);
}

template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureOffset_impl(
    thread metal::depth2d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    metal::int2 const offset,
    float bias)
{
    return texture.sample_compare(sampler, coord.xy, coord.z, metal::bias(bias), offset);
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(textureProj,
                        R"(
#define ANGLE_textureProj(env, ...) ANGLE_textureProj_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProj_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float bias = 0)
{
    return texture.sample(sampler, coord.xy/coord.z, metal::bias(bias));
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProj_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float bias = 0)
{
    return texture.sample(sampler, coord.xy/coord.w, metal::bias(bias));
}

template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProj_impl(
    thread metal::texture3d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float bias = 0)
{
    return texture.sample(sampler, coord.xyz/coord.w, metal::bias(bias));
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(textureProjGrad,
                        R"(
#define ANGLE_textureProjGrad(env, ...) ANGLE_textureProjGrad_impl(*env.texture, *env.sampler, __VA_ARGS__)
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(textureProjGrad_generic_float3_float2_float2,
                        R"(
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjGrad_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    metal::float2 const dPdx,
    metal::float2 const dPdy)
{
    return texture.sample(sampler, coord.xy/coord.z, metal::gradient2d(dPdx, dPdy));
}
)",
                        textureProjGrad())

PROGRAM_PRELUDE_DECLARE(textureProjGrad_generic_float4_float2_float2,
                        R"(
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjGrad_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    metal::float2 const dPdx,
    metal::float2 const dPdy)
{
    return texture.sample(sampler, coord.xy/coord.w, metal::gradient2d(dPdx, dPdy));
}
)",
                        textureProjGrad())

PROGRAM_PRELUDE_DECLARE(textureProjGrad_depth2d_float4_float2_float2,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjGrad_impl(
    thread metal::depth2d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    metal::float2 const dPdx,
    metal::float2 const dPdy)
{
    if (ANGLEUseSampleCompareGradient)
    {
        return static_cast<T>(texture.sample_compare(sampler, coord.xy/coord.w, coord.z/coord.w, metal::gradient2d(dPdx, dPdy)));
    }
    else
    {
        return static_cast<T>(texture.sample(sampler, coord.xy/coord.w, metal::gradient2d(dPdx, dPdy)) > coord.z/coord.w);
    }
}
)",
                        functionConstants(),
                        textureProjGrad())

PROGRAM_PRELUDE_DECLARE(textureProjGrad_texture3d_float4_float3_float3,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjGrad_impl(
    thread metal::texture3d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    metal::float3 const dPdx,
    metal::float3 const dPdy)
{
    return texture.sample(sampler, coord.xyz/coord.w, metal::gradient3d(dPdx, dPdy));
}
)",
                        textureProjGrad())

PROGRAM_PRELUDE_DECLARE(textureProjGradOffset,
                        R"(
#define ANGLE_textureProjGradOffset(env, ...) ANGLE_textureProjGradOffset_impl(*env.texture, *env.sampler, __VA_ARGS__)
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(textureProjGradOffset_generic_float3_float2_float2_int2,
                        R"(
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjGradOffset_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    metal::float2 const dPdx,
    metal::float2 const dPdy,
    int2 const offset)
{
    return texture.sample(sampler, coord.xy/coord.z, metal::gradient2d(dPdx, dPdy), offset);
}
)",
                        textureProjGradOffset())

PROGRAM_PRELUDE_DECLARE(textureProjGradOffset_generic_float4_float2_float2_int2,
                        R"(
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjGradOffset_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    metal::float2 const dPdx,
    metal::float2 const dPdy,
    int2 const offset)
{
    return texture.sample(sampler, coord.xy/coord.w, metal::gradient2d(dPdx, dPdy), offset);
}
)",
                        textureProjGradOffset())

PROGRAM_PRELUDE_DECLARE(textureProjGradOffset_depth2d_float4_float2_float2_int2,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjGradOffset_impl(
    thread metal::depth2d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    metal::float2 const dPdx,
    metal::float2 const dPdy,
    int2 const offset)
{
    if (ANGLEUseSampleCompareGradient)
    {
        return static_cast<T>(texture.sample_compare(sampler, coord.xy/coord.w, coord.z/coord.w, metal::gradient2d(dPdx, dPdy), offset));
    }
    else
    {
        return static_cast<T>(texture.sample(sampler, coord.xy/coord.w, metal::gradient2d(dPdx, dPdy), offset) > coord.z/coord.w);
    }
}
)",
                        functionConstants(),
                        textureProjGradOffset())

PROGRAM_PRELUDE_DECLARE(textureProjGradOffset_texture3d_float4_float3_float3_int3,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjGradOffset_impl(
    thread metal::texture3d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    metal::float3 const dPdx,
    metal::float3 const dPdy,
    int3 const offset)
{
    return texture.sample(sampler, coord.xyz/coord.w, metal::gradient3d(dPdx, dPdy), offset);
}
)",
                        textureProjGradOffset())

PROGRAM_PRELUDE_DECLARE(textureProjLod,
                        R"(
#define ANGLE_textureProjLod(env, ...) ANGLE_textureProjLod_impl(*env.texture, *env.sampler, __VA_ARGS__)
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(textureProjLod_generic_float3,
                        R"(
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjLod_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float level)
{
    return texture.sample(sampler, coord.xy/coord.z, metal::level(level));
}
)",
                        textureProjLod())

PROGRAM_PRELUDE_DECLARE(textureProjLod_generic_float4,
                        R"(
template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjLod_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float level)
{
    return texture.sample(sampler, coord.xy/coord.w, metal::level(level));
}
)",
                        textureProjLod())

PROGRAM_PRELUDE_DECLARE(textureProjLod_depth2d_float4,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjLod_impl(
    thread metal::depth2d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float level)
{
    if (ANGLEUseSampleCompareLod)
    {
        return static_cast<T>(texture.sample_compare(sampler, coord.xy/coord.w, coord.z/coord.w, metal::level(level)));
    }
    else
    {
        return static_cast<T>(texture.sample(sampler, coord.xy/coord.w, metal::level(level)) > coord.z/coord.w);
    }
}
)",
                        functionConstants(),
                        textureProjLod())

PROGRAM_PRELUDE_DECLARE(textureProjLod_texture3d_float4,
                        R"(
template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjLod_impl(
    thread metal::texture3d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float level)
{
    return texture.sample(sampler, coord.xyz/coord.w, metal::level(level));
}
)",
                        textureProjLod())

PROGRAM_PRELUDE_DECLARE(textureProjLodOffset,
                        R"(
#define ANGLE_textureProjLodOffset(env, ...) ANGLE_textureProjLodOffset_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjLodOffset_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    float level,
    int2 const offset)
{
    return texture.sample(sampler, coord.xy/coord.z, metal::level(level), offset);
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjLodOffset_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float level,
    int2 const offset)
{
    return texture.sample(sampler, coord.xy/coord.w, metal::level(level), offset);
}

template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjLodOffset_impl(
    thread metal::depth2d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float level,
    int2 const offset)
{
    if (ANGLEUseSampleCompareLod)
    {
        return static_cast<T>(texture.sample_compare(sampler, coord.xy/coord.w, coord.z/coord.w, metal::level(level), offset));
    }
    else
    {
        return static_cast<T>(texture.sample(sampler, coord.xy/coord.w, metal::level(level), offset) > coord.z/coord.w);
    }
}

template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjLodOffset_impl(
    thread metal::texture3d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    float level,
    int3 const offset)
{
    return texture.sample(sampler, coord.xyz/coord.w, metal::level(level), offset);
}
)",
                        functionConstants(),
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(textureProjOffset,
                        R"(
#define ANGLE_textureProjOffset(env, ...) ANGLE_textureProjOffset_impl(*env.texture, *env.sampler, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjOffset_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float3 const coord,
    int2 const offset,
    float bias = 0)
{
    return texture.sample(sampler, coord.xy/coord.z, metal::bias(bias), offset);
}

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjOffset_impl(
    thread Texture &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    int2 const offset,
    float bias = 0)
{
    return texture.sample(sampler, coord.xy/coord.w, metal::bias(bias), offset);
}

template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureProjOffset_impl(
    thread metal::texture3d<T> &texture,
    thread metal::sampler const &sampler,
    metal::float4 const coord,
    int3 const offset,
    float bias = 0)
{
    return texture.sample(sampler, coord.xyz/coord.w, metal::bias(bias), offset);
}
)",
                        textureEnv())

PROGRAM_PRELUDE_DECLARE(textureSize,
                        R"(
#define ANGLE_textureSize(env, ...) ANGLE_textureSize_impl(*env.texture, __VA_ARGS__)

template <typename Texture>
ANGLE_ALWAYS_INLINE auto ANGLE_textureSize_impl(
    thread Texture &texture,
    int level)
{
    return int2(texture.get_width(uint32_t(level)), texture.get_height(uint32_t(level)));
}

template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureSize_impl(
    thread metal::texture3d<T> &texture,
    int level)
{
    return int3(texture.get_width(uint32_t(level)), texture.get_height(uint32_t(level)), texture.get_depth(uint32_t(level)));
}

template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureSize_impl(
    thread metal::depth2d_array<T> &texture,
    int level)
{
    return int3(texture.get_width(uint32_t(level)), texture.get_height(uint32_t(level)), texture.get_array_size());
}

template <typename T>
ANGLE_ALWAYS_INLINE auto ANGLE_textureSize_impl(
    thread metal::texture2d_array<T> &texture,
    int level)
{
    return int3(texture.get_width(uint32_t(level)), texture.get_height(uint32_t(level)), texture.get_array_size());
}
)",
                        textureEnv())

////////////////////////////////////////////////////////////////////////////////

// Returned Name is valid for as long as `buffer` is still alive.
// Returns false if no template args exist.
// Returns false if buffer is not large enough.
//
// Example:
//  "foo<1,2>" --> "foo<>"
static std::pair<Name, bool> MaskTemplateArgs(const Name &name, size_t bufferSize, char *buffer)
{
    const char *begin = name.rawName().data();
    const char *end   = strchr(begin, '<');
    if (!end)
    {
        return {{}, false};
    }
    size_t n = end - begin;
    if (n + 3 > bufferSize)
    {
        return {{}, false};
    }
    for (size_t i = 0; i < n; ++i)
    {
        buffer[i] = begin[i];
    }
    buffer[n + 0] = '<';
    buffer[n + 1] = '>';
    buffer[n + 2] = '\0';
    return {Name(buffer, name.symbolType()), true};
}

ProgramPrelude::FuncToEmitter ProgramPrelude::BuildFuncToEmitter()
{
#define EMIT_METHOD(method) \
    [](ProgramPrelude &pp, const TFunction &) -> void { return pp.method(); }
    FuncToEmitter map;

    auto put = [&](Name name, FuncEmitter emitter) {
        FuncEmitter &dest = map[name];
        ASSERT(!dest);
        dest = emitter;
    };

    auto putAngle = [&](const char *nameStr, FuncEmitter emitter) {
        Name name(nameStr, SymbolType::AngleInternal);
        put(name, emitter);
    };

    auto putBuiltIn = [&](const char *nameStr, FuncEmitter emitter) {
        Name name(nameStr, SymbolType::BuiltIn);
        put(name, emitter);
    };

    putAngle("addressof", EMIT_METHOD(addressof));
    putAngle("cast<>", EMIT_METHOD(castMatrix));
    putAngle("elem_ref", EMIT_METHOD(vectorElemRef));
    putAngle("flatten", EMIT_METHOD(flattenArray));
    putAngle("inout", EMIT_METHOD(inout));
    putAngle("out", EMIT_METHOD(out));
    putAngle("swizzle_ref", EMIT_METHOD(swizzleRef));

    putBuiltIn("texelFetch", EMIT_METHOD(texelFetch));
    putBuiltIn("texelFetchOffset", EMIT_METHOD(texelFetchOffset));
    putBuiltIn("texture", [](ProgramPrelude &pp, const TFunction &func) {
        const ImmutableString textureName =
            GetTextureTypeName(func.getParam(0)->getType().getBasicType()).rawName();
        const TType &coord          = func.getParam(1)->getType();
        const TBasicType coordBasic = coord.getBasicType();
        const uint8_t coordN        = coord.getNominalSize();
        const bool bias             = func.getParamCount() >= 3;
        if (textureName.beginsWith("metal::depth2d<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 3)
            {
                if (bias)
                {
                    return pp.texture_depth2d_float3_float();
                }
                return pp.texture_depth2d_float3();
            }
        }
        if (textureName.beginsWith("metal::depthcube<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 4)
            {
                if (bias)
                {
                    return pp.texture_depthcube_float4_float();
                }
                return pp.texture_depthcube_float4();
            }
        }
        if (textureName.beginsWith("metal::depth2d_array<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 4)
            {
                if (bias)
                {
                    return pp.texture_depth2darray_float4_float();
                }
                return pp.texture_depth2darray_float4();
            }
        }
        if (textureName.beginsWith("metal::texture2d_array<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 3)
            {
                if (bias)
                {
                    return pp.texture_texture2darray_float3_float();
                }
                return pp.texture_texture2darray_float3();
            }
            if (coordBasic == TBasicType::EbtFloat && coordN == 4)
            {
                if (bias)
                {
                    return pp.texture_texture2darray_float4_float();
                }
                return pp.texture_texture2darray_float4();
            }
        }
        if (coordBasic == TBasicType::EbtFloat && coordN == 2)
        {
            if (bias)
            {
                return pp.texture_generic_float2_float();
            }
            return pp.texture_generic_float2();
        }
        if (coordBasic == TBasicType::EbtFloat && coordN == 3)
        {
            if (bias)
            {
                return pp.texture_generic_float3_float();
            }
            return pp.texture_generic_float3();
        }
        UNIMPLEMENTED();
    });
    putBuiltIn("texture1DLod", EMIT_METHOD(texture1DLod));
    putBuiltIn("texture1DProj", EMIT_METHOD(texture1DProj));
    putBuiltIn("texture1DProjLod", EMIT_METHOD(texture1DProjLod));
    putBuiltIn("texture2D", EMIT_METHOD(texture2D));
    putBuiltIn("texture2DGradEXT", EMIT_METHOD(texture2DGradEXT));
    putBuiltIn("texture2DLod", EMIT_METHOD(texture2DLod));
    putBuiltIn("texture2DLodEXT", EMIT_METHOD(texture2DLodEXT));
    putBuiltIn("texture2DProj", EMIT_METHOD(texture2DProj));
    putBuiltIn("texture2DProjGradEXT", EMIT_METHOD(texture2DProjGradEXT));
    putBuiltIn("texture2DProjLod", EMIT_METHOD(texture2DProjLod));
    putBuiltIn("texture2DProjLodEXT", EMIT_METHOD(texture2DProjLodEXT));
    putBuiltIn("texture2DRect", EMIT_METHOD(texture2DRect));
    putBuiltIn("texture2DRectProj", EMIT_METHOD(texture2DRectProj));
    putBuiltIn("texture3DLod", EMIT_METHOD(texture3DLod));
    putBuiltIn("texture3DProj", EMIT_METHOD(texture3DProj));
    putBuiltIn("texture3DProjLod", EMIT_METHOD(texture3DProjLod));
    putBuiltIn("textureCube", EMIT_METHOD(textureCube));
    putBuiltIn("textureCubeGradEXT", EMIT_METHOD(textureCubeGradEXT));
    putBuiltIn("textureCubeLod", EMIT_METHOD(textureCubeLod));
    putBuiltIn("textureCubeLodEXT", EMIT_METHOD(textureCubeLodEXT));
    putBuiltIn("textureCubeProj", EMIT_METHOD(textureCubeProj));
    putBuiltIn("textureCubeProjLod", EMIT_METHOD(textureCubeProjLod));
    putBuiltIn("textureGrad", [](ProgramPrelude &pp, const TFunction &func) {
        const ImmutableString textureName =
            GetTextureTypeName(func.getParam(0)->getType().getBasicType()).rawName();
        const TType &coord          = func.getParam(1)->getType();
        const TBasicType coordBasic = coord.getBasicType();
        const uint8_t coordN        = coord.getNominalSize();
        const TType &dPdx           = func.getParam(2)->getType();
        const uint8_t dPdxN         = dPdx.getNominalSize();
        if (textureName.beginsWith("metal::depth2d<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 3 && dPdxN == 2)
            {
                return pp.textureGrad_depth2d_float3_float2_float2();
            }
        }
        if (textureName.beginsWith("metal::depth2d_array<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 4 && dPdxN == 2)
            {
                return pp.textureGrad_depth2darray_float4_float2_float2();
            }
        }
        if (textureName.beginsWith("metal::depthcube<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 4 && dPdxN == 3)
            {
                return pp.textureGrad_depthcube_float4_float3_float3();
            }
        }
        if (textureName.beginsWith("metal::texturecube<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 3 && dPdxN == 3)
            {
                return pp.textureGrad_texturecube_float3_float3_float3();
            }
        }
        if (coordBasic == TBasicType::EbtFloat && coordN == 3 && dPdxN == 2)
        {
            return pp.textureGrad_generic_float3_float2_float2();
        }
        if (coordBasic == TBasicType::EbtFloat && coordN == 4 && dPdxN == 2)
        {
            return pp.textureGrad_generic_float4_float2_float2();
        }
        if (coordBasic == TBasicType::EbtFloat && coordN == dPdxN)
        {
            return pp.textureGrad_generic_floatN_floatN_floatN();
        }
        UNIMPLEMENTED();
    });
    putBuiltIn("textureGradOffset", [](ProgramPrelude &pp, const TFunction &func) {
        const ImmutableString textureName =
            GetTextureTypeName(func.getParam(0)->getType().getBasicType()).rawName();
        const TType &coord          = func.getParam(1)->getType();
        const TBasicType coordBasic = coord.getBasicType();
        const uint8_t coordN        = coord.getNominalSize();
        const TType &dPdx           = func.getParam(2)->getType();
        const uint8_t dPdxN         = dPdx.getNominalSize();
        if (textureName.beginsWith("metal::depth2d<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 3 && dPdxN == 2)
            {
                return pp.textureGradOffset_depth2d_float3_float2_float2_int2();
            }
        }
        if (textureName.beginsWith("metal::depth2d_array<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 4 && dPdxN == 2)
            {
                return pp.textureGradOffset_depth2darray_float4_float2_float2_int2();
            }
        }
        if (textureName.beginsWith("metal::depthcube<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 4 && dPdxN == 3)
            {
                return pp.textureGradOffset_depthcube_float4_float3_float3_int3();
            }
        }
        if (textureName.beginsWith("metal::texturecube<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 3 && dPdxN == 3)
            {
                return pp.textureGradOffset_texturecube_float3_float3_float3_int3();
            }
        }
        if (coordBasic == TBasicType::EbtFloat && coordN == 3 && dPdxN == 2)
        {
            return pp.textureGradOffset_generic_float3_float2_float2_int2();
        }
        if (coordBasic == TBasicType::EbtFloat && coordN == 4 && dPdxN == 2)
        {
            return pp.textureGradOffset_generic_float4_float2_float2_int2();
        }
        if (coordBasic == TBasicType::EbtFloat && coordN == dPdxN)
        {
            return pp.textureGradOffset_generic_floatN_floatN_floatN_intN();
        }
        UNIMPLEMENTED();
    });
    putBuiltIn("textureLod", [](ProgramPrelude &pp, const TFunction &func) {
        const ImmutableString textureName =
            GetTextureTypeName(func.getParam(0)->getType().getBasicType()).rawName();
        const TType &coord          = func.getParam(1)->getType();
        const TBasicType coordBasic = coord.getBasicType();
        const uint8_t coordN        = coord.getNominalSize();
        if (textureName.beginsWith("metal::depth2d<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 3)
            {
                return pp.textureLod_depth2d_float3();
            }
        }
        if (textureName.beginsWith("metal::texture2d_array<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 3)
            {
                return pp.textureLod_texture2darray_float3();
            }
            if (coordBasic == TBasicType::EbtFloat && coordN == 4)
            {
                return pp.textureLod_texture2darray_float4();
            }
        }
        if (coordBasic == TBasicType::EbtFloat && coordN == 2)
        {
            return pp.textureLod_generic_float2();
        }
        if (coordBasic == TBasicType::EbtFloat && coordN == 3)
        {
            return pp.textureLod_generic_float3();
        }
        UNIMPLEMENTED();
    });
    putBuiltIn("textureLodOffset", EMIT_METHOD(textureLodOffset));
    putBuiltIn("textureOffset", EMIT_METHOD(textureOffset));
    putBuiltIn("textureProj", EMIT_METHOD(textureProj));
    putBuiltIn("textureProjGrad", [](ProgramPrelude &pp, const TFunction &func) {
        const ImmutableString textureName =
            GetTextureTypeName(func.getParam(0)->getType().getBasicType()).rawName();
        const TType &coord          = func.getParam(1)->getType();
        const TBasicType coordBasic = coord.getBasicType();
        const uint8_t coordN        = coord.getNominalSize();
        const TType &dPdx           = func.getParam(2)->getType();
        const uint8_t dPdxN         = dPdx.getNominalSize();
        if (textureName.beginsWith("metal::depth2d<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 4 && dPdxN == 2)
            {
                return pp.textureProjGrad_depth2d_float4_float2_float2();
            }
        }
        if (textureName.beginsWith("metal::texture3d<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 4 && dPdxN == 3)
            {
                return pp.textureProjGrad_texture3d_float4_float3_float3();
            }
        }
        if (coordBasic == TBasicType::EbtFloat && coordN == 3 && dPdxN == 2)
        {
            return pp.textureProjGrad_generic_float3_float2_float2();
        }
        if (coordBasic == TBasicType::EbtFloat && coordN == 4 && dPdxN == 2)
        {
            return pp.textureProjGrad_generic_float4_float2_float2();
        }
        UNIMPLEMENTED();
    });
    putBuiltIn("textureProjGradOffset", [](ProgramPrelude &pp, const TFunction &func) {
        const ImmutableString textureName =
            GetTextureTypeName(func.getParam(0)->getType().getBasicType()).rawName();
        const TType &coord          = func.getParam(1)->getType();
        const TBasicType coordBasic = coord.getBasicType();
        const uint8_t coordN        = coord.getNominalSize();
        const TType &dPdx           = func.getParam(2)->getType();
        const uint8_t dPdxN         = dPdx.getNominalSize();
        if (textureName.beginsWith("metal::depth2d<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 4 && dPdxN == 2)
            {
                return pp.textureProjGradOffset_depth2d_float4_float2_float2_int2();
            }
        }
        if (textureName.beginsWith("metal::texture3d<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 4 && dPdxN == 3)
            {
                return pp.textureProjGradOffset_texture3d_float4_float3_float3_int3();
            }
        }
        if (coordBasic == TBasicType::EbtFloat && coordN == 3 && dPdxN == 2)
        {
            return pp.textureProjGradOffset_generic_float3_float2_float2_int2();
        }
        if (coordBasic == TBasicType::EbtFloat && coordN == 4 && dPdxN == 2)
        {
            return pp.textureProjGradOffset_generic_float4_float2_float2_int2();
        }
        UNIMPLEMENTED();
    });
    putBuiltIn("textureProjLod", [](ProgramPrelude &pp, const TFunction &func) {
        const ImmutableString textureName =
            GetTextureTypeName(func.getParam(0)->getType().getBasicType()).rawName();
        const TType &coord          = func.getParam(1)->getType();
        const TBasicType coordBasic = coord.getBasicType();
        const uint8_t coordN        = coord.getNominalSize();
        if (textureName.beginsWith("metal::depth2d<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 4)
            {
                return pp.textureProjLod_depth2d_float4();
            }
        }
        if (textureName.beginsWith("metal::texture3d<"))
        {
            if (coordBasic == TBasicType::EbtFloat && coordN == 4)
            {
                return pp.textureProjLod_texture3d_float4();
            }
        }
        if (coordBasic == TBasicType::EbtFloat && coordN == 3)
        {
            return pp.textureProjLod_generic_float3();
        }
        if (coordBasic == TBasicType::EbtFloat && coordN == 4)
        {
            return pp.textureProjLod_generic_float4();
        }
        UNIMPLEMENTED();
    });
    putBuiltIn("textureProjLodOffset", EMIT_METHOD(textureProjLodOffset));
    putBuiltIn("textureProjOffset", EMIT_METHOD(textureProjOffset));
    putBuiltIn("textureSize", EMIT_METHOD(textureSize));

    return map;

#undef EMIT_METHOD
}

void ProgramPrelude::visitOperator(TOperator op, const TFunction *func, const TType *argType0)
{
    visitOperator(op, func, argType0, nullptr, nullptr);
}

void ProgramPrelude::visitOperator(TOperator op,
                                   const TFunction *func,
                                   const TType *argType0,
                                   const TType *argType1)
{
    visitOperator(op, func, argType0, argType1, nullptr);
}
void ProgramPrelude::visitOperator(TOperator op,
                                   const TFunction *func,
                                   const TType *argType0,
                                   const TType *argType1,
                                   const TType *argType2)
{
    switch (op)
    {
        case TOperator::EOpRadians:
            radians();
            break;
        case TOperator::EOpDegrees:
            degrees();
            break;
        case TOperator::EOpAtan:
            atan();
            break;
        case TOperator::EOpMod:
            mod();
            break;
        case TOperator::EOpRefract:
            refract();
            break;
        case TOperator::EOpDistance:
            distance();
            break;
        case TOperator::EOpLength:
            length();
            break;
        case TOperator::EOpDot:
            dot();
            break;
        case TOperator::EOpNormalize:
            normalize();
            break;
        case TOperator::EOpFaceforward:
            faceforward();
            break;
        case TOperator::EOpReflect:
            reflect();
            break;

        case TOperator::EOpSin:
        case TOperator::EOpCos:
        case TOperator::EOpTan:
        case TOperator::EOpAsin:
        case TOperator::EOpAcos:
        case TOperator::EOpSinh:
        case TOperator::EOpCosh:
        case TOperator::EOpTanh:
        case TOperator::EOpAsinh:
        case TOperator::EOpAcosh:
        case TOperator::EOpAtanh:
        case TOperator::EOpAbs:
        case TOperator::EOpFma:
        case TOperator::EOpPow:
        case TOperator::EOpExp:
        case TOperator::EOpExp2:
        case TOperator::EOpLog:
        case TOperator::EOpLog2:
        case TOperator::EOpSqrt:
        case TOperator::EOpFloor:
        case TOperator::EOpTrunc:
        case TOperator::EOpCeil:
        case TOperator::EOpFract:
        case TOperator::EOpRound:
        case TOperator::EOpRoundEven:
        case TOperator::EOpModf:
        case TOperator::EOpLdexp:
        case TOperator::EOpFrexp:
        case TOperator::EOpInversesqrt:
            include_metal_math();
            break;

        case TOperator::EOpEqual:
            if (argType0->isVector() && argType1->isVector())
            {
                equalVector();
            }
            // Even if Arg0 is a vector or matrix, it could also be an array.
            if (argType0->isArray() && argType1->isArray())
            {
                equalArray();
            }
            if (argType0->getStruct() && argType1->getStruct() && argType0->isArray() &&
                argType1->isArray())
            {
                equalStructArray();
            }
            if (argType0->isMatrix() && argType1->isMatrix())
            {
                equalMatrix();
            }
            break;

        case TOperator::EOpNotEqual:
            if (argType0->isVector() && argType1->isVector())
            {
                notEqualVector();
            }
            else if (argType0->getStruct() && argType1->getStruct())
            {
                notEqualStruct();
            }
            // Same as above.
            if (argType0->isArray() && argType1->isArray())
            {
                notEqualArray();
            }
            if (argType0->getStruct() && argType1->getStruct() && argType0->isArray() &&
                argType1->isArray())
            {
                notEqualStructArray();
            }
            if (argType0->isMatrix() && argType1->isMatrix())
            {
                notEqualMatrix();
            }
            break;

        case TOperator::EOpCross:
            include_metal_geometric();
            break;

        case TOperator::EOpSign:
            sign();
            break;

        case TOperator::EOpClamp:
        case TOperator::EOpMin:
        case TOperator::EOpMax:
        case TOperator::EOpStep:
        case TOperator::EOpSmoothstep:
            include_metal_common();
            break;
        case TOperator::EOpMix:
            include_metal_common();
            if (argType2->getBasicType() == TBasicType::EbtBool)
            {
                mixBool();
            }
            break;

        case TOperator::EOpAll:
        case TOperator::EOpAny:
        case TOperator::EOpIsnan:
        case TOperator::EOpIsinf:
            include_metal_relational();
            break;

        case TOperator::EOpDFdx:
        case TOperator::EOpDFdy:
        case TOperator::EOpFwidth:
            include_metal_graphics();
            break;

        case TOperator::EOpTranspose:
        case TOperator::EOpDeterminant:
            include_metal_matrix();
            break;

        case TOperator::EOpAdd:
            if (argType0->isMatrix() && argType1->isScalar())
            {
                addMatrixScalar();
            }
            break;

        case TOperator::EOpAddAssign:
            if (argType0->isMatrix() && argType1->isScalar())
            {
                addMatrixScalarAssign();
            }
            break;

        case TOperator::EOpSub:
            if (argType0->isMatrix() && argType1->isScalar())
            {
                subMatrixScalar();
            }
            break;

        case TOperator::EOpSubAssign:
            if (argType0->isMatrix() && argType1->isScalar())
            {
                subMatrixScalarAssign();
            }
            break;

        case TOperator::EOpDiv:
            if (argType0->isMatrix())
            {
                if (argType1->isMatrix())
                {
                    componentWiseDivide();
                }
                else if (argType1->isScalar())
                {
                    divMatrixScalar();
                }
            }
            break;

        case TOperator::EOpDivAssign:
            if (argType0->isMatrix())
            {
                if (argType1->isMatrix())
                {
                    componentWiseDivideAssign();
                }
                else if (argType1->isScalar())
                {
                    divMatrixScalarAssign();
                }
            }
            break;

        case TOperator::EOpMatrixCompMult:
            if (argType0->isMatrix() && argType1->isMatrix())
            {
                componentWiseMultiply();
            }
            break;

        case TOperator::EOpOuterProduct:
            outerProduct();
            break;

        case TOperator::EOpInverse:
            switch (argType0->getCols())
            {
                case 2:
                    inverse2();
                    break;
                case 3:
                    inverse3();
                    break;
                case 4:
                    inverse4();
                    break;
                default:
                    UNREACHABLE();
            }
            break;

        case TOperator::EOpMatrixTimesMatrixAssign:
            matmulAssign();
            break;

        case TOperator::EOpPreIncrement:
            if (argType0->isMatrix())
            {
                preIncrementMatrix();
            }
            break;

        case TOperator::EOpPostIncrement:
            if (argType0->isMatrix())
            {
                postIncrementMatrix();
            }
            break;

        case TOperator::EOpPreDecrement:
            if (argType0->isMatrix())
            {
                preDecrementMatrix();
            }
            break;

        case TOperator::EOpPostDecrement:
            if (argType0->isMatrix())
            {
                postDecrementMatrix();
            }
            break;

        case TOperator::EOpNegative:
            if (argType0->isMatrix())
            {
                negateMatrix();
            }
            break;

        case TOperator::EOpComma:
        case TOperator::EOpAssign:
        case TOperator::EOpInitialize:
        case TOperator::EOpMulAssign:
        case TOperator::EOpIModAssign:
        case TOperator::EOpBitShiftLeftAssign:
        case TOperator::EOpBitShiftRightAssign:
        case TOperator::EOpBitwiseAndAssign:
        case TOperator::EOpBitwiseXorAssign:
        case TOperator::EOpBitwiseOrAssign:
        case TOperator::EOpMul:
        case TOperator::EOpIMod:
        case TOperator::EOpBitShiftLeft:
        case TOperator::EOpBitShiftRight:
        case TOperator::EOpBitwiseAnd:
        case TOperator::EOpBitwiseXor:
        case TOperator::EOpBitwiseOr:
        case TOperator::EOpLessThan:
        case TOperator::EOpGreaterThan:
        case TOperator::EOpLessThanEqual:
        case TOperator::EOpGreaterThanEqual:
        case TOperator::EOpLessThanComponentWise:
        case TOperator::EOpLessThanEqualComponentWise:
        case TOperator::EOpGreaterThanEqualComponentWise:
        case TOperator::EOpGreaterThanComponentWise:
        case TOperator::EOpLogicalOr:
        case TOperator::EOpLogicalXor:
        case TOperator::EOpLogicalAnd:
        case TOperator::EOpPositive:
        case TOperator::EOpLogicalNot:
        case TOperator::EOpNotComponentWise:
        case TOperator::EOpBitwiseNot:
        case TOperator::EOpVectorTimesScalarAssign:
        case TOperator::EOpVectorTimesMatrixAssign:
        case TOperator::EOpMatrixTimesScalarAssign:
        case TOperator::EOpVectorTimesScalar:
        case TOperator::EOpVectorTimesMatrix:
        case TOperator::EOpMatrixTimesVector:
        case TOperator::EOpMatrixTimesScalar:
        case TOperator::EOpMatrixTimesMatrix:
        case TOperator::EOpReturn:
        case TOperator::EOpBreak:
        case TOperator::EOpContinue:
        case TOperator::EOpEqualComponentWise:
        case TOperator::EOpNotEqualComponentWise:
        case TOperator::EOpIndexDirect:
        case TOperator::EOpIndexIndirect:
        case TOperator::EOpIndexDirectStruct:
        case TOperator::EOpIndexDirectInterfaceBlock:
        case TOperator::EOpFloatBitsToInt:
        case TOperator::EOpIntBitsToFloat:
        case TOperator::EOpUintBitsToFloat:
        case TOperator::EOpFloatBitsToUint:
        case TOperator::EOpNull:
            // do nothing
            break;

        case TOperator::EOpKill:
            include_metal_graphics();
            break;

        case TOperator::EOpPackUnorm2x16:
        case TOperator::EOpPackSnorm2x16:
        case TOperator::EOpPackUnorm4x8:
        case TOperator::EOpPackSnorm4x8:
        case TOperator::EOpUnpackSnorm2x16:
        case TOperator::EOpUnpackUnorm2x16:
        case TOperator::EOpUnpackUnorm4x8:
        case TOperator::EOpUnpackSnorm4x8:
            include_metal_pack();
            break;

        case TOperator::EOpPackHalf2x16:
            pack_half_2x16();
            break;
        case TOperator::EOpUnpackHalf2x16:
            unpack_half_2x16();
            break;

        case TOperator::EOpBitfieldExtract:
        case TOperator::EOpBitfieldInsert:
        case TOperator::EOpBitfieldReverse:
        case TOperator::EOpBitCount:
        case TOperator::EOpFindLSB:
        case TOperator::EOpFindMSB:
        case TOperator::EOpUaddCarry:
        case TOperator::EOpUsubBorrow:
        case TOperator::EOpUmulExtended:
        case TOperator::EOpImulExtended:
        case TOperator::EOpBarrier:
        case TOperator::EOpMemoryBarrier:
        case TOperator::EOpMemoryBarrierAtomicCounter:
        case TOperator::EOpMemoryBarrierBuffer:
        case TOperator::EOpMemoryBarrierImage:
        case TOperator::EOpMemoryBarrierShared:
        case TOperator::EOpGroupMemoryBarrier:
        case TOperator::EOpAtomicAdd:
        case TOperator::EOpAtomicMin:
        case TOperator::EOpAtomicMax:
        case TOperator::EOpAtomicAnd:
        case TOperator::EOpAtomicOr:
        case TOperator::EOpAtomicXor:
        case TOperator::EOpAtomicExchange:
        case TOperator::EOpAtomicCompSwap:
        case TOperator::EOpEmitVertex:
        case TOperator::EOpEndPrimitive:
        case TOperator::EOpFtransform:
        case TOperator::EOpPackDouble2x32:
        case TOperator::EOpUnpackDouble2x32:
        case TOperator::EOpArrayLength:
            UNIMPLEMENTED();
            break;

        case TOperator::EOpConstruct:
            ASSERT(!func);
            break;

        case TOperator::EOpCallFunctionInAST:
        case TOperator::EOpCallInternalRawFunction:
        default:
            ASSERT(func);
            if (mHandled.insert(func).second)
            {
                const Name name(*func);
                const auto end = mFuncToEmitter.end();
                auto iter      = mFuncToEmitter.find(name);
                if (iter == end)
                {
                    char buffer[32];
                    auto mask = MaskTemplateArgs(name, sizeof(buffer), buffer);
                    if (mask.second)
                    {
                        iter = mFuncToEmitter.find(mask.first);
                    }
                }
                if (iter != end)
                {
                    const auto &emitter = iter->second;
                    emitter(*this, *func);
                }
            }
            break;
    }
}

void ProgramPrelude::visitVariable(const Name &name, const TType &type)
{
    if (const TStructure *s = type.getStruct())
    {
        const Name typeName(*s);
        if (typeName.beginsWith(Name("TextureEnv<")))
        {
            textureEnv();
        }
    }
    else
    {
        if (name.rawName() == sh::mtl::kRasterizerDiscardEnabledConstName ||
            name.rawName() == sh::mtl::kDepthWriteEnabledConstName)
        {
            functionConstants();
        }
    }
}

void ProgramPrelude::visitVariable(const TVariable &var)
{
    if (mHandled.insert(&var).second)
    {
        visitVariable(Name(var), var.getType());
    }
}

void ProgramPrelude::visitStructure(const TStructure &s)
{
    if (mHandled.insert(&s).second)
    {
        for (const TField *field : s.fields())
        {
            const TType &type = *field->type();
            visitVariable(Name(*field), type);
        }
    }
}

bool ProgramPrelude::visitBinary(Visit visit, TIntermBinary *node)
{
    const TType &leftType  = node->getLeft()->getType();
    const TType &rightType = node->getRight()->getType();
    visitOperator(node->getOp(), nullptr, &leftType, &rightType);
    return true;
}

bool ProgramPrelude::visitUnary(Visit visit, TIntermUnary *node)
{
    const TType &argType = node->getOperand()->getType();
    visitOperator(node->getOp(), nullptr, &argType);
    return true;
}

bool ProgramPrelude::visitAggregate(Visit visit, TIntermAggregate *node)
{
    const size_t argCount = node->getChildCount();

    auto getArgType = [node, argCount](size_t index) -> const TType & {
        ASSERT(index < argCount);
        TIntermTyped *arg = node->getChildNode(index)->getAsTyped();
        ASSERT(arg);
        return arg->getType();
    };

    const TFunction *func = node->getFunction();

    switch (node->getChildCount())
    {
        case 0:
        {
            visitOperator(node->getOp(), func, nullptr);
        }
        break;

        case 1:
        {
            const TType &argType0 = getArgType(0);
            visitOperator(node->getOp(), func, &argType0);
        }
        break;

        case 2:
        {
            const TType &argType0 = getArgType(0);
            const TType &argType1 = getArgType(1);
            visitOperator(node->getOp(), func, &argType0, &argType1);
        }
        break;

        case 3:
        {
            const TType &argType0 = getArgType(0);
            const TType &argType1 = getArgType(1);
            const TType &argType2 = getArgType(2);
            visitOperator(node->getOp(), func, &argType0, &argType1, &argType2);
        }
        break;

        default:
        {
            const TType &argType0 = getArgType(0);
            const TType &argType1 = getArgType(1);
            visitOperator(node->getOp(), func, &argType0, &argType1);
        }
        break;
    }

    return true;
}

bool ProgramPrelude::visitDeclaration(Visit, TIntermDeclaration *node)
{
    Declaration decl  = ViewDeclaration(*node);
    const TType &type = decl.symbol.getType();
    if (type.isStructSpecifier())
    {
        const TStructure *s = type.getStruct();
        ASSERT(s);
        visitStructure(*s);
    }
    return true;
}

void ProgramPrelude::visitSymbol(TIntermSymbol *node)
{
    visitVariable(node->variable());
}

bool sh::EmitProgramPrelude(TIntermBlock &root, TInfoSinkBase &out, const ProgramPreludeConfig &ppc)
{
    ProgramPrelude programPrelude(out, ppc);
    root.traverse(&programPrelude);
    return true;
}
