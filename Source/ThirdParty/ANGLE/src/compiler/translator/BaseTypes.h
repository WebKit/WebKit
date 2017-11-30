//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_BASETYPES_H_
#define COMPILER_TRANSLATOR_BASETYPES_H_

#include <algorithm>
#include <array>

#include "common/debug.h"
#include "GLSLANG/ShaderLang.h"

namespace sh
{

//
// Precision qualifiers
//
enum TPrecision
{
    // These need to be kept sorted
    EbpUndefined,
    EbpLow,
    EbpMedium,
    EbpHigh,

    // end of list
    EbpLast
};

inline const char *getPrecisionString(TPrecision p)
{
    switch (p)
    {
        case EbpHigh:
            return "highp";
        case EbpMedium:
            return "mediump";
        case EbpLow:
            return "lowp";
        default:
            return "mediump";  // Safest fallback
    }
}

//
// Basic type.  Arrays, vectors, etc., are orthogonal to this.
//
enum TBasicType
{
    EbtVoid,
    EbtFloat,
    EbtInt,
    EbtUInt,
    EbtBool,
    EbtGVec4,              // non type: represents vec4, ivec4, and uvec4
    EbtGenType,            // non type: represents float, vec2, vec3, and vec4
    EbtGenIType,           // non type: represents int, ivec2, ivec3, and ivec4
    EbtGenUType,           // non type: represents uint, uvec2, uvec3, and uvec4
    EbtGenBType,           // non type: represents bool, bvec2, bvec3, and bvec4
    EbtVec,                // non type: represents vec2, vec3, and vec4
    EbtIVec,               // non type: represents ivec2, ivec3, and ivec4
    EbtUVec,               // non type: represents uvec2, uvec3, and uvec4
    EbtBVec,               // non type: represents bvec2, bvec3, and bvec4
    EbtYuvCscStandardEXT,  // Only valid if EXT_YUV_target exists.
    EbtGuardSamplerBegin,  // non type: see implementation of IsSampler()
    EbtSampler2D,
    EbtSampler3D,
    EbtSamplerCube,
    EbtSampler2DArray,
    EbtSamplerExternalOES,       // Only valid if OES_EGL_image_external exists.
    EbtSamplerExternal2DY2YEXT,  // Only valid if GL_EXT_YUV_target exists.
    EbtSampler2DRect,            // Only valid if GL_ARB_texture_rectangle exists.
    EbtSampler2DMS,
    EbtISampler2D,
    EbtISampler3D,
    EbtISamplerCube,
    EbtISampler2DArray,
    EbtISampler2DMS,
    EbtUSampler2D,
    EbtUSampler3D,
    EbtUSamplerCube,
    EbtUSampler2DArray,
    EbtUSampler2DMS,
    EbtSampler2DShadow,
    EbtSamplerCubeShadow,
    EbtSampler2DArrayShadow,
    EbtGuardSamplerEnd,  // non type: see implementation of IsSampler()
    EbtGSampler2D,       // non type: represents sampler2D, isampler2D, and usampler2D
    EbtGSampler3D,       // non type: represents sampler3D, isampler3D, and usampler3D
    EbtGSamplerCube,     // non type: represents samplerCube, isamplerCube, and usamplerCube
    EbtGSampler2DArray,  // non type: represents sampler2DArray, isampler2DArray, and
                         // usampler2DArray
    EbtGSampler2DMS,     // non type: represents sampler2DMS, isampler2DMS, and usampler2DMS

    // images
    EbtGuardImageBegin,
    EbtImage2D,
    EbtIImage2D,
    EbtUImage2D,
    EbtImage3D,
    EbtIImage3D,
    EbtUImage3D,
    EbtImage2DArray,
    EbtIImage2DArray,
    EbtUImage2DArray,
    EbtImageCube,
    EbtIImageCube,
    EbtUImageCube,
    EbtGuardImageEnd,

    EbtGuardGImageBegin,
    EbtGImage2D,       // non type: represents image2D, uimage2D, iimage2D
    EbtGImage3D,       // non type: represents image3D, uimage3D, iimage3D
    EbtGImage2DArray,  // non type: represents image2DArray, uimage2DArray, iimage2DArray
    EbtGImageCube,     // non type: represents imageCube, uimageCube, iimageCube
    EbtGuardGImageEnd,

    EbtStruct,
    EbtInterfaceBlock,
    EbtAddress,  // should be deprecated??

