/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
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
 * \brief Shader utilities.
 *//*--------------------------------------------------------------------*/

#include "gluShaderUtil.hpp"
#include "glwEnums.hpp"
#include "deArrayUtil.hpp"

#include <fstream>

namespace glu
{

// ShadingLanguageVersion

const char *getGLSLVersionName(GLSLVersion version)
{
    static const char *s_names[] = {
        "GLSL ES 1.0", "GLSL ES 3.0", "GLSL ES 3.1", "GLSL ES 3.2", "GLSL 1.3", "GLSL 1.4", "GLSL 1.5", "GLSL 3.3",
        "GLSL 4.0",    "GLSL 4.1",    "GLSL 4.2",    "GLSL 4.3",    "GLSL 4.4", "GLSL 4.5", "GLSL 4.6",
    };

    return de::getSizedArrayElement<GLSL_VERSION_LAST>(s_names, version);
}

const char *getGLSLVersionDeclaration(GLSLVersion version)
{
    static const char *s_decl[] = {
        "#version 100", "#version 300 es", "#version 310 es", "#version 320 es", "#version 130",
        "#version 140", "#version 150",    "#version 330",    "#version 400",    "#version 410",
        "#version 420", "#version 430",    "#version 440",    "#version 450",    "#version 460",
    };

    return de::getSizedArrayElement<GLSL_VERSION_LAST>(s_decl, version);
}

bool glslVersionUsesInOutQualifiers(GLSLVersion version)
{
    return de::inRange<int>(version, GLSL_VERSION_300_ES, GLSL_VERSION_320_ES) ||
           de::inRange<int>(version, GLSL_VERSION_330, GLSL_VERSION_460);
}

bool glslVersionIsES(GLSLVersion version)
{
    DE_STATIC_ASSERT(GLSL_VERSION_LAST == 15);
    DE_ASSERT(version != GLSL_VERSION_LAST);

    if (version == GLSL_VERSION_100_ES || version == GLSL_VERSION_300_ES || version == GLSL_VERSION_310_ES ||
        version == GLSL_VERSION_320_ES)
        return true;
    else
        return false;
}

// \todo [2014-10-06 pyry] Export this.
static ApiType getMinAPIForGLSLVersion(GLSLVersion version)
{
    static const ApiType s_minApi[] = {
        ApiType::es(2, 0),   ApiType::es(3, 0),   ApiType::es(3, 1),   ApiType::es(3, 2),   ApiType::core(3, 0),
        ApiType::core(3, 1), ApiType::core(3, 2), ApiType::core(3, 3), ApiType::core(4, 0), ApiType::core(4, 1),
        ApiType::core(4, 2), ApiType::core(4, 3), ApiType::core(4, 4), ApiType::core(4, 5), ApiType::core(4, 6),
    };

    return de::getSizedArrayElement<GLSL_VERSION_LAST>(s_minApi, version);
}

bool isGLSLVersionSupported(ContextType type, GLSLVersion version)
{
    return contextSupports(type, getMinAPIForGLSLVersion(version));
}

GLSLVersion getContextTypeGLSLVersion(ContextType type)
{
    // \note From newer to older
    for (int version = GLSL_VERSION_LAST - 1; version >= 0; version--)
    {
        if (isGLSLVersionSupported(type, GLSLVersion(version)))
            return GLSLVersion(version);
    }

    DE_ASSERT(false);
    return GLSL_VERSION_LAST;
}

// ShaderType

const char *getShaderTypeName(ShaderType shaderType)
{
    static const char *s_names[] = {
        "vertex",  "fragment",    "geometry", "tess_control", "tess_eval", "compute", "ray_gen",
        "any_hit", "closest_hit", "miss",     "intersection", "callable",  "task",    "mesh",
    };

    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_names) == SHADERTYPE_LAST);
    DE_ASSERT(deInBounds32((int)shaderType, 0, SHADERTYPE_LAST));
    return s_names[(int)shaderType];
}

std::string getShaderTypePostfix(ShaderType shaderType)
{
    return "_" + std::string(getShaderTypeName(shaderType));
}

// Precision

const char *getPrecisionName(Precision precision)
{
    static const char *s_names[] = {"lowp", "mediump", "highp"};

    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_names) == PRECISION_LAST);
    DE_ASSERT(deInBounds32((int)precision, 0, PRECISION_LAST));
    return s_names[(int)precision];
}

std::string getPrecisionPostfix(Precision precision)
{
    return "_" + std::string(getPrecisionName(precision));
}

// DataType

const char *getDataTypeName(DataType dataType)
{
    static const char *s_names[] = {
        "invalid",
        "float",
        "vec2",
        "vec3",
        "vec4",
        "vector<float, 5>",
        "mat2",
        "mat2x3",
        "mat2x4",
        "mat3x2",
        "mat3",
        "mat3x4",
        "mat4x2",
        "mat4x3",
        "mat4",
        "double",
        "dvec2",
        "dvec3",
        "dvec4",
        "vector<double, 5>",
        "dmat2",
        "dmat2x3",
        "dmat2x4",
        "dmat3x2",
        "dmat3",
        "dmat3x4",
        "dmat4x2",
        "dmat4x3",
        "dmat4",
        "int",
        "ivec2",
        "ivec3",
        "ivec4",
        "vector<int, 5>",
        "uint",
        "uvec2",
        "uvec3",
        "uvec4",
        "vector<uint, 5>",
        "bool",
        "bvec2",
        "bvec3",
        "bvec4",
        "vector<bool, 5>",
        "sampler1D",
        "sampler2D",
        "samplerCube",
        "sampler1DArray",
        "sampler2DArray",
        "sampler3D",
        "samplerCubeArray",
        "sampler1DShadow",
        "sampler2DShadow",
        "samplerCubeShadow",
        "sampler1DArrayShadow",
        "sampler2DArrayShadow",
        "samplerCubeArrayShadow",
        "isampler1D",
        "isampler2D",
        "isamplerCube",
        "isampler1DArray",
        "isampler2DArray",
        "isampler3D",
        "isamplerCubeArray",
        "usampler1D",
        "usampler2D",
        "usamplerCube",
        "usampler1DArray",
        "usampler2DArray",
        "usampler3D",
        "usamplerCubeArray",
        "sampler2DMS",
        "isampler2DMS",
        "usampler2DMS",
        "image2D",
        "imageCube",
        "image2DArray",
        "image3D",
        "imageCubeArray",
        "iimage2D",
        "iimageCube",
        "iimage2DArray",
        "iimage3D",
        "iimageCubeArray",
        "uimage2D",
        "uimageCube",
        "uimage2DArray",
        "uimage3D",
        "uimageCubeArray",
        "atomic_uint",
        "samplerBuffer",
        "isamplerBuffer",
        "usamplerBuffer",
        "sampler2DMSArray",
        "isampler2DMSArray",
        "usampler2DMSArray",
        "imageBuffer",
        "iimageBuffer",
        "uimageBuffer",
        "uint8_t",
        "u8vec2",
        "u8vec3",
        "u8vec4",
        "vector<uint8_t, 5>",
        "int8_t",
        "i8vec2",
        "i8vec3",
        "i8vec4",
        "vector<int8_t, 5>",
        "uint16_t",
        "u16vec2",
        "u16vec3",
        "u16vec4",
        "vector<uint16_t, 5>",
        "int16_t",
        "i16vec2",
        "i16vec3",
        "i16vec4",
        "vector<int16_t, 5>",
        "uint64_t",
        "u64vec2",
        "u64vec3",
        "u64vec4",
        "vector<uint64_t, 5>",
        "int64_t",
        "i64vec2",
        "i64vec3",
        "i64vec4",
        "vector<int64_t, 5>",
        "float16_t",
        "f16vec2",
        "f16vec3",
        "f16vec4",
        "vector<float16_t, 5>",
        "f16mat2",
        "f16mat2x3",
        "f16mat2x4",
        "f16mat3x2",
        "f16mat3",
        "f16mat3x4",
        "f16mat4x2",
        "f16mat4x3",
        "f16mat4",
    };

    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_names) == TYPE_LAST);
    DE_ASSERT(deInBounds32((int)dataType, 0, DE_LENGTH_OF_ARRAY(s_names)));
    return s_names[(int)dataType];
}

