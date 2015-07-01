//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/TranslatorHLSL.h"

#include "compiler/translator/ArrayReturnValueToOutParameter.h"
#include "compiler/translator/OutputHLSL.h"
#include "compiler/translator/SeparateArrayInitialization.h"
#include "compiler/translator/SeparateDeclarations.h"
#include "compiler/translator/SimplifyArrayAssignment.h"

TranslatorHLSL::TranslatorHLSL(sh::GLenum type, ShShaderSpec spec, ShShaderOutput output)
    : TCompiler(type, spec, output)
{
}

void TranslatorHLSL::translate(TIntermNode *root, int compileOptions)
{
    const ShBuiltInResources &resources = getResources();
    int numRenderTargets = resources.EXT_draw_buffers ? resources.MaxDrawBuffers : 1;

    SeparateArrayDeclarations(root);

    // Note that SeparateDeclarations needs to be run before SeparateArrayInitialization.
    SeparateArrayInitialization(root);

    SimplifyArrayAssignment simplify;
    root->traverse(&simplify);

    // HLSL doesn't support arrays as return values, we'll need to make functions that have an array
    // as a return value to use an out parameter to transfer the array data instead.
    ArrayReturnValueToOutParameter(root);

    sh::OutputHLSL outputHLSL(getShaderType(), getShaderVersion(), getExtensionBehavior(),
        getSourcePath(), getOutputType(), numRenderTargets, getUniforms(), compileOptions);

    outputHLSL.output(root, getInfoSink().obj);

    mInterfaceBlockRegisterMap = outputHLSL.getInterfaceBlockRegisterMap();
    mUniformRegisterMap = outputHLSL.getUniformRegisterMap();
}

bool TranslatorHLSL::hasInterfaceBlock(const std::string &interfaceBlockName) const
{
    return (mInterfaceBlockRegisterMap.count(interfaceBlockName) > 0);
}

unsigned int TranslatorHLSL::getInterfaceBlockRegister(const std::string &interfaceBlockName) const
{
    ASSERT(hasInterfaceBlock(interfaceBlockName));
    return mInterfaceBlockRegisterMap.find(interfaceBlockName)->second;
}

bool TranslatorHLSL::hasUniform(const std::string &uniformName) const
{
    return (mUniformRegisterMap.count(uniformName) > 0);
}

unsigned int TranslatorHLSL::getUniformRegister(const std::string &uniformName) const
{
    ASSERT(hasUniform(uniformName));
    return mUniformRegisterMap.find(uniformName)->second;
}
