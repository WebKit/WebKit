//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramExecutableWgpu.cpp: Implementation of ProgramExecutableWgpu.

#include "libANGLE/renderer/wgpu/ProgramExecutableWgpu.h"

#include "libANGLE/Error.h"
#include "libANGLE/Program.h"
#include "libANGLE/renderer/renderer_utils.h"

namespace rx
{

ProgramExecutableWgpu::ProgramExecutableWgpu(const gl::ProgramExecutable *executable)
    : ProgramExecutableImpl(executable)
{
    for (std::shared_ptr<BufferAndLayout> &defaultBlock : mDefaultUniformBlocks)
    {
        defaultBlock = std::make_shared<BufferAndLayout>();
    }
}

ProgramExecutableWgpu::~ProgramExecutableWgpu() = default;

void ProgramExecutableWgpu::destroy(const gl::Context *context) {}

angle::Result ProgramExecutableWgpu::resizeUniformBlockMemory(
    const gl::ShaderMap<size_t> &requiredBufferSize)
{
    for (gl::ShaderType shaderType : mExecutable->getLinkedShaderStages())
    {
        if (requiredBufferSize[shaderType] > 0)
        {
            if (!mDefaultUniformBlocks[shaderType]->uniformData.resize(
                    requiredBufferSize[shaderType]))
            {
                return angle::Result::Stop;
            }

            // Initialize uniform buffer memory to zero by default.
            mDefaultUniformBlocks[shaderType]->uniformData.fill(0);
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }

    return angle::Result::Continue;
}

void ProgramExecutableWgpu::setUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    SetUniform(mExecutable, location, count, v, GL_FLOAT, &mDefaultUniformBlocks,
               &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    SetUniform(mExecutable, location, count, v, GL_FLOAT_VEC2, &mDefaultUniformBlocks,
               &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    SetUniform(mExecutable, location, count, v, GL_FLOAT_VEC3, &mDefaultUniformBlocks,
               &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    SetUniform(mExecutable, location, count, v, GL_FLOAT_VEC4, &mDefaultUniformBlocks,
               &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniform1iv(GLint location, GLsizei count, const GLint *v)
{
    const gl::VariableLocation &locationInfo = mExecutable->getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mExecutable->getUniforms()[locationInfo.index];
    if (linkedUniform.isSampler())
    {
        // TODO(anglebug.com/42267100): handle samplers.
        return;
    }

    SetUniform(mExecutable, location, count, v, GL_INT, &mDefaultUniformBlocks,
               &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniform2iv(GLint location, GLsizei count, const GLint *v)
{
    SetUniform(mExecutable, location, count, v, GL_INT_VEC2, &mDefaultUniformBlocks,
               &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniform3iv(GLint location, GLsizei count, const GLint *v)
{
    SetUniform(mExecutable, location, count, v, GL_INT_VEC3, &mDefaultUniformBlocks,
               &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniform4iv(GLint location, GLsizei count, const GLint *v)
{
    SetUniform(mExecutable, location, count, v, GL_INT_VEC4, &mDefaultUniformBlocks,
               &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
    SetUniform(mExecutable, location, count, v, GL_UNSIGNED_INT, &mDefaultUniformBlocks,
               &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
    SetUniform(mExecutable, location, count, v, GL_UNSIGNED_INT_VEC2, &mDefaultUniformBlocks,
               &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
    SetUniform(mExecutable, location, count, v, GL_UNSIGNED_INT_VEC3, &mDefaultUniformBlocks,
               &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
    SetUniform(mExecutable, location, count, v, GL_UNSIGNED_INT_VEC4, &mDefaultUniformBlocks,
               &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniformMatrix2fv(GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat *value)
{
    SetUniformMatrixfv<2, 2>(mExecutable, location, count, transpose, value, &mDefaultUniformBlocks,
                             &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniformMatrix3fv(GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat *value)
{
    SetUniformMatrixfv<3, 3>(mExecutable, location, count, transpose, value, &mDefaultUniformBlocks,
                             &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniformMatrix4fv(GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat *value)
{
    SetUniformMatrixfv<4, 4>(mExecutable, location, count, transpose, value, &mDefaultUniformBlocks,
                             &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniformMatrix2x3fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat *value)
{
    SetUniformMatrixfv<2, 3>(mExecutable, location, count, transpose, value, &mDefaultUniformBlocks,
                             &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniformMatrix3x2fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat *value)
{
    SetUniformMatrixfv<3, 2>(mExecutable, location, count, transpose, value, &mDefaultUniformBlocks,
                             &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniformMatrix2x4fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat *value)
{
    SetUniformMatrixfv<2, 4>(mExecutable, location, count, transpose, value, &mDefaultUniformBlocks,
                             &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniformMatrix4x2fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat *value)
{
    SetUniformMatrixfv<4, 2>(mExecutable, location, count, transpose, value, &mDefaultUniformBlocks,
                             &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniformMatrix3x4fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat *value)
{
    SetUniformMatrixfv<3, 4>(mExecutable, location, count, transpose, value, &mDefaultUniformBlocks,
                             &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::setUniformMatrix4x3fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat *value)
{
    SetUniformMatrixfv<4, 3>(mExecutable, location, count, transpose, value, &mDefaultUniformBlocks,
                             &mDefaultUniformBlocksDirty);
}

void ProgramExecutableWgpu::getUniformfv(const gl::Context *context,
                                         GLint location,
                                         GLfloat *params) const
{
    GetUniform(mExecutable, location, params, GL_FLOAT, &mDefaultUniformBlocks);
}

void ProgramExecutableWgpu::getUniformiv(const gl::Context *context,
                                         GLint location,
                                         GLint *params) const
{
    GetUniform(mExecutable, location, params, GL_INT, &mDefaultUniformBlocks);
}

void ProgramExecutableWgpu::getUniformuiv(const gl::Context *context,
                                          GLint location,
                                          GLuint *params) const
{
    GetUniform(mExecutable, location, params, GL_UNSIGNED_INT, &mDefaultUniformBlocks);
}

TranslatedWGPUShaderModule &ProgramExecutableWgpu::getShaderModule(gl::ShaderType type)
{
    return mShaderModules[type];
}

angle::Result ProgramExecutableWgpu::getRenderPipeline(ContextWgpu *context,
                                                       const webgpu::RenderPipelineDesc &desc,
                                                       wgpu::RenderPipeline *pipelineOut)
{
    gl::ShaderMap<wgpu::ShaderModule> shaders;
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        shaders[shaderType] = mShaderModules[shaderType].module;
    }

    return mPipelineCache.getRenderPipeline(context, desc, nullptr, shaders, pipelineOut);
}

}  // namespace rx