int getDataTypeScalarSize(DataType dataType)
{
    static const int s_sizes[] = {
        -1, // invalid
        1,  // float
        2,  // vec2
        3,  // vec3
        4,  // vec4
        5,  // vec5
        4,  // mat2
        6,  // mat2x3
        8,  // mat2x4
        6,  // mat3x2
        9,  // mat3
        12, // mat3x4
        8,  // mat4x2
        12, // mat4x3
        16, // mat4
        1,  // double
        2,  // dvec2
        3,  // dvec3
        4,  // dvec4
        5,  // dvec5
        4,  // dmat2
        6,  // dmat2x3
        8,  // dmat2x4
        6,  // dmat3x2
        9,  // dmat3
        12, // dmat3x4
        8,  // dmat4x2
        12, // dmat4x3
        16, // dmat4
        1,  // int
        2,  // ivec2
        3,  // ivec3
        4,  // ivec4
        5,  // ivec5
        1,  // uint
        2,  // uvec2
        3,  // uvec3
        4,  // uvec4
        5,  // uvec5
        1,  // bool
        2,  // bvec2
        3,  // bvec3
        4,  // bvec4
        5,  // bvec5
        1,  // sampler1D
        1,  // sampler2D
        1,  // samplerCube
        1,  // sampler1DArray
        1,  // sampler2DArray
        1,  // sampler3D
        1,  // samplerCubeArray
        1,  // sampler1DShadow
        1,  // sampler2DShadow
        1,  // samplerCubeShadow
        1,  // sampler1DArrayShadow
        1,  // sampler2DArrayShadow
        1,  // samplerCubeArrayShadow
        1,  // isampler1D
        1,  // isampler2D
        1,  // isamplerCube
        1,  // isampler1DArray
        1,  // isampler2DArray
        1,  // isampler3D
        1,  // isamplerCubeArray
        1,  // usampler1D
        1,  // usampler2D
        1,  // usamplerCube
        1,  // usampler1DArray
        1,  // usampler2DArray
        1,  // usampler3D
        1,  // usamplerCubeArray
        1,  // sampler2DMS
        1,  // isampler2DMS
        1,  // usampler2DMS
        1,  // image2D
        1,  // imageCube
        1,  // image2DArray
        1,  // image3D
        1,  // imageCubeArray
        1,  // iimage2D
        1,  // iimageCube
        1,  // iimage2DArray
        1,  // iimage3D
        1,  // iimageCubeArray
        1,  // uimage2D
        1,  // uimageCube
        1,  // uimage2DArray
        1,  // uimage3D
        1,  // uimageCubeArray
        1,  // atomic_uint
        1,  // samplerBuffer
        1,  // isamplerBuffer
        1,  // usamplerBuffer
        1,  // sampler2DMSArray
        1,  // isampler2DMSArray
        1,  // usampler2DMSArray
        1,  // imageBuffer
        1,  // iimageBuffer
        1,  // uimageBuffer
        1,  // uint8_t
        2,  // u8vec2
        3,  // u8vec3
        4,  // u8vec4
        5,  // u8vec5
        1,  // int8_t
        2,  // i8vec2
        3,  // i8vec3
        4,  // i8vec4
        5,  // i8vec5
        1,  // uint16_t
        2,  // u16vec2
        3,  // u16vec3
        4,  // u16vec4
        5,  // u16vec5
        1,  // int16_t
        2,  // i16vec2
        3,  // i16vec3
        4,  // i16vec4
        5,  // i16vec5
        1,  // uint64_t
        2,  // u64vec2
        3,  // u64vec3
        4,  // u64vec4
        5,  // u64vec5
        1,  // int64_t
        2,  // i64vec2
        3,  // i64vec3
        4,  // i64vec4
        5,  // i64vec5
        1,  // float16_t
        2,  // f16vec2
        3,  // f16vec3
        4,  // f16vec4
        5,  // f16vec5
        4,  // f16mat2
        6,  // f16mat2x3
        8,  // f16mat2x4
        6,  // f16mat3x2
        9,  // f16mat3
        12, // f16mat3x4
        8,  // f16mat4x2
        12, // f16mat4x3
        16, // f16mat4
    };

    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_sizes) == TYPE_LAST);
    DE_ASSERT(deInBounds32((int)dataType, 0, DE_LENGTH_OF_ARRAY(s_sizes)));
    return s_sizes[(int)dataType];
}

