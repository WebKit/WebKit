/*
 * Copyright (C) 2012-2013, 2015-2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef ParserModes_h
#define ParserModes_h

#include "Identifier.h"

namespace JSC {

enum class JSParserStrictMode { NotStrict, Strict };
enum class JSParserBuiltinMode { NotBuiltin, Builtin };
enum class JSParserCommentMode { Classic, Module };
enum class JSParserCodeType { Program, Function, Module };

enum class ConstructorKind { None, Base, Extends };
enum class SuperBinding { Needed, NotNeeded };

enum DebuggerMode { DebuggerOff, DebuggerOn };

enum class FunctionMode { FunctionExpression, FunctionDeclaration, MethodDefinition };

enum class SourceParseMode : uint8_t {
    NormalFunctionMode,
    GeneratorBodyMode,
    GeneratorWrapperFunctionMode,
    GetterMode,
    SetterMode,
    MethodMode,
    ArrowFunctionMode,
    ProgramMode,
    ModuleAnalyzeMode,
    ModuleEvaluateMode
};

inline bool isFunctionParseMode(SourceParseMode parseMode)
{
    switch (parseMode) {
    case SourceParseMode::NormalFunctionMode:
    case SourceParseMode::GeneratorBodyMode:
    case SourceParseMode::GeneratorWrapperFunctionMode:
    case SourceParseMode::GetterMode:
    case SourceParseMode::SetterMode:
    case SourceParseMode::MethodMode:
    case SourceParseMode::ArrowFunctionMode:
        return true;

    case SourceParseMode::ProgramMode:
    case SourceParseMode::ModuleAnalyzeMode:
    case SourceParseMode::ModuleEvaluateMode:
        return false;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

inline bool isModuleParseMode(SourceParseMode parseMode)
{
    switch (parseMode) {
    case SourceParseMode::ModuleAnalyzeMode:
    case SourceParseMode::ModuleEvaluateMode:
        return true;

    case SourceParseMode::NormalFunctionMode:
    case SourceParseMode::GeneratorBodyMode:
    case SourceParseMode::GeneratorWrapperFunctionMode:
    case SourceParseMode::GetterMode:
    case SourceParseMode::SetterMode:
    case SourceParseMode::MethodMode:
    case SourceParseMode::ArrowFunctionMode:
    case SourceParseMode::ProgramMode:
        return false;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

inline bool isProgramParseMode(SourceParseMode parseMode)
{
    switch (parseMode) {
    case SourceParseMode::ProgramMode:
        return true;

    case SourceParseMode::NormalFunctionMode:
    case SourceParseMode::GeneratorBodyMode:
    case SourceParseMode::GeneratorWrapperFunctionMode:
    case SourceParseMode::GetterMode:
    case SourceParseMode::SetterMode:
    case SourceParseMode::MethodMode:
    case SourceParseMode::ArrowFunctionMode:
    case SourceParseMode::ModuleAnalyzeMode:
    case SourceParseMode::ModuleEvaluateMode:
        return false;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

inline bool functionNameIsInScope(const Identifier& name, FunctionMode functionMode)
{
    if (name.isNull())
        return false;

    if (functionMode != FunctionMode::FunctionExpression)
        return false;

    return true;
}

inline bool functionNameScopeIsDynamic(bool usesEval, bool isStrictMode)
{
    // If non-strict eval is in play, a function gets a separate object in the scope chain for its name.
    // This enables eval to declare and then delete a name that shadows the function's name.

    if (!usesEval)
        return false;

    if (isStrictMode)
        return false;

    return true;
}

typedef uint16_t CodeFeatures;

const CodeFeatures NoFeatures =                       0;
const CodeFeatures EvalFeature =                 1 << 0;
const CodeFeatures ArgumentsFeature =            1 << 1;
const CodeFeatures WithFeature =                 1 << 2;
const CodeFeatures ThisFeature =                 1 << 3;
const CodeFeatures StrictModeFeature =           1 << 4;
const CodeFeatures ShadowsArgumentsFeature =     1 << 5;
const CodeFeatures ArrowFunctionFeature =        1 << 6;
const CodeFeatures ArrowFunctionContextFeature = 1 << 7;
const CodeFeatures SuperCallFeature =            1 << 8;
const CodeFeatures SuperPropertyFeature =        1 << 9;
const CodeFeatures NewTargetFeature =            1 << 10;

const CodeFeatures AllFeatures = EvalFeature | ArgumentsFeature | WithFeature | ThisFeature | StrictModeFeature | ShadowsArgumentsFeature | ArrowFunctionFeature | ArrowFunctionContextFeature |
    SuperCallFeature | SuperPropertyFeature | NewTargetFeature;

typedef uint8_t InnerArrowFunctionCodeFeatures;
    
const InnerArrowFunctionCodeFeatures NoInnerArrowFunctionFeatures =                0;
const InnerArrowFunctionCodeFeatures EvalInnerArrowFunctionFeature =          1 << 0;
const InnerArrowFunctionCodeFeatures ArgumentsInnerArrowFunctionFeature =     1 << 1;
const InnerArrowFunctionCodeFeatures ThisInnerArrowFunctionFeature =          1 << 2;
const InnerArrowFunctionCodeFeatures SuperCallInnerArrowFunctionFeature =     1 << 3;
const InnerArrowFunctionCodeFeatures SuperPropertyInnerArrowFunctionFeature = 1 << 4;
const InnerArrowFunctionCodeFeatures NewTargetInnerArrowFunctionFeature =     1 << 5;
    
const InnerArrowFunctionCodeFeatures AllInnerArrowFunctionCodeFeatures = EvalInnerArrowFunctionFeature | ArgumentsInnerArrowFunctionFeature | ThisInnerArrowFunctionFeature | SuperCallInnerArrowFunctionFeature | SuperPropertyInnerArrowFunctionFeature | NewTargetInnerArrowFunctionFeature;
} // namespace JSC

#endif // ParserModes_h
