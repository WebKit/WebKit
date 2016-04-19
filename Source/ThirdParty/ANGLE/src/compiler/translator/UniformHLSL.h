//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// UniformHLSL.h:
//   Methods for GLSL to HLSL translation for uniforms and interface blocks.
//

#ifndef COMPILER_TRANSLATOR_UNIFORMHLSL_H_
#define COMPILER_TRANSLATOR_UNIFORMHLSL_H_

#include "compiler/translator/OutputHLSL.h"
#include "compiler/translator/UtilsHLSL.h"

namespace sh
{
class StructureHLSL;

class UniformHLSL : angle::NonCopyable
{
  public:
    UniformHLSL(StructureHLSL *structureHLSL, ShShaderOutput outputType, const std::vector<Uniform> &uniforms);

    void reserveUniformRegisters(unsigned int registerCount);
    void reserveInterfaceBlockRegisters(unsigned int registerCount);
    void uniformsHeader(TInfoSinkBase &out,
                        ShShaderOutput outputType,
                        const ReferencedSymbols &referencedUniforms);

    // Must be called after uniformsHeader
    void samplerMetadataUniforms(TInfoSinkBase &out, const char *reg);

    TString interfaceBlocksHeader(const ReferencedSymbols &referencedInterfaceBlocks);

    // Used for direct index references
    static TString interfaceBlockInstanceString(const TInterfaceBlock& interfaceBlock, unsigned int arrayIndex);

    const std::map<std::string, unsigned int> &getInterfaceBlockRegisterMap() const
    {
        return mInterfaceBlockRegisterMap;
    }
    const std::map<std::string, unsigned int> &getUniformRegisterMap() const
    {
        return mUniformRegisterMap;
    }

  private:
    TString interfaceBlockString(const TInterfaceBlock &interfaceBlock, unsigned int registerIndex, unsigned int arrayIndex);
    TString interfaceBlockMembersString(const TInterfaceBlock &interfaceBlock, TLayoutBlockStorage blockStorage);
    TString interfaceBlockStructString(const TInterfaceBlock &interfaceBlock);
    const Uniform *findUniformByName(const TString &name) const;

    void outputHLSL4_0_FL9_3Sampler(TInfoSinkBase &out,
                                    const TType &type,
                                    const TName &name,
                                    const unsigned int registerIndex);

    void outputUniform(TInfoSinkBase &out,
                       const TType &type,
                       const TName &name,
                       const unsigned int registerIndex);

    // Returns the uniform's register index
    unsigned int assignUniformRegister(const TType &type,
                                       const TString &name,
                                       unsigned int *outRegisterCount);
    unsigned int assignSamplerInStructUniformRegister(const TType &type,
                                                      const TString &name,
                                                      unsigned int *outRegisterCount);

    void outputHLSLSamplerUniformGroup(
        TInfoSinkBase &out,
        const HLSLTextureSamplerGroup textureGroup,
        const TVector<const TIntermSymbol *> &group,
        const TMap<const TIntermSymbol *, TString> &samplerInStructSymbolsToAPINames,
        unsigned int *groupTextureRegisterIndex);

    unsigned int mUniformRegister;
    unsigned int mInterfaceBlockRegister;
    unsigned int mSamplerRegister;
    StructureHLSL *mStructureHLSL;
    ShShaderOutput mOutputType;

    const std::vector<Uniform> &mUniforms;
    std::map<std::string, unsigned int> mInterfaceBlockRegisterMap;
    std::map<std::string, unsigned int> mUniformRegisterMap;
};

}

#endif // COMPILER_TRANSLATOR_UNIFORMHLSL_H_