DataType getDataTypeScalarType(DataType dataType)
{
    static const DataType s_scalarTypes[] = {
        TYPE_INVALID,                           // invalid
        TYPE_FLOAT,                             // float
        TYPE_FLOAT,                             // vec2
        TYPE_FLOAT,                             // vec3
        TYPE_FLOAT,                             // vec4
        TYPE_FLOAT,                             // vec5
        TYPE_FLOAT,                             // mat2
        TYPE_FLOAT,                             // mat2x3
        TYPE_FLOAT,                             // mat2x4
        TYPE_FLOAT,                             // mat3x2
        TYPE_FLOAT,                             // mat3
        TYPE_FLOAT,                             // mat3x4
        TYPE_FLOAT,                             // mat4x2
        TYPE_FLOAT,                             // mat4x3
        TYPE_FLOAT,                             // mat4
        TYPE_DOUBLE,                            // double
        TYPE_DOUBLE,                            // dvec2
        TYPE_DOUBLE,                            // dvec3
        TYPE_DOUBLE,                            // dvec4
        TYPE_DOUBLE,                            // dvec5
        TYPE_DOUBLE,                            // dmat2
        TYPE_DOUBLE,                            // dmat2x3
        TYPE_DOUBLE,                            // dmat2x4
        TYPE_DOUBLE,                            // dmat3x2
        TYPE_DOUBLE,                            // dmat3
        TYPE_DOUBLE,                            // dmat3x4
        TYPE_DOUBLE,                            // dmat4x2
        TYPE_DOUBLE,                            // dmat4x3
        TYPE_DOUBLE,                            // dmat4
        TYPE_INT,                               // int
        TYPE_INT,                               // ivec2
        TYPE_INT,                               // ivec3
        TYPE_INT,                               // ivec4
        TYPE_INT,                               // ivec5
        TYPE_UINT,                              // uint
        TYPE_UINT,                              // uvec2
        TYPE_UINT,                              // uvec3
        TYPE_UINT,                              // uvec4
        TYPE_UINT,                              // uvec5
        TYPE_BOOL,                              // bool
        TYPE_BOOL,                              // bvec2
        TYPE_BOOL,                              // bvec3
        TYPE_BOOL,                              // bvec4
        TYPE_BOOL,                              // bvec5
        TYPE_SAMPLER_1D,                        // sampler1D
        TYPE_SAMPLER_2D,                        // sampler2D
        TYPE_SAMPLER_CUBE,                      // samplerCube
        TYPE_SAMPLER_1D_ARRAY,                  // sampler1DArray
        TYPE_SAMPLER_2D_ARRAY,                  // sampler2DArray
        TYPE_SAMPLER_3D,                        // sampler3D
        TYPE_SAMPLER_CUBE_ARRAY,                // samplerCubeArray
        TYPE_SAMPLER_1D_SHADOW,                 // sampler1DShadow
        TYPE_SAMPLER_2D_SHADOW,                 // sampler2DShadow
        TYPE_SAMPLER_CUBE_SHADOW,               // samplerCubeShadow
        TYPE_SAMPLER_1D_ARRAY_SHADOW,           // sampler1DArrayShadow
        TYPE_SAMPLER_2D_ARRAY_SHADOW,           // sampler2DArrayShadow
        TYPE_SAMPLER_CUBE_ARRAY_SHADOW,         // samplerCubeArrayShadow
        TYPE_INT_SAMPLER_1D,                    // isampler1D
        TYPE_INT_SAMPLER_2D,                    // isampler2D
        TYPE_INT_SAMPLER_CUBE,                  // isamplerCube
        TYPE_INT_SAMPLER_1D_ARRAY,              // isampler1DArray
        TYPE_INT_SAMPLER_2D_ARRAY,              // isampler2DArray
        TYPE_INT_SAMPLER_3D,                    // isampler3D
        TYPE_INT_SAMPLER_CUBE_ARRAY,            // isamplerCubeArray
        TYPE_UINT_SAMPLER_1D,                   // usampler1D
        TYPE_UINT_SAMPLER_2D,                   // usampler2D
        TYPE_UINT_SAMPLER_CUBE,                 // usamplerCube
        TYPE_UINT_SAMPLER_1D_ARRAY,             // usampler1DArray
        TYPE_UINT_SAMPLER_2D_ARRAY,             // usampler2DArray
        TYPE_UINT_SAMPLER_3D,                   // usampler3D
        TYPE_UINT_SAMPLER_CUBE_ARRAY,           // usamplerCubeArray
        TYPE_SAMPLER_2D_MULTISAMPLE,            // sampler2DMS
        TYPE_INT_SAMPLER_2D_MULTISAMPLE,        // isampler2DMS
        TYPE_UINT_SAMPLER_2D_MULTISAMPLE,       // usampler2DMS
        TYPE_IMAGE_2D,                          // image2D
        TYPE_IMAGE_CUBE,                        // imageCube
        TYPE_IMAGE_2D_ARRAY,                    // image2DArray
        TYPE_IMAGE_3D,                          // image3D
        TYPE_IMAGE_CUBE_ARRAY,                  // imageCubeArray
        TYPE_INT_IMAGE_2D,                      // iimage2D
        TYPE_INT_IMAGE_CUBE,                    // iimageCube
        TYPE_INT_IMAGE_2D_ARRAY,                // iimage2DArray
        TYPE_INT_IMAGE_3D,                      // iimage3D
        TYPE_INT_IMAGE_CUBE_ARRAY,              // iimageCubeArray
        TYPE_UINT_IMAGE_2D,                     // uimage2D
        TYPE_UINT_IMAGE_CUBE,                   // uimageCube
        TYPE_UINT_IMAGE_2D_ARRAY,               // uimage2DArray
        TYPE_UINT_IMAGE_3D,                     // uimage3D
        TYPE_UINT_IMAGE_CUBE_ARRAY,             // uimageCubeArray
        TYPE_UINT_ATOMIC_COUNTER,               // atomic_uint
        TYPE_SAMPLER_BUFFER,                    // samplerBuffer
        TYPE_INT_SAMPLER_BUFFER,                // isamplerBuffer
        TYPE_UINT_SAMPLER_BUFFER,               // usamplerBuffer
        TYPE_SAMPLER_2D_MULTISAMPLE_ARRAY,      // sampler2DMSArray
        TYPE_INT_SAMPLER_2D_MULTISAMPLE_ARRAY,  // isampler2DMSArray
        TYPE_UINT_SAMPLER_2D_MULTISAMPLE_ARRAY, // usampler2DMSArray
        TYPE_IMAGE_BUFFER,                      // imageBuffer
        TYPE_INT_IMAGE_BUFFER,                  // iimageBuffer
        TYPE_UINT_IMAGE_BUFFER,                 // uimageBuffer
        TYPE_UINT8,                             // uint8_t
        TYPE_UINT8,                             // u8vec2
        TYPE_UINT8,                             // u8vec3
        TYPE_UINT8,                             // u8vec4
        TYPE_UINT8,                             // u8vec5
        TYPE_INT8,                              // int8_t
        TYPE_INT8,                              // i8vec2
        TYPE_INT8,                              // i8vec3
        TYPE_INT8,                              // i8vec4
        TYPE_INT8,                              // i8vec5
        TYPE_UINT16,                            // uint16_t
        TYPE_UINT16,                            // u16vec2
        TYPE_UINT16,                            // u16vec3
        TYPE_UINT16,                            // u16vec4
        TYPE_UINT16,                            // u16vec5
        TYPE_INT16,                             // int16_t
        TYPE_INT16,                             // i16vec2
        TYPE_INT16,                             // i16vec3
        TYPE_INT16,                             // i16vec4
        TYPE_INT16,                             // i16vec5
        TYPE_UINT64,                            // uint64_t
        TYPE_UINT64,                            // u64vec2
        TYPE_UINT64,                            // u64vec3
        TYPE_UINT64,                            // u64vec4
        TYPE_UINT64,                            // u64vec5
        TYPE_INT64,                             // int64_t
        TYPE_INT64,                             // i64vec2
        TYPE_INT64,                             // i64vec3
        TYPE_INT64,                             // i64vec4
        TYPE_INT64,                             // i64vec5
        TYPE_FLOAT16,                           // float16_t
        TYPE_FLOAT16,                           // f16vec2
        TYPE_FLOAT16,                           // f16vec3
        TYPE_FLOAT16,                           // f16vec4
        TYPE_FLOAT16,                           // f16vec5
        TYPE_FLOAT16,                           // f16mat2
        TYPE_FLOAT16,                           // f16mat2x3
        TYPE_FLOAT16,                           // f16mat2x4
        TYPE_FLOAT16,                           // f16mat3x2
        TYPE_FLOAT16,                           // f16mat3
        TYPE_FLOAT16,                           // f16mat3x4
        TYPE_FLOAT16,                           // f16mat4x2
        TYPE_FLOAT16,                           // f16mat4x3
        TYPE_FLOAT16,                           // f16mat4
    };

    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_scalarTypes) == TYPE_LAST);
    DE_ASSERT(deInBounds32((int)dataType, 0, DE_LENGTH_OF_ARRAY(s_scalarTypes)));
    return s_scalarTypes[(int)dataType];
}

