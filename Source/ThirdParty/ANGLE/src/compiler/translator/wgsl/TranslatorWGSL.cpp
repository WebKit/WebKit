//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/wgsl/TranslatorWGSL.h"

namespace sh
{
TranslatorWGSL::TranslatorWGSL(sh::GLenum type, ShShaderSpec spec, ShShaderOutput output)
    : TCompiler(type, spec, output)
{}

bool TranslatorWGSL::translate(TIntermBlock *root,
                               const ShCompileOptions &compileOptions,
                               PerformanceDiagnostics *perfDiagnostics)
{
    TInfoSinkBase &sink = getInfoSink().obj;
    if (getShaderType() == GL_VERTEX_SHADER)
    {
        constexpr const char *kVertexShader = R"(@vertex
fn main(@builtin(vertex_index) vertex_index : u32) -> @builtin(position) vec4f
{
    const pos = array(
        vec2( 0.0,  0.5),
        vec2(-0.5, -0.5),
        vec2( 0.5, -0.5)
    );

    return vec4f(pos[vertex_index % 3], 0, 1);
})";
        sink << kVertexShader;
    }
    else if (getShaderType() == GL_FRAGMENT_SHADER)
    {
        constexpr const char *kFragmentShader = R"(@fragment
fn main() -> @location(0) vec4f
{
    return vec4(1, 0, 0, 1);
})";
        sink << kFragmentShader;
    }
    else
    {
        UNREACHABLE();
        return false;
    }

    return true;
}

bool TranslatorWGSL::shouldFlattenPragmaStdglInvariantAll()
{
    // Not neccesary for WGSL transformation.
    return false;
}
}  // namespace sh