    EbtAtomicCounter,

    // end of list
    EbtLast
};

inline TBasicType convertGImageToFloatImage(TBasicType type)
{
    switch (type)
    {
        case EbtGImage2D:
            return EbtImage2D;
        case EbtGImage3D:
            return EbtImage3D;
        case EbtGImage2DArray:
            return EbtImage2DArray;
        case EbtGImageCube:
            return EbtImageCube;
        default:
            UNREACHABLE();
    }
    return EbtLast;
}

inline TBasicType convertGImageToIntImage(TBasicType type)
{
    switch (type)
    {
        case EbtGImage2D:
            return EbtIImage2D;
        case EbtGImage3D:
            return EbtIImage3D;
        case EbtGImage2DArray:
            return EbtIImage2DArray;
        case EbtGImageCube:
            return EbtIImageCube;
        default:
            UNREACHABLE();
    }
    return EbtLast;
}

inline TBasicType convertGImageToUnsignedImage(TBasicType type)
{
    switch (type)
    {
        case EbtGImage2D:
            return EbtUImage2D;
        case EbtGImage3D:
            return EbtUImage3D;
        case EbtGImage2DArray:
            return EbtUImage2DArray;
        case EbtGImageCube:
            return EbtUImageCube;
        default:
            UNREACHABLE();
    }
    return EbtLast;
}

const char *getBasicString(TBasicType t);

inline bool IsSampler(TBasicType type)
{
    return type > EbtGuardSamplerBegin && type < EbtGuardSamplerEnd;
}

inline bool IsImage(TBasicType type)
{
    return type > EbtGuardImageBegin && type < EbtGuardImageEnd;
}

inline bool IsGImage(TBasicType type)
{
    return type > EbtGuardGImageBegin && type < EbtGuardGImageEnd;
}

inline bool IsAtomicCounter(TBasicType type)
{
    return type == EbtAtomicCounter;
}

inline bool IsOpaqueType(TBasicType type)
{
    return IsSampler(type) || IsImage(type) || IsAtomicCounter(type);
}

inline bool IsIntegerSampler(TBasicType type)
{
    switch (type)
    {
        case EbtISampler2D:
        case EbtISampler3D:
        case EbtISamplerCube:
        case EbtISampler2DArray:
        case EbtISampler2DMS:
        case EbtUSampler2D:
        case EbtUSampler3D:
        case EbtUSamplerCube:
        case EbtUSampler2DArray:
        case EbtUSampler2DMS:
            return true;
        case EbtSampler2D:
        case EbtSampler3D:
        case EbtSamplerCube:
        case EbtSamplerExternalOES:
        case EbtSamplerExternal2DY2YEXT:
        case EbtSampler2DRect:
        case EbtSampler2DArray:
        case EbtSampler2DShadow:
        case EbtSamplerCubeShadow:
        case EbtSampler2DArrayShadow:
        case EbtSampler2DMS:
            return false;
        default:
            assert(!IsSampler(type));
    }

    return false;
}

inline bool IsSampler2DMS(TBasicType type)
{
    switch (type)
    {
        case EbtSampler2DMS:
        case EbtISampler2DMS:
        case EbtUSampler2DMS:
            return true;
        default:
            return false;
    }
}

inline bool IsFloatImage(TBasicType type)
{
    switch (type)
    {
        case EbtImage2D:
        case EbtImage3D:
        case EbtImage2DArray:
        case EbtImageCube:
            return true;
        default:
            break;
    }

    return false;
}

inline bool IsIntegerImage(TBasicType type)
{

    switch (type)
    {
        case EbtIImage2D:
        case EbtIImage3D:
        case EbtIImage2DArray:
        case EbtIImageCube:
            return true;
        default:
            break;
    }

    return false;
}

inline bool IsUnsignedImage(TBasicType type)
{

    switch (type)
    {
        case EbtUImage2D:
        case EbtUImage3D:
        case EbtUImage2DArray:
        case EbtUImageCube:
            return true;
        default:
            break;
    }

    return false;
}

inline bool IsSampler2D(TBasicType type)
{
    switch (type)
    {
        case EbtSampler2D:
        case EbtISampler2D:
        case EbtUSampler2D:
        case EbtSampler2DArray:
        case EbtISampler2DArray:
        case EbtUSampler2DArray:
        case EbtSampler2DRect:
        case EbtSamplerExternalOES:
        case EbtSamplerExternal2DY2YEXT:
        case EbtSampler2DShadow:
        case EbtSampler2DArrayShadow:
        case EbtSampler2DMS:
        case EbtISampler2DMS:
        case EbtUSampler2DMS:
            return true;
        case EbtSampler3D:
        case EbtISampler3D:
        case EbtUSampler3D:
        case EbtISamplerCube:
        case EbtUSamplerCube:
        case EbtSamplerCube:
        case EbtSamplerCubeShadow:
            return false;
        default:
            assert(!IsSampler(type));
    }

    return false;
}

inline bool IsSamplerCube(TBasicType type)
{
    switch (type)
    {
        case EbtSamplerCube:
        case EbtISamplerCube:
        case EbtUSamplerCube:
        case EbtSamplerCubeShadow:
            return true;
        case EbtSampler2D:
        case EbtSampler3D:
        case EbtSamplerExternalOES:
        case EbtSamplerExternal2DY2YEXT:
        case EbtSampler2DRect:
        case EbtSampler2DArray:
        case EbtSampler2DMS:
        case EbtISampler2D:
        case EbtISampler3D:
        case EbtISampler2DArray:
        case EbtISampler2DMS:
        case EbtUSampler2D:
        case EbtUSampler3D:
        case EbtUSampler2DArray:
        case EbtUSampler2DMS:
        case EbtSampler2DShadow:
        case EbtSampler2DArrayShadow:
            return false;
        default:
            assert(!IsSampler(type));
    }

    return false;
}

inline bool IsSampler3D(TBasicType type)
{
    switch (type)
    {
        case EbtSampler3D:
        case EbtISampler3D:
        case EbtUSampler3D:
            return true;
        case EbtSampler2D:
        case EbtSamplerCube:
        case EbtSamplerExternalOES:
        case EbtSamplerExternal2DY2YEXT:
        case EbtSampler2DRect:
        case EbtSampler2DArray:
        case EbtSampler2DMS:
        case EbtISampler2D:
        case EbtISamplerCube:
        case EbtISampler2DArray:
        case EbtISampler2DMS:
        case EbtUSampler2D:
        case EbtUSamplerCube:
        case EbtUSampler2DArray:
        case EbtUSampler2DMS:
        case EbtSampler2DShadow:
        case EbtSamplerCubeShadow:
        case EbtSampler2DArrayShadow:
            return false;
        default:
            assert(!IsSampler(type));
    }

    return false;
}

inline bool IsSamplerArray(TBasicType type)
{
    switch (type)
    {
        case EbtSampler2DArray:
        case EbtISampler2DArray:
        case EbtUSampler2DArray:
        case EbtSampler2DArrayShadow:
            return true;
        case EbtSampler2D:
        case EbtISampler2D:
        case EbtUSampler2D:
        case EbtSampler2DRect:
        case EbtSamplerExternalOES:
        case EbtSamplerExternal2DY2YEXT:
        case EbtSampler3D:
        case EbtISampler3D:
        case EbtUSampler3D:
        case EbtISamplerCube:
        case EbtUSamplerCube:
        case EbtSamplerCube:
        case EbtSampler2DShadow:
        case EbtSamplerCubeShadow:
        case EbtSampler2DMS:
        case EbtISampler2DMS:
        case EbtUSampler2DMS:
            return false;
        default:
            assert(!IsSampler(type));
    }

    return false;
}

inline bool IsShadowSampler(TBasicType type)
{
    switch (type)
    {
        case EbtSampler2DShadow:
        case EbtSamplerCubeShadow:
        case EbtSampler2DArrayShadow:
            return true;
        case EbtISampler2D:
        case EbtISampler3D:
        case EbtISamplerCube:
        case EbtISampler2DArray:
        case EbtISampler2DMS:
        case EbtUSampler2D:
        case EbtUSampler3D:
        case EbtUSamplerCube:
        case EbtUSampler2DArray:
        case EbtUSampler2DMS:
        case EbtSampler2D:
        case EbtSampler3D:
        case EbtSamplerCube:
        case EbtSamplerExternalOES:
        case EbtSamplerExternal2DY2YEXT:
        case EbtSampler2DRect:
        case EbtSampler2DArray:
        case EbtSampler2DMS:
            return false;
        default:
            assert(!IsSampler(type));
    }

    return false;
}

inline bool IsImage2D(TBasicType type)
{
    switch (type)
    {
        case EbtImage2D:
        case EbtIImage2D:
        case EbtUImage2D:
            return true;
        case EbtImage3D:
        case EbtIImage3D:
        case EbtUImage3D:
        case EbtImage2DArray:
        case EbtIImage2DArray:
        case EbtUImage2DArray:
        case EbtImageCube:
        case EbtIImageCube:
        case EbtUImageCube:
            return false;
        default:
            assert(!IsImage(type));
    }

    return false;
}

inline bool IsImage3D(TBasicType type)
{
    switch (type)
    {
        case EbtImage3D:
        case EbtIImage3D:
        case EbtUImage3D:
            return true;
        case EbtImage2D:
        case EbtIImage2D:
        case EbtUImage2D:
        case EbtImage2DArray:
        case EbtIImage2DArray:
        case EbtUImage2DArray:
        case EbtImageCube:
        case EbtIImageCube:
        case EbtUImageCube:
            return false;
        default:
            assert(!IsImage(type));
    }

    return false;
}

inline bool IsImage2DArray(TBasicType type)
{
    switch (type)
    {
        case EbtImage2DArray:
        case EbtIImage2DArray:
        case EbtUImage2DArray:
            return true;
        case EbtImage2D:
        case EbtIImage2D:
        case EbtUImage2D:
        case EbtImage3D:
        case EbtIImage3D:
        case EbtUImage3D:
        case EbtImageCube:
        case EbtIImageCube:
        case EbtUImageCube:
            return false;
        default:
            assert(!IsImage(type));
    }

    return false;
}

inline bool IsImageCube(TBasicType type)
{
    switch (type)
    {
        case EbtImageCube:
        case EbtIImageCube:
        case EbtUImageCube:
            return true;
        case EbtImage2D:
        case EbtIImage2D:
        case EbtUImage2D:
        case EbtImage3D:
        case EbtIImage3D:
        case EbtUImage3D:
        case EbtImage2DArray:
        case EbtIImage2DArray:
        case EbtUImage2DArray:
            return false;
        default:
            assert(!IsImage(type));
    }

    return false;
}

inline bool IsInteger(TBasicType type)
{
    return type == EbtInt || type == EbtUInt;
}

inline bool SupportsPrecision(TBasicType type)
{
    return type == EbtFloat || type == EbtInt || type == EbtUInt || IsOpaqueType(type);
}

//
// Qualifiers and built-ins.  These are mainly used to see what can be read
// or written, and by the machine dependent translator to know which registers
// to allocate variables in.  Since built-ins tend to go to different registers
// than varying or uniform, it makes sense they are peers, not sub-classes.
//
enum TQualifier
{
    EvqTemporary,   // For temporaries (within a function), read/write
    EvqGlobal,      // For globals read/write
    EvqConst,       // User defined constants and non-output parameters in functions
    EvqAttribute,   // Readonly
    EvqVaryingIn,   // readonly, fragment shaders only
    EvqVaryingOut,  // vertex shaders only  read/write
    EvqUniform,     // Readonly, vertex and fragment
    EvqBuffer,      // read/write, vertex, fragment and compute shader