DataType getDataTypeFloat16Scalars(DataType dataType)
{
    static const DataType s_floatTypes[] = {
        TYPE_INVALID,        // invalid
        TYPE_FLOAT16,        // float
        TYPE_FLOAT16_VEC2,   // vec2
        TYPE_FLOAT16_VEC3,   // vec3
        TYPE_FLOAT16_VEC4,   // vec4
        TYPE_FLOAT16_VEC5,   // vec5
        TYPE_FLOAT16_MAT2,   // mat2
        TYPE_FLOAT16_MAT2X3, // mat2x3
        TYPE_FLOAT16_MAT2X4, // mat2x4
        TYPE_FLOAT16_MAT3X2, // mat3x2
        TYPE_FLOAT16_MAT3,   // mat3
        TYPE_FLOAT16_MAT3X4, // mat3x4
        TYPE_FLOAT16_MAT4X2, // mat4x2
        TYPE_FLOAT16_MAT4X3, // mat4x3
        TYPE_FLOAT16_MAT4,   // mat4
        TYPE_FLOAT16,        // double
        TYPE_FLOAT16_VEC2,   // dvec2
        TYPE_FLOAT16_VEC3,   // dvec3
        TYPE_FLOAT16_VEC4,   // dvec4
        TYPE_FLOAT16_VEC5,   // dvec5
        TYPE_FLOAT16_MAT2,   // dmat2
        TYPE_FLOAT16_MAT2X3, // dmat2x3
        TYPE_FLOAT16_MAT2X4, // dmat2x4
        TYPE_FLOAT16_MAT3X2, // dmat3x2
        TYPE_FLOAT16_MAT3,   // dmat3
        TYPE_FLOAT16_MAT3X4, // dmat3x4
        TYPE_FLOAT16_MAT4X2, // dmat4x2
        TYPE_FLOAT16_MAT4X3, // dmat4x3
        TYPE_FLOAT16_MAT4,   // dmat4
        TYPE_FLOAT16,        // int
        TYPE_FLOAT16_VEC2,   // ivec2
        TYPE_FLOAT16_VEC3,   // ivec3
        TYPE_FLOAT16_VEC4,   // ivec4
        TYPE_FLOAT16_VEC5,   // ivec5
        TYPE_FLOAT16,        // uint
        TYPE_FLOAT16_VEC2,   // uvec2
        TYPE_FLOAT16_VEC3,   // uvec3
        TYPE_FLOAT16_VEC4,   // uvec4
        TYPE_FLOAT16_VEC5,   // uvec5
        TYPE_FLOAT16,        // bool
        TYPE_FLOAT16_VEC2,   // bvec2
        TYPE_FLOAT16_VEC3,   // bvec3
        TYPE_FLOAT16_VEC4,   // bvec4
        TYPE_FLOAT16_VEC5,   // bvec5
        TYPE_INVALID,        // sampler1D
        TYPE_INVALID,        // sampler2D
        TYPE_INVALID,        // samplerCube
        TYPE_INVALID,        // sampler1DArray
        TYPE_INVALID,        // sampler2DArray
        TYPE_INVALID,        // sampler3D
        TYPE_INVALID,        // samplerCubeArray
        TYPE_INVALID,        // sampler1DShadow
        TYPE_INVALID,        // sampler2DShadow
        TYPE_INVALID,        // samplerCubeShadow
        TYPE_INVALID,        // sampler1DArrayShadow
        TYPE_INVALID,        // sampler2DArrayShadow
        TYPE_INVALID,        // samplerCubeArrayShadow
        TYPE_INVALID,        // isampler1D
        TYPE_INVALID,        // isampler2D
        TYPE_INVALID,        // isamplerCube
        TYPE_INVALID,        // isampler1DArray
        TYPE_INVALID,        // isampler2DArray
        TYPE_INVALID,        // isampler3D
        TYPE_INVALID,        // isamplerCubeArray
        TYPE_INVALID,        // usampler1D
        TYPE_INVALID,        // usampler2D
        TYPE_INVALID,        // usamplerCube
        TYPE_INVALID,        // usampler1DArray
        TYPE_INVALID,        // usampler2DArray
        TYPE_INVALID,        // usampler3D
        TYPE_INVALID,        // usamplerCubeArray
        TYPE_INVALID,        // sampler2DMS
        TYPE_INVALID,        // isampler2DMS
        TYPE_INVALID,        // usampler2DMS
        TYPE_INVALID,        // image2D
        TYPE_INVALID,        // imageCube
        TYPE_INVALID,        // image2DArray
        TYPE_INVALID,        // image3D
        TYPE_INVALID,        // imageCubeArray
        TYPE_INVALID,        // iimage2D
        TYPE_INVALID,        // iimageCube
        TYPE_INVALID,        // iimage2DArray
        TYPE_INVALID,        // iimage3D
        TYPE_INVALID,        // iimageCubeArray
        TYPE_INVALID,        // uimage2D
        TYPE_INVALID,        // uimageCube
        TYPE_INVALID,        // uimage2DArray
        TYPE_INVALID,        // uimage3D
        TYPE_INVALID,        // uimageCubeArray
        TYPE_INVALID,        // atomic_uint
        TYPE_INVALID,        // samplerBuffer
        TYPE_INVALID,        // isamplerBuffer
        TYPE_INVALID,        // usamplerBuffer
        TYPE_INVALID,        // sampler2DMSArray
        TYPE_INVALID,        // isampler2DMSArray
        TYPE_INVALID,        // usampler2DMSArray
        TYPE_INVALID,        // imageBuffer
        TYPE_INVALID,        // iimageBuffer
        TYPE_INVALID,        // uimageBuffer
        TYPE_FLOAT16,        // uint8_t
        TYPE_FLOAT16_VEC2,   // u8vec2
        TYPE_FLOAT16_VEC3,   // u8vec3
        TYPE_FLOAT16_VEC4,   // u8vec4
        TYPE_FLOAT16_VEC5,   // u8vec5
        TYPE_FLOAT16,        // int8_t
        TYPE_FLOAT16_VEC2,   // i8vec2
        TYPE_FLOAT16_VEC3,   // i8vec3
        TYPE_FLOAT16_VEC4,   // i8vec4
        TYPE_FLOAT16_VEC5,   // i8vec5
        TYPE_FLOAT16,        // uint16_t
        TYPE_FLOAT16_VEC2,   // u16vec2
        TYPE_FLOAT16_VEC3,   // u16vec3
        TYPE_FLOAT16_VEC4,   // u16vec4
        TYPE_FLOAT16_VEC5,   // u16vec5
        TYPE_FLOAT16,        // int16_t
        TYPE_FLOAT16_VEC2,   // i16vec2
        TYPE_FLOAT16_VEC3,   // i16vec3
        TYPE_FLOAT16_VEC4,   // i16vec4
        TYPE_FLOAT16_VEC5,   // i16vec5
        TYPE_FLOAT16,        // uint64_t
        TYPE_FLOAT16_VEC2,   // u64vec2
        TYPE_FLOAT16_VEC3,   // u64vec3
        TYPE_FLOAT16_VEC4,   // u64vec4
        TYPE_FLOAT16_VEC5,   // u64vec5
        TYPE_FLOAT16,        // int64_t
        TYPE_FLOAT16_VEC2,   // i64vec2
        TYPE_FLOAT16_VEC3,   // i64vec3
        TYPE_FLOAT16_VEC4,   // i64vec4
        TYPE_FLOAT16_VEC5,   // i64vec5
        TYPE_FLOAT16,        // float16_t
        TYPE_FLOAT16_VEC2,   // f16vec2
        TYPE_FLOAT16_VEC3,   // f16vec3
        TYPE_FLOAT16_VEC4,   // f16vec4
        TYPE_FLOAT16_VEC5,   // f16vec5
        TYPE_FLOAT16_MAT2,   // f16mat2
        TYPE_FLOAT16_MAT2X3, // f16mat2x3
        TYPE_FLOAT16_MAT2X4, // f16mat2x4
        TYPE_FLOAT16_MAT3X2, // f16mat3x2
        TYPE_FLOAT16_MAT3,   // f16mat3
        TYPE_FLOAT16_MAT3X4, // f16mat3x4
        TYPE_FLOAT16_MAT4X2, // f16mat4x2
        TYPE_FLOAT16_MAT4X3, // f16mat4x3
        TYPE_FLOAT16_MAT4,   // f16mat4
    };

    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_floatTypes) == TYPE_LAST);
    DE_ASSERT(deInBounds32((int)dataType, 0, DE_LENGTH_OF_ARRAY(s_floatTypes)));
    return s_floatTypes[(int)dataType];
}

