//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/glsl/OutputESSL.h"

namespace sh
{

TOutputESSL::TOutputESSL(TCompiler *compiler,
                         TInfoSinkBase &objSink,
                         const ShCompileOptions &compileOptions)
    : TOutputGLSLBase(compiler, objSink, compileOptions)
{}

bool TOutputESSL::writeVariablePrecision(TPrecision precision)
{
    if (precision == EbpUndefined)
        return false;

    TInfoSinkBase &out = objSink();
    out << getPrecisionString(precision);
    return true;
}

ImmutableString TOutputESSL::translateTextureFunction(const ImmutableString &name,
                                                      const ShCompileOptions &option)
{
    // Check WEBGL_video_texture invocation first.
    if (name == "textureVideoWEBGL")
    {
        if (option.takeVideoTextureAsExternalOES)
        {
            // TODO(http://anglebug.com/42262534): Implement external image situation.
            UNIMPLEMENTED();
            return ImmutableString("");
        }
        else
        {
            // For ESSL 300+ (ES 3.0+), use "texture" instead of "texture2D" to match the
            // translation of samplerVideoWEBGL to sampler2D and the ESSL version's texture
            // function naming. ESSL 100 (ES 2.0) still uses texture2D.
            return (getShaderVersion() >= 300) ? ImmutableString("texture")
                                               : ImmutableString("texture2D");
        }
    }

    return name;
}

}  // namespace sh