    EvqVertexIn,     // Vertex shader input
    EvqFragmentOut,  // Fragment shader output
    EvqVertexOut,    // Vertex shader output
    EvqFragmentIn,   // Fragment shader input

    // parameters
    EvqIn,
    EvqOut,
    EvqInOut,
    EvqConstReadOnly,

    // built-ins read by vertex shader
    EvqInstanceID,
    EvqVertexID,

    // built-ins written by vertex shader
    EvqPosition,
    EvqPointSize,

    // built-ins read by fragment shader
    EvqFragCoord,
    EvqFrontFacing,
    EvqPointCoord,

    // built-ins written by fragment shader
    EvqFragColor,
    EvqFragData,

    EvqFragDepth,     // gl_FragDepth for ESSL300.
    EvqFragDepthEXT,  // gl_FragDepthEXT for ESSL100, EXT_frag_depth.

    EvqSecondaryFragColorEXT,  // EXT_blend_func_extended
    EvqSecondaryFragDataEXT,   // EXT_blend_func_extended

    EvqViewIDOVR,      // OVR_multiview
    EvqViewportIndex,  // gl_ViewportIndex

    // built-ins written by the shader_framebuffer_fetch extension(s)
    EvqLastFragColor,
    EvqLastFragData,

    // GLSL ES 3.0 vertex output and fragment input
    EvqSmooth,    // Incomplete qualifier, smooth is the default
    EvqFlat,      // Incomplete qualifier
    EvqCentroid,  // Incomplete qualifier
    EvqSmoothOut,
    EvqFlatOut,
    EvqCentroidOut,  // Implies smooth
    EvqSmoothIn,
    EvqFlatIn,
    EvqCentroidIn,  // Implies smooth

