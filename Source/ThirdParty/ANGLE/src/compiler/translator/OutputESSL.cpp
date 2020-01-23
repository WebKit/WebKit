//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/OutputESSL.h"

namespace sh
{

TOutputESSL::TOutputESSL(TInfoSinkBase &objSink,
                         ShArrayIndexClampingStrategy clampingStrategy,
                         ShHashFunction64 hashFunction,
                         NameMap &nameMap,
                         TSymbolTable *symbolTable,
                         sh::GLenum shaderType,
                         int shaderVersion,
                         bool forceHighp,
                         ShCompileOptions compileOptions)
    : TOutputGLSLBase(objSink,
                      clampingStrategy,
                      hashFunction,
                      nameMap,
                      symbolTable,
                      shaderType,
                      shaderVersion,
                      SH_ESSL_OUTPUT,
                      compileOptions),
      mForceHighp(forceHighp)
{}

bool TOutputESSL::writeVariablePrecision(TPrecision precision)
{
    if (precision == EbpUndefined)
        return false;

    TInfoSinkBase &out = objSink();
    if (mForceHighp)
        out << getPrecisionString(EbpHigh);
    else
        out << getPrecisionString(precision);
    return true;
}

ImmutableString TOutputESSL::translateTextureFunction(const ImmutableString &name,
                                                      const ShCompileOptions &option)
{
    // Check WEBGL_video_texture invocation first.
    if (name == "textureVideoWEBGL")
    {
        if (option & SH_TAKE_VIDEO_TEXTURE_AS_EXTERNAL_OES)
        {
            // TODO(http://anglebug.com/3889): Implement external image situation.
            UNIMPLEMENTED();
            return ImmutableString("");
        }
        else
        {
            // Default translating textureVideoWEBGL to texture2D.
            return ImmutableString("texture2D");
        }
    }

    return name;
}

}  // namespace sh
