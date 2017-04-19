//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// UniformLinker.h: implements link-time checks for default block uniforms, and generates uniform
// locations. Populates data structures related to uniforms so that they can be stored in program
// state.

#ifndef LIBANGLE_UNIFORMLINKER_H_
#define LIBANGLE_UNIFORMLINKER_H_

#include "libANGLE/Program.h"
#include "libANGLE/Uniform.h"

namespace gl
{

class UniformLinker
{
  public:
    UniformLinker(const ProgramState &state);

    bool link(InfoLog &infoLog, const Caps &caps, const Program::Bindings &uniformLocationBindings);

    void getResults(std::vector<LinkedUniform> *uniforms,
                    std::vector<VariableLocation> *uniformLocations);

  private:
    struct VectorAndSamplerCount
    {
        VectorAndSamplerCount() : vectorCount(0), samplerCount(0) {}
        VectorAndSamplerCount(const VectorAndSamplerCount &other) = default;
        VectorAndSamplerCount &operator=(const VectorAndSamplerCount &other) = default;

        VectorAndSamplerCount &operator+=(const VectorAndSamplerCount &other)
        {
            vectorCount += other.vectorCount;
            samplerCount += other.samplerCount;
            return *this;
        }

        unsigned int vectorCount;
        unsigned int samplerCount;
    };

    bool validateVertexAndFragmentUniforms(InfoLog &infoLog) const;

    static bool linkValidateUniforms(InfoLog &infoLog,
                                     const std::string &uniformName,
                                     const sh::Uniform &vertexUniform,
                                     const sh::Uniform &fragmentUniform);

    bool flattenUniformsAndCheckCapsForShader(const Shader &shader,
                                              GLuint maxUniformComponents,
                                              GLuint maxTextureImageUnits,
                                              const std::string &componentsErrorMessage,
                                              const std::string &samplerErrorMessage,
                                              std::vector<LinkedUniform> &samplerUniforms,
                                              InfoLog &infoLog);
    bool flattenUniformsAndCheckCaps(const Caps &caps, InfoLog &infoLog);

    VectorAndSamplerCount flattenUniform(const sh::Uniform &uniform,
                                         std::vector<LinkedUniform> *samplerUniforms);

    // markStaticUse is given as a separate parameter because it is tracked here at struct
    // granularity.
    VectorAndSamplerCount flattenUniformImpl(const sh::ShaderVariable &uniform,
                                             const std::string &fullName,
                                             std::vector<LinkedUniform> *samplerUniforms,
                                             bool markStaticUse,
                                             int binding,
                                             int *location);

    bool indexUniforms(InfoLog &infoLog, const Program::Bindings &uniformLocationBindings);
    bool gatherUniformLocationsAndCheckConflicts(InfoLog &infoLog,
                                                 const Program::Bindings &uniformLocationBindings,
                                                 std::set<GLuint> *reservedLocations,
                                                 std::set<GLuint> *ignoredLocations,
                                                 int *maxUniformLocation);
    void pruneUnusedUniforms();

    const ProgramState &mState;
    std::vector<LinkedUniform> mUniforms;
    std::vector<VariableLocation> mUniformLocations;
};

}  // namespace gl

#endif  // LIBANGLE_UNIFORMLINKER_H_