    // GLSL ES 3.1 compute shader special variables
    EvqShared,
    EvqComputeIn,
    EvqNumWorkGroups,
    EvqWorkGroupSize,
    EvqWorkGroupID,
    EvqLocalInvocationID,
    EvqGlobalInvocationID,
    EvqLocalInvocationIndex,

    // GLSL ES 3.1 memory qualifiers
    EvqReadOnly,
    EvqWriteOnly,
    EvqCoherent,
    EvqRestrict,
    EvqVolatile,

    // GLSL ES 3.1 extension OES_geometry_shader qualifiers
    EvqGeometryIn,
    EvqGeometryOut,
    EvqPerVertexIn,    // gl_in
    EvqPrimitiveIDIn,  // gl_PrimitiveIDIn
    EvqInvocationID,   // gl_InvocationID
    EvqPrimitiveID,    // gl_PrimitiveID
    EvqLayer,          // gl_Layer

    // end of list
    EvqLast
};

inline bool IsQualifierUnspecified(TQualifier qualifier)
{
    return (qualifier == EvqTemporary || qualifier == EvqGlobal);
}

enum TLayoutImageInternalFormat
{
    EiifUnspecified,
    EiifRGBA32F,
    EiifRGBA16F,
    EiifR32F,
    EiifRGBA32UI,
    EiifRGBA16UI,
    EiifRGBA8UI,
    EiifR32UI,
    EiifRGBA32I,
    EiifRGBA16I,
    EiifRGBA8I,
    EiifR32I,
    EiifRGBA8,
    EiifRGBA8_SNORM
};

enum TLayoutMatrixPacking
{
    EmpUnspecified,
    EmpRowMajor,
    EmpColumnMajor
};

enum TLayoutBlockStorage
{
    EbsUnspecified,
    EbsShared,
    EbsPacked,
    EbsStd140,
    EbsStd430
};

enum TYuvCscStandardEXT
{
    EycsUndefined,
    EycsItu601,
    EycsItu601FullRange,
    EycsItu709
};

enum TLayoutPrimitiveType
{
    EptUndefined,
    EptPoints,
    EptLines,
    EptLinesAdjacency,
    EptTriangles,
    EptTrianglesAdjacency,
    EptLineStrip,
    EptTriangleStrip
};

struct TLayoutQualifier
{
    // Must have a trivial default constructor since it is used in YYSTYPE.
    TLayoutQualifier() = default;

