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
constexpr const char kViewport[]                  = "viewport";
constexpr const char kHalfRenderArea[]            = "halfRenderArea";
constexpr const char kFlipXY[]                    = "flipXY";
constexpr const char kNegFlipXY[]                 = "negFlipXY";
constexpr const char kClipDistancesEnabled[]      = "clipDistancesEnabled";
constexpr const char kXfbActiveUnpaused[]         = "xfbActiveUnpaused";
constexpr const char kXfbVerticesPerDraw[]        = "xfbVerticesPerDraw";
constexpr const char kXfbBufferOffsets[]          = "xfbBufferOffsets";
constexpr const char kAcbBufferOffsets[]          = "acbBufferOffsets";
constexpr const char kDepthRange[]                = "depthRange";
constexpr const char kUnassignedAttributeString[] = " __unassigned_attribute__";

class TOutputMSL;

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

class TranslatorMetalReflection
{
  public:
    TranslatorMetalReflection():
        hasUBOs(false), hasFlatInput(false)  {}
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
    userUniformBufferBindingMap getUserUniformBufferBindings() const { return userUniformBufferBindings; }
    uniformBufferBindingMap getUniformBufferBindings() const { return uniformBufferBindings; }
    size_t getSamplerBinding(const std::string &name) const
    {
        auto it = samplerBindings.find(name);
        if (it != samplerBindings.end())
        {
            return it->second;
        }
        //If we can't find a matching sampler, assert out on Debug, and return an invalid value on release.
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
        //If we can't find a matching texture, assert out on Debug, and return an invalid value on release.
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
        //If we can't find a matching Uniform binding, assert out on Debug, and return an invalid value.
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
        //If we can't find a matching UBO binding by name, assert out on Debug, and return an invalid value.
        ASSERT(0);
        return {.bindIndex = std::numeric_limits<size_t>::max(),
                .arraySize = std::numeric_limits<size_t>::max()};
    }
    void reset()
    {
        hasUBOs = false;
        hasFlatInput = false;
        hasAtan = false;
        hasInvariance = false;
        originalNames.clear();
        samplerBindings.clear();
        textureBindings.clear();
        userUniformBufferBindings.clear();
        uniformBufferBindings.clear();
    }

    bool hasUBOs = false;
    bool hasFlatInput = false;
    bool hasAtan = false;
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

    static const char *GetCoverageMaskEnabledConstName();
    static const char *GetRasterizationDiscardEnabledConstName();

    void enableEmulatedInstanceID(bool e) { mEmulatedInstanceID = e; }
    TranslatorMetalReflection *getTranslatorMetalReflection() { return &translatorMetalReflection; }

  protected:
    bool translate(TIntermBlock *root,
                   ShCompileOptions compileOptions,
                   PerformanceDiagnostics *perfDiagnostics) override;

    // Need to collect variables so that RemoveInactiveInterfaceVariables works.
    bool shouldCollectVariables(ShCompileOptions compileOptions) override { return true; }

    ANGLE_NO_DISCARD bool translateImpl(TIntermBlock &root, ShCompileOptions compileOptions);

    ANGLE_NO_DISCARD bool shouldFlattenPragmaStdglInvariantAll() override;

    ANGLE_NO_DISCARD bool transformDepthBeforeCorrection(TIntermBlock &root,
                                                         const TVariable &driverUniforms);
    ANGLE_NO_DISCARD bool insertSampleMaskWritingLogic(TIntermBlock &root,
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