DataType getDataTypeFloatScalars(DataType dataType)
{
    static const DataType s_floatTypes[] = {
        TYPE_INVALID,      // invalid
        TYPE_FLOAT,        // float
        TYPE_FLOAT_VEC2,   // vec2
        TYPE_FLOAT_VEC3,   // vec3
        TYPE_FLOAT_VEC4,   // vec4
        TYPE_FLOAT_VEC5,   // vec5
        TYPE_FLOAT_MAT2,   // mat2
        TYPE_FLOAT_MAT2X3, // mat2x3
        TYPE_FLOAT_MAT2X4, // mat2x4
        TYPE_FLOAT_MAT3X2, // mat3x2
        TYPE_FLOAT_MAT3,   // mat3
        TYPE_FLOAT_MAT3X4, // mat3x4
        TYPE_FLOAT_MAT4X2, // mat4x2
        TYPE_FLOAT_MAT4X3, // mat4x3
        TYPE_FLOAT_MAT4,   // mat4
        TYPE_FLOAT,        // double
        TYPE_FLOAT_VEC2,   // dvec2
        TYPE_FLOAT_VEC3,   // dvec3
        TYPE_FLOAT_VEC4,   // dvec4
        TYPE_FLOAT_VEC5,   // dvec5
        TYPE_FLOAT_MAT2,   // dmat2
        TYPE_FLOAT_MAT2X3, // dmat2x3
        TYPE_FLOAT_MAT2X4, // dmat2x4
        TYPE_FLOAT_MAT3X2, // dmat3x2
        TYPE_FLOAT_MAT3,   // dmat3
        TYPE_FLOAT_MAT3X4, // dmat3x4
        TYPE_FLOAT_MAT4X2, // dmat4x2
        TYPE_FLOAT_MAT4X3, // dmat4x3
        TYPE_FLOAT_MAT4,   // dmat4
        TYPE_FLOAT,        // int
        TYPE_FLOAT_VEC2,   // ivec2
        TYPE_FLOAT_VEC3,   // ivec3
        TYPE_FLOAT_VEC4,   // ivec4
        TYPE_FLOAT_VEC5,   // ivec5
        TYPE_FLOAT,        // uint
        TYPE_FLOAT_VEC2,   // uvec2
        TYPE_FLOAT_VEC3,   // uvec3
        TYPE_FLOAT_VEC4,   // uvec4
        TYPE_FLOAT_VEC5,   // uvec5
        TYPE_FLOAT,        // bool
        TYPE_FLOAT_VEC2,   // bvec2
        TYPE_FLOAT_VEC3,   // bvec3
        TYPE_FLOAT_VEC4,   // bvec4
        TYPE_FLOAT_VEC5,   // bvec5
        TYPE_INVALID,      // sampler1D
        TYPE_INVALID,      // sampler2D
        TYPE_INVALID,      // samplerCube
        TYPE_INVALID,      // sampler1DArray
        TYPE_INVALID,      // sampler2DArray
        TYPE_INVALID,      // sampler3D
        TYPE_INVALID,      // samplerCubeArray
        TYPE_INVALID,      // sampler1DShadow
        TYPE_INVALID,      // sampler2DShadow
        TYPE_INVALID,      // samplerCubeShadow
        TYPE_INVALID,      // sampler1DArrayShadow
        TYPE_INVALID,      // sampler2DArrayShadow
        TYPE_INVALID,      // samplerCubeArrayShadow
        TYPE_INVALID,      // isampler1D
        TYPE_INVALID,      // isampler2D
        TYPE_INVALID,      // isamplerCube
        TYPE_INVALID,      // isampler1DArray
        TYPE_INVALID,      // isampler2DArray
        TYPE_INVALID,      // isampler3D
        TYPE_INVALID,      // isamplerCubeArray
        TYPE_INVALID,      // usampler1D
        TYPE_INVALID,      // usampler2D
        TYPE_INVALID,      // usamplerCube
        TYPE_INVALID,      // usampler1DArray
        TYPE_INVALID,      // usampler2DArray
        TYPE_INVALID,      // usampler3D
        TYPE_INVALID,      // usamplerCubeArray
        TYPE_INVALID,      // sampler2DMS
        TYPE_INVALID,      // isampler2DMS
        TYPE_INVALID,      // usampler2DMS
        TYPE_INVALID,      // image2D
        TYPE_INVALID,      // imageCube
        TYPE_INVALID,      // image2DArray
        TYPE_INVALID,      // image3D
        TYPE_INVALID,      // imageCubeArray
        TYPE_INVALID,      // iimage2D
        TYPE_INVALID,      // iimageCube
        TYPE_INVALID,      // iimage2DArray
        TYPE_INVALID,      // iimage3D
        TYPE_INVALID,      // iimageCubeArray
        TYPE_INVALID,      // uimage2D
        TYPE_INVALID,      // uimageCube
        TYPE_INVALID,      // uimage2DArray
        TYPE_INVALID,      // uimage3D
        TYPE_INVALID,      // uimageCubeArray
        TYPE_INVALID,      // atomic_uint
        TYPE_INVALID,      // samplerBuffer
        TYPE_INVALID,      // isamplerBuffer
        TYPE_INVALID,      // usamplerBuffer
        TYPE_INVALID,      // sampler2DMSArray
        TYPE_INVALID,      // isampler2DMSArray
        TYPE_INVALID,      // usampler2DMSArray
        TYPE_INVALID,      // imageBuffer
        TYPE_INVALID,      // iimageBuffer
        TYPE_INVALID,      // uimageBuffer
        TYPE_FLOAT,        // uint8_t
        TYPE_FLOAT_VEC2,   // u8vec2
        TYPE_FLOAT_VEC3,   // u8vec3
        TYPE_FLOAT_VEC4,   // u8vec4
        TYPE_FLOAT_VEC5,   // u8vec5
        TYPE_FLOAT,        // int8_t
        TYPE_FLOAT_VEC2,   // i8vec2
        TYPE_FLOAT_VEC3,   // i8vec3
        TYPE_FLOAT_VEC4,   // i8vec4
        TYPE_FLOAT_VEC5,   // i8vec5
        TYPE_FLOAT,        // uint16_t
        TYPE_FLOAT_VEC2,   // u16vec2
        TYPE_FLOAT_VEC3,   // u16vec3
        TYPE_FLOAT_VEC4,   // u16vec4
        TYPE_FLOAT_VEC5,   // u16vec5
        TYPE_FLOAT,        // int16_t
        TYPE_FLOAT_VEC2,   // i16vec2
        TYPE_FLOAT_VEC3,   // i16vec3
        TYPE_FLOAT_VEC4,   // i16vec4
        TYPE_FLOAT_VEC5,   // i16vec5
        TYPE_FLOAT,        // uint64_t
        TYPE_FLOAT_VEC2,   // u64vec2
        TYPE_FLOAT_VEC3,   // u64vec3
        TYPE_FLOAT_VEC4,   // u64vec4
        TYPE_FLOAT_VEC5,   // u64vec5
        TYPE_FLOAT,        // int64_t
        TYPE_FLOAT_VEC2,   // i64vec2
        TYPE_FLOAT_VEC3,   // i64vec3
        TYPE_FLOAT_VEC4,   // i64vec4
        TYPE_FLOAT_VEC5,   // i64vec5
        TYPE_FLOAT,        // float16_t
        TYPE_FLOAT_VEC2,   // f16vec2
        TYPE_FLOAT_VEC3,   // f16vec3
        TYPE_FLOAT_VEC4,   // f16vec4
        TYPE_FLOAT_VEC5,   // f16vec5
        TYPE_FLOAT_MAT2,   // f16mat2
        TYPE_FLOAT_MAT2X3, // f16mat2x3
        TYPE_FLOAT_MAT2X4, // f16mat2x4
        TYPE_FLOAT_MAT3X2, // f16mat3x2
        TYPE_FLOAT_MAT3,   // f16mat3
        TYPE_FLOAT_MAT3X4, // f16mat3x4
        TYPE_FLOAT_MAT4X2, // f16mat4x2
        TYPE_FLOAT_MAT4X3, // f16mat4x3
        TYPE_FLOAT_MAT4,   // f16mat4
    };

    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_floatTypes) == TYPE_LAST);
    DE_ASSERT(deInBounds32((int)dataType, 0, DE_LENGTH_OF_ARRAY(s_floatTypes)));
    return s_floatTypes[(int)dataType];
}

