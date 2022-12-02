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

constexpr const char kUniformsVar[]               = "angleUniforms";
constexpr const char kUnassignedAttributeString[] = " __unassigned_attribute__";

class DriverUniform;
class DriverUniformMetal;
class SpecConst;
class TOutputMSL;
class TranslatorMetalReflection;
typedef std::unordered_map<size_t, std::string> originalNamesMap;
typedef std::unordered_map<std::string, size_t> samplerBindingMap;
typedef std::unordered_map<std::string, size_t> textureBindingMap;
typedef std::unordered_map<std::string, size_t> userUniformBufferBindingMap;
typedef std::pair<size_t, size_t> uboBindingInfo;
struct UBOBindingInfo
{
    size_t bindIndex = 0;
    size_t arraySize = 0;
};
typedef std::unordered_map<std::string, UBOBindingInfo> uniformBufferBindingMap;

namespace mtl
{
TranslatorMetalReflection *getTranslatorMetalReflection(const TCompiler *compiler);
}

class TranslatorMetalReflection
{
  public:
    TranslatorMetalReflection() : hasUBOs(false), hasFlatInput(false) {}
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
    void addUserUniformBufferBinding(const std::string &name, size_t userUniformBufferBinding)
    {
        userUniformBufferBindings.insert({name, userUniformBufferBinding});
    }
    void addUniformBufferBinding(const std::string &name, UBOBindingInfo bindingInfo)
    {
        uniformBufferBindings.insert({name, bindingInfo});
    }
    std::string getOriginalName(const size_t id) { return originalNames.at(id); }
    samplerBindingMap getSamplerBindings() const { return samplerBindings; }
    textureBindingMap getTextureBindings() const { return textureBindings; }
    userUniformBufferBindingMap getUserUniformBufferBindings() const
    {
        return userUniformBufferBindings;
    }
    uniformBufferBindingMap getUniformBufferBindings() const { return uniformBufferBindings; }
    size_t getSamplerBinding(const std::string &name) const
    {
        auto it = samplerBindings.find(name);
        if (it != samplerBindings.end())
        {
            return it->second;
        }
        // If we can't find a matching sampler, assert out on Debug, and return an invalid value on
        // release.
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
        // If we can't find a matching texture, assert out on Debug, and return an invalid value on
        // release.
        ASSERT(0);
        return std::numeric_limits<size_t>::max();
    }
    size_t getUserUniformBufferBinding(const std::string &name) const
    {
        auto it = userUniformBufferBindings.find(name);
        if (it != userUniformBufferBindings.end())
        {
            return it->second;
        }
        // If we can't find a matching Uniform binding, assert out on Debug, and return an invalid
        // value.
        ASSERT(0);
        return std::numeric_limits<size_t>::max();
    }
    UBOBindingInfo getUniformBufferBinding(const std::string &name) const
    {
        auto it = uniformBufferBindings.find(name);
        if (it != uniformBufferBindings.end())
        {
            return it->second;
        }
        // If we can't find a matching UBO binding by name, assert out on Debug, and return an
        // invalid value.
        ASSERT(0);
        return {.bindIndex = std::numeric_limits<size_t>::max(),
                .arraySize = std::numeric_limits<size_t>::max()};
    }
    void reset()
    {
        hasUBOs       = false;
        hasFlatInput  = false;
        hasAtan       = false;
        hasInvariance = false;
        originalNames.clear();
        samplerBindings.clear();
        textureBindings.clear();
        userUniformBufferBindings.clear();
        uniformBufferBindings.clear();
    }

    bool hasUBOs       = false;
    bool hasFlatInput  = false;
    bool hasAtan       = false;
    bool hasInvariance = false;

  private:
    originalNamesMap originalNames;
    samplerBindingMap samplerBindings;
    textureBindingMap textureBindings;
    userUniformBufferBindingMap userUniformBufferBindings;
    uniformBufferBindingMap uniformBufferBindings;
};

class TranslatorMetalDirect : public TCompiler
{
  public:
    TranslatorMetalDirect(sh::GLenum type, ShShaderSpec spec, ShShaderOutput output);

#ifdef ANGLE_ENABLE_METAL
    TranslatorMetalDirect *getAsTranslatorMetalDirect() override { return this; }
#endif

    TranslatorMetalReflection *getTranslatorMetalReflection() { return &translatorMetalReflection; }

  protected:
    bool translate(TIntermBlock *root,
                   const ShCompileOptions &compileOptions,
                   PerformanceDiagnostics *perfDiagnostics) override;

    // The sample mask can't be in our fragment output struct if we read the framebuffer. Luckily,
    // pixel local storage bans gl_SampleMask, so we can just not use it when PLS is active.
    bool usesSampleMask() const { return !hasPixelLocalStorageUniforms(); }

    // Need to collect variables so that RemoveInactiveInterfaceVariables works.
    bool shouldCollectVariables(const ShCompileOptions &compileOptions) override { return true; }

    [[nodiscard]] bool translateImpl(TInfoSinkBase &sink,
                                     TIntermBlock *root,
                                     const ShCompileOptions &compileOptions,
                                     PerformanceDiagnostics *perfDiagnostics,
                                     SpecConst *specConst,
                                     DriverUniformMetal *driverUniforms);

    [[nodiscard]] bool shouldFlattenPragmaStdglInvariantAll() override;

    [[nodiscard]] bool transformDepthBeforeCorrection(TIntermBlock *root,
                                                      const DriverUniformMetal *driverUniforms);

    [[nodiscard]] bool appendVertexShaderDepthCorrectionToMain(TIntermBlock *root);

    [[nodiscard]] bool insertSampleMaskWritingLogic(TIntermBlock &root,
                                                    DriverUniformMetal &driverUniforms);
    [[nodiscard]] bool insertRasterizationDiscardLogic(TIntermBlock &root);

    TranslatorMetalReflection translatorMetalReflection = {};
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_H_
