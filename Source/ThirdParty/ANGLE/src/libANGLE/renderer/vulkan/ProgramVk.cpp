//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramVk.cpp:
//    Implements the class methods for ProgramVk.
//

#include "libANGLE/renderer/vulkan/ProgramVk.h"

#include "common/debug.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/GlslangWrapper.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"

namespace rx
{

ProgramVk::ProgramVk(const gl::ProgramState &state) : ProgramImpl(state)
{
}

ProgramVk::~ProgramVk()
{
}

void ProgramVk::destroy(const ContextImpl *contextImpl)
{
    VkDevice device = GetAs<ContextVk>(contextImpl)->getDevice();

    mLinkedFragmentModule.destroy(device);
    mLinkedVertexModule.destroy(device);
    mPipelineLayout.destroy(device);
}

LinkResult ProgramVk::load(const ContextImpl *contextImpl,
                           gl::InfoLog &infoLog,
                           gl::BinaryInputStream *stream)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error ProgramVk::save(gl::BinaryOutputStream *stream)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

void ProgramVk::setBinaryRetrievableHint(bool retrievable)
{
    UNIMPLEMENTED();
}

void ProgramVk::setSeparable(bool separable)
{
    UNIMPLEMENTED();
}

LinkResult ProgramVk::link(ContextImpl *contextImpl,
                           const gl::VaryingPacking &packing,
                           gl::InfoLog &infoLog)
{
    ContextVk *context             = GetAs<ContextVk>(contextImpl);
    RendererVk *renderer           = context->getRenderer();
    GlslangWrapper *glslangWrapper = renderer->getGlslangWrapper();

    const std::string &vertexSource   = mState.getAttachedVertexShader()->getTranslatedSource();
    const std::string &fragmentSource = mState.getAttachedFragmentShader()->getTranslatedSource();

    std::vector<uint32_t> vertexCode;
    std::vector<uint32_t> fragmentCode;
    bool linkSuccess = false;
    ANGLE_TRY_RESULT(
        glslangWrapper->linkProgram(vertexSource, fragmentSource, &vertexCode, &fragmentCode),
        linkSuccess);
    if (!linkSuccess)
    {
        return false;
    }

    vk::ShaderModule vertexModule;
    vk::ShaderModule fragmentModule;
    VkDevice device = renderer->getDevice();

    {
        VkShaderModuleCreateInfo vertexShaderInfo;
        vertexShaderInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vertexShaderInfo.pNext    = nullptr;
        vertexShaderInfo.flags    = 0;
        vertexShaderInfo.codeSize = vertexCode.size() * sizeof(uint32_t);
        vertexShaderInfo.pCode    = vertexCode.data();
        ANGLE_TRY(vertexModule.init(device, vertexShaderInfo));
    }

    {
        VkShaderModuleCreateInfo fragmentShaderInfo;
        fragmentShaderInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        fragmentShaderInfo.pNext    = nullptr;
        fragmentShaderInfo.flags    = 0;
        fragmentShaderInfo.codeSize = fragmentCode.size() * sizeof(uint32_t);
        fragmentShaderInfo.pCode    = fragmentCode.data();

        ANGLE_TRY(fragmentModule.init(device, fragmentShaderInfo));
    }

    mLinkedVertexModule.retain(device, std::move(vertexModule));
    mLinkedFragmentModule.retain(device, std::move(fragmentModule));

    // TODO(jmadill): Use pipeline cache.
    context->invalidateCurrentPipeline();

    return true;
}

GLboolean ProgramVk::validate(const gl::Caps &caps, gl::InfoLog *infoLog)
{
    UNIMPLEMENTED();
    return GLboolean();
}

void ProgramVk::setUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform1iv(GLint location, GLsizei count, const GLint *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform2iv(GLint location, GLsizei count, const GLint *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform3iv(GLint location, GLsizei count, const GLint *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform4iv(GLint location, GLsizei count, const GLint *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix2fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix3fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix4fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix2x3fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix3x2fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix2x4fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix4x2fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix3x4fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformMatrix4x3fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    UNIMPLEMENTED();
}

void ProgramVk::setUniformBlockBinding(GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    UNIMPLEMENTED();
}

bool ProgramVk::getUniformBlockSize(const std::string &blockName, size_t *sizeOut) const
{
    UNIMPLEMENTED();
    return bool();
}

bool ProgramVk::getUniformBlockMemberInfo(const std::string &memberUniformName,
                                          sh::BlockMemberInfo *memberInfoOut) const
{
    UNIMPLEMENTED();
    return bool();
}

void ProgramVk::setPathFragmentInputGen(const std::string &inputName,
                                        GLenum genMode,
                                        GLint components,
                                        const GLfloat *coeffs)
{
    UNIMPLEMENTED();
}

const vk::ShaderModule &ProgramVk::getLinkedVertexModule() const
{
    ASSERT(mLinkedVertexModule.getHandle() != VK_NULL_HANDLE);
    return mLinkedVertexModule;
}

const vk::ShaderModule &ProgramVk::getLinkedFragmentModule() const
{
    ASSERT(mLinkedFragmentModule.getHandle() != VK_NULL_HANDLE);
    return mLinkedFragmentModule;
}

gl::ErrorOrResult<vk::PipelineLayout *> ProgramVk::getPipelineLayout(VkDevice device)
{
    vk::PipelineLayout newLayout;

    // TODO(jmadill): Descriptor sets.
    VkPipelineLayoutCreateInfo createInfo;
    createInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.pNext                  = nullptr;
    createInfo.flags                  = 0;
    createInfo.setLayoutCount         = 0;
    createInfo.pSetLayouts            = nullptr;
    createInfo.pushConstantRangeCount = 0;
    createInfo.pPushConstantRanges    = nullptr;

    ANGLE_TRY(newLayout.init(device, createInfo));
    mPipelineLayout.retain(device, std::move(newLayout));

    return &mPipelineLayout;
}

}  // namespace rx
