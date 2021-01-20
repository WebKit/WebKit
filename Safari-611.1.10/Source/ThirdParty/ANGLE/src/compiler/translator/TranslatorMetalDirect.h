//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_H_
#define COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_H_

#include "compiler/translator/Compiler.h"

namespace sh
{
class TOutputMSL;

typedef std::unordered_map<size_t, std::string> originalNamesMap;
typedef std::unordered_map<std::string, size_t> samplerBindingMap;
typedef std::unordered_map<std::string, size_t> textureBindingMap;
typedef std::unordered_map<std::string, size_t> uniformBufferBindingMap;

class TranslatorMetalReflection
{
  public:
    TranslatorMetalReflection() {}
    ~TranslatorMetalReflection() {}

    void addOriginalName(const size_t id, const std::string &name)
    {
        originalNames.insert({id, name});
    }
    void addSamplerBinding(const std::string &name, size_t samplerBinding)
    {
        samplerBindings.insert({name, samplerBinding});
    }
    void addTextureBinding(const std::string &name, size_t textureBinding)
    {
        textureBindings.insert({name, textureBinding});
    }
    void addUniformBufferBinding(const std::string &name, size_t uniformBufferBinding)
    {
        uniformBufferBindings.insert({name, uniformBufferBinding});
    }
    std::string getOriginalName(const size_t id) { return originalNames.at(id); }
    samplerBindingMap getSamplerBindings() const { return samplerBindings; }
    textureBindingMap getTextureBindings() const { return textureBindings; }
    uniformBufferBindingMap getUniformBufferBindings() const { return uniformBufferBindings; }
    size_t getSamplerBinding(const std::string &name) const
    {
        auto it = samplerBindings.find(name);
        if (it != samplerBindings.end())
        {
            return it->second;
        }
        ASSERT(0);
        return std::numeric_limits<size_t>::max();
    }
    size_t getTextureBinding(const std::string &name) const
    {
        auto it = textureBindings.find(name);
        if (it != textureBindings.end())
        {
            return it->second;
        }
        ASSERT(0);
        return std::numeric_limits<size_t>::max();
    }
    size_t getUniformBufferBinding(const std::string &name) const
    {
        auto it = uniformBufferBindings.find(name);
        if (it != uniformBufferBindings.end())
        {
            return it->second;
        }
        ASSERT(0);
        return std::numeric_limits<size_t>::max();
    }
    void reset()
    {
        originalNames.clear();
        samplerBindings.clear();
        textureBindings.clear();
        uniformBufferBindings.clear();
    }

  private:
    originalNamesMap originalNames;
    samplerBindingMap samplerBindings;
    textureBindingMap textureBindings;
    uniformBufferBindingMap uniformBufferBindings;
};

class TranslatorMetalDirect : public TCompiler
{
  public:
    TranslatorMetalDirect(sh::GLenum type, ShShaderSpec spec, ShShaderOutput output);

#ifdef ANGLE_ENABLE_METAL
    TranslatorMetalDirect *getAsTranslatorMetalDirect() override { return this; }
#endif

    static const char *GetCoverageMaskEnabledConstName();
    static const char *GetRasterizationDiscardEnabledConstName();

    void enableEmulatedInstanceID(bool e) { mEmulatedInstanceID = e; }
    TranslatorMetalReflection *getTranslatorMetalReflection() { return &translatorMetalReflection; }

  protected:
    bool translate(TIntermBlock *root,
                   ShCompileOptions compileOptions,
                   PerformanceDiagnostics *perfDiagnostics) override;

    ANGLE_NO_DISCARD bool translateImpl(TIntermBlock &root, ShCompileOptions compileOptions);

    ANGLE_NO_DISCARD bool shouldFlattenPragmaStdglInvariantAll() override;

    ANGLE_NO_DISCARD bool transformDepthBeforeCorrection(TIntermBlock &root,
                                                         const TVariable &driverUniforms);
    ANGLE_NO_DISCARD bool insertRasterizationDiscardLogic(TIntermBlock &root);

    void createAdditionalGraphicsDriverUniformFields(std::vector<TField *> &fieldsOut);

    ANGLE_NO_DISCARD TIntermSwizzle *getDriverUniformNegFlipYRef(
        const TVariable &driverUniforms) const;

    bool mEmulatedInstanceID = false;

    TranslatorMetalReflection translatorMetalReflection = {};
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_H_
