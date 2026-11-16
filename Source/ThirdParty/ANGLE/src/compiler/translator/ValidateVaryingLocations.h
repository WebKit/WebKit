//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The ValidateVaryingLocation function checks if there exists location conflicts on shader
// varyings.
//

#ifndef COMPILER_TRANSLATOR_VALIDATEVARYINGLOCATIONS_H_
#define COMPILER_TRANSLATOR_VALIDATEVARYINGLOCATIONS_H_

#include "GLSLANG/ShaderVars.h"
#include "compiler/translator/Common.h"

namespace sh
{

class TVariable;
class TField;
class TType;

struct VariableAndField
{
    const TVariable *variable = nullptr;
    const TField *field       = nullptr;
};
using LocationValidationMap = TUnorderedMap<int, VariableAndField>;

unsigned int CalculateVaryingLocationCount(const TType &varyingType, GLenum shaderType);
// Add locations used by |newVariable| to the location map.  If a conflict is detected, |false| is
// returned, and |conflictingSymbolOut| will return what is the conflicting symbol.  If
// |newVariable| is a struct or interface block, the conflicting field is returned in
// |conflictingFieldInNewSymbolOut|.
bool ValidateVaryingLocation(const TVariable *newVariable,
                             LocationValidationMap *locationMap,
                             GLenum shaderType,
                             VariableAndField *conflictingSymbolOut,
                             const TField **conflictingFieldInNewSymbolOut);

}  // namespace sh

#endif