    constexpr static TLayoutQualifier Create() { return TLayoutQualifier(0); }

    bool isEmpty() const
    {
        return location == -1 && binding == -1 && offset == -1 && numViews == -1 && yuv == false &&
               matrixPacking == EmpUnspecified && blockStorage == EbsUnspecified &&
               !localSize.isAnyValueSet() && imageInternalFormat == EiifUnspecified &&
               primitiveType == EptUndefined && invocations == 0 && maxVertices == -1;
    }

    bool isCombinationValid() const
    {
        bool workSizeSpecified = localSize.isAnyValueSet();
        bool numViewsSet       = (numViews != -1);
        bool geometryShaderSpecified =
            (primitiveType != EptUndefined) || (invocations != 0) || (maxVertices != -1);
        bool otherLayoutQualifiersSpecified =
            (location != -1 || binding != -1 || matrixPacking != EmpUnspecified ||
             blockStorage != EbsUnspecified || imageInternalFormat != EiifUnspecified);

        // we can have either the work group size specified, or number of views,
        // or yuv layout qualifier, or the other layout qualifiers.
        return (workSizeSpecified ? 1 : 0) + (numViewsSet ? 1 : 0) + (yuv ? 1 : 0) +
                   (otherLayoutQualifiersSpecified ? 1 : 0) + (geometryShaderSpecified ? 1 : 0) <=
               1;
    }

