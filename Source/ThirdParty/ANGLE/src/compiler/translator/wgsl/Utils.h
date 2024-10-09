//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_WGSL_UTILS_H_
#define COMPILER_TRANSLATOR_WGSL_UTILS_H_

#include "compiler/translator/Common.h"
#include "compiler/translator/InfoSink.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/Types.h"

namespace sh
{

// Can be used with TSymbol or TField or TFunc.
template <typename StringStreamType, typename Object>
void WriteNameOf(StringStreamType &output, const Object &namedObject)
{
    WriteNameOf(output, namedObject.symbolType(), namedObject.name());
}

template <typename StringStreamType>
void WriteNameOf(StringStreamType &output, SymbolType symbolType, const ImmutableString &name);
template <typename StringStreamType>
void WriteWgslBareTypeName(StringStreamType &output, const TType &type);
template <typename StringStreamType>
void WriteWgslType(StringStreamType &output, const TType &type);

using GlobalVars = TMap<ImmutableString, TIntermDeclaration *>;
GlobalVars FindGlobalVars(TIntermBlock *root);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_WGSL_UTILS_H_
