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
 * \brief OpenGL State Reset.
 *//*--------------------------------------------------------------------*/

#include "gluStateReset.hpp"
#include "gluContextInfo.hpp"
#include "gluRenderContext.hpp"
#include "tcuRenderTarget.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "deUniquePtr.hpp"

namespace glu
{
namespace
{
enum
{
    MAX_ERROR_COUNT = 10
};

void resetErrors(const glw::Functions &gl)
{
    size_t errorNdx = 0;

    for (errorNdx = 0; errorNdx < MAX_ERROR_COUNT; errorNdx++)
    {
        if (gl.getError() == GL_NONE)
            break;
    }

    if (errorNdx == MAX_ERROR_COUNT)
        TCU_FAIL("Couldn't reset error state");
}

} // namespace

void resetStateES(const RenderContext &renderCtx, const ContextInfo &ctxInfo)
{
    const glw::Functions &gl = renderCtx.getFunctions();
    const ContextType type   = renderCtx.getType();

    // Reset error state
    resetErrors(gl);

    DE_ASSERT(isContextTypeES(type));

    // Vertex attrib array state.
    {
        int numVertexAttribArrays = 0;
        gl.getIntegerv(GL_MAX_VERTEX_ATTRIBS, &numVertexAttribArrays);

        gl.bindBuffer(GL_ARRAY_BUFFER, 0);
        gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        if (contextSupports(type, ApiType::es(3, 0)))
        {
            gl.bindVertexArray(0);
            gl.disable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
        }

        if (contextSupports(type, ApiType::es(3, 1)))
            gl.bindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

        for (int ndx = 0; ndx < numVertexAttribArrays; ndx++)
        {
            gl.disableVertexAttribArray(ndx);
            gl.vertexAttribPointer(ndx, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

            if (contextSupports(type, ApiType::es(3, 0)))
                gl.vertexAttribDivisor(ndx, 0);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Vertex attrib array state reset failed");
    }

    // Transformation state.
    {
        const tcu::RenderTarget &renderTarget = renderCtx.getRenderTarget();

        gl.viewport(0, 0, renderTarget.getWidth(), renderTarget.getHeight());
        gl.depthRangef(0.0f, 1.0f);

        if (contextSupports(type, ApiType::es(3, 0)))
            gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Transformation state reset failed");
    }

    // Rasterization state
    {
        gl.lineWidth(1.0f);
        gl.disable(GL_CULL_FACE);
        gl.cullFace(GL_BACK);
        gl.frontFace(GL_CCW);
        gl.polygonOffset(0.0f, 0.0f);
        gl.disable(GL_POLYGON_OFFSET_FILL);

        if (contextSupports(type, ApiType::es(3, 0)))
            gl.disable(GL_RASTERIZER_DISCARD);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Rasterization state reset failed");
    }

    // Multisampling state
    {
        gl.disable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        gl.disable(GL_SAMPLE_COVERAGE);
        gl.sampleCoverage(1.0f, GL_FALSE);

        if (contextSupports(type, ApiType::es(3, 1)))
        {
            int numSampleMaskWords = 0;
            gl.getIntegerv(GL_MAX_SAMPLE_MASK_WORDS, &numSampleMaskWords);

            gl.disable(GL_SAMPLE_MASK);

            for (int ndx = 0; ndx < numSampleMaskWords; ndx++)
                gl.sampleMaski(ndx, ~0u);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Multisampling state reset failed");
    }

    // Texture state.
    // \todo [2013-04-08 pyry] Reset all levels?
    {
        const float borderColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
        int numTexUnits           = 0;
        const bool supportsBorderClamp =
            ctxInfo.isExtensionSupported("GL_EXT_texture_border_clamp") || contextSupports(type, ApiType::es(3, 2));

        gl.getIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &numTexUnits);

        for (int ndx = 0; ndx < numTexUnits; ndx++)
        {
            gl.activeTexture(GL_TEXTURE0 + ndx);

            // Reset 2D texture.
            gl.bindTexture(GL_TEXTURE_2D, 0);
            gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

            if (contextSupports(type, ApiType::es(3, 0)))
            {
                gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
                gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
                gl.texParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, -1000.0f);
                gl.texParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 1000.0f);
                gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
                gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1000);
                gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
                gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
            }

            if (contextSupports(type, ApiType::es(3, 1)))
                gl.texParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);

            if (supportsBorderClamp)
                gl.texParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &borderColor[0]);

