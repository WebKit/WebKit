//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ResourcesHLSL.h:
//   Methods for GLSL to HLSL translation for uniforms and interface blocks.
//

#ifndef COMPILER_TRANSLATOR_RESOURCESHLSL_H_
#define COMPILER_TRANSLATOR_RESOURCESHLSL_H_

#include "compiler/translator/OutputHLSL.h"
#include "compiler/translator/UtilsHLSL.h"

namespace sh
{
class ImmutableString;
class StructureHLSL;
class TSymbolTable;

class ResourcesHLSL : angle::NonCopyable
{
  public:
    ResourcesHLSL(StructureHLSL *structureHLSL,
                  ShShaderOutput outputType,
                  const std::vector<Uniform> &uniforms,
                  unsigned int firstUniformRegister);

    void reserveUniformRegisters(unsigned int registerCount);
    void reserveUniformBlockRegisters(unsigned int registerCount);
    void uniformsHeader(TInfoSinkBase &out,
                        ShShaderOutput outputType,
                        const ReferencedVariables &referencedUniforms,
                        TSymbolTable *symbolTable);

    // Must be called after uniformsHeader
    void samplerMetadataUniforms(TInfoSinkBase &out, unsigned int regIndex);
    unsigned int getSamplerCount() const { return mSamplerCount; }
    void imageMetadataUniforms(TInfoSinkBase &out, unsigned int regIndex);
    TString uniformBlocksHeader(const ReferencedInterfaceBlocks &referencedInterfaceBlocks);
    TString shaderStorageBlocksHeader(const ReferencedInterfaceBlocks &referencedInterfaceBlocks);

    // Used for direct index references
    static TString InterfaceBlockInstanceString(const ImmutableString &instanceName,
                                                unsigned int arrayIndex);

    const std::map<std::string, unsigned int> &getShaderStorageBlockRegisterMap() const
    {
        return mShaderStorageBlockRegisterMap;
    }

    const std::map<std::string, unsigned int> &getUniformBlockRegisterMap() const
    {
        return mUniformBlockRegisterMap;
    }
    const std::map<std::string, unsigned int> &getUniformRegisterMap() const
    {
        return mUniformRegisterMap;
    }

    unsigned int getReadonlyImage2DRegisterIndex() const { return mReadonlyImage2DRegisterIndex; }
    unsigned int getImage2DRegisterIndex() const { return mImage2DRegisterIndex; }

  private:
    TString uniformBlockString(const TInterfaceBlock &interfaceBlock,
                               const TVariable *instanceVariable,
                               unsigned int registerIndex,
                               unsigned int arrayIndex);

    TString shaderStorageBlockString(const TInterfaceBlock &interfaceBlock,
                                     const TVariable *instanceVariable,
                                     unsigned int registerIndex,
                                     unsigned int arrayIndex);
    TString uniformBlockMembersString(const TInterfaceBlock &interfaceBlock,
                                      TLayoutBlockStorage blockStorage);
    TString uniformBlockStructString(const TInterfaceBlock &interfaceBlock);
    const Uniform *findUniformByName(const ImmutableString &name) const;

    void outputHLSL4_0_FL9_3Sampler(TInfoSinkBase &out,
                                    const TType &type,
                                    const TVariable &variable,
                                    const unsigned int registerIndex);
    void outputUniform(TInfoSinkBase &out,
                       const TType &type,
                       const TVariable &variable,
                       const unsigned int registerIndex);
    void outputAtomicCounterBuffer(TInfoSinkBase &out,
                                   const int binding,
                                   const unsigned int registerIndex);

    // Returns the uniform's register index
    unsigned int assignUniformRegister(const TType &type,
                                       const ImmutableString &name,
                                       unsigned int *outRegisterCount);
    unsigned int assignSamplerInStructUniformRegister(const TType &type,
                                                      const TString &name,
                                                      unsigned int *outRegisterCount);

    void outputHLSLSamplerUniformGroup(
        TInfoSinkBase &out,
        const HLSLTextureGroup textureGroup,
        const TVector<const TVariable *> &group,
        const TMap<const TVariable *, TString> &samplerInStructSymbolsToAPINames,
        unsigned int *groupTextureRegisterIndex);

    void outputHLSLImageUniformIndices(TInfoSinkBase &out,
                                       const TVector<const TVariable *> &group,
                                       unsigned int imageArrayIndex,
                                       unsigned int *groupRegisterCount);
    void outputHLSLReadonlyImageUniformGroup(TInfoSinkBase &out,
                                             const HLSLTextureGroup textureGroup,
                                             const TVector<const TVariable *> &group,
                                             unsigned int *groupTextureRegisterIndex);
    void outputHLSLImageUniformGroup(TInfoSinkBase &out,
                                     const HLSLRWTextureGroup textureGroup,
                                     const TVector<const TVariable *> &group,
                                     unsigned int *groupTextureRegisterIndex);

    unsigned int mUniformRegister;
    unsigned int mUniformBlockRegister;
    unsigned int mTextureRegister;
    unsigned int mUAVRegister;
    unsigned int mSamplerCount;
    unsigned int mReadonlyImageCount;
    unsigned int mImageCount;
    StructureHLSL *mStructureHLSL;
    ShShaderOutput mOutputType;

    const std::vector<Uniform> &mUniforms;
    std::map<std::string, unsigned int> mUniformBlockRegisterMap;
    std::map<std::string, unsigned int> mShaderStorageBlockRegisterMap;
    std::map<std::string, unsigned int> mUniformRegisterMap;
    unsigned int mReadonlyImage2DRegisterIndex;
    unsigned int mImage2DRegisterIndex;
};
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_RESOURCESHLSL_H_
