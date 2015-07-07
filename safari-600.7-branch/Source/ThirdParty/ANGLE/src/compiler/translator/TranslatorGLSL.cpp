//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/TranslatorGLSL.h"

#include "compiler/translator/OutputGLSL.h"
#include "compiler/translator/VersionGLSL.h"

static void writeVersion(ShShaderType type, TIntermNode* root,
                         TInfoSinkBase& sink) {
    TVersionGLSL versionGLSL(type);
    root->traverse(&versionGLSL);
    int version = versionGLSL.getVersion();
    // We need to write version directive only if it is greater than 110.
    // If there is no version directive in the shader, 110 is implied.
    if (version > 110) {
        sink << "#version " << version << "\n";
    }
}

TranslatorGLSL::TranslatorGLSL(ShShaderType type, ShShaderSpec spec)
    : TCompiler(type, spec) {
}

void TranslatorGLSL::translate(TIntermNode* root) {
    TInfoSinkBase& sink = getInfoSink().obj;

    // Write GLSL version.
    writeVersion(getShaderType(), root, sink);

    // Write extension behaviour as needed
    writeExtensionBehavior();

    // Write emulated built-in functions if needed.
    getBuiltInFunctionEmulator().OutputEmulatedFunctionDefinition(
        sink, false);

    // Write array bounds clamping emulation if needed.
    getArrayBoundsClamper().OutputClampingFunctionDefinition(sink);

    // Write translated shader.
    TOutputGLSL outputGLSL(sink, getArrayIndexClampingStrategy(), getHashFunction(), getNameMap(), getSymbolTable(), getShaderVersion());
    root->traverse(&outputGLSL);
}

void TranslatorGLSL::writeExtensionBehavior() {
    TInfoSinkBase& sink = getInfoSink().obj;
    const TExtensionBehavior& extensionBehavior = getExtensionBehavior();
    for (TExtensionBehavior::const_iterator iter = extensionBehavior.begin();
         iter != extensionBehavior.end(); ++iter) {
        if (iter->second == EBhUndefined)
            continue;

        // For GLSL output, we don't need to emit most extensions explicitly,
        // but some we need to translate.
        if (iter->first == "GL_EXT_shader_texture_lod") {
            sink << "#extension GL_ARB_shader_texture_lod : "
                 << getBehaviorString(iter->second) << "\n";
        }
    }
}