    bool isLocalSizeEqual(const sh::WorkGroupSize &localSizeIn) const
    {
        return localSize.isWorkGroupSizeMatching(localSizeIn);
    }

    int location;
    unsigned int locationsSpecified;
    TLayoutMatrixPacking matrixPacking;
    TLayoutBlockStorage blockStorage;

    // Compute shader layout qualifiers.
    sh::WorkGroupSize localSize;

    int binding;
    int offset;

    // Image format layout qualifier
    TLayoutImageInternalFormat imageInternalFormat;

    // OVR_multiview num_views.
    int numViews;

    // EXT_YUV_target yuv layout qualifier.
    bool yuv;

    // OES_geometry_shader layout qualifiers.
    TLayoutPrimitiveType primitiveType;
    int invocations;
    int maxVertices;

  private:
    explicit constexpr TLayoutQualifier(int /*placeholder*/)
        : location(-1),
          locationsSpecified(0),
          matrixPacking(EmpUnspecified),
          blockStorage(EbsUnspecified),
          localSize(-1),
          binding(-1),
          offset(-1),
          imageInternalFormat(EiifUnspecified),
          numViews(-1),
          yuv(false),
          primitiveType(EptUndefined),
          invocations(0),
          maxVertices(-1)
    {
    }
};

struct TMemoryQualifier
{
    // Must have a trivial default constructor since it is used in YYSTYPE.
    TMemoryQualifier() = default;

    bool isEmpty() const
    {
        return !readonly && !writeonly && !coherent && !restrictQualifier && !volatileQualifier;
    }

    constexpr static TMemoryQualifier Create() { return TMemoryQualifier(0); }

    // GLSL ES 3.10 Revision 4, 4.9 Memory Access Qualifiers
    // An image can be qualified as both readonly and writeonly. It still can be can be used with
    // imageSize().
    bool readonly;
    bool writeonly;
    bool coherent;

    // restrict and volatile are reserved keywords in C/C++
    bool restrictQualifier;
    bool volatileQualifier;