            // Reset cube map texture.
            gl.bindTexture(GL_TEXTURE_CUBE_MAP, 0);
            gl.texImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
            gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_REPEAT);
            gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_REPEAT);

            if (contextSupports(type, ApiType::es(3, 0)))
            {
                gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_SWIZZLE_R, GL_RED);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
                gl.texParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_LOD, -1000.0f);
                gl.texParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LOD, 1000.0f);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 1000);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_NONE);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
            }

            if (contextSupports(type, ApiType::es(3, 1)))
                gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);

            if (supportsBorderClamp)
                gl.texParameterfv(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BORDER_COLOR, &borderColor[0]);

            if (contextSupports(type, ApiType::es(3, 0)))
            {
                // Reset 2D array texture.
                gl.bindTexture(GL_TEXTURE_2D_ARRAY, 0);
                gl.texImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
                gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
                gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
                gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_R, GL_RED);
                gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
                gl.texParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_LOD, -1000.0f);
                gl.texParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LOD, 1000.0f);
                gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
                gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 1000);
                gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_NONE);
                gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

                if (supportsBorderClamp)
                    gl.texParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, &borderColor[0]);
            }

            if (contextSupports(type, ApiType::es(3, 1)))
                gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);

            if (contextSupports(type, ApiType::es(3, 0)))
            {
                // Reset 3D texture.
                gl.bindTexture(GL_TEXTURE_3D, 0);
                gl.texImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
                gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
                gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_R, GL_RED);
                gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
                gl.texParameterf(GL_TEXTURE_3D, GL_TEXTURE_MIN_LOD, -1000.0f);
                gl.texParameterf(GL_TEXTURE_3D, GL_TEXTURE_MAX_LOD, 1000.0f);
                gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
                gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 1000);
                gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
                gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

                if (supportsBorderClamp)
                    gl.texParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, &borderColor[0]);
            }

            if (contextSupports(type, ApiType::es(3, 1)))
                gl.texParameteri(GL_TEXTURE_3D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);

            if (contextSupports(type, ApiType::es(3, 1)))
            {
                // Reset multisample textures.
                gl.bindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_SWIZZLE_R, GL_RED);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_BASE_LEVEL, 0);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAX_LEVEL, 1000);
            }

            if (ctxInfo.isExtensionSupported("GL_OES_texture_storage_multisample_2d_array"))
            {
                gl.bindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 0);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_TEXTURE_SWIZZLE_R, GL_RED);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_TEXTURE_MAX_LEVEL, 1000);
            }

            if (ctxInfo.isExtensionSupported("GL_EXT_texture_cube_map_array"))
            {
                // Reset cube array texture.
                gl.bindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_SWIZZLE_R, GL_RED);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
                gl.texParameterf(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_LOD, -1000.0f);
                gl.texParameterf(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAX_LOD, 1000.0f);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAX_LEVEL, 1000);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_NONE);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

                if (supportsBorderClamp)
                    gl.texParameterfv(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_BORDER_COLOR, &borderColor[0]);
            }
        }

        gl.activeTexture(GL_TEXTURE0);

        if (contextSupports(type, ApiType::es(3, 0)))
        {
            for (int ndx = 0; ndx < numTexUnits; ndx++)
                gl.bindSampler(ndx, 0);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Texture state reset failed");
    }

    // Resetting state using non-indexed variants should be enough, but some
    // implementations have bugs so we need to make sure indexed state gets
    // set back to initial values.
    if (ctxInfo.isExtensionSupported("GL_EXT_draw_buffers_indexed"))
    {
        int numDrawBuffers = 0;

        gl.getIntegerv(GL_MAX_DRAW_BUFFERS, &numDrawBuffers);

        for (int drawBufferNdx = 0; drawBufferNdx < numDrawBuffers; drawBufferNdx++)
        {
            gl.disablei(GL_BLEND, drawBufferNdx);
            gl.blendFunci(drawBufferNdx, GL_ONE, GL_ZERO);
            gl.blendEquationi(drawBufferNdx, GL_FUNC_ADD);
            gl.colorMaski(drawBufferNdx, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to reset indexed draw buffer state");
    }

    // Pixel operations.
    {
        const tcu::RenderTarget &renderTarget = renderCtx.getRenderTarget();

        gl.disable(GL_SCISSOR_TEST);
        gl.scissor(0, 0, renderTarget.getWidth(), renderTarget.getHeight());

        gl.disable(GL_STENCIL_TEST);
        gl.stencilFunc(GL_ALWAYS, 0, ~0u);
        gl.stencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

        gl.disable(GL_DEPTH_TEST);
        gl.depthFunc(GL_LESS);

        gl.disable(GL_BLEND);
        gl.blendFunc(GL_ONE, GL_ZERO);
        gl.blendEquation(GL_FUNC_ADD);
        gl.blendColor(0.0f, 0.0f, 0.0f, 0.0f);

        gl.enable(GL_DITHER);

        if (ctxInfo.isExtensionSupported("GL_EXT_sRGB_write_control"))
        {
            gl.enable(GL_FRAMEBUFFER_SRGB);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Pixel operation state reset failed");
    }

    // Framebuffer control.
    {
        gl.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        gl.depthMask(GL_TRUE);
        gl.stencilMask(~0u);

        gl.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
        gl.clearDepthf(1.0f);
        gl.clearStencil(0);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Framebuffer control state reset failed");
    }

    // Framebuffer state.
    {
        // \note Actually spec explictly says 0 but on some platforms (iOS) no default framebuffer exists.
        const uint32_t defaultFbo = renderCtx.getDefaultFramebuffer();
        const uint32_t drawBuffer = defaultFbo != 0 ? GL_COLOR_ATTACHMENT0 : GL_BACK;
        const uint32_t readBuffer = defaultFbo != 0 ? GL_COLOR_ATTACHMENT0 : GL_BACK;

        gl.bindFramebuffer(GL_FRAMEBUFFER, defaultFbo);

        if (contextSupports(type, ApiType::es(3, 0)))
        {
            gl.drawBuffers(1, &drawBuffer);
            gl.readBuffer(readBuffer);
        }

        if (contextSupports(type, ApiType::es(3, 1)) && defaultFbo != 0)
        {
            gl.framebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, 0);
            gl.framebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, 0);
            gl.framebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_SAMPLES, 0);
            gl.framebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS, GL_FALSE);
            GLU_EXPECT_NO_ERROR(gl.getError(), "Framebuffer default state reset failed");
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Framebuffer state reset failed");
    }

    // Renderbuffer state.
    {
        gl.bindRenderbuffer(GL_RENDERBUFFER, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Renderbuffer state reset failed");
    }

    // Pixel transfer state.
    {
        gl.pixelStorei(GL_UNPACK_ALIGNMENT, 4);
        gl.pixelStorei(GL_PACK_ALIGNMENT, 4);

        if (contextSupports(type, ApiType::es(3, 0)))
        {
            gl.pixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
            gl.pixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
            gl.pixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            gl.pixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            gl.pixelStorei(GL_UNPACK_SKIP_PIXELS, 0);

            gl.pixelStorei(GL_PACK_ROW_LENGTH, 0);
            gl.pixelStorei(GL_PACK_SKIP_ROWS, 0);
            gl.pixelStorei(GL_PACK_SKIP_PIXELS, 0);

            gl.bindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            gl.bindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Pixel transfer state reset failed");
    }

    // Program object state.
    {
        gl.useProgram(0);

        if (contextSupports(type, ApiType::es(3, 0)))
        {
            int maxUniformBufferBindings = 0;
            gl.getIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &maxUniformBufferBindings);
            gl.bindBuffer(GL_UNIFORM_BUFFER, 0);

            for (int ndx = 0; ndx < maxUniformBufferBindings; ndx++)
                gl.bindBufferBase(GL_UNIFORM_BUFFER, ndx, 0);
        }

        if (contextSupports(type, ApiType::es(3, 1)))
        {
            gl.bindProgramPipeline(0);

            {
                int maxAtomicCounterBufferBindings = 0;
                gl.getIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, &maxAtomicCounterBufferBindings);
                gl.bindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

                for (int ndx = 0; ndx < maxAtomicCounterBufferBindings; ndx++)
                    gl.bindBufferBase(GL_ATOMIC_COUNTER_BUFFER, ndx, 0);
            }

            {
                int maxShaderStorageBufferBindings = 0;
                gl.getIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &maxShaderStorageBufferBindings);
                gl.bindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

                for (int ndx = 0; ndx < maxShaderStorageBufferBindings; ndx++)
                    gl.bindBufferBase(GL_SHADER_STORAGE_BUFFER, ndx, 0);
            }
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Program object state reset failed");
    }

    // Vertex shader state.
    {
        int numVertexAttribArrays = 0;
        gl.getIntegerv(GL_MAX_VERTEX_ATTRIBS, &numVertexAttribArrays);

        for (int ndx = 0; ndx < numVertexAttribArrays; ndx++)
            gl.vertexAttrib4f(ndx, 0.0f, 0.0f, 0.0f, 1.0f);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Vertex shader state reset failed");
    }

    // Transform feedback state.
    if (contextSupports(type, ApiType::es(3, 0)))
    {
        int numTransformFeedbackSeparateAttribs = 0;
        glw::GLboolean transformFeedbackActive  = 0;
        gl.getIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &numTransformFeedbackSeparateAttribs);
        gl.getBooleanv(GL_TRANSFORM_FEEDBACK_ACTIVE, &transformFeedbackActive);

        if (transformFeedbackActive)
            gl.endTransformFeedback();

        gl.bindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

        for (int ndx = 0; ndx < numTransformFeedbackSeparateAttribs; ndx++)
            gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, ndx, 0);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Transform feedback state reset failed");
    }

    // Asynchronous query state.
    if (contextSupports(type, ApiType::es(3, 0)))
    {
        static const uint32_t targets[] = {GL_ANY_SAMPLES_PASSED, GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                                           GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN};

        for (int i = 0; i < DE_LENGTH_OF_ARRAY(targets); i++)
        {
            int queryActive = 0;
            gl.getQueryiv(targets[i], GL_CURRENT_QUERY, &queryActive);

            if (queryActive != 0)
                gl.endQuery(targets[i]);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Asynchronous query state reset failed");
    }

    // Hints.
    {
        gl.hint(GL_GENERATE_MIPMAP_HINT, GL_DONT_CARE);

        if (contextSupports(type, ApiType::es(3, 0)))
            gl.hint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, GL_DONT_CARE);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Hints reset failed");
    }

    // Compute.
    if (contextSupports(type, ApiType::es(3, 1)))
    {
        gl.bindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Compute dispatch state reset failed");
    }

    // Buffer copy state.
    if (contextSupports(type, ApiType::es(3, 0)))
    {
        gl.bindBuffer(GL_COPY_READ_BUFFER, 0);
        gl.bindBuffer(GL_COPY_WRITE_BUFFER, 0);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Buffer copy state reset failed");
    }

    // Images.
    if (contextSupports(type, ApiType::es(3, 1)))
    {
        int numImageUnits = 0;
        gl.getIntegerv(GL_MAX_IMAGE_UNITS, &numImageUnits);

        for (int ndx = 0; ndx < numImageUnits; ndx++)
            gl.bindImageTexture(ndx, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Image state reset failed");
    }

    // Sample shading state.
    if (contextSupports(type, ApiType::es(3, 1)) && ctxInfo.isExtensionSupported("GL_OES_sample_shading"))
    {
        gl.minSampleShading(0.0f);
        gl.disable(GL_SAMPLE_SHADING);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Sample shading state reset failed");
    }

    // Debug state
    if (ctxInfo.isExtensionSupported("GL_KHR_debug"))
    {
        const bool entrypointsPresent =
            gl.debugMessageControl != nullptr && gl.debugMessageCallback != nullptr && gl.popDebugGroup != nullptr;

        // some drivers advertise GL_KHR_debug but give out null pointers. Silently ignore.
        if (entrypointsPresent)
        {
            int stackDepth = 0;
            gl.getIntegerv(GL_DEBUG_GROUP_STACK_DEPTH, &stackDepth);
            for (int ndx = 1; ndx < stackDepth; ++ndx)
                gl.popDebugGroup();

            gl.debugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, true);
            gl.debugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW, 0, nullptr, false);
            gl.debugMessageCallback(nullptr, nullptr);

            if (type.getFlags() & glu::CONTEXT_DEBUG)
                gl.enable(GL_DEBUG_OUTPUT);
            else
                gl.disable(GL_DEBUG_OUTPUT);
            gl.disable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

            GLU_EXPECT_NO_ERROR(gl.getError(), "Debug state reset failed");
        }
    }

    // Primitive bounding box state.
    if (ctxInfo.isExtensionSupported("GL_EXT_primitive_bounding_box"))
    {
        gl.primitiveBoundingBox(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Primitive bounding box state reset failed");
    }

    // Tessellation state
    if (ctxInfo.isExtensionSupported("GL_EXT_tessellation_shader"))
    {
        gl.patchParameteri(GL_PATCH_VERTICES, 3);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Tessellation patch vertices state reset failed");
    }

    // Advanced coherent blending
    if (ctxInfo.isExtensionSupported("GL_KHR_blend_equation_advanced_coherent"))
    {
        gl.enable(GL_BLEND_ADVANCED_COHERENT_KHR);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Blend equation advanced coherent state reset failed");
    }

    // Texture buffer
    if (ctxInfo.isExtensionSupported("GL_EXT_texture_buffer"))
    {
        gl.bindTexture(GL_TEXTURE_BUFFER, 0);
        gl.bindBuffer(GL_TEXTURE_BUFFER, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Texture buffer state reset failed");
    }
}

void resetStateGLCore(const RenderContext &renderCtx, const ContextInfo &ctxInfo)
{
    const glw::Functions &gl = renderCtx.getFunctions();
    const ContextType type   = renderCtx.getType();

    // Reset error state
    resetErrors(gl);

    // Primitives and vertices state
    {
        if (contextSupports(type, glu::ApiType::core(4, 0)))
        {
            const float defaultTessLevels[] = {1.0f, 1.0f, 1.0f, 1.0f};
            gl.patchParameteri(GL_PATCH_VERTICES_EXT, 3);
            gl.patchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, defaultTessLevels);
            gl.patchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, defaultTessLevels);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Primitives and vertices state reset failed");
    }

    // Vertex attrib array state.
    {
        gl.bindVertexArray(0);
        gl.bindBuffer(GL_ARRAY_BUFFER, 0);
        gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        if (contextSupports(type, ApiType::core(3, 1)))
        {
            gl.disable(GL_PRIMITIVE_RESTART);
            gl.primitiveRestartIndex(0);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Vertex attrib array state reset failed");
    }

    // Transformation state.
    {
        const tcu::RenderTarget &renderTarget = renderCtx.getRenderTarget();
        int numUserClipPlanes                 = 0;

        gl.getIntegerv(GL_MAX_CLIP_DISTANCES, &numUserClipPlanes);

        gl.viewport(0, 0, renderTarget.getWidth(), renderTarget.getHeight());
        gl.depthRange(0.0, 1.0);

        for (int ndx = 0; ndx < numUserClipPlanes; ndx++)
            gl.disable(GL_CLIP_DISTANCE0 + ndx);

        if (contextSupports(type, ApiType::core(3, 2)))
            gl.disable(GL_DEPTH_CLAMP);

        //gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Transformation state reset failed");
    }

    // Coloring
    {
        gl.clampColor(GL_CLAMP_READ_COLOR, GL_FIXED_ONLY);

        if (contextSupports(type, ApiType::core(3, 2)))
            gl.provokingVertex(GL_LAST_VERTEX_CONVENTION);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Coloring state reset failed");
    }

    // Rasterization state
    {
        gl.disable(GL_RASTERIZER_DISCARD);
        gl.pointSize(1.0f);
        gl.pointParameterf(GL_POINT_FADE_THRESHOLD_SIZE, 1.0f);
        gl.pointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_UPPER_LEFT);
        gl.lineWidth(1.0f);
        gl.disable(GL_LINE_SMOOTH);
        gl.disable(GL_CULL_FACE);
        gl.cullFace(GL_BACK);
        gl.frontFace(GL_CCW);
        gl.disable(GL_POLYGON_SMOOTH);
        gl.polygonOffset(0.0f, 0.0f);
        gl.disable(GL_POLYGON_OFFSET_POINT);
        gl.disable(GL_POLYGON_OFFSET_LINE);
        gl.disable(GL_POLYGON_OFFSET_FILL);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Rasterization state reset failed");
    }

    // Multisampling state
    {
        gl.enable(GL_MULTISAMPLE);
        gl.disable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        gl.disable(GL_SAMPLE_ALPHA_TO_ONE);
        gl.disable(GL_SAMPLE_COVERAGE);
        gl.sampleCoverage(1.0f, GL_FALSE);

        if (contextSupports(type, ApiType::core(3, 2)))
        {
            int numSampleMaskWords = 0;
            gl.getIntegerv(GL_MAX_SAMPLE_MASK_WORDS, &numSampleMaskWords);

            gl.disable(GL_SAMPLE_MASK);

            for (int ndx = 0; ndx < numSampleMaskWords; ndx++)
                gl.sampleMaski(ndx, ~0u);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Multisampling state reset failed");
    }

    // Texture state.
    // \todo [2013-04-08 pyry] Reset all levels?
    {
        const float borderColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
        int numTexUnits           = 0;
        gl.getIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &numTexUnits);

        gl.bindBuffer(GL_TEXTURE_BUFFER, 0);

        const bool supportsCubemapArray =
            ctxInfo.isExtensionSupported("GL_ARB_texture_cube_map_array") || contextSupports(type, ApiType::core(4, 0));

        for (int ndx = 0; ndx < numTexUnits; ndx++)
        {
            gl.activeTexture(GL_TEXTURE0 + ndx);

            // Reset 1D texture.
            gl.bindTexture(GL_TEXTURE_1D, 0);
            gl.texImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
            gl.texParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl.texParameterfv(GL_TEXTURE_1D, GL_TEXTURE_BORDER_COLOR, &borderColor[0]);
            gl.texParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            gl.texParameterf(GL_TEXTURE_1D, GL_TEXTURE_MIN_LOD, -1000.0f);
            gl.texParameterf(GL_TEXTURE_1D, GL_TEXTURE_MAX_LOD, 1000.0f);
            gl.texParameteri(GL_TEXTURE_1D, GL_TEXTURE_BASE_LEVEL, 0);
            gl.texParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAX_LEVEL, 1000);
            gl.texParameterf(GL_TEXTURE_1D, GL_TEXTURE_LOD_BIAS, 0.0f);
            gl.texParameteri(GL_TEXTURE_1D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
            gl.texParameteri(GL_TEXTURE_1D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

            if (contextSupports(type, ApiType::core(3, 3)))
            {
                gl.texParameteri(GL_TEXTURE_1D, GL_TEXTURE_SWIZZLE_R, GL_RED);
                gl.texParameteri(GL_TEXTURE_1D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                gl.texParameteri(GL_TEXTURE_1D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                gl.texParameteri(GL_TEXTURE_1D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
            }

            // Reset 2D texture.
            gl.bindTexture(GL_TEXTURE_2D, 0);
            gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl.texParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &borderColor[0]);
            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            gl.texParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, -1000.0f);
            gl.texParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 1000.0f);
            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1000);
            gl.texParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 0.0f);
            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
            gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

            if (contextSupports(type, ApiType::core(3, 3)))
            {
                gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
                gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
            }

            // Reset cube map texture.
            gl.bindTexture(GL_TEXTURE_CUBE_MAP, 0);
            gl.texImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
            gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl.texParameterfv(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BORDER_COLOR, &borderColor[0]);
            gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_REPEAT);
            gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_REPEAT);
            gl.texParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_LOD, -1000.0f);
            gl.texParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LOD, 1000.0f);
            gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
            gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 1000);
            gl.texParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_LOD_BIAS, 0.0f);
            gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_NONE);
            gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

            if (contextSupports(type, ApiType::core(3, 3)))
            {
                gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_SWIZZLE_R, GL_RED);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
            }

            if (supportsCubemapArray)
            {
                // Reset cube array texture.
                gl.bindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                gl.texParameterfv(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_BORDER_COLOR, &borderColor[0]);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
                gl.texParameterf(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_LOD, -1000.0f);
                gl.texParameterf(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAX_LOD, 1000.0f);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAX_LEVEL, 1000);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_NONE);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_SWIZZLE_R, GL_RED);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                gl.texParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
            }

            // Reset 1D array texture.
            gl.bindTexture(GL_TEXTURE_1D_ARRAY, 0);
            gl.texImage2D(GL_TEXTURE_1D_ARRAY, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
            gl.texParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl.texParameterfv(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_BORDER_COLOR, &borderColor[0]);
            gl.texParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
            gl.texParameterf(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MIN_LOD, -1000.0f);
            gl.texParameterf(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MAX_LOD, 1000.0f);
            gl.texParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
            gl.texParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MAX_LEVEL, 1000);
            gl.texParameterf(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_LOD_BIAS, 0.0f);
            gl.texParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_NONE);
            gl.texParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

            if (contextSupports(type, ApiType::core(3, 3)))
            {
                gl.texParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_SWIZZLE_R, GL_RED);
                gl.texParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                gl.texParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                gl.texParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
            }

            // Reset 2D array texture.
            gl.bindTexture(GL_TEXTURE_2D_ARRAY, 0);
            gl.texImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
            gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl.texParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, &borderColor[0]);
            gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
            gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
            gl.texParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_LOD, -1000.0f);
            gl.texParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LOD, 1000.0f);
            gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
            gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 1000);
            gl.texParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_LOD_BIAS, 0.0f);
            gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_NONE);
            gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

            if (contextSupports(type, ApiType::core(3, 3)))
            {
                gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_R, GL_RED);
                gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                gl.texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
            }

            // Reset 3D texture.
            gl.bindTexture(GL_TEXTURE_3D, 0);
            gl.texImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
            gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl.texParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, &borderColor[0]);
            gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
            gl.texParameterf(GL_TEXTURE_3D, GL_TEXTURE_MIN_LOD, -1000.0f);
            gl.texParameterf(GL_TEXTURE_3D, GL_TEXTURE_MAX_LOD, 1000.0f);
            gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
            gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 1000);
            gl.texParameterf(GL_TEXTURE_3D, GL_TEXTURE_LOD_BIAS, 0.0f);
            gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
            gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

            if (contextSupports(type, ApiType::core(3, 3)))
            {
                gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_R, GL_RED);
                gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                gl.texParameteri(GL_TEXTURE_3D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
            }

            if (contextSupports(type, ApiType::core(3, 1)))
            {
                // Reset rectangle texture.
                gl.bindTexture(GL_TEXTURE_RECTANGLE, 0);
                gl.texImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                gl.texParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                gl.texParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                gl.texParameterfv(GL_TEXTURE_RECTANGLE, GL_TEXTURE_BORDER_COLOR, &borderColor[0]);
                gl.texParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                gl.texParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                gl.texParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_BASE_LEVEL, 0);
                gl.texParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAX_LEVEL, 1000);
                gl.texParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_COMPARE_MODE, GL_NONE);
                gl.texParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
                // \todo [2013-06-17 pyry] Drivers don't accept GL_MIN_LOD, GL_MAX_LOD for rectangle textures. Is that okay?

                if (contextSupports(type, ApiType::core(3, 3)))
                {
                    gl.texParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_SWIZZLE_R, GL_RED);
                    gl.texParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                    gl.texParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                    gl.texParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
                }

                // Reset buffer texture.
                gl.bindTexture(GL_TEXTURE_BUFFER, 0);
                gl.texBuffer(GL_TEXTURE_BUFFER, GL_R8, 0);
                // \todo [2013-05-04 pyry] Which parameters apply to buffer textures?
            }

            if (contextSupports(type, ApiType::core(3, 2)))
            {
                // Reset 2D multisample texture.
                gl.bindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_SWIZZLE_R, GL_RED);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_BASE_LEVEL, 0);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAX_LEVEL, 1000);
                gl.texImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGBA8, 0, 0, GL_TRUE);

                // Reset 2D multisample array texture.
                gl.bindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 0);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_TEXTURE_SWIZZLE_R, GL_RED);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
                gl.texParameteri(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_TEXTURE_MAX_LEVEL, 1000);
                gl.texImage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 1, GL_RGBA8, 0, 0, 0, GL_TRUE);
            }
        }

        gl.activeTexture(GL_TEXTURE0);

        if (contextSupports(type, ApiType::core(3, 3)))
        {
            for (int ndx = 0; ndx < numTexUnits; ndx++)
                gl.bindSampler(ndx, 0);

            gl.disable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Texture state reset failed");
    }

    // Pixel operations.
    {
        const tcu::RenderTarget &renderTarget = renderCtx.getRenderTarget();

        gl.disable(GL_SCISSOR_TEST);
        gl.scissor(0, 0, renderTarget.getWidth(), renderTarget.getHeight());

        gl.disable(GL_STENCIL_TEST);
        gl.stencilFunc(GL_ALWAYS, 0, ~0u);
        gl.stencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

        gl.disable(GL_DEPTH_TEST);
        gl.depthFunc(GL_LESS);

        gl.disable(GL_BLEND);
        gl.blendFunc(GL_ONE, GL_ZERO);
        gl.blendEquation(GL_FUNC_ADD);
        gl.blendColor(0.0f, 0.0f, 0.0f, 0.0f);

        gl.disable(GL_FRAMEBUFFER_SRGB);
        gl.enable(GL_DITHER);

        gl.disable(GL_COLOR_LOGIC_OP);
        gl.logicOp(GL_COPY);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Pixel operation state reset failed");
    }

    // Framebuffer control.
    {
        gl.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        gl.depthMask(GL_TRUE);
        gl.stencilMask(~0u);

        gl.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
        gl.clearDepth(1.0);
        gl.clearStencil(0);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Framebuffer control state reset failed");
    }

    // Framebuffer state.
    {
        const uint32_t framebuffer = renderCtx.getDefaultFramebuffer();

        gl.bindFramebuffer(GL_FRAMEBUFFER, framebuffer);

        if (framebuffer == 0)
        {
            gl.drawBuffer(GL_BACK);
            gl.readBuffer(GL_BACK);

            // This is a workaround for supporting single-buffered configurations.
            // Since there is no other place where we need to know if we are dealing
            // with single-buffered config, it is not worthwhile to add additional
            // state into RenderContext for that.
            if (gl.getError() != GL_NO_ERROR)
            {
                gl.drawBuffer(GL_FRONT);
                gl.readBuffer(GL_FRONT);
            }
        }
        else
        {
            gl.drawBuffer(GL_COLOR_ATTACHMENT0);
            gl.readBuffer(GL_COLOR_ATTACHMENT0);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Framebuffer state reset failed");
    }

    // Renderbuffer state.
    {
        gl.bindRenderbuffer(GL_RENDERBUFFER, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Renderbuffer state reset failed");
    }

    // Pixel transfer state.
    {
        gl.pixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
        gl.pixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
        gl.pixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
        gl.pixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
        gl.pixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        gl.pixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        gl.pixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        gl.pixelStorei(GL_UNPACK_ALIGNMENT, 4);

        gl.pixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
        gl.pixelStorei(GL_PACK_LSB_FIRST, GL_FALSE);
        gl.pixelStorei(GL_PACK_IMAGE_HEIGHT, 0);
        gl.pixelStorei(GL_PACK_SKIP_IMAGES, 0);
        gl.pixelStorei(GL_PACK_ROW_LENGTH, 0);
        gl.pixelStorei(GL_PACK_SKIP_ROWS, 0);
        gl.pixelStorei(GL_PACK_SKIP_PIXELS, 0);
        gl.pixelStorei(GL_PACK_ALIGNMENT, 4);

        gl.bindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        gl.bindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Pixel transfer state reset failed");
    }

    // Program object state.
    {
        gl.useProgram(0);

        if (contextSupports(type, ApiType::core(3, 1)))
        {
            int maxUniformBufferBindings = 0;
            gl.getIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &maxUniformBufferBindings);

            gl.bindBuffer(GL_UNIFORM_BUFFER, 0);

            for (int ndx = 0; ndx < maxUniformBufferBindings; ndx++)
                gl.bindBufferBase(GL_UNIFORM_BUFFER, ndx, 0);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Program object state reset failed");
    }

    // Vertex shader state.
    {
        int numVertexAttribArrays = 0;
        gl.getIntegerv(GL_MAX_VERTEX_ATTRIBS, &numVertexAttribArrays);

        for (int ndx = 0; ndx < numVertexAttribArrays; ndx++)
            gl.vertexAttrib4f(ndx, 0.0f, 0.0f, 0.0f, 1.0f);

        gl.disable(GL_VERTEX_PROGRAM_POINT_SIZE);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Vertex shader state reset failed");
    }

    // Transform feedback state.
    {
        int numTransformFeedbackSeparateAttribs = 0;
        gl.getIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &numTransformFeedbackSeparateAttribs);

        if (contextSupports(type, ApiType::core(4, 0)))
        {
            glw::GLboolean transformFeedbackActive = 0;
            gl.getBooleanv(GL_TRANSFORM_FEEDBACK_ACTIVE, &transformFeedbackActive);

            if (transformFeedbackActive)
                gl.endTransformFeedback();
        }

        gl.bindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

        for (int ndx = 0; ndx < numTransformFeedbackSeparateAttribs; ndx++)
            gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, ndx, 0);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Transform feedback state reset failed");
    }

    // Asynchronous query state.
    {
        uint32_t queryTargets[8];
        int numTargets = 0;

        queryTargets[numTargets++] = GL_PRIMITIVES_GENERATED;
        queryTargets[numTargets++] = GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN;
        queryTargets[numTargets++] = GL_SAMPLES_PASSED;

        DE_ASSERT(numTargets <= DE_LENGTH_OF_ARRAY(queryTargets));

        for (int i = 0; i < numTargets; i++)
        {
            int queryActive = 0;
            gl.getQueryiv(queryTargets[i], GL_CURRENT_QUERY, &queryActive);

            if (queryActive != 0)
                gl.endQuery(queryTargets[i]);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Asynchronous query state reset failed");
    }

    // Hints.
    {
        gl.hint(GL_LINE_SMOOTH_HINT, GL_DONT_CARE);
        gl.hint(GL_POLYGON_SMOOTH_HINT, GL_DONT_CARE);
        gl.hint(GL_TEXTURE_COMPRESSION_HINT, GL_DONT_CARE);
        gl.hint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, GL_DONT_CARE);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Hints reset failed");
    }

    // Buffer copy state.
    if (contextSupports(type, ApiType::core(3, 1)))
    {
        gl.bindBuffer(GL_COPY_READ_BUFFER, 0);
        gl.bindBuffer(GL_COPY_WRITE_BUFFER, 0);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Buffer copy state reset failed");
    }

    // Images.
    if (contextSupports(type, ApiType::core(4, 4)))
    {
        int numImageUnits = 0;
        gl.getIntegerv(GL_MAX_IMAGE_UNITS, &numImageUnits);

        for (int ndx = 0; ndx < numImageUnits; ndx++)
            gl.bindImageTexture(ndx, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Image state reset failed");
    }

    // Sample shading state.
    if (contextSupports(type, ApiType::core(4, 0)))
    {
        gl.minSampleShading(0.0f);
        gl.disable(GL_SAMPLE_SHADING);

        GLU_EXPECT_NO_ERROR(gl.getError(), "Sample shading state reset failed");
    }

    // Debug state
    if (ctxInfo.isExtensionSupported("GL_KHR_debug"))
    {
        const bool entrypointsPresent = gl.debugMessageControl != nullptr && gl.debugMessageCallback != nullptr;

        // some drivers advertise GL_KHR_debug but give out null pointers. Silently ignore.
        if (entrypointsPresent)
        {
            gl.debugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, true);
            gl.debugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW, 0, nullptr, false);
            gl.debugMessageCallback(nullptr, nullptr);

            if (type.getFlags() & glu::CONTEXT_DEBUG)
                gl.enable(GL_DEBUG_OUTPUT);
            else
                gl.disable(GL_DEBUG_OUTPUT);
        }
    }
}

void resetState(const RenderContext &renderCtx, const ContextInfo &ctxInfo)
{
    if (isContextTypeES(renderCtx.getType()))
        resetStateES(renderCtx, ctxInfo);
    else if (isContextTypeGLCore(renderCtx.getType()))
        resetStateGLCore(renderCtx, ctxInfo);
    else if (isContextTypeGLCompatibility(renderCtx.getType()))
    {
        // TODO: handle reset state correctly for compatibility profile
        resetStateGLCore(renderCtx, ctxInfo);
    }
    else
        throw tcu::InternalError("State reset requested for unsupported context type");
}

} // namespace glu
