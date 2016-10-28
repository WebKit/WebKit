//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/TranslatorESSL.h"

#include "compiler/translator/EmulatePrecision.h"
#include "compiler/translator/RecordConstantPrecision.h"
#include "compiler/translator/OutputESSL.h"
#include "angle_gl.h"

TranslatorESSL::TranslatorESSL(sh::GLenum type, ShShaderSpec spec)
    : TCompiler(type, spec, SH_ESSL_OUTPUT)
{
}

void TranslatorESSL::translate(TIntermNode *root, ShCompileOptions compileOptions)
{
    TInfoSinkBase& sink = getInfoSink().obj;

    int shaderVer = getShaderVersion();
    if (shaderVer > 100)
    {
        sink << "#version " << shaderVer << " es\n";
    }

    // Write built-in extension behaviors.
    writeExtensionBehavior();

    // Write pragmas after extensions because some drivers consider pragmas
    // like non-preprocessor tokens.
    writePragma(compileOptions);

    bool precisionEmulation = getResources().WEBGL_debug_shader_precision && getPragma().debugShaderPrecision;

    if (precisionEmulation)
    {
        EmulatePrecision emulatePrecision(getSymbolTable(), shaderVer);
        root->traverse(&emulatePrecision);
        emulatePrecision.updateTree();
        emulatePrecision.writeEmulationHelpers(sink, shaderVer, SH_ESSL_OUTPUT);
    }

    RecordConstantPrecision(root, getTemporaryIndex());

    // Write emulated built-in functions if needed.
    if (!getBuiltInFunctionEmulator().IsOutputEmpty())
    {
        sink << "// BEGIN: Generated code for built-in function emulation\n\n";
        if (getShaderType() == GL_FRAGMENT_SHADER)
        {
            sink << "#if defined(GL_FRAGMENT_PRECISION_HIGH)\n"
                 << "#define webgl_emu_precision highp\n"
                 << "#else\n"
                 << "#define webgl_emu_precision mediump\n"
                 << "#endif\n\n";
        }
        else
        {
            sink << "#define webgl_emu_precision highp\n";
        }

        getBuiltInFunctionEmulator().OutputEmulatedFunctions(sink);
        sink << "// END: Generated code for built-in function emulation\n\n";
    }

    // Write array bounds clamping emulation if needed.
    getArrayBoundsClamper().OutputClampingFunctionDefinition(sink);

    if (getShaderType() == GL_COMPUTE_SHADER && isComputeShaderLocalSizeDeclared())
    {
        const sh::WorkGroupSize &localSize = getComputeShaderLocalSize();
        sink << "layout (local_size_x=" << localSize[0] << ", local_size_y=" << localSize[1]
             << ", local_size_z=" << localSize[2] << ") in;\n";
    }

    // Write translated shader.
    TOutputESSL outputESSL(sink, getArrayIndexClampingStrategy(), getHashFunction(), getNameMap(),
                           getSymbolTable(), shaderVer, precisionEmulation);
    root->traverse(&outputESSL);
}

bool TranslatorESSL::shouldFlattenPragmaStdglInvariantAll()
{
    // Not necessary when translating to ESSL.
    return false;
}

void TranslatorESSL::writeExtensionBehavior() {
    TInfoSinkBase& sink = getInfoSink().obj;
    const TExtensionBehavior& extBehavior = getExtensionBehavior();
    for (TExtensionBehavior::const_iterator iter = extBehavior.begin();
         iter != extBehavior.end(); ++iter) {
        if (iter->second != EBhUndefined) {
            if (getResources().NV_shader_framebuffer_fetch && iter->first == "GL_EXT_shader_framebuffer_fetch") {
                sink << "#extension GL_NV_shader_framebuffer_fetch : "
                     << getBehaviorString(iter->second) << "\n";
            } else if (getResources().NV_draw_buffers && iter->first == "GL_EXT_draw_buffers") {
                sink << "#extension GL_NV_draw_buffers : "
                     << getBehaviorString(iter->second) << "\n";
            } else {
                sink << "#extension " << iter->first << " : "
                     << getBehaviorString(iter->second) << "\n";
            }
        }
    }
}
