//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramD3D.cpp: Defines the rx::ProgramD3D class which implements rx::ProgramImpl.

#include "libANGLE/renderer/ProgramImpl.h"

#include "common/utilities.h"

namespace rx
{

LinkResult::LinkResult(bool linkSuccess, const gl::Error &error)
    : linkSuccess(linkSuccess),
      error(error)
{
}

ProgramImpl::~ProgramImpl()
{
    // Ensure that reset was called by the inherited class during destruction
    ASSERT(mUniformIndex.size() == 0);
}

gl::LinkedUniform *ProgramImpl::getUniformByLocation(GLint location) const
{
    ASSERT(location >= 0 && static_cast<size_t>(location) < mUniformIndex.size());
    return mUniforms[mUniformIndex[location].index];
}

gl::LinkedUniform *ProgramImpl::getUniformByName(const std::string &name) const
{
    for (size_t uniformIndex = 0; uniformIndex < mUniforms.size(); uniformIndex++)
    {
        if (mUniforms[uniformIndex]->name == name)
        {
            return mUniforms[uniformIndex];
        }
    }

    return NULL;
}

gl::UniformBlock *ProgramImpl::getUniformBlockByIndex(GLuint blockIndex) const
{
    ASSERT(blockIndex < mUniformBlocks.size());
    return mUniformBlocks[blockIndex];
}

GLint ProgramImpl::getUniformLocation(const std::string &name) const
{
    size_t subscript = GL_INVALID_INDEX;
    std::string baseName = gl::ParseUniformName(name, &subscript);

    unsigned int numUniforms = mUniformIndex.size();
    for (unsigned int location = 0; location < numUniforms; location++)
    {
        if (mUniformIndex[location].name == baseName)
        {
            const int index = mUniformIndex[location].index;
            const bool isArray = mUniforms[index]->isArray();

            if ((isArray && mUniformIndex[location].element == subscript) ||
                (subscript == GL_INVALID_INDEX))
            {
                return location;
            }
        }
    }

    return -1;
}

GLuint ProgramImpl::getUniformIndex(const std::string &name) const
{
    size_t subscript = GL_INVALID_INDEX;
    std::string baseName = gl::ParseUniformName(name, &subscript);

    // The app is not allowed to specify array indices other than 0 for arrays of basic types
    if (subscript != 0 && subscript != GL_INVALID_INDEX)
    {
        return GL_INVALID_INDEX;
    }

    unsigned int numUniforms = mUniforms.size();
    for (unsigned int index = 0; index < numUniforms; index++)
    {
        if (mUniforms[index]->name == baseName)
        {
            if (mUniforms[index]->isArray() || subscript == GL_INVALID_INDEX)
            {
                return index;
            }
        }
    }

    return GL_INVALID_INDEX;
}

GLuint ProgramImpl::getUniformBlockIndex(const std::string &name) const
{
    size_t subscript = GL_INVALID_INDEX;
    std::string baseName = gl::ParseUniformName(name, &subscript);

    unsigned int numUniformBlocks = mUniformBlocks.size();
    for (unsigned int blockIndex = 0; blockIndex < numUniformBlocks; blockIndex++)
    {
        const gl::UniformBlock &uniformBlock = *mUniformBlocks[blockIndex];
        if (uniformBlock.name == baseName)
        {
            const bool arrayElementZero = (subscript == GL_INVALID_INDEX && uniformBlock.elementIndex == 0);
            if (subscript == uniformBlock.elementIndex || arrayElementZero)
            {
                return blockIndex;
            }
        }
    }

    return GL_INVALID_INDEX;
}

void ProgramImpl::reset()
{
    std::fill(mSemanticIndex, mSemanticIndex + ArraySize(mSemanticIndex), -1);
    SafeDeleteContainer(mUniforms);
    mUniformIndex.clear();
    SafeDeleteContainer(mUniformBlocks);
    mTransformFeedbackLinkedVaryings.clear();
}

void ProgramImpl::setShaderAttribute(size_t index, const sh::Attribute &attrib)
{
    if (mShaderAttributes.size() <= index)
    {
        mShaderAttributes.resize(index + 1);
    }
    mShaderAttributes[index] = attrib;
}

void ProgramImpl::setShaderAttribute(size_t index, GLenum type, GLenum precision, const std::string &name, GLint size, int location)
{
    if (mShaderAttributes.size() <= index)
    {
        mShaderAttributes.resize(index + 1);
    }
    mShaderAttributes[index].type = type;
    mShaderAttributes[index].precision = precision;
    mShaderAttributes[index].name = name;
    mShaderAttributes[index].arraySize = size;
    mShaderAttributes[index].location = location;
}

}
