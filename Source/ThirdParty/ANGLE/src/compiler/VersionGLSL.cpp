//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/VersionGLSL.h"

static const int GLSL_VERSION_110 = 110;
static const int GLSL_VERSION_120 = 120;

// We need to scan for two things:
// 1. "invariant" keyword: This can occur in both - vertex and fragment shaders
//    but only at the global scope.
// 2. "gl_PointCoord" built-in variable: This can only occur in fragment shader
//    but inside any scope.
// So we need to scan the entire fragment shader but only the global scope
// of vertex shader.
//
// TODO(alokp): The following two cases of invariant decalaration get lost
// during parsing - they do not get carried over to the intermediate tree.
// Handle these cases:
// 1. When a pragma is used to force all output variables to be invariant:
//    - #pragma STDGL invariant(all)
// 2. When a previously decalared or built-in variable is marked invariant:
//    - invariant gl_Position;
//    - varying vec3 color; invariant color;
//
TVersionGLSL::TVersionGLSL(ShShaderType type)
    : mShaderType(type),
      mVersion(GLSL_VERSION_110)
{
}

void TVersionGLSL::visitSymbol(TIntermSymbol* node)
{
    ASSERT(mShaderType == SH_FRAGMENT_SHADER);

    if (node->getSymbol() == "gl_PointCoord")
        updateVersion(GLSL_VERSION_120);
}

void TVersionGLSL::visitConstantUnion(TIntermConstantUnion*)
{
    ASSERT(mShaderType == SH_FRAGMENT_SHADER);
}

bool TVersionGLSL::visitBinary(Visit, TIntermBinary*)
{
    ASSERT(mShaderType == SH_FRAGMENT_SHADER);
    return true;
}

bool TVersionGLSL::visitUnary(Visit, TIntermUnary*)
{
    ASSERT(mShaderType == SH_FRAGMENT_SHADER);
    return true;
}

bool TVersionGLSL::visitSelection(Visit, TIntermSelection*)
{
    ASSERT(mShaderType == SH_FRAGMENT_SHADER);
    return true;
}

bool TVersionGLSL::visitAggregate(Visit, TIntermAggregate* node)
{
    // We need to scan the entire fragment shader but only the global scope
    // of vertex shader.
    bool visitChildren = mShaderType == SH_FRAGMENT_SHADER ? true : false;

    switch (node->getOp()) {
      case EOpSequence:
        // We need to visit sequence children to get to global or inner scope.
        visitChildren = true;
        break;
      case EOpDeclaration: {
        const TIntermSequence& sequence = node->getSequence();
        TQualifier qualifier = sequence.front()->getAsTyped()->getQualifier();
        if ((qualifier == EvqInvariantVaryingIn) ||
            (qualifier == EvqInvariantVaryingOut)) {
            updateVersion(GLSL_VERSION_120);
        }
        break;
      }
      default: break;
    }

    return visitChildren;
}

bool TVersionGLSL::visitLoop(Visit, TIntermLoop*)
{
    ASSERT(mShaderType == SH_FRAGMENT_SHADER);
    return true;
}

bool TVersionGLSL::visitBranch(Visit, TIntermBranch*)
{
    ASSERT(mShaderType == SH_FRAGMENT_SHADER);
    return true;
}

void TVersionGLSL::updateVersion(int version)
{
    mVersion = std::max(version, mVersion);
}