DataType getDataTypeDoubleScalars(DataType dataType)
{
    static const DataType s_doubleTypes[] = {
        TYPE_INVALID,       // invalid
        TYPE_DOUBLE,        // float
        TYPE_DOUBLE_VEC2,   // vec2
        TYPE_DOUBLE_VEC3,   // vec3
        TYPE_DOUBLE_VEC4,   // vec4
        TYPE_DOUBLE_VEC5,   // vec5
        TYPE_DOUBLE_MAT2,   // mat2
        TYPE_DOUBLE_MAT2X3, // mat2x3
        TYPE_DOUBLE_MAT2X4, // mat2x4
        TYPE_DOUBLE_MAT3X2, // mat3x2
        TYPE_DOUBLE_MAT3,   // mat3
        TYPE_DOUBLE_MAT3X4, // mat3x4
        TYPE_DOUBLE_MAT4X2, // mat4x2
        TYPE_DOUBLE_MAT4X3, // mat4x3
        TYPE_DOUBLE_MAT4,   // mat4
        TYPE_DOUBLE,        // double
        TYPE_DOUBLE_VEC2,   // dvec2
        TYPE_DOUBLE_VEC3,   // dvec3
        TYPE_DOUBLE_VEC4,   // dvec4
        TYPE_DOUBLE_VEC5,   // dvec5
        TYPE_DOUBLE_MAT2,   // dmat2
        TYPE_DOUBLE_MAT2X3, // dmat2x3
        TYPE_DOUBLE_MAT2X4, // dmat2x4
        TYPE_DOUBLE_MAT3X2, // dmat3x2
        TYPE_DOUBLE_MAT3,   // dmat3
        TYPE_DOUBLE_MAT3X4, // dmat3x4
        TYPE_DOUBLE_MAT4X2, // dmat4x2
        TYPE_DOUBLE_MAT4X3, // dmat4x3
        TYPE_DOUBLE_MAT4,   // dmat4
        TYPE_DOUBLE,        // int
        TYPE_DOUBLE_VEC2,   // ivec2
        TYPE_DOUBLE_VEC3,   // ivec3
        TYPE_DOUBLE_VEC4,   // ivec4
        TYPE_DOUBLE_VEC5,   // ivec5
        TYPE_DOUBLE,        // uint
        TYPE_DOUBLE_VEC2,   // uvec2
        TYPE_DOUBLE_VEC3,   // uvec3
        TYPE_DOUBLE_VEC4,   // uvec4
        TYPE_DOUBLE_VEC5,   // uvec5
        TYPE_DOUBLE,        // bool
        TYPE_DOUBLE_VEC2,   // bvec2
        TYPE_DOUBLE_VEC3,   // bvec3
        TYPE_DOUBLE_VEC4,   // bvec4
        TYPE_DOUBLE_VEC5,   // bvec5
        TYPE_INVALID,       // sampler1D
        TYPE_INVALID,       // sampler2D
        TYPE_INVALID,       // samplerCube
        TYPE_INVALID,       // sampler1DArray
        TYPE_INVALID,       // sampler2DArray
        TYPE_INVALID,       // sampler3D
        TYPE_INVALID,       // samplerCubeArray
        TYPE_INVALID,       // sampler1DShadow
        TYPE_INVALID,       // sampler2DShadow
        TYPE_INVALID,       // samplerCubeShadow
        TYPE_INVALID,       // sampler1DArrayShadow
        TYPE_INVALID,       // sampler2DArrayShadow
        TYPE_INVALID,       // samplerCubeArrayShadow
        TYPE_INVALID,       // isampler1D
        TYPE_INVALID,       // isampler2D
        TYPE_INVALID,       // isamplerCube
        TYPE_INVALID,       // isampler1DArray
        TYPE_INVALID,       // isampler2DArray
        TYPE_INVALID,       // isampler3D
        TYPE_INVALID,       // isamplerCubeArray
        TYPE_INVALID,       // usampler1D
        TYPE_INVALID,       // usampler2D
        TYPE_INVALID,       // usamplerCube
        TYPE_INVALID,       // usampler1DArray
        TYPE_INVALID,       // usampler2DArray
        TYPE_INVALID,       // usampler3D
        TYPE_INVALID,       // usamplerCubeArray
        TYPE_INVALID,       // sampler2DMS
        TYPE_INVALID,       // isampler2DMS
        TYPE_INVALID,       // usampler2DMS
        TYPE_INVALID,       // image2D
        TYPE_INVALID,       // imageCube
        TYPE_INVALID,       // image2DArray
        TYPE_INVALID,       // image3D
        TYPE_INVALID,       // imageCubeArray
        TYPE_INVALID,       // iimage2D
        TYPE_INVALID,       // iimageCube
        TYPE_INVALID,       // iimage2DArray
        TYPE_INVALID,       // iimage3D
        TYPE_INVALID,       // iimageCubeArray
        TYPE_INVALID,       // uimage2D
        TYPE_INVALID,       // uimageCube
        TYPE_INVALID,       // uimage2DArray
        TYPE_INVALID,       // uimage3D
        TYPE_INVALID,       // uimageCubeArray
        TYPE_INVALID,       // atomic_uint
        TYPE_INVALID,       // samplerBuffer
        TYPE_INVALID,       // isamplerBuffer
        TYPE_INVALID,       // usamplerBuffer
        TYPE_INVALID,       // sampler2DMSArray
        TYPE_INVALID,       // isampler2DMSArray
        TYPE_INVALID,       // usampler2DMSArray
        TYPE_INVALID,       // imageBuffer
        TYPE_INVALID,       // iimageBuffer
        TYPE_INVALID,       // uimageBuffer
        TYPE_DOUBLE,        // uint18_t
        TYPE_DOUBLE_VEC2,   // u8vec2
        TYPE_DOUBLE_VEC3,   // u8vec3
        TYPE_DOUBLE_VEC4,   // u8vec4
        TYPE_DOUBLE_VEC5,   // u8vec5
        TYPE_DOUBLE,        // int8_t
        TYPE_DOUBLE_VEC2,   // i8vec2
        TYPE_DOUBLE_VEC3,   // i8vec3
        TYPE_DOUBLE_VEC4,   // i8vec4
        TYPE_DOUBLE_VEC5,   // i8vec5
        TYPE_DOUBLE,        // uint16_t
        TYPE_DOUBLE_VEC2,   // u16vec2
        TYPE_DOUBLE_VEC3,   // u16vec3
        TYPE_DOUBLE_VEC4,   // u16vec4
        TYPE_DOUBLE_VEC5,   // u16vec5
        TYPE_DOUBLE,        // int16_t
        TYPE_DOUBLE_VEC2,   // i16vec2
        TYPE_DOUBLE_VEC3,   // i16vec3
        TYPE_DOUBLE_VEC4,   // i16vec4
        TYPE_DOUBLE_VEC5,   // i16vec5
        TYPE_DOUBLE,        // uint64_t
        TYPE_DOUBLE_VEC2,   // u64vec2
        TYPE_DOUBLE_VEC3,   // u64vec3
        TYPE_DOUBLE_VEC4,   // u64vec4
        TYPE_DOUBLE_VEC5,   // u64vec5
        TYPE_DOUBLE,        // int64_t
        TYPE_DOUBLE_VEC2,   // i64vec2
        TYPE_DOUBLE_VEC3,   // i64vec3
        TYPE_DOUBLE_VEC4,   // i64vec4
        TYPE_DOUBLE_VEC5,   // i64vec5
        TYPE_DOUBLE,        // float16_t
        TYPE_DOUBLE_VEC2,   // f16vec2
        TYPE_DOUBLE_VEC3,   // f16vec3
        TYPE_DOUBLE_VEC4,   // f16vec4
        TYPE_DOUBLE_VEC5,   // f16vec5
        TYPE_DOUBLE_MAT2,   // f16mat2
        TYPE_DOUBLE_MAT2X3, // f16mat2x3
        TYPE_DOUBLE_MAT2X4, // f16mat2x4
        TYPE_DOUBLE_MAT3X2, // f16mat3x2
        TYPE_DOUBLE_MAT3,   // f16mat3
        TYPE_DOUBLE_MAT3X4, // f16mat3x4
        TYPE_DOUBLE_MAT4X2, // f16mat4x2
        TYPE_DOUBLE_MAT4X3, // f16mat4x3
        TYPE_DOUBLE_MAT4,   // f16mat4
    };

    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_doubleTypes) == TYPE_LAST);
    DE_ASSERT(deInBounds32((int)dataType, 0, DE_LENGTH_OF_ARRAY(s_doubleTypes)));
    return s_doubleTypes[(int)dataType];
}

DataType getDataTypeVector(DataType scalarType, int size)
{
    DE_ASSERT(deInRange32(size, 1, 5));
    switch (scalarType)
    {
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_BOOL:
    case TYPE_UINT8:
    case TYPE_INT8:
    case TYPE_UINT16:
    case TYPE_INT16:
    case TYPE_UINT64:
    case TYPE_INT64:
    case TYPE_FLOAT16:
        return (DataType)((int)scalarType + size - 1);
    default:
        return TYPE_INVALID;
    }
}

DataType getDataTypeFloatVec(int vecSize)
{
    return getDataTypeVector(TYPE_FLOAT, vecSize);
}

DataType getDataTypeIntVec(int vecSize)
{
    return getDataTypeVector(TYPE_INT, vecSize);
}

DataType getDataTypeUintVec(int vecSize)
{
    return getDataTypeVector(TYPE_UINT, vecSize);
}

DataType getDataTypeBoolVec(int vecSize)
{
    return getDataTypeVector(TYPE_BOOL, vecSize);
}

DataType getDataTypeMatrix(int numCols, int numRows)
{
    DE_ASSERT(de::inRange(numCols, 2, 4) && de::inRange(numRows, 2, 4));
    return (DataType)((int)TYPE_FLOAT_MAT2 + (numCols - 2) * 3 + (numRows - 2));
}

