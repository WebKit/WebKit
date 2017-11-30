//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_UTIL_H_
#define COMPILER_TRANSLATOR_UTIL_H_

#include <stack>

#include "angle_gl.h"
#include <GLSLANG/ShaderLang.h>

#include "compiler/translator/HashNames.h"
#include "compiler/translator/Operator.h"
#include "compiler/translator/Types.h"

// If overflow happens, clamp the value to UINT_MIN or UINT_MAX.
// Return false if overflow happens.
bool atoi_clamp(const char *str, unsigned int *value);

namespace sh
{
class TSymbolTable;

float NumericLexFloat32OutOfRangeToInfinity(const std::string &str);

// strtof_clamp is like strtof but
//   1. it forces C locale, i.e. forcing '.' as decimal point.
//   2. it sets the value to infinity if overflow happens.
//   3. str should be guaranteed to be in the valid format for a floating point number as defined
//      by the grammar in the ESSL 3.00.6 spec section 4.1.4.
// Return false if overflow happens.
bool strtof_clamp(const std::string &str, float *value);

GLenum GLVariableType(const TType &type);
GLenum GLVariablePrecision(const TType &type);
bool IsVaryingIn(TQualifier qualifier);
bool IsVaryingOut(TQualifier qualifier);
bool IsVarying(TQualifier qualifier);
bool IsGeometryShaderInput(GLenum shaderType, TQualifier qualifier);
InterpolationType GetInterpolationType(TQualifier qualifier);

// Returns array brackets including size with outermost array size first, as specified in GLSL ES
// 3.10 section 4.1.9.
TString ArrayString(const TType &type);

TString GetTypeName(const TType &type, ShHashFunction64 hashFunction, NameMap *nameMap);

TType GetShaderVariableBasicType(const sh::ShaderVariable &var);

bool IsBuiltinOutputVariable(TQualifier qualifier);
bool IsBuiltinFragmentInputVariable(TQualifier qualifier);
bool CanBeInvariantESSL1(TQualifier qualifier);
bool CanBeInvariantESSL3OrGreater(TQualifier qualifier);
bool IsOutputESSL(ShShaderOutput output);
bool IsOutputGLSL(ShShaderOutput output);
bool IsOutputHLSL(ShShaderOutput output);
bool IsOutputVulkan(ShShaderOutput output);
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_UTIL_H_
