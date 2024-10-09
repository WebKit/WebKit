//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/wgsl/OutputUniformBlocks.h"

#include "common/utilities.h"
#include "compiler/translator/Compiler.h"
#include "compiler/translator/InfoSink.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/util.h"
#include "compiler/translator/wgsl/Utils.h"

namespace sh
{

bool OutputUniformBlocks(TCompiler *compiler, TIntermBlock *root)
{
    // TODO(anglebug.com/42267100): This should eventually just be handled the same way as a regular
    // UBO, like in Vulkan which create a block out of the default uniforms with a traverser:
    // https://source.chromium.org/chromium/chromium/src/+/main:third_party/angle/src/compiler/translator/spirv/TranslatorSPIRV.cpp;l=70;drc=451093bbaf7fe812bf67d27d760f3bb64c92830b
    const std::vector<ShaderVariable> &basicUniforms = compiler->getUniforms();
    TInfoSinkBase &output                            = compiler->getInfoSink().obj;
    GlobalVars globalVars                            = FindGlobalVars(root);

    // Only output a struct at all if there are going to be members.
    bool outputStructHeader = false;
    for (const ShaderVariable &shaderVar : basicUniforms)
    {
        if (gl::IsOpaqueType(shaderVar.type))
        {
            continue;
        }
        if (shaderVar.isBuiltIn())
        {
            // gl_DepthRange and also the GLSL 4.2 gl_NumSamples are uniforms.
            // TODO(anglebug.com/42267100): put gl_DepthRange into default uniform block.
            continue;
        }
        if (!outputStructHeader)
        {
            output << "struct ANGLE_DefaultUniformBlock {\n";
            outputStructHeader = true;
        }
        output << "  ";
        // TODO(anglebug.com/42267100): some types will NOT match std140 layout here, namely matCx2,
        // bool, and arrays with stride less than 16.
        // (this check does not cover the unsupported case where there is an array of structs of
        // size < 16).
        if (gl::VariableRowCount(shaderVar.type) == 2 || shaderVar.type == GL_BOOL ||
            (shaderVar.isArray() && !shaderVar.isStruct() &&
             gl::VariableComponentCount(shaderVar.type) < 3))
        {
            return false;
        }
        output << shaderVar.name << " : ";

        TIntermDeclaration *declNode = globalVars.find(shaderVar.name)->second;
        const TVariable *astVar      = &ViewDeclaration(*declNode).symbol.variable();
        WriteWgslType(output, astVar->getType());

        output << "\n";
    }
    // TODO(anglebug.com/42267100): might need string replacement for @group(0) and @binding(0)
    // annotations. All WGSL resources available to shaders share the same (group, binding) ID
    // space.
    if (outputStructHeader)
    {
        output << "};\n\n"
               << "@group(" << kDefaultUniformBlockBindGroup << ") @binding("
               << kDefaultUniformBlockBinding << ") var<uniform> " << kDefaultUniformBlockVarName
               << " : " << kDefaultUniformBlockVarType << ";\n";
    }

    return true;
}

}  // namespace sh