int getDataTypeMatrixNumRows(DataType dataType)
{
    switch (dataType)
    {
    case TYPE_FLOAT_MAT2:
        return 2;
    case TYPE_FLOAT_MAT2X3:
        return 3;
    case TYPE_FLOAT_MAT2X4:
        return 4;
    case TYPE_FLOAT_MAT3X2:
        return 2;
    case TYPE_FLOAT_MAT3:
        return 3;
    case TYPE_FLOAT_MAT3X4:
        return 4;
    case TYPE_FLOAT_MAT4X2:
        return 2;
    case TYPE_FLOAT_MAT4X3:
        return 3;
    case TYPE_FLOAT_MAT4:
        return 4;
    case TYPE_DOUBLE_MAT2:
        return 2;
    case TYPE_DOUBLE_MAT2X3:
        return 3;
    case TYPE_DOUBLE_MAT2X4:
        return 4;
    case TYPE_DOUBLE_MAT3X2:
        return 2;
    case TYPE_DOUBLE_MAT3:
        return 3;
    case TYPE_DOUBLE_MAT3X4:
        return 4;
    case TYPE_DOUBLE_MAT4X2:
        return 2;
    case TYPE_DOUBLE_MAT4X3:
        return 3;
    case TYPE_DOUBLE_MAT4:
        return 4;
    case TYPE_FLOAT16_MAT2:
        return 2;
    case TYPE_FLOAT16_MAT2X3:
        return 3;
    case TYPE_FLOAT16_MAT2X4:
        return 4;
    case TYPE_FLOAT16_MAT3X2:
        return 2;
    case TYPE_FLOAT16_MAT3:
        return 3;
    case TYPE_FLOAT16_MAT3X4:
        return 4;
    case TYPE_FLOAT16_MAT4X2:
        return 2;
    case TYPE_FLOAT16_MAT4X3:
        return 3;
    case TYPE_FLOAT16_MAT4:
        return 4;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

int getDataTypeMatrixNumColumns(DataType dataType)
{
    switch (dataType)
    {
    case TYPE_FLOAT_MAT2:
        return 2;
    case TYPE_FLOAT_MAT2X3:
        return 2;
    case TYPE_FLOAT_MAT2X4:
        return 2;
    case TYPE_FLOAT_MAT3X2:
        return 3;
    case TYPE_FLOAT_MAT3:
        return 3;
    case TYPE_FLOAT_MAT3X4:
        return 3;
    case TYPE_FLOAT_MAT4X2:
        return 4;
    case TYPE_FLOAT_MAT4X3:
        return 4;
    case TYPE_FLOAT_MAT4:
        return 4;
    case TYPE_DOUBLE_MAT2:
        return 2;
    case TYPE_DOUBLE_MAT2X3:
        return 2;
    case TYPE_DOUBLE_MAT2X4:
        return 2;
    case TYPE_DOUBLE_MAT3X2:
        return 3;
    case TYPE_DOUBLE_MAT3:
        return 3;
    case TYPE_DOUBLE_MAT3X4:
        return 3;
    case TYPE_DOUBLE_MAT4X2:
        return 4;
    case TYPE_DOUBLE_MAT4X3:
        return 4;
    case TYPE_DOUBLE_MAT4:
        return 4;
    case TYPE_FLOAT16_MAT2:
        return 2;
    case TYPE_FLOAT16_MAT2X3:
        return 2;
    case TYPE_FLOAT16_MAT2X4:
        return 2;
    case TYPE_FLOAT16_MAT3X2:
        return 3;
    case TYPE_FLOAT16_MAT3:
        return 3;
    case TYPE_FLOAT16_MAT3X4:
        return 3;
    case TYPE_FLOAT16_MAT4X2:
        return 4;
    case TYPE_FLOAT16_MAT4X3:
        return 4;
    case TYPE_FLOAT16_MAT4:
        return 4;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

DataType getDataTypeMatrixColumnType(DataType dataType)
{
    switch (dataType)
    {
    case TYPE_FLOAT_MAT2:
        return TYPE_FLOAT_VEC2;
    case TYPE_FLOAT_MAT2X3:
        return TYPE_FLOAT_VEC3;
    case TYPE_FLOAT_MAT2X4:
        return TYPE_FLOAT_VEC4;
    case TYPE_FLOAT_MAT3X2:
        return TYPE_FLOAT_VEC2;
    case TYPE_FLOAT_MAT3:
        return TYPE_FLOAT_VEC3;
    case TYPE_FLOAT_MAT3X4:
        return TYPE_FLOAT_VEC4;
    case TYPE_FLOAT_MAT4X2:
        return TYPE_FLOAT_VEC2;
    case TYPE_FLOAT_MAT4X3:
        return TYPE_FLOAT_VEC3;
    case TYPE_FLOAT_MAT4:
        return TYPE_FLOAT_VEC4;
    case TYPE_DOUBLE_MAT2:
        return TYPE_DOUBLE_VEC2;
    case TYPE_DOUBLE_MAT2X3:
        return TYPE_DOUBLE_VEC3;
    case TYPE_DOUBLE_MAT2X4:
        return TYPE_DOUBLE_VEC4;
    case TYPE_DOUBLE_MAT3X2:
        return TYPE_DOUBLE_VEC2;
    case TYPE_DOUBLE_MAT3:
        return TYPE_DOUBLE_VEC3;
    case TYPE_DOUBLE_MAT3X4:
        return TYPE_DOUBLE_VEC4;
    case TYPE_DOUBLE_MAT4X2:
        return TYPE_DOUBLE_VEC2;
    case TYPE_DOUBLE_MAT4X3:
        return TYPE_DOUBLE_VEC3;
    case TYPE_DOUBLE_MAT4:
        return TYPE_DOUBLE_VEC4;
    case TYPE_FLOAT16_MAT2:
        return TYPE_FLOAT16_VEC2;
    case TYPE_FLOAT16_MAT2X3:
        return TYPE_FLOAT16_VEC3;
    case TYPE_FLOAT16_MAT2X4:
        return TYPE_FLOAT16_VEC4;
    case TYPE_FLOAT16_MAT3X2:
        return TYPE_FLOAT16_VEC2;
    case TYPE_FLOAT16_MAT3:
        return TYPE_FLOAT16_VEC3;
    case TYPE_FLOAT16_MAT3X4:
        return TYPE_FLOAT16_VEC4;
    case TYPE_FLOAT16_MAT4X2:
        return TYPE_FLOAT16_VEC2;
    case TYPE_FLOAT16_MAT4X3:
        return TYPE_FLOAT16_VEC3;
    case TYPE_FLOAT16_MAT4:
        return TYPE_FLOAT16_VEC4;
    default:
        DE_ASSERT(false);
        return TYPE_INVALID;
    }
}

int getDataTypeNumLocations(DataType dataType)
{
    if (isDataTypeScalarOrVector(dataType))
        return 1;
    else if (isDataTypeMatrix(dataType))
        return getDataTypeMatrixNumColumns(dataType);

    DE_FATAL("Illegal datatype.");
    return 0;
}

int getDataTypeNumComponents(DataType dataType)
{
    if (isDataTypeScalarOrVector(dataType))
        return getDataTypeScalarSize(dataType);
    else if (isDataTypeMatrix(dataType))
        return getDataTypeMatrixNumRows(dataType);

    DE_FATAL("Illegal datatype.");
    return 0;
}

DataType getDataTypeFromGLType(uint32_t glType)
{
    switch (glType)
    {
    case GL_FLOAT:
        return TYPE_FLOAT;
    case GL_FLOAT_VEC2:
        return TYPE_FLOAT_VEC2;
    case GL_FLOAT_VEC3:
        return TYPE_FLOAT_VEC3;
    case GL_FLOAT_VEC4:
        return TYPE_FLOAT_VEC4;

    case GL_FLOAT_MAT2:
        return TYPE_FLOAT_MAT2;
    case GL_FLOAT_MAT2x3:
        return TYPE_FLOAT_MAT2X3;
    case GL_FLOAT_MAT2x4:
        return TYPE_FLOAT_MAT2X4;

    case GL_FLOAT_MAT3x2:
        return TYPE_FLOAT_MAT3X2;
    case GL_FLOAT_MAT3:
        return TYPE_FLOAT_MAT3;
    case GL_FLOAT_MAT3x4:
        return TYPE_FLOAT_MAT3X4;

    case GL_FLOAT_MAT4x2:
        return TYPE_FLOAT_MAT4X2;
    case GL_FLOAT_MAT4x3:
        return TYPE_FLOAT_MAT4X3;
    case GL_FLOAT_MAT4:
        return TYPE_FLOAT_MAT4;

    case GL_DOUBLE:
        return TYPE_DOUBLE;
    case GL_DOUBLE_VEC2:
        return TYPE_DOUBLE_VEC2;
    case GL_DOUBLE_VEC3:
        return TYPE_DOUBLE_VEC3;
    case GL_DOUBLE_VEC4:
        return TYPE_DOUBLE_VEC4;

    case GL_DOUBLE_MAT2:
        return TYPE_DOUBLE_MAT2;
    case GL_DOUBLE_MAT2x3:
        return TYPE_DOUBLE_MAT2X3;
    case GL_DOUBLE_MAT2x4:
        return TYPE_DOUBLE_MAT2X4;

    case GL_DOUBLE_MAT3x2:
        return TYPE_DOUBLE_MAT3X2;
    case GL_DOUBLE_MAT3:
        return TYPE_DOUBLE_MAT3;
    case GL_DOUBLE_MAT3x4:
        return TYPE_DOUBLE_MAT3X4;

    case GL_DOUBLE_MAT4x2:
        return TYPE_DOUBLE_MAT4X2;
    case GL_DOUBLE_MAT4x3:
        return TYPE_DOUBLE_MAT4X3;
    case GL_DOUBLE_MAT4:
        return TYPE_DOUBLE_MAT4;

    case GL_INT:
        return TYPE_INT;
    case GL_INT_VEC2:
        return TYPE_INT_VEC2;
    case GL_INT_VEC3:
        return TYPE_INT_VEC3;
    case GL_INT_VEC4:
        return TYPE_INT_VEC4;

    case GL_UNSIGNED_INT:
        return TYPE_UINT;
    case GL_UNSIGNED_INT_VEC2:
        return TYPE_UINT_VEC2;
    case GL_UNSIGNED_INT_VEC3:
        return TYPE_UINT_VEC3;
    case GL_UNSIGNED_INT_VEC4:
        return TYPE_UINT_VEC4;

    case GL_BOOL:
        return TYPE_BOOL;
    case GL_BOOL_VEC2:
        return TYPE_BOOL_VEC2;
    case GL_BOOL_VEC3:
        return TYPE_BOOL_VEC3;
    case GL_BOOL_VEC4:
        return TYPE_BOOL_VEC4;

    case GL_SAMPLER_1D:
        return TYPE_SAMPLER_1D;
    case GL_SAMPLER_2D:
        return TYPE_SAMPLER_2D;
    case GL_SAMPLER_CUBE:
        return TYPE_SAMPLER_CUBE;
    case GL_SAMPLER_1D_ARRAY:
        return TYPE_SAMPLER_1D_ARRAY;
    case GL_SAMPLER_2D_ARRAY:
        return TYPE_SAMPLER_2D_ARRAY;
    case GL_SAMPLER_3D:
        return TYPE_SAMPLER_3D;
    case GL_SAMPLER_CUBE_MAP_ARRAY:
        return TYPE_SAMPLER_CUBE_ARRAY;

    case GL_SAMPLER_1D_SHADOW:
        return TYPE_SAMPLER_1D_SHADOW;
    case GL_SAMPLER_2D_SHADOW:
        return TYPE_SAMPLER_2D_SHADOW;
    case GL_SAMPLER_CUBE_SHADOW:
        return TYPE_SAMPLER_CUBE_SHADOW;
    case GL_SAMPLER_1D_ARRAY_SHADOW:
        return TYPE_SAMPLER_1D_ARRAY_SHADOW;
    case GL_SAMPLER_2D_ARRAY_SHADOW:
        return TYPE_SAMPLER_2D_ARRAY_SHADOW;
    case GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
        return TYPE_SAMPLER_CUBE_ARRAY_SHADOW;

    case GL_INT_SAMPLER_1D:
        return TYPE_INT_SAMPLER_1D;
    case GL_INT_SAMPLER_2D:
        return TYPE_INT_SAMPLER_2D;
    case GL_INT_SAMPLER_CUBE:
        return TYPE_INT_SAMPLER_CUBE;
    case GL_INT_SAMPLER_1D_ARRAY:
        return TYPE_INT_SAMPLER_1D_ARRAY;
    case GL_INT_SAMPLER_2D_ARRAY:
        return TYPE_INT_SAMPLER_2D_ARRAY;
    case GL_INT_SAMPLER_3D:
        return TYPE_INT_SAMPLER_3D;
    case GL_INT_SAMPLER_CUBE_MAP_ARRAY:
        return TYPE_INT_SAMPLER_CUBE_ARRAY;

    case GL_UNSIGNED_INT_SAMPLER_1D:
        return TYPE_UINT_SAMPLER_1D;
    case GL_UNSIGNED_INT_SAMPLER_2D:
        return TYPE_UINT_SAMPLER_2D;
    case GL_UNSIGNED_INT_SAMPLER_CUBE:
        return TYPE_UINT_SAMPLER_CUBE;
    case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
        return TYPE_UINT_SAMPLER_1D_ARRAY;
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
        return TYPE_UINT_SAMPLER_2D_ARRAY;
    case GL_UNSIGNED_INT_SAMPLER_3D:
        return TYPE_UINT_SAMPLER_3D;
    case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:
        return TYPE_UINT_SAMPLER_CUBE_ARRAY;

    case GL_SAMPLER_2D_MULTISAMPLE:
        return TYPE_SAMPLER_2D_MULTISAMPLE;
    case GL_INT_SAMPLER_2D_MULTISAMPLE:
        return TYPE_INT_SAMPLER_2D_MULTISAMPLE;
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
        return TYPE_UINT_SAMPLER_2D_MULTISAMPLE;

    case GL_IMAGE_2D:
        return TYPE_IMAGE_2D;
    case GL_IMAGE_CUBE:
        return TYPE_IMAGE_CUBE;
    case GL_IMAGE_2D_ARRAY:
        return TYPE_IMAGE_2D_ARRAY;
    case GL_IMAGE_3D:
        return TYPE_IMAGE_3D;
    case GL_INT_IMAGE_2D:
        return TYPE_INT_IMAGE_2D;
    case GL_INT_IMAGE_CUBE:
        return TYPE_INT_IMAGE_CUBE;
    case GL_INT_IMAGE_2D_ARRAY:
        return TYPE_INT_IMAGE_2D_ARRAY;
    case GL_INT_IMAGE_3D:
        return TYPE_INT_IMAGE_3D;
    case GL_UNSIGNED_INT_IMAGE_2D:
        return TYPE_UINT_IMAGE_2D;
    case GL_UNSIGNED_INT_IMAGE_CUBE:
        return TYPE_UINT_IMAGE_CUBE;
    case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:
        return TYPE_UINT_IMAGE_2D_ARRAY;
    case GL_UNSIGNED_INT_IMAGE_3D:
        return TYPE_UINT_IMAGE_3D;

    case GL_UNSIGNED_INT_ATOMIC_COUNTER:
        return TYPE_UINT_ATOMIC_COUNTER;

    case GL_SAMPLER_BUFFER:
        return TYPE_SAMPLER_BUFFER;
    case GL_INT_SAMPLER_BUFFER:
        return TYPE_INT_SAMPLER_BUFFER;
    case GL_UNSIGNED_INT_SAMPLER_BUFFER:
        return TYPE_UINT_SAMPLER_BUFFER;

    case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
        return TYPE_SAMPLER_2D_MULTISAMPLE_ARRAY;
    case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
        return TYPE_INT_SAMPLER_2D_MULTISAMPLE_ARRAY;
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
        return TYPE_UINT_SAMPLER_2D_MULTISAMPLE_ARRAY;

    case GL_IMAGE_BUFFER:
        return TYPE_IMAGE_BUFFER;
    case GL_INT_IMAGE_BUFFER:
        return TYPE_INT_IMAGE_BUFFER;
    case GL_UNSIGNED_INT_IMAGE_BUFFER:
        return TYPE_UINT_IMAGE_BUFFER;

    default:
        return TYPE_LAST;
    }
}

// Miscellaneous
bool saveShader(const std::string &name, const std::string &code)
{
    bool equals = false;
    std::ifstream existingShaderFile(name + ".glsl");
    if (existingShaderFile.is_open())
    {
        std::string existingCode;
        existingShaderFile >> existingCode;
        equals = code == existingCode;
        existingShaderFile.close();
    }
    if (false == equals)
    {
        std::ofstream newShaderFile(name + ".x");
        if (newShaderFile.is_open())
        {
            newShaderFile << code;
            newShaderFile.close();
            equals = true;
        }
    }
    return equals;
}

} // namespace glu