  private:
    explicit constexpr TMemoryQualifier(int /*placeholder*/)
        : readonly(false),
          writeonly(false),
          coherent(false),
          restrictQualifier(false),
          volatileQualifier(false)
    {
    }
};

inline const char *getWorkGroupSizeString(size_t dimension)
{
    switch (dimension)
    {
        case 0u:
            return "local_size_x";
        case 1u:
            return "local_size_y";
        case 2u:
            return "local_size_z";
        default:
            UNREACHABLE();
            return "dimension out of bounds";
    }
}

//
// This is just for debug and error message print out, carried along with the definitions above.
//
inline const char *getQualifierString(TQualifier q)
{
    // clang-format off
    switch(q)
    {
    case EvqTemporary:              return "Temporary";
    case EvqGlobal:                 return "Global";
    case EvqConst:                  return "const";
    case EvqAttribute:              return "attribute";
    case EvqVaryingIn:              return "varying";
    case EvqVaryingOut:             return "varying";
    case EvqUniform:                return "uniform";
    case EvqBuffer:                 return "buffer";
    case EvqVertexIn:               return "in";
    case EvqFragmentOut:            return "out";
    case EvqVertexOut:              return "out";
    case EvqFragmentIn:             return "in";
    case EvqIn:                     return "in";
    case EvqOut:                    return "out";
    case EvqInOut:                  return "inout";
    case EvqConstReadOnly:          return "const";
    case EvqInstanceID:             return "InstanceID";
    case EvqVertexID:               return "VertexID";
    case EvqPosition:               return "Position";
    case EvqPointSize:              return "PointSize";
    case EvqFragCoord:              return "FragCoord";
    case EvqFrontFacing:            return "FrontFacing";
    case EvqPointCoord:             return "PointCoord";
    case EvqFragColor:              return "FragColor";
    case EvqFragData:               return "FragData";
    case EvqFragDepthEXT:           return "FragDepth";
    case EvqFragDepth:              return "FragDepth";
    case EvqSecondaryFragColorEXT:  return "SecondaryFragColorEXT";
    case EvqSecondaryFragDataEXT:   return "SecondaryFragDataEXT";
    case EvqViewIDOVR:              return "ViewIDOVR";
    case EvqViewportIndex:          return "ViewportIndex";
    case EvqLayer:                  return "Layer";
    case EvqLastFragColor:          return "LastFragColor";
    case EvqLastFragData:           return "LastFragData";
    case EvqSmoothOut:              return "smooth out";
    case EvqCentroidOut:            return "smooth centroid out";
    case EvqFlatOut:                return "flat out";
    case EvqSmoothIn:               return "smooth in";
    case EvqFlatIn:                 return "flat in";
    case EvqCentroidIn:             return "smooth centroid in";
    case EvqCentroid:               return "centroid";
    case EvqFlat:                   return "flat";
    case EvqSmooth:                 return "smooth";
    case EvqShared:                 return "shared";
    case EvqComputeIn:              return "in";
    case EvqNumWorkGroups:          return "NumWorkGroups";
    case EvqWorkGroupSize:          return "WorkGroupSize";
    case EvqWorkGroupID:            return "WorkGroupID";
    case EvqLocalInvocationID:      return "LocalInvocationID";
    case EvqGlobalInvocationID:     return "GlobalInvocationID";
    case EvqLocalInvocationIndex:   return "LocalInvocationIndex";
    case EvqReadOnly:               return "readonly";
    case EvqWriteOnly:              return "writeonly";
    case EvqGeometryIn:             return "in";
    case EvqGeometryOut:            return "out";
    case EvqPerVertexIn:            return "gl_in";
    default: UNREACHABLE();         return "unknown qualifier";
    }
    // clang-format on
}

inline const char *getMatrixPackingString(TLayoutMatrixPacking mpq)
{
    switch (mpq)
    {
        case EmpUnspecified:
            return "mp_unspecified";
        case EmpRowMajor:
            return "row_major";
        case EmpColumnMajor:
            return "column_major";
        default:
            UNREACHABLE();
            return "unknown matrix packing";
    }
}

inline const char *getBlockStorageString(TLayoutBlockStorage bsq)
{
    switch (bsq)
    {
        case EbsUnspecified:
            return "bs_unspecified";
        case EbsShared:
            return "shared";
        case EbsPacked:
            return "packed";
        case EbsStd140:
            return "std140";
        case EbsStd430:
            return "std430";
        default:
            UNREACHABLE();
            return "unknown block storage";
    }
}

inline const char *getImageInternalFormatString(TLayoutImageInternalFormat iifq)
{
    switch (iifq)
    {
        case EiifRGBA32F:
            return "rgba32f";
        case EiifRGBA16F:
            return "rgba16f";
        case EiifR32F:
            return "r32f";
        case EiifRGBA32UI:
            return "rgba32ui";
        case EiifRGBA16UI:
            return "rgba16ui";
        case EiifRGBA8UI:
            return "rgba8ui";
        case EiifR32UI:
            return "r32ui";
        case EiifRGBA32I:
            return "rgba32i";
        case EiifRGBA16I:
            return "rgba16i";
        case EiifRGBA8I:
            return "rgba8i";
        case EiifR32I:
            return "r32i";
        case EiifRGBA8:
            return "rgba8";
        case EiifRGBA8_SNORM:
            return "rgba8_snorm";
        default:
            UNREACHABLE();
            return "unknown internal image format";
    }
}

inline TYuvCscStandardEXT getYuvCscStandardEXT(const std::string &str)
{
    if (str == "itu_601")
        return EycsItu601;
    else if (str == "itu_601_full_range")
        return EycsItu601FullRange;
    else if (str == "itu_709")
        return EycsItu709;
    return EycsUndefined;
}

inline const char *getYuvCscStandardEXTString(TYuvCscStandardEXT ycsq)
{
    switch (ycsq)
    {
        case EycsItu601:
            return "itu_601";
        case EycsItu601FullRange:
            return "itu_601_full_range";
        case EycsItu709:
            return "itu_709";
        default:
            UNREACHABLE();
            return "unknown color space conversion standard";
    }
}

inline const char *getGeometryShaderPrimitiveTypeString(TLayoutPrimitiveType primitiveType)
{
    switch (primitiveType)
    {
        case EptPoints:
            return "points";
        case EptLines:
            return "lines";
        case EptTriangles:
            return "triangles";
        case EptLinesAdjacency:
            return "lines_adjacency";
        case EptTrianglesAdjacency:
            return "triangles_adjacency";
        case EptLineStrip:
            return "line_strip";
        case EptTriangleStrip:
            return "triangle_strip";
        default:
            UNREACHABLE();
            return "unknown geometry shader primitive type";
    }
}

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_BASETYPES_H_
