//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// OutputVulkanGLSL:
//   Code that outputs shaders that fit GL_KHR_vulkan_glsl, to be fed to glslang to generate
//   SPIR-V.
//   See: https://www.khronos.org/registry/vulkan/specs/misc/GL_KHR_vulkan_glsl.txt
//

#ifndef COMPILER_TRANSLATOR_OUTPUTVULKANGLSL_H_
#define COMPILER_TRANSLATOR_OUTPUTVULKANGLSL_H_

#include "compiler/translator/OutputGLSL.h"

namespace sh
{

class TOutputVulkanGLSL : public TOutputGLSL
{
  public:
    TOutputVulkanGLSL(TCompiler *compiler,
                      TInfoSinkBase &objSink,
                      bool enablePrecision,
                      ShCompileOptions compileOptions);

    uint32_t nextUnusedBinding() { return mNextUnusedBinding++; }
    uint32_t nextUnusedInputLocation(uint32_t consumedCount)
    {
        uint32_t nextUnused = mNextUnusedInputLocation;
        mNextUnusedInputLocation += consumedCount;
        return nextUnused;
    }
    uint32_t nextUnusedOutputLocation(uint32_t consumedCount)
    {
        uint32_t nextUnused = mNextUnusedOutputLocation;
        mNextUnusedOutputLocation += consumedCount;
        return nextUnused;
    }

  protected:
    void writeLayoutQualifier(TIntermSymbol *variable) override;
    void writeVariableType(const TType &type,
                           const TSymbol *symbol,
                           bool isFunctionArgument) override;
    bool writeVariablePrecision(TPrecision) override;

    // Every resource that requires set & binding layout qualifiers is assigned set 0 and an
    // arbitrary binding when outputting GLSL.  Every input/output that requires a location
    // layout qualifiers is assigned an arbitrary location as well.
    //
    // Glslang wrapper modifies set, binding and location decorations in SPIR-V directly.
    uint32_t mNextUnusedBinding;
    uint32_t mNextUnusedInputLocation;
    uint32_t mNextUnusedOutputLocation;

  private:
    bool mEnablePrecision;
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_OUTPUTVULKANGLSL_H_
