//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef _BUILTIN_SYMBOL_TABLE_INCLUDED_
#define _BUILTIN_SYMBOL_TABLE_INCLUDED_

#include "GLSLANG/ShaderLang.h"

class TSymbolTable;

extern void InsertBuiltInFunctionsCommon(const ShBuiltInResources& resources, TSymbolTable* t);
extern void InsertBuiltInFunctionsVertex(const ShBuiltInResources& resources, TSymbolTable* t);

#endif // _BUILTIN_SYMBOL_TABLE_INCLUDED_
