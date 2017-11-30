//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// renderergl_utils.cpp: Conversion functions and other utility routines
// specific to the OpenGL renderer.

#include "libANGLE/renderer/gl/renderergl_utils.h"

#include <limits>

#include "common/mathutil.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Workarounds.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/QueryGL.h"
#include "libANGLE/renderer/gl/WorkaroundsGL.h"
#include "libANGLE/renderer/gl/formatutilsgl.h"

#include <algorithm>
#include <sstream>
#include <EGL/eglext.h>

using angle::CheckedNumeric;

namespace rx
{
VendorID GetVendorID(const FunctionsGL *functions)
{
    std::string nativeVendorString(reinterpret_cast<const char *>(functions->getString(GL_VENDOR)));
    if (nativeVendorString.find("Intel") != std::string::npos)
    {
        return VENDOR_ID_INTEL;
    }
    else if (nativeVendorString.find("NVIDIA") != std::string::npos)
    {
        return VENDOR_ID_NVIDIA;
    }
    else if (nativeVendorString.find("ATI") != std::string::npos ||
             nativeVendorString.find("AMD") != std::string::npos)
    {
        return VENDOR_ID_AMD;
    }
    else if (nativeVendorString.find("Qualcomm") != std::string::npos)
    {
        return VENDOR_ID_QUALCOMM;
    }
    else
    {
        return VENDOR_ID_UNKNOWN;
    }
}

namespace nativegl_gl
{

static bool MeetsRequirements(const FunctionsGL *functions, const nativegl::SupportRequirement &requirements)
{
    for (const std::string &extension : requirements.requiredExtensions)
    {
        if (!functions->hasExtension(extension))
        {
            return false;
        }
    }

    if (functions->version >= requirements.version)
    {
        return true;
    }
    else if (!requirements.versionExtensions.empty())
    {
        for (const std::string &extension : requirements.versionExtensions)
        {
            if (!functions->hasExtension(extension))
            {
                return false;
            }
        }
        return true;
    }
    else
    {
        return false;
    }
}

static gl::TextureCaps GenerateTextureFormatCaps(const FunctionsGL *functions,
                                                 GLenum internalFormat)
{
    ASSERT(functions->getError() == GL_NO_ERROR);

    gl::TextureCaps textureCaps;

    const nativegl::InternalFormat &formatInfo = nativegl::GetInternalFormatInfo(internalFormat, functions->standard);
    textureCaps.texturable = MeetsRequirements(functions, formatInfo.texture);
    textureCaps.filterable = textureCaps.texturable && MeetsRequirements(functions, formatInfo.filter);
    textureCaps.renderable = MeetsRequirements(functions, formatInfo.framebufferAttachment);

    // glGetInternalformativ is not available until version 4.2 but may be available through the 3.0
    // extension GL_ARB_internalformat_query
    if (textureCaps.renderable && functions->getInternalformativ)
    {
        GLenum queryInternalFormat = internalFormat;

        if (internalFormat == GL_BGRA8_EXT)
        {
            // Querying GL_NUM_SAMPLE_COUNTS for GL_BGRA8_EXT generates an INVALID_ENUM on some
            // drivers.  It seems however that allocating a multisampled renderbuffer of this format
            // succeeds. To avoid breaking multisampling for this format, query the supported sample
            // counts for GL_RGBA8 instead.
            queryInternalFormat = GL_RGBA8;
        }

        GLint numSamples = 0;
        functions->getInternalformativ(GL_RENDERBUFFER, queryInternalFormat, GL_NUM_SAMPLE_COUNTS,
                                       1, &numSamples);

        if (numSamples > 0)
        {
            std::vector<GLint> samples(numSamples);
            functions->getInternalformativ(GL_RENDERBUFFER, queryInternalFormat, GL_SAMPLES,
                                           static_cast<GLsizei>(samples.size()), &samples[0]);

            if (internalFormat == GL_STENCIL_INDEX8)
            {
                // The query below does generates an error with STENCIL_INDEX8 on NVIDIA driver
                // 382.33. So for now we assume that the same sampling modes are conformant for
                // STENCIL_INDEX8 as for DEPTH24_STENCIL8. Clean this up once the driver is fixed.
                // http://anglebug.com/2059
                queryInternalFormat = GL_DEPTH24_STENCIL8;
            }
            for (size_t sampleIndex = 0; sampleIndex < samples.size(); sampleIndex++)
            {
                // Some NVIDIA drivers expose multisampling modes implemented as a combination of
                // multisampling and supersampling. These are non-conformant and should not be
                // exposed through ANGLE. Query which formats are conformant from the driver if
                // supported.
                GLint conformant = GL_TRUE;
                if (functions->getInternalformatSampleivNV)
                {
                    ASSERT(functions->getError() == GL_NO_ERROR);
                    functions->getInternalformatSampleivNV(GL_RENDERBUFFER, queryInternalFormat,
                                                           samples[sampleIndex], GL_CONFORMANT_NV,
                                                           1, &conformant);
                    // getInternalFormatSampleivNV does not work for all formats on NVIDIA Shield TV
                    // drivers. Assume that formats with large sample counts are non-conformant in
                    // case the query generates an error.
                    if (functions->getError() != GL_NO_ERROR)
                    {
                        conformant = (samples[sampleIndex] <= 8) ? GL_TRUE : GL_FALSE;
                    }
                }
                if (conformant == GL_TRUE)
                {
                    textureCaps.sampleCounts.insert(samples[sampleIndex]);
                }
            }
        }
    }

    ASSERT(functions->getError() == GL_NO_ERROR);
    return textureCaps;
}

static GLint QuerySingleGLInt(const FunctionsGL *functions, GLenum name)
{
    GLint result = 0;
    functions->getIntegerv(name, &result);
    return result;
}

static GLint QuerySingleIndexGLInt(const FunctionsGL *functions, GLenum name, GLuint index)
{
    GLint result;
    functions->getIntegeri_v(name, index, &result);
    return result;
}

static GLint QueryGLIntRange(const FunctionsGL *functions, GLenum name, size_t index)
{
    GLint result[2] = {};
    functions->getIntegerv(name, result);
    return result[index];
}

static GLint64 QuerySingleGLInt64(const FunctionsGL *functions, GLenum name)
{
    // Fall back to 32-bit int if 64-bit query is not available. This can become relevant for some
    // caps that are defined as 64-bit values in core spec, but were introduced earlier in
    // extensions as 32-bit. Triggered in some cases by RenderDoc's emulated OpenGL driver.
    if (!functions->getInteger64v)
    {
        GLint result = 0;
        functions->getIntegerv(name, &result);
        return static_cast<GLint64>(result);
    }
    else
    {
        GLint64 result = 0;
        functions->getInteger64v(name, &result);
        return result;
    }
}

static GLfloat QuerySingleGLFloat(const FunctionsGL *functions, GLenum name)
{
    GLfloat result = 0.0f;
    functions->getFloatv(name, &result);
    return result;
}

static GLfloat QueryGLFloatRange(const FunctionsGL *functions, GLenum name, size_t index)
{
    GLfloat result[2] = {};
    functions->getFloatv(name, result);
    return result[index];
}

static gl::TypePrecision QueryTypePrecision(const FunctionsGL *functions, GLenum shaderType, GLenum precisionType)
{
    gl::TypePrecision precision;
    functions->getShaderPrecisionFormat(shaderType, precisionType, precision.range.data(),
                                        &precision.precision);
    return precision;
}

static GLint QueryQueryValue(const FunctionsGL *functions, GLenum target, GLenum name)
{
    GLint result;
    functions->getQueryiv(target, name, &result);
    return result;
}

static void LimitVersion(gl::Version *curVersion, const gl::Version &maxVersion)
{
    if (*curVersion >= maxVersion)
    {
        *curVersion = maxVersion;
    }
}

void GenerateCaps(const FunctionsGL *functions,
                  const WorkaroundsGL &workarounds,
                  gl::Caps *caps,
                  gl::TextureCapsMap *textureCapsMap,
                  gl::Extensions *extensions,
                  gl::Version *maxSupportedESVersion,
                  MultiviewImplementationTypeGL *multiviewImplementationType)
{
    // Texture format support checks
    const gl::FormatSet &allFormats = gl::GetAllSizedInternalFormats();
    for (GLenum internalFormat : allFormats)
    {
        gl::TextureCaps textureCaps = GenerateTextureFormatCaps(functions, internalFormat);
        textureCapsMap->insert(internalFormat, textureCaps);

        if (gl::GetSizedInternalFormatInfo(internalFormat).compressed)
        {
            caps->compressedTextureFormats.push_back(internalFormat);
        }
    }

    // Start by assuming ES3.1 support and work down
    *maxSupportedESVersion = gl::Version(3, 1);

    // Table 6.28, implementation dependent values
    if (functions->isAtLeastGL(gl::Version(4, 3)) || functions->hasGLExtension("GL_ARB_ES3_compatibility") ||
        functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        caps->maxElementIndex = QuerySingleGLInt64(functions, GL_MAX_ELEMENT_INDEX);
    }
    else
    {
        // Doesn't affect ES3 support, can use a pre-defined limit
        caps->maxElementIndex = static_cast<GLint64>(std::numeric_limits<unsigned int>::max());
    }

    if (functions->isAtLeastGL(gl::Version(1, 2)) ||
        functions->isAtLeastGLES(gl::Version(3, 0)) || functions->hasGLESExtension("GL_OES_texture_3D"))
    {
        caps->max3DTextureSize = QuerySingleGLInt(functions, GL_MAX_3D_TEXTURE_SIZE);
    }
    else
    {
        // Can't support ES3 without 3D textures
        LimitVersion(maxSupportedESVersion, gl::Version(2, 0));
    }

    caps->max2DTextureSize = QuerySingleGLInt(functions, GL_MAX_TEXTURE_SIZE); // GL 1.0 / ES 2.0
    caps->maxCubeMapTextureSize = QuerySingleGLInt(functions, GL_MAX_CUBE_MAP_TEXTURE_SIZE); // GL 1.3 / ES 2.0

    if (functions->isAtLeastGL(gl::Version(3, 0)) || functions->hasGLExtension("GL_EXT_texture_array") ||
        functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        caps->maxArrayTextureLayers = QuerySingleGLInt(functions, GL_MAX_ARRAY_TEXTURE_LAYERS);
    }
    else
    {
        // Can't support ES3 without array textures
        LimitVersion(maxSupportedESVersion, gl::Version(2, 0));
    }

    if (functions->isAtLeastGL(gl::Version(1, 5)) || functions->hasGLExtension("GL_EXT_texture_lod_bias") ||
        functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        caps->maxLODBias = QuerySingleGLFloat(functions, GL_MAX_TEXTURE_LOD_BIAS);
    }
    else
    {
        LimitVersion(maxSupportedESVersion, gl::Version(2, 0));
    }

    if (functions->isAtLeastGL(gl::Version(3, 0)) || functions->hasGLExtension("GL_EXT_framebuffer_object") ||
        functions->isAtLeastGLES(gl::Version(2, 0)))
    {
        caps->maxRenderbufferSize = QuerySingleGLInt(functions, GL_MAX_RENDERBUFFER_SIZE);
        caps->maxColorAttachments = QuerySingleGLInt(functions, GL_MAX_COLOR_ATTACHMENTS);
    }
    else
    {
        // Can't support ES2 without framebuffers and renderbuffers
        LimitVersion(maxSupportedESVersion, gl::Version(0, 0));
    }

    if (functions->isAtLeastGL(gl::Version(2, 0)) || functions->hasGLExtension("ARB_draw_buffers") ||
        functions->isAtLeastGLES(gl::Version(3, 0)) || functions->hasGLESExtension("GL_EXT_draw_buffers"))
    {
        caps->maxDrawBuffers = QuerySingleGLInt(functions, GL_MAX_DRAW_BUFFERS);
    }
    else
    {
        // Framebuffer is required to have at least one drawbuffer even if the extension is not
        // supported
        caps->maxDrawBuffers = 1;
        LimitVersion(maxSupportedESVersion, gl::Version(2, 0));
    }

    caps->maxViewportWidth = QueryGLIntRange(functions, GL_MAX_VIEWPORT_DIMS, 0); // GL 1.0 / ES 2.0
    caps->maxViewportHeight = QueryGLIntRange(functions, GL_MAX_VIEWPORT_DIMS, 1); // GL 1.0 / ES 2.0

    if (functions->standard == STANDARD_GL_DESKTOP &&
        (functions->profile & GL_CONTEXT_CORE_PROFILE_BIT) != 0)
    {
        // Desktop GL core profile deprecated the GL_ALIASED_POINT_SIZE_RANGE query.  Use
        // GL_POINT_SIZE_RANGE instead.
        caps->minAliasedPointSize = QueryGLFloatRange(functions, GL_POINT_SIZE_RANGE, 0);
        caps->maxAliasedPointSize = QueryGLFloatRange(functions, GL_POINT_SIZE_RANGE, 1);
    }
    else
    {
        caps->minAliasedPointSize = QueryGLFloatRange(functions, GL_ALIASED_POINT_SIZE_RANGE, 0);
        caps->maxAliasedPointSize = QueryGLFloatRange(functions, GL_ALIASED_POINT_SIZE_RANGE, 1);
    }

    caps->minAliasedLineWidth = QueryGLFloatRange(functions, GL_ALIASED_LINE_WIDTH_RANGE, 0); // GL 1.2 / ES 2.0
    caps->maxAliasedLineWidth = QueryGLFloatRange(functions, GL_ALIASED_LINE_WIDTH_RANGE, 1); // GL 1.2 / ES 2.0

    // Table 6.29, implementation dependent values (cont.)
    if (functions->isAtLeastGL(gl::Version(1, 2)) ||
        functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        caps->maxElementsIndices = QuerySingleGLInt(functions, GL_MAX_ELEMENTS_INDICES);
        caps->maxElementsVertices = QuerySingleGLInt(functions, GL_MAX_ELEMENTS_VERTICES);
    }
    else
    {
        // Doesn't impact supported version
    }

    if (functions->isAtLeastGL(gl::Version(4, 1)) ||
        functions->hasGLExtension("GL_ARB_get_program_binary") ||
        functions->isAtLeastGLES(gl::Version(3, 0)) ||
        functions->hasGLExtension("GL_OES_get_program_binary"))
    {
        // Able to support the GL_PROGRAM_BINARY_ANGLE format as long as another program binary
        // format is available.
        GLint numBinaryFormats = QuerySingleGLInt(functions, GL_NUM_PROGRAM_BINARY_FORMATS_OES);
        if (numBinaryFormats > 0)
        {
            caps->programBinaryFormats.push_back(GL_PROGRAM_BINARY_ANGLE);
        }
    }
    else
    {
        // Doesn't impact supported version
    }

    // glGetShaderPrecisionFormat is not available until desktop GL version 4.1 or GL_ARB_ES2_compatibility exists
    if (functions->isAtLeastGL(gl::Version(4, 1)) || functions->hasGLExtension("GL_ARB_ES2_compatibility") ||
        functions->isAtLeastGLES(gl::Version(2, 0)))
    {
        caps->vertexHighpFloat = QueryTypePrecision(functions, GL_VERTEX_SHADER, GL_HIGH_FLOAT);
        caps->vertexMediumpFloat = QueryTypePrecision(functions, GL_VERTEX_SHADER, GL_MEDIUM_FLOAT);
        caps->vertexLowpFloat = QueryTypePrecision(functions, GL_VERTEX_SHADER, GL_LOW_FLOAT);
        caps->fragmentHighpFloat = QueryTypePrecision(functions, GL_FRAGMENT_SHADER, GL_HIGH_FLOAT);
        caps->fragmentMediumpFloat = QueryTypePrecision(functions, GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT);
        caps->fragmentLowpFloat = QueryTypePrecision(functions, GL_FRAGMENT_SHADER, GL_LOW_FLOAT);
        caps->vertexHighpInt = QueryTypePrecision(functions, GL_VERTEX_SHADER, GL_HIGH_INT);
        caps->vertexMediumpInt = QueryTypePrecision(functions, GL_VERTEX_SHADER, GL_MEDIUM_INT);
        caps->vertexLowpInt = QueryTypePrecision(functions, GL_VERTEX_SHADER, GL_LOW_INT);
        caps->fragmentHighpInt = QueryTypePrecision(functions, GL_FRAGMENT_SHADER, GL_HIGH_INT);
        caps->fragmentMediumpInt = QueryTypePrecision(functions, GL_FRAGMENT_SHADER, GL_MEDIUM_INT);
        caps->fragmentLowpInt = QueryTypePrecision(functions, GL_FRAGMENT_SHADER, GL_LOW_INT);
    }
    else
    {
        // Doesn't impact supported version, set some default values
        caps->vertexHighpFloat.setIEEEFloat();
        caps->vertexMediumpFloat.setIEEEFloat();
        caps->vertexLowpFloat.setIEEEFloat();
        caps->fragmentHighpFloat.setIEEEFloat();
        caps->fragmentMediumpFloat.setIEEEFloat();
        caps->fragmentLowpFloat.setIEEEFloat();
        caps->vertexHighpInt.setTwosComplementInt(32);
        caps->vertexMediumpInt.setTwosComplementInt(32);
        caps->vertexLowpInt.setTwosComplementInt(32);
        caps->fragmentHighpInt.setTwosComplementInt(32);
        caps->fragmentMediumpInt.setTwosComplementInt(32);
        caps->fragmentLowpInt.setTwosComplementInt(32);
    }

    if (functions->isAtLeastGL(gl::Version(3, 2)) || functions->hasGLExtension("GL_ARB_sync") ||
        functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        caps->maxServerWaitTimeout = QuerySingleGLInt64(functions, GL_MAX_SERVER_WAIT_TIMEOUT);
    }
    else
    {
        LimitVersion(maxSupportedESVersion, gl::Version(2, 0));
    }

    // Table 6.31, implementation dependent vertex shader limits
    if (functions->isAtLeastGL(gl::Version(2, 0)) ||
        functions->isAtLeastGLES(gl::Version(2, 0)))
    {
        caps->maxVertexAttributes = QuerySingleGLInt(functions, GL_MAX_VERTEX_ATTRIBS);
        caps->maxVertexUniformComponents = QuerySingleGLInt(functions, GL_MAX_VERTEX_UNIFORM_COMPONENTS);
        caps->maxVertexTextureImageUnits = QuerySingleGLInt(functions, GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
    }
    else
    {
        // Can't support ES2 version without these caps
        LimitVersion(maxSupportedESVersion, gl::Version(0, 0));
    }

    if (functions->isAtLeastGL(gl::Version(4, 1)) || functions->hasGLExtension("GL_ARB_ES2_compatibility") ||
        functions->isAtLeastGLES(gl::Version(2, 0)))
    {
        caps->maxVertexUniformVectors = QuerySingleGLInt(functions, GL_MAX_VERTEX_UNIFORM_VECTORS);
        caps->maxFragmentUniformVectors =
            QuerySingleGLInt(functions, GL_MAX_FRAGMENT_UNIFORM_VECTORS);
    }
    else
    {
        // Doesn't limit ES version, GL_MAX_VERTEX_UNIFORM_COMPONENTS / 4 is acceptable.
        caps->maxVertexUniformVectors = caps->maxVertexUniformComponents / 4;
        // Doesn't limit ES version, GL_MAX_FRAGMENT_UNIFORM_COMPONENTS / 4 is acceptable.
        caps->maxFragmentUniformVectors = caps->maxFragmentUniformComponents / 4;
    }

    if (functions->isAtLeastGL(gl::Version(3, 2)) ||
        functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        caps->maxVertexOutputComponents = QuerySingleGLInt(functions, GL_MAX_VERTEX_OUTPUT_COMPONENTS);
    }
    else
    {
        // There doesn't seem, to be a desktop extension to add this cap, maybe it could be given a safe limit
        // instead of limiting the supported ES version.
        LimitVersion(maxSupportedESVersion, gl::Version(2, 0));
    }

    // Table 6.32, implementation dependent fragment shader limits
    if (functions->isAtLeastGL(gl::Version(2, 0)) ||
        functions->isAtLeastGLES(gl::Version(2, 0)))
    {
        caps->maxFragmentUniformComponents = QuerySingleGLInt(functions, GL_MAX_FRAGMENT_UNIFORM_COMPONENTS);
        caps->maxTextureImageUnits = QuerySingleGLInt(functions, GL_MAX_TEXTURE_IMAGE_UNITS);
    }
    else
    {
        // Can't support ES2 version without these caps
        LimitVersion(maxSupportedESVersion, gl::Version(0, 0));
    }

    if (functions->isAtLeastGL(gl::Version(3, 2)) ||
        functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        caps->maxFragmentInputComponents = QuerySingleGLInt(functions, GL_MAX_FRAGMENT_INPUT_COMPONENTS);
    }
    else
    {
        // There doesn't seem, to be a desktop extension to add this cap, maybe it could be given a safe limit
        // instead of limiting the supported ES version.
        LimitVersion(maxSupportedESVersion, gl::Version(2, 0));
    }

    if (functions->isAtLeastGL(gl::Version(3, 0)) ||
        functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        caps->minProgramTexelOffset = QuerySingleGLInt(functions, GL_MIN_PROGRAM_TEXEL_OFFSET);
        caps->maxProgramTexelOffset = QuerySingleGLInt(functions, GL_MAX_PROGRAM_TEXEL_OFFSET);
    }
    else
    {
        // Can't support ES3 without texel offset, could possibly be emulated in the shader
        LimitVersion(maxSupportedESVersion, gl::Version(2, 0));
    }

    // Table 6.33, implementation dependent aggregate shader limits
    if (functions->isAtLeastGL(gl::Version(3, 1)) || functions->hasGLExtension("GL_ARB_uniform_buffer_object") ||
        functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        caps->maxVertexUniformBlocks = QuerySingleGLInt(functions, GL_MAX_VERTEX_UNIFORM_BLOCKS);
        caps->maxFragmentUniformBlocks =
            QuerySingleGLInt(functions, GL_MAX_FRAGMENT_UNIFORM_BLOCKS);
        caps->maxUniformBufferBindings = QuerySingleGLInt(functions, GL_MAX_UNIFORM_BUFFER_BINDINGS);
        caps->maxUniformBlockSize = QuerySingleGLInt64(functions, GL_MAX_UNIFORM_BLOCK_SIZE);
        caps->uniformBufferOffsetAlignment = QuerySingleGLInt(functions, GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);

        GLuint maxCombinedUniformBlocks =
            QuerySingleGLInt(functions, GL_MAX_COMBINED_UNIFORM_BLOCKS);
        // The real cap contains the limits for shader types that are not available to ES, so limit
        // the cap to the sum of vertex+fragment shader caps.
        caps->maxCombinedUniformBlocks =
            std::min(maxCombinedUniformBlocks,
                     caps->maxVertexUniformBlocks + caps->maxFragmentUniformBlocks);

        caps->maxCombinedVertexUniformComponents = QuerySingleGLInt64(functions, GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS);
        caps->maxCombinedFragmentUniformComponents = QuerySingleGLInt64(functions, GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS);
    }
    else
    {
        // Can't support ES3 without uniform blocks
        LimitVersion(maxSupportedESVersion, gl::Version(2, 0));
    }

    if (functions->isAtLeastGL(gl::Version(3, 2)) &&
        (functions->profile & GL_CONTEXT_CORE_PROFILE_BIT) != 0)
    {
        caps->maxVaryingComponents = QuerySingleGLInt(functions, GL_MAX_VERTEX_OUTPUT_COMPONENTS);
    }
    else if (functions->isAtLeastGL(gl::Version(3, 0)) ||
             functions->hasGLExtension("GL_ARB_ES2_compatibility") ||
             functions->isAtLeastGLES(gl::Version(2, 0)))
    {
        caps->maxVaryingComponents = QuerySingleGLInt(functions, GL_MAX_VARYING_COMPONENTS);
    }
    else if (functions->isAtLeastGL(gl::Version(2, 0)))
    {
        caps->maxVaryingComponents = QuerySingleGLInt(functions, GL_MAX_VARYING_FLOATS);
        LimitVersion(maxSupportedESVersion, gl::Version(2, 0));
    }
    else
    {
        LimitVersion(maxSupportedESVersion, gl::Version(0, 0));
    }

    if (functions->isAtLeastGL(gl::Version(4, 1)) || functions->hasGLExtension("GL_ARB_ES2_compatibility") ||
        functions->isAtLeastGLES(gl::Version(2, 0)))
    {
        caps->maxVaryingVectors = QuerySingleGLInt(functions, GL_MAX_VARYING_VECTORS);
    }
    else
    {
        // Doesn't limit ES version, GL_MAX_VARYING_COMPONENTS / 4 is acceptable.
        caps->maxVaryingVectors = caps->maxVaryingComponents / 4;
    }

    // Determine the max combined texture image units by adding the vertex and fragment limits.  If
    // the real cap is queried, it would contain the limits for shader types that are not available to ES.
    caps->maxCombinedTextureImageUnits = caps->maxVertexTextureImageUnits + caps->maxTextureImageUnits;

    // Table 6.34, implementation dependent transform feedback limits
    if (functions->isAtLeastGL(gl::Version(4, 0)) ||
        functions->hasGLExtension("GL_ARB_transform_feedback2") ||
        functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        caps->maxTransformFeedbackInterleavedComponents = QuerySingleGLInt(functions, GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS);
        caps->maxTransformFeedbackSeparateAttributes = QuerySingleGLInt(functions, GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS);
        caps->maxTransformFeedbackSeparateComponents = QuerySingleGLInt(functions, GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS);
    }
    else
    {
        // Can't support ES3 without transform feedback
        LimitVersion(maxSupportedESVersion, gl::Version(2, 0));
    }

    // Table 6.35, Framebuffer Dependent Values
    if (functions->isAtLeastGL(gl::Version(3, 0)) || functions->hasGLExtension("GL_EXT_framebuffer_multisample") ||
        functions->isAtLeastGLES(gl::Version(3, 0)) || functions->hasGLESExtension("GL_EXT_multisampled_render_to_texture"))
    {
        caps->maxSamples = QuerySingleGLInt(functions, GL_MAX_SAMPLES);
    }
    else
    {
        LimitVersion(maxSupportedESVersion, gl::Version(2, 0));
    }

    // Non-constant sampler array indexing is required for OpenGL ES 2 and OpenGL ES after 3.2.
    // However having it available on OpenGL ES 2 is a specification bug, and using this
    // indexing in WebGL is undefined. Requiring this feature would break WebGL 1 for some users
    // so we don't check for it. (it is present with ESSL 100, ESSL >= 320, GLSL >= 400 and
    // GL_ARB_gpu_shader5)

    // Check if sampler objects are supported
    if (!functions->isAtLeastGL(gl::Version(3, 3)) &&
        !functions->hasGLExtension("GL_ARB_sampler_objects") &&
        !functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        // Can't support ES3 without sampler objects
        LimitVersion(maxSupportedESVersion, gl::Version(2, 0));
    }

    // Can't support ES3 without texture swizzling
    if (!functions->isAtLeastGL(gl::Version(3, 3)) &&
        !functions->hasGLExtension("GL_ARB_texture_swizzle") &&
        !functions->hasGLExtension("GL_EXT_texture_swizzle") &&
        !functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        LimitVersion(maxSupportedESVersion, gl::Version(2, 0));

        // Texture swizzling is required to work around the luminance texture format not being
        // present in the core profile
        if (functions->profile & GL_CONTEXT_CORE_PROFILE_BIT)
        {
            LimitVersion(maxSupportedESVersion, gl::Version(0, 0));
        }
    }

    // Can't support ES3 without the GLSL packing builtins. We have a workaround for all
    // desktop OpenGL versions starting from 3.3 with the bit packing extension.
    if (!functions->isAtLeastGL(gl::Version(4, 2)) &&
        !(functions->isAtLeastGL(gl::Version(3, 2)) &&
          functions->hasGLExtension("GL_ARB_shader_bit_encoding")) &&
        !functions->hasGLExtension("GL_ARB_shading_language_packing") &&
        !functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        LimitVersion(maxSupportedESVersion, gl::Version(2, 0));
    }

    // ES3 needs to support explicit layout location qualifiers, while it might be possible to
    // fake them in our side, we currently don't support that.
    if (!functions->isAtLeastGL(gl::Version(3, 3)) &&
        !functions->hasGLExtension("GL_ARB_explicit_attrib_location") &&
        !functions->isAtLeastGLES(gl::Version(3, 0)))
    {
        LimitVersion(maxSupportedESVersion, gl::Version(2, 0));
    }

    if (functions->isAtLeastGL(gl::Version(4, 3)) || functions->isAtLeastGLES(gl::Version(3, 1)) ||
        functions->hasGLExtension("GL_ARB_framebuffer_no_attachments"))
    {
        caps->maxFramebufferWidth   = QuerySingleGLInt(functions, GL_MAX_FRAMEBUFFER_WIDTH);
        caps->maxFramebufferHeight  = QuerySingleGLInt(functions, GL_MAX_FRAMEBUFFER_HEIGHT);
        caps->maxFramebufferSamples = QuerySingleGLInt(functions, GL_MAX_FRAMEBUFFER_SAMPLES);
    }
    else
    {
        LimitVersion(maxSupportedESVersion, gl::Version(3, 0));
    }

    if (functions->isAtLeastGL(gl::Version(3, 2)) || functions->isAtLeastGLES(gl::Version(3, 1)) ||
        functions->hasGLExtension("GL_ARB_texture_multisample"))
    {
        caps->maxSampleMaskWords     = QuerySingleGLInt(functions, GL_MAX_SAMPLE_MASK_WORDS);
        caps->maxColorTextureSamples = QuerySingleGLInt(functions, GL_MAX_COLOR_TEXTURE_SAMPLES);
        caps->maxDepthTextureSamples = QuerySingleGLInt(functions, GL_MAX_DEPTH_TEXTURE_SAMPLES);
        caps->maxIntegerSamples      = QuerySingleGLInt(functions, GL_MAX_INTEGER_SAMPLES);
    }
    else
    {
        LimitVersion(maxSupportedESVersion, gl::Version(3, 0));
    }

    if (functions->isAtLeastGL(gl::Version(4, 3)) || functions->isAtLeastGLES(gl::Version(3, 1)) ||
        functions->hasGLExtension("GL_ARB_vertex_attrib_binding"))
    {
        caps->maxVertexAttribRelativeOffset =
            QuerySingleGLInt(functions, GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET);
        caps->maxVertexAttribBindings = QuerySingleGLInt(functions, GL_MAX_VERTEX_ATTRIB_BINDINGS);

        // OpenGL 4.3 has no limit on maximum value of stride.
        // [OpenGL 4.3 (Core Profile) - February 14, 2013] Chapter 10.3.1 Page 298
        if (workarounds.emulateMaxVertexAttribStride ||
            (functions->standard == STANDARD_GL_DESKTOP && functions->version == gl::Version(4, 3)))
        {
            caps->maxVertexAttribStride = 2048;
        }
        else
        {
            caps->maxVertexAttribStride = QuerySingleGLInt(functions, GL_MAX_VERTEX_ATTRIB_STRIDE);
        }
    }
    else
    {
        LimitVersion(maxSupportedESVersion, gl::Version(3, 0));
    }

    if (functions->isAtLeastGL(gl::Version(4, 3)) || functions->isAtLeastGLES(gl::Version(3, 1)) ||
        functions->hasGLExtension("GL_ARB_shader_storage_buffer_object"))
    {
        caps->maxCombinedShaderOutputResources =
            QuerySingleGLInt(functions, GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES);
        caps->maxFragmentShaderStorageBlocks =
            QuerySingleGLInt(functions, GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS);
        caps->maxVertexShaderStorageBlocks =
            QuerySingleGLInt(functions, GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS);
        caps->maxShaderStorageBufferBindings =
            QuerySingleGLInt(functions, GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS);
        caps->maxShaderStorageBlockSize =
            QuerySingleGLInt64(functions, GL_MAX_SHADER_STORAGE_BLOCK_SIZE);
        caps->maxCombinedShaderStorageBlocks =
            QuerySingleGLInt(functions, GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS);
        caps->shaderStorageBufferOffsetAlignment =
            QuerySingleGLInt(functions, GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT);
    }
    else
    {
        LimitVersion(maxSupportedESVersion, gl::Version(3, 0));
    }

    // OpenGL 4.2 is required for GL_ARB_compute_shader, some platform drivers have the extension,
    // but their maximum supported GL versions are less than 4.2. Explicitly limit the minimum
    // GL version to 4.2.
    if (functions->isAtLeastGL(gl::Version(4, 3)) || functions->isAtLeastGLES(gl::Version(3, 1)) ||
        (functions->isAtLeastGL(gl::Version(4, 2)) &&
         functions->hasGLExtension("GL_ARB_compute_shader") &&
         functions->hasGLExtension("GL_ARB_shader_storage_buffer_object")))
    {
        for (GLuint index = 0u; index < 3u; ++index)
        {
            caps->maxComputeWorkGroupCount[index] =
                QuerySingleIndexGLInt(functions, GL_MAX_COMPUTE_WORK_GROUP_COUNT, index);

            caps->maxComputeWorkGroupSize[index] =
                QuerySingleIndexGLInt(functions, GL_MAX_COMPUTE_WORK_GROUP_SIZE, index);
        }
        caps->maxComputeWorkGroupInvocations =
            QuerySingleGLInt(functions, GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS);
        caps->maxComputeUniformBlocks = QuerySingleGLInt(functions, GL_MAX_COMPUTE_UNIFORM_BLOCKS);
        caps->maxComputeTextureImageUnits =
            QuerySingleGLInt(functions, GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS);
        caps->maxComputeSharedMemorySize =
            QuerySingleGLInt(functions, GL_MAX_COMPUTE_SHARED_MEMORY_SIZE);
        caps->maxComputeUniformComponents =
            QuerySingleGLInt(functions, GL_MAX_COMPUTE_UNIFORM_COMPONENTS);
        caps->maxComputeAtomicCounterBuffers =
            QuerySingleGLInt(functions, GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS);
        caps->maxComputeAtomicCounters =
            QuerySingleGLInt(functions, GL_MAX_COMPUTE_ATOMIC_COUNTERS);
        caps->maxComputeImageUniforms = QuerySingleGLInt(functions, GL_MAX_COMPUTE_IMAGE_UNIFORMS);
        caps->maxCombinedComputeUniformComponents =
            QuerySingleGLInt(functions, GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS);
        caps->maxComputeShaderStorageBlocks =
            QuerySingleGLInt(functions, GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS);
    }
    else
    {
        LimitVersion(maxSupportedESVersion, gl::Version(3, 0));
    }

    if (functions->isAtLeastGL(gl::Version(4, 3)) || functions->isAtLeastGLES(gl::Version(3, 1)) ||
        functions->hasGLExtension("GL_ARB_explicit_uniform_location"))
    {
        caps->maxUniformLocations = QuerySingleGLInt(functions, GL_MAX_UNIFORM_LOCATIONS);
    }
    else
    {
        LimitVersion(maxSupportedESVersion, gl::Version(3, 0));
    }

    if (functions->isAtLeastGL(gl::Version(4, 0)) || functions->isAtLeastGLES(gl::Version(3, 1)) ||
        functions->hasGLExtension("GL_ARB_texture_gather"))
    {
        caps->minProgramTextureGatherOffset =
            QuerySingleGLInt(functions, GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET);
        caps->maxProgramTextureGatherOffset =
            QuerySingleGLInt(functions, GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET);
    }
    else
    {
        LimitVersion(maxSupportedESVersion, gl::Version(3, 0));
    }

    if (functions->isAtLeastGL(gl::Version(4, 2)) || functions->isAtLeastGLES(gl::Version(3, 1)) ||
        functions->hasGLExtension("GL_ARB_shader_image_load_store"))
    {
        caps->maxVertexImageUniforms = QuerySingleGLInt(functions, GL_MAX_VERTEX_IMAGE_UNIFORMS);
        caps->maxFragmentImageUniforms =
            QuerySingleGLInt(functions, GL_MAX_FRAGMENT_IMAGE_UNIFORMS);
        caps->maxImageUnits = QuerySingleGLInt(functions, GL_MAX_IMAGE_UNITS);
        caps->maxCombinedImageUniforms =
            QuerySingleGLInt(functions, GL_MAX_COMBINED_IMAGE_UNIFORMS);
    }
    else
    {
        LimitVersion(maxSupportedESVersion, gl::Version(3, 0));
    }

    if (functions->isAtLeastGL(gl::Version(4, 2)) || functions->isAtLeastGLES(gl::Version(3, 1)) ||
        functions->hasGLExtension("GL_ARB_shader_atomic_counters"))
    {
        caps->maxVertexAtomicCounterBuffers =
            QuerySingleGLInt(functions, GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS);
        caps->maxVertexAtomicCounters = QuerySingleGLInt(functions, GL_MAX_VERTEX_ATOMIC_COUNTERS);
        caps->maxFragmentAtomicCounterBuffers =
            QuerySingleGLInt(functions, GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS);
        caps->maxFragmentAtomicCounters =
            QuerySingleGLInt(functions, GL_MAX_FRAGMENT_ATOMIC_COUNTERS);
        caps->maxAtomicCounterBufferBindings =
            QuerySingleGLInt(functions, GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS);
        caps->maxAtomicCounterBufferSize =
            QuerySingleGLInt(functions, GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE);
        caps->maxCombinedAtomicCounterBuffers =
            QuerySingleGLInt(functions, GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS);
        caps->maxCombinedAtomicCounters =
            QuerySingleGLInt(functions, GL_MAX_COMBINED_ATOMIC_COUNTERS);
    }
    else
    {
        LimitVersion(maxSupportedESVersion, gl::Version(3, 0));
    }

    // TODO(geofflang): The gl-uniform-arrays WebGL conformance test struggles to complete on time
    // if the max uniform vectors is too large.  Artificially limit the maximum until the test is
    // updated.
    caps->maxVertexUniformVectors = std::min(1024u, caps->maxVertexUniformVectors);
    caps->maxVertexUniformComponents =
        std::min(caps->maxVertexUniformVectors * 4, caps->maxVertexUniformComponents);
    caps->maxFragmentUniformVectors = std::min(1024u, caps->maxFragmentUniformVectors);
    caps->maxFragmentUniformComponents =
        std::min(caps->maxFragmentUniformVectors * 4, caps->maxFragmentUniformComponents);

    // If it is not possible to support reading buffer data back, a shadow copy of the buffers must
    // be held. This disallows writing to buffers indirectly through transform feedback, thus
    // disallowing ES3.
    if (!CanMapBufferForRead(functions))
    {
        LimitVersion(maxSupportedESVersion, gl::Version(2, 0));
    }

    // Extension support
    extensions->setTextureExtensionSupport(*textureCapsMap);
    extensions->elementIndexUint = functions->standard == STANDARD_GL_DESKTOP ||
                                   functions->isAtLeastGLES(gl::Version(3, 0)) || functions->hasGLESExtension("GL_OES_element_index_uint");
    extensions->getProgramBinary = caps->programBinaryFormats.size() > 0;
    extensions->readFormatBGRA = functions->isAtLeastGL(gl::Version(1, 2)) || functions->hasGLExtension("GL_EXT_bgra") ||
                                 functions->hasGLESExtension("GL_EXT_read_format_bgra");
    extensions->mapBuffer = functions->isAtLeastGL(gl::Version(1, 5)) ||
                            functions->isAtLeastGLES(gl::Version(3, 0)) ||
                            functions->hasGLESExtension("GL_OES_mapbuffer");
    extensions->mapBufferRange = functions->isAtLeastGL(gl::Version(3, 0)) || functions->hasGLExtension("GL_ARB_map_buffer_range") ||
                                 functions->isAtLeastGLES(gl::Version(3, 0)) || functions->hasGLESExtension("GL_EXT_map_buffer_range");
    extensions->textureNPOT = functions->standard == STANDARD_GL_DESKTOP ||
                              functions->isAtLeastGLES(gl::Version(3, 0)) || functions->hasGLESExtension("GL_OES_texture_npot");
    // TODO(jmadill): Investigate emulating EXT_draw_buffers on ES 3.0's core functionality.
    extensions->drawBuffers = functions->isAtLeastGL(gl::Version(2, 0)) ||
                              functions->hasGLExtension("ARB_draw_buffers") ||
                              functions->hasGLESExtension("GL_EXT_draw_buffers");
    extensions->textureStorage = functions->standard == STANDARD_GL_DESKTOP ||
                                 functions->hasGLESExtension("GL_EXT_texture_storage");
    extensions->textureFilterAnisotropic = functions->hasGLExtension("GL_EXT_texture_filter_anisotropic") || functions->hasGLESExtension("GL_EXT_texture_filter_anisotropic");
    extensions->occlusionQueryBoolean    = nativegl::SupportsOcclusionQueries(functions);
    extensions->maxTextureAnisotropy = extensions->textureFilterAnisotropic ? QuerySingleGLFloat(functions, GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT) : 0.0f;
    extensions->fence = functions->hasGLExtension("GL_NV_fence") || functions->hasGLESExtension("GL_NV_fence");
    extensions->blendMinMax = functions->isAtLeastGL(gl::Version(1, 5)) || functions->hasGLExtension("GL_EXT_blend_minmax") ||
                              functions->isAtLeastGLES(gl::Version(3, 0)) || functions->hasGLESExtension("GL_EXT_blend_minmax");
    extensions->framebufferBlit = (functions->blitFramebuffer != nullptr);
    extensions->framebufferMultisample = caps->maxSamples > 0;
    extensions->standardDerivatives    = functions->isAtLeastGL(gl::Version(2, 0)) ||
                                      functions->hasGLExtension("GL_ARB_fragment_shader") ||
                                      functions->hasGLESExtension("GL_OES_standard_derivatives");
    extensions->shaderTextureLOD = functions->isAtLeastGL(gl::Version(3, 0)) ||
                                   functions->hasGLExtension("GL_ARB_shader_texture_lod") ||
                                   functions->hasGLESExtension("GL_EXT_shader_texture_lod");
    extensions->fragDepth = functions->standard == STANDARD_GL_DESKTOP ||
                            functions->hasGLESExtension("GL_EXT_frag_depth");

    if (functions->hasGLExtension("GL_NV_viewport_array2"))
    {
        extensions->multiview = true;
        // GL_MAX_ARRAY_TEXTURE_LAYERS is guaranteed to be at least 256.
        const int maxLayers = QuerySingleGLInt(functions, GL_MAX_ARRAY_TEXTURE_LAYERS);
        // GL_MAX_VIEWPORTS is guaranteed to be at least 16.
        const int maxViewports       = QuerySingleGLInt(functions, GL_MAX_VIEWPORTS);
        extensions->maxViews         = static_cast<GLuint>(
            std::min(static_cast<int>(gl::IMPLEMENTATION_ANGLE_MULTIVIEW_MAX_VIEWS),
                     std::min(maxLayers, maxViewports)));
        *multiviewImplementationType = MultiviewImplementationTypeGL::NV_VIEWPORT_ARRAY2;
    }

    extensions->fboRenderMipmap = functions->isAtLeastGL(gl::Version(3, 0)) || functions->hasGLExtension("GL_EXT_framebuffer_object") ||
                                  functions->isAtLeastGLES(gl::Version(3, 0)) || functions->hasGLESExtension("GL_OES_fbo_render_mipmap");
    extensions->instancedArrays = functions->isAtLeastGL(gl::Version(3, 1)) ||
                                  (functions->hasGLExtension("GL_ARB_instanced_arrays") &&
                                   (functions->hasGLExtension("GL_ARB_draw_instanced") ||
                                    functions->hasGLExtension("GL_EXT_draw_instanced"))) ||
                                  functions->isAtLeastGLES(gl::Version(3, 0)) ||
                                  functions->hasGLESExtension("GL_EXT_instanced_arrays");
    extensions->unpackSubimage = functions->standard == STANDARD_GL_DESKTOP ||
                                 functions->isAtLeastGLES(gl::Version(3, 0)) ||
                                 functions->hasGLESExtension("GL_EXT_unpack_subimage");
    extensions->packSubimage = functions->standard == STANDARD_GL_DESKTOP ||
                               functions->isAtLeastGLES(gl::Version(3, 0)) ||
                               functions->hasGLESExtension("GL_NV_pack_subimage");
    extensions->vertexArrayObject = functions->isAtLeastGL(gl::Version(3, 0)) ||
                                    functions->hasGLExtension("GL_ARB_vertex_array_object") ||
                                    functions->isAtLeastGLES(gl::Version(3, 0)) ||
                                    functions->hasGLESExtension("GL_OES_vertex_array_object");
    extensions->debugMarker = functions->isAtLeastGL(gl::Version(4, 3)) ||
                              functions->hasGLExtension("GL_KHR_debug") ||
                              functions->hasGLExtension("GL_EXT_debug_marker") ||
                              functions->isAtLeastGLES(gl::Version(3, 2)) ||
                              functions->hasGLESExtension("GL_KHR_debug") ||
                              functions->hasGLESExtension("GL_EXT_debug_marker");
    if (functions->isAtLeastGL(gl::Version(3, 3)) ||
        functions->hasGLExtension("GL_ARB_timer_query") ||
        functions->hasGLESExtension("GL_EXT_disjoint_timer_query"))
    {
        extensions->disjointTimerQuery = true;
        extensions->queryCounterBitsTimeElapsed =
            QueryQueryValue(functions, GL_TIME_ELAPSED, GL_QUERY_COUNTER_BITS);
        extensions->queryCounterBitsTimestamp =
            QueryQueryValue(functions, GL_TIMESTAMP, GL_QUERY_COUNTER_BITS);
    }

    // the EXT_multisample_compatibility is written against ES3.1 but can apply
    // to earlier versions so therefore we're only checking for the extension string
    // and not the specific GLES version.
    extensions->multisampleCompatibility = functions->isAtLeastGL(gl::Version(1, 3)) ||
        functions->hasGLESExtension("GL_EXT_multisample_compatibility");

    extensions->framebufferMixedSamples =
        functions->hasGLExtension("GL_NV_framebuffer_mixed_samples") ||
        functions->hasGLESExtension("GL_NV_framebuffer_mixed_samples");

    extensions->robustness = functions->isAtLeastGL(gl::Version(4, 5)) ||
                             functions->hasGLExtension("GL_KHR_robustness") ||
                             functions->hasGLExtension("GL_ARB_robustness") ||
                             functions->isAtLeastGLES(gl::Version(3, 2)) ||
                             functions->hasGLESExtension("GL_KHR_robustness") ||
                             functions->hasGLESExtension("GL_EXT_robustness");

    extensions->robustBufferAccessBehavior =
        extensions->robustness &&
        (functions->hasGLExtension("GL_ARB_robust_buffer_access_behavior") ||
         functions->hasGLESExtension("GL_KHR_robust_buffer_access_behavior"));

    extensions->copyTexture = true;
    extensions->syncQuery   = SyncQueryGL::IsSupported(functions);

    // NV_path_rendering
    // We also need interface query which is available in
    // >= 4.3 core or ARB_interface_query or >= GLES 3.1
    const bool canEnableGLPathRendering =
        functions->hasGLExtension("GL_NV_path_rendering") &&
        (functions->hasGLExtension("GL_ARB_program_interface_query") ||
         functions->isAtLeastGL(gl::Version(4, 3)));

    const bool canEnableESPathRendering =
        functions->hasGLESExtension("GL_NV_path_rendering") &&
        functions->isAtLeastGLES(gl::Version(3, 1));

    extensions->pathRendering = canEnableGLPathRendering || canEnableESPathRendering;

    extensions->textureSRGBDecode = functions->hasGLExtension("GL_EXT_texture_sRGB_decode") ||
                                    functions->hasGLESExtension("GL_EXT_texture_sRGB_decode");

#if defined(ANGLE_PLATFORM_APPLE)
    VendorID vendor = GetVendorID(functions);
    if ((IsAMD(vendor) || IsIntel(vendor)) && *maxSupportedESVersion >= gl::Version(3, 0))
    {
        // Apple Intel/AMD drivers do not correctly use the TEXTURE_SRGB_DECODE property of sampler
        // states.  Disable this extension when we would advertise any ES version that has samplers.
        extensions->textureSRGBDecode = false;
    }
#endif

    extensions->sRGBWriteControl = functions->isAtLeastGL(gl::Version(3, 0)) ||
                                   functions->hasGLExtension("GL_EXT_framebuffer_sRGB") ||
                                   functions->hasGLExtension("GL_ARB_framebuffer_sRGB") ||
                                   functions->hasGLESExtension("GL_EXT_sRGB_write_control");

#if defined(ANGLE_PLATFORM_ANDROID)
    // SRGB blending does not appear to work correctly on the Nexus 5. Writing to an SRGB
    // framebuffer with GL_FRAMEBUFFER_SRGB enabled and then reading back returns the same value.
    // Disabling GL_FRAMEBUFFER_SRGB will then convert in the wrong direction.
    extensions->sRGBWriteControl = false;

    // BGRA formats do not appear to be accepted by the Nexus 5X driver dispite the extension being
    // exposed.
    extensions->textureFormatBGRA8888 = false;
#endif

    // EXT_discard_framebuffer can be implemented as long as glDiscardFramebufferEXT or
    // glInvalidateFramebuffer is available
    extensions->discardFramebuffer = functions->isAtLeastGL(gl::Version(4, 3)) ||
                                     functions->hasGLExtension("GL_ARB_invalidate_subdata") ||
                                     functions->isAtLeastGLES(gl::Version(3, 0)) ||
                                     functions->hasGLESExtension("GL_EXT_discard_framebuffer") ||
                                     functions->hasGLESExtension("GL_ARB_invalidate_subdata");

    extensions->translatedShaderSource = true;

    if (functions->isAtLeastGL(gl::Version(3, 1)) ||
        functions->hasGLExtension("GL_ARB_texture_rectangle"))
    {
        extensions->textureRectangle = true;
        caps->maxRectangleTextureSize =
            QuerySingleGLInt(functions, GL_MAX_RECTANGLE_TEXTURE_SIZE_ANGLE);
    }

    // We need at least OpenGL 4.3 to support all features and constants defined in
    // GL_EXT_geometry_shader.
    if (functions->isAtLeastGL(gl::Version(4, 3)) || functions->isAtLeastGLES(gl::Version(3, 2)) ||
        functions->hasGLESExtension("GL_OES_geometry_shader") ||
        functions->hasGLESExtension("GL_EXT_geometry_shader"))
    {
        extensions->geometryShader = true;

        // TODO(jiawei.shao@intel.com): initialize all implementation dependent geometry shader
        // limits.
        extensions->maxGeometryOutputVertices =
            QuerySingleGLInt(functions, GL_MAX_GEOMETRY_OUTPUT_VERTICES_EXT);
        extensions->maxGeometryShaderInvocations =
            QuerySingleGLInt(functions, GL_MAX_GEOMETRY_SHADER_INVOCATIONS_EXT);
    }
}

void GenerateWorkarounds(const FunctionsGL *functions, WorkaroundsGL *workarounds)
{
    VendorID vendor = GetVendorID(functions);

    workarounds->dontRemoveInvariantForFragmentInput =
        functions->standard == STANDARD_GL_DESKTOP && IsAMD(vendor);

    // Don't use 1-bit alpha formats on desktop GL with AMD or Intel drivers.
    workarounds->avoid1BitAlphaTextureFormats =
        functions->standard == STANDARD_GL_DESKTOP && (IsAMD(vendor) || IsIntel(vendor));

    workarounds->rgba4IsNotSupportedForColorRendering =
        functions->standard == STANDARD_GL_DESKTOP && IsIntel(vendor);

    workarounds->emulateAbsIntFunction = IsIntel(vendor);

    workarounds->addAndTrueToLoopCondition = IsIntel(vendor);

    workarounds->emulateIsnanFloat = IsIntel(vendor);

    workarounds->doesSRGBClearsOnLinearFramebufferAttachments =
        functions->standard == STANDARD_GL_DESKTOP && (IsIntel(vendor) || IsAMD(vendor));

#if defined(ANGLE_PLATFORM_LINUX)
    workarounds->emulateMaxVertexAttribStride =
        functions->standard == STANDARD_GL_DESKTOP && IsAMD(vendor);
    workarounds->useUnusedBlocksWithStandardOrSharedLayout = IsAMD(vendor);
#endif

#if defined(ANGLE_PLATFORM_APPLE)
    workarounds->doWhileGLSLCausesGPUHang = true;
    workarounds->useUnusedBlocksWithStandardOrSharedLayout = true;
    workarounds->rewriteFloatUnaryMinusOperator            = IsIntel(vendor);
#endif

#if defined(ANGLE_PLATFORM_ANDROID)
    // Triggers a bug on Marshmallow Adreno (4xx?) driver.
    // http://anglebug.com/2046
    workarounds->dontInitializeUninitializedLocals = IsQualcomm(vendor);
#endif

    workarounds->finishDoesNotCauseQueriesToBeAvailable =
        functions->standard == STANDARD_GL_DESKTOP && IsNvidia(vendor);

    // TODO(cwallez): Disable this workaround for MacOSX versions 10.9 or later.
    workarounds->alwaysCallUseProgramAfterLink = true;

    workarounds->unpackOverlappingRowsSeparatelyUnpackBuffer = IsNvidia(vendor);
    workarounds->packOverlappingRowsSeparatelyPackBuffer     = IsNvidia(vendor);

    workarounds->initializeCurrentVertexAttributes = IsNvidia(vendor);

#if defined(ANGLE_PLATFORM_APPLE)
    workarounds->unpackLastRowSeparatelyForPaddingInclusion = true;
    workarounds->packLastRowSeparatelyForPaddingInclusion   = true;
#else
    workarounds->unpackLastRowSeparatelyForPaddingInclusion = IsNvidia(vendor);
    workarounds->packLastRowSeparatelyForPaddingInclusion   = IsNvidia(vendor);
#endif

    workarounds->removeInvariantAndCentroidForESSL3 =
        functions->isAtMostGL(gl::Version(4, 1)) ||
        (functions->standard == STANDARD_GL_DESKTOP && IsAMD(vendor));

    // TODO(oetuaho): Make this specific to the affected driver versions. Versions that came after
    // 364 are known to be affected, at least up to 375.
    workarounds->emulateAtan2Float = IsNvidia(vendor);

    workarounds->reapplyUBOBindingsAfterUsingBinaryProgram = IsAMD(vendor);

    workarounds->clampPointSize = IsNvidia(vendor);

    workarounds->rewriteVectorScalarArithmetic = IsNvidia(vendor);

#if defined(ANGLE_PLATFORM_ANDROID)
    // TODO(jmadill): Narrow workaround range for specific devices.
    workarounds->reapplyUBOBindingsAfterUsingBinaryProgram = true;

    workarounds->clampPointSize = true;

    workarounds->dontUseLoopsToInitializeVariables = !IsNvidia(vendor);
#endif
}

void ApplyWorkarounds(const FunctionsGL *functions, gl::Workarounds *workarounds)
{
#if defined(ANGLE_PLATFORM_ANDROID)
    VendorID vendor = GetVendorID(functions);

    if (IsQualcomm(vendor))
    {
        workarounds->disableProgramCachingForTransformFeedback = true;
    }
#endif  // defined(ANGLE_PLATFORM_ANDROID)
}

}  // namespace nativegl_gl

namespace nativegl
{
bool SupportsFenceSync(const FunctionsGL *functions)
{
    return functions->isAtLeastGL(gl::Version(3, 2)) || functions->hasGLExtension("GL_ARB_sync") ||
           functions->isAtLeastGLES(gl::Version(3, 0));
}

bool SupportsOcclusionQueries(const FunctionsGL *functions)
{
    return functions->isAtLeastGL(gl::Version(1, 5)) ||
           functions->hasGLExtension("GL_ARB_occlusion_query2") ||
           functions->isAtLeastGLES(gl::Version(3, 0)) ||
           functions->hasGLESExtension("GL_EXT_occlusion_query_boolean");
}

bool SupportsNativeRendering(const FunctionsGL *functions, GLenum target, GLenum internalFormat)
{
    // Some desktop drivers allow rendering to formats that are not required by the spec, this is
    // exposed through the GL_FRAMEBUFFER_RENDERABLE query.
    if (functions->isAtLeastGL(gl::Version(4, 3)) ||
        functions->hasGLExtension("GL_ARB_internalformat_query2"))
    {
        GLint framebufferRenderable = GL_FALSE;
        functions->getInternalformativ(target, internalFormat, GL_FRAMEBUFFER_RENDERABLE, 1,
                                       &framebufferRenderable);
        return gl::ConvertToBool(framebufferRenderable);
    }
    else
    {
        const nativegl::InternalFormat &nativeInfo =
            nativegl::GetInternalFormatInfo(internalFormat, functions->standard);
        return nativegl_gl::MeetsRequirements(functions, nativeInfo.framebufferAttachment);
    }
}
}

bool CanMapBufferForRead(const FunctionsGL *functions)
{
    return (functions->mapBufferRange != nullptr) ||
           (functions->mapBuffer != nullptr && functions->standard == STANDARD_GL_DESKTOP);
}

uint8_t *MapBufferRangeWithFallback(const FunctionsGL *functions,
                                    GLenum target,
                                    size_t offset,
                                    size_t length,
                                    GLbitfield access)
{
    if (functions->mapBufferRange != nullptr)
    {
        return reinterpret_cast<uint8_t *>(
            functions->mapBufferRange(target, offset, length, access));
    }
    else if (functions->mapBuffer != nullptr &&
             (functions->standard == STANDARD_GL_DESKTOP || access == GL_MAP_WRITE_BIT))
    {
        // Only the read and write bits are supported
        ASSERT((access & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)) != 0);

        GLenum accessEnum = 0;
        if (access == (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT))
        {
            accessEnum = GL_READ_WRITE;
        }
        else if (access == GL_MAP_READ_BIT)
        {
            accessEnum = GL_READ_ONLY;
        }
        else if (access == GL_MAP_WRITE_BIT)
        {
            accessEnum = GL_WRITE_ONLY;
        }
        else
        {
            UNREACHABLE();
            return nullptr;
        }

        return reinterpret_cast<uint8_t *>(functions->mapBuffer(target, accessEnum)) + offset;
    }
    else
    {
        // No options available
        UNREACHABLE();
        return nullptr;
    }
}

gl::ErrorOrResult<bool> ShouldApplyLastRowPaddingWorkaround(const gl::Extents &size,
                                                            const gl::PixelStoreStateBase &state,
                                                            const gl::Buffer *pixelBuffer,
                                                            GLenum format,
                                                            GLenum type,
                                                            bool is3D,
                                                            const void *pixels)
{
    if (pixelBuffer == nullptr)
    {
        return false;
    }

    // We are using an pack or unpack buffer, compute what the driver thinks is going to be the
    // last byte read or written. If it is past the end of the buffer, we will need to use the
    // workaround otherwise the driver will generate INVALID_OPERATION and not do the operation.
    CheckedNumeric<size_t> checkedEndByte;
    CheckedNumeric<size_t> pixelBytes;
    size_t rowPitch;

    const gl::InternalFormat &glFormat = gl::GetInternalFormatInfo(format, type);
    ANGLE_TRY_RESULT(glFormat.computePackUnpackEndByte(type, size, state, is3D), checkedEndByte);
    ANGLE_TRY_RESULT(glFormat.computeRowPitch(type, size.width, state.alignment, state.rowLength),
                     rowPitch);
    pixelBytes = glFormat.computePixelBytes(type);

    checkedEndByte += reinterpret_cast<intptr_t>(pixels);

    // At this point checkedEndByte is the actual last byte read.
    // The driver adds an extra row padding (if any), mimic it.
    ANGLE_TRY_CHECKED_MATH(pixelBytes);
    if (pixelBytes.ValueOrDie() * size.width < rowPitch)
    {
        checkedEndByte += rowPitch - pixelBytes * size.width;
    }

    ANGLE_TRY_CHECKED_MATH(checkedEndByte);

    return checkedEndByte.ValueOrDie() > static_cast<size_t>(pixelBuffer->getSize());
}

std::vector<ContextCreationTry> GenerateContextCreationToTry(EGLint requestedType, bool isMesaGLX)
{
    using Type                         = ContextCreationTry::Type;
    constexpr EGLint kPlatformOpenGL   = EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE;
    constexpr EGLint kPlatformOpenGLES = EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE;

    std::vector<ContextCreationTry> contextsToTry;

    if (requestedType == EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE || requestedType == kPlatformOpenGL)
    {
        contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_CORE, gl::Version(4, 5));
        contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_CORE, gl::Version(4, 4));
        contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_CORE, gl::Version(4, 3));
        contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_CORE, gl::Version(4, 2));
        contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_CORE, gl::Version(4, 1));
        contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_CORE, gl::Version(4, 0));
        contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_CORE, gl::Version(3, 3));
        contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_CORE, gl::Version(3, 2));
        contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_LEGACY, gl::Version(3, 3));

        // On Mesa, do not try to create OpenGL context versions between 3.0 and
        // 3.2 because of compatibility problems. See crbug.com/659030
        if (!isMesaGLX)
        {
            contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_LEGACY, gl::Version(3, 2));
            contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_LEGACY, gl::Version(3, 1));
            contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_LEGACY, gl::Version(3, 0));
        }

        contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_LEGACY, gl::Version(2, 1));
        contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_LEGACY, gl::Version(2, 0));
        contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_LEGACY, gl::Version(1, 5));
        contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_LEGACY, gl::Version(1, 4));
        contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_LEGACY, gl::Version(1, 3));
        contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_LEGACY, gl::Version(1, 2));
        contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_LEGACY, gl::Version(1, 1));
        contextsToTry.emplace_back(kPlatformOpenGL, Type::DESKTOP_LEGACY, gl::Version(1, 0));
    }

    if (requestedType == EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE ||
        requestedType == kPlatformOpenGLES)
    {
        contextsToTry.emplace_back(kPlatformOpenGLES, Type::ES, gl::Version(3, 2));
        contextsToTry.emplace_back(kPlatformOpenGLES, Type::ES, gl::Version(3, 1));
        contextsToTry.emplace_back(kPlatformOpenGLES, Type::ES, gl::Version(3, 0));
        contextsToTry.emplace_back(kPlatformOpenGLES, Type::ES, gl::Version(2, 0));
    }

    return contextsToTry;
}
}
