//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/TranslatorGLSL.h"

#include "angle_gl.h"
#include "compiler/translator/BuiltInFunctionEmulatorGLSL.h"
#include "compiler/translator/EmulatePrecision.h"
#include "compiler/translator/OutputGLSL.h"
#include "compiler/translator/VersionGLSL.h"

namespace
{

// To search for what output variables are used in a fragment shader.
// We handle gl_FragColor and gl_FragData at the moment.
class TFragmentOutSearcher : public TIntermTraverser
{
  public:
    TFragmentOutSearcher()
        : mUsesGlFragColor(false),
          mUsesGlFragData(false)
    {
    }

    bool usesGlFragColor() const
    {
        return mUsesGlFragColor;
    }

    bool usesGlFragData() const
    {
        return mUsesGlFragData;
    }

  protected:
    virtual void visitSymbol(TIntermSymbol *node) override
    {
        if (node->getSymbol() == "gl_FragColor")
        {
            mUsesGlFragColor = true;
        }
        else if (node->getSymbol() == "gl_FragData")
        {
            mUsesGlFragData = true;
        }
    }

  private:
    bool mUsesGlFragColor;
    bool mUsesGlFragData;
};

}  // namespace anonymous

TranslatorGLSL::TranslatorGLSL(sh::GLenum type,
                               ShShaderSpec spec,
                               ShShaderOutput output)
    : TCompiler(type, spec, output) {
}

void TranslatorGLSL::initBuiltInFunctionEmulator(BuiltInFunctionEmulator *emu, int compileOptions)
{
    if (compileOptions & SH_EMULATE_BUILT_IN_FUNCTIONS)
        InitBuiltInFunctionEmulatorForGLSL(emu, getShaderType());
}

void TranslatorGLSL::translate(TIntermNode *root, int) {
    TInfoSinkBase& sink = getInfoSink().obj;

    // Write GLSL version.
    writeVersion(root);

    writePragma();

    // Write extension behaviour as needed
    writeExtensionBehavior();

    bool precisionEmulation = getResources().WEBGL_debug_shader_precision && getPragma().debugShaderPrecision;

    if (precisionEmulation)
    {
        EmulatePrecision emulatePrecision;
        root->traverse(&emulatePrecision);
        emulatePrecision.updateTree();
        emulatePrecision.writeEmulationHelpers(sink, getOutputType());
    }

    // Write emulated built-in functions if needed.
    if (!getBuiltInFunctionEmulator().IsOutputEmpty())
    {
        sink << "// BEGIN: Generated code for built-in function emulation\n\n";
        sink << "#define webgl_emu_precision\n\n";
        getBuiltInFunctionEmulator().OutputEmulatedFunctions(sink);
        sink << "// END: Generated code for built-in function emulation\n\n";
    }

    // Write array bounds clamping emulation if needed.
    getArrayBoundsClamper().OutputClampingFunctionDefinition(sink);

    // Declare gl_FragColor and glFragData as webgl_FragColor and webgl_FragData
    // if it's core profile shaders and they are used.
    if (getShaderType() == GL_FRAGMENT_SHADER && IsGLSL130OrNewer(getOutputType()))
    {
        TFragmentOutSearcher searcher;
        root->traverse(&searcher);
        ASSERT(!(searcher.usesGlFragData() && searcher.usesGlFragColor()));
        if (searcher.usesGlFragColor())
        {
            sink << "out vec4 webgl_FragColor;\n";
        }
        if (searcher.usesGlFragData())
        {
            sink << "out vec4 webgl_FragData[gl_MaxDrawBuffers];\n";
        }
    }

    // Write translated shader.
    TOutputGLSL outputGLSL(sink,
                           getArrayIndexClampingStrategy(),
                           getHashFunction(),
                           getNameMap(),
                           getSymbolTable(),
                           getShaderVersion(),
                           getOutputType());
    root->traverse(&outputGLSL);
}

void TranslatorGLSL::writeVersion(TIntermNode *root)
{
    TVersionGLSL versionGLSL(getShaderType(), getPragma(), getOutputType());
    root->traverse(&versionGLSL);
    int version = versionGLSL.getVersion();
    // We need to write version directive only if it is greater than 110.
    // If there is no version directive in the shader, 110 is implied.
    if (version > 110)
    {
        TInfoSinkBase& sink = getInfoSink().obj;
        sink << "#version " << version << "\n";
    }
}

void TranslatorGLSL::writeExtensionBehavior() {
    TInfoSinkBase& sink = getInfoSink().obj;
    const TExtensionBehavior& extBehavior = getExtensionBehavior();
    for (TExtensionBehavior::const_iterator iter = extBehavior.begin();
         iter != extBehavior.end(); ++iter) {
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
