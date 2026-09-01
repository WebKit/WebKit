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
    : TOutputGLSLBase(compiler, objSink, compileOptions, false)
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
    return name;
}

}  // namespace sh
