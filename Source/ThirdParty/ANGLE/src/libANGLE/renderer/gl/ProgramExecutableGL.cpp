//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramExecutableGL.cpp: Implementation of ProgramExecutableGL.

#include "libANGLE/renderer/gl/ProgramExecutableGL.h"

#include "common/string_utils.h"
#include "libANGLE/Program.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/RendererGL.h"

namespace rx
{
ProgramExecutableGL::ProgramExecutableGL(const gl::ProgramExecutable *executable)
    : ProgramExecutableImpl(executable),
      mHasAppliedTransformFeedbackVaryings(false),
      mClipDistanceEnabledUniformLocation(-1),
      mMultiviewBaseViewLayerIndexUniformLocation(-1)
{}

ProgramExecutableGL::~ProgramExecutableGL() {}

void ProgramExecutableGL::destroy(const gl::Context *context) {}

void ProgramExecutableGL::reset()
{
    mUniformRealLocationMap.clear();
    mUniformBlockRealLocationMap.clear();

    mClipDistanceEnabledUniformLocation         = -1;
    mMultiviewBaseViewLayerIndexUniformLocation = -1;
}

void ProgramExecutableGL::postLink(const FunctionsGL *functions,
                                   const angle::FeaturesGL &features,
                                   GLuint programID)
{
    // Query the uniform information
    ASSERT(mUniformRealLocationMap.empty());
    const auto &uniformLocations = mExecutable->getUniformLocations();
    const auto &uniforms         = mExecutable->getUniforms();
    mUniformRealLocationMap.resize(uniformLocations.size(), GL_INVALID_INDEX);
    for (size_t uniformLocation = 0; uniformLocation < uniformLocations.size(); uniformLocation++)
    {
        const auto &entry = uniformLocations[uniformLocation];
        if (!entry.used())
        {
            continue;
        }

        // From the GLES 3.0.5 spec:
        // "Locations for sequential array indices are not required to be sequential."
        const gl::LinkedUniform &uniform     = uniforms[entry.index];
        const std::string &uniformMappedName = mExecutable->getUniformMappedNames()[entry.index];
        std::stringstream fullNameStr;
        if (uniform.isArray())
        {
            ASSERT(angle::EndsWith(uniformMappedName, "[0]"));
            fullNameStr << uniformMappedName.substr(0, uniformMappedName.length() - 3);
            fullNameStr << "[" << entry.arrayIndex << "]";
        }
        else
        {
            fullNameStr << uniformMappedName;
        }
        const std::string &fullName = fullNameStr.str();

        GLint realLocation = functions->getUniformLocation(programID, fullName.c_str());
        mUniformRealLocationMap[uniformLocation] = realLocation;
    }

    if (features.emulateClipDistanceState.enabled && mExecutable->hasClipDistance())
    {
        ASSERT(functions->standard == STANDARD_GL_ES);
        mClipDistanceEnabledUniformLocation =
            functions->getUniformLocation(programID, "angle_ClipDistanceEnabled");
        ASSERT(mClipDistanceEnabledUniformLocation != -1);
    }

    if (mExecutable->usesMultiview())
    {
        mMultiviewBaseViewLayerIndexUniformLocation =
            functions->getUniformLocation(programID, "multiviewBaseViewLayerIndex");
        ASSERT(mMultiviewBaseViewLayerIndexUniformLocation != -1);
    }
}
}  // namespace rx
