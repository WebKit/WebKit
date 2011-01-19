//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef _INITIALIZE_INCLUDED_
#define _INITIALIZE_INCLUDED_

#include "compiler/Common.h"
#include "compiler/ShHandle.h"
#include "compiler/SymbolTable.h"

typedef TVector<TString> TBuiltInStrings;

class TBuiltIns {
public:
    POOL_ALLOCATOR_NEW_DELETE(GlobalPoolAllocator)

    void initialize(ShShaderType type, ShShaderSpec spec,
                    const ShBuiltInResources& resources);
    const TBuiltInStrings& getBuiltInStrings() { return builtInStrings; }

protected:
    TBuiltInStrings builtInStrings;
};

void IdentifyBuiltIns(ShShaderType type, ShShaderSpec spec,
                      const ShBuiltInResources& resources,
                      TSymbolTable& symbolTable);

void InitExtensionBehavior(const ShBuiltInResources& resources,
                           TExtensionBehavior& extensionBehavior);

#endif // _INITIALIZE_INCLUDED_
