//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef _INITIALIZE_INCLUDED_
#define _INITIALIZE_INCLUDED_

#include "GLSLANG/ResourceLimits.h"

#include "compiler/Common.h"
#include "compiler/ShHandle.h"
#include "compiler/SymbolTable.h"

typedef TVector<TString> TBuiltInStrings;

class TBuiltIns {
public:
	POOL_ALLOCATOR_NEW_DELETE(GlobalPoolAllocator)
	void initialize();
	void initialize(const TBuiltInResource& resources);
	TBuiltInStrings* getBuiltInStrings() { return builtInStrings; }
protected:
	TBuiltInStrings builtInStrings[EShLangCount];
};

void IdentifyBuiltIns(EShLanguage, TSymbolTable&);
void IdentifyBuiltIns(EShLanguage, TSymbolTable&, const TBuiltInResource &resources);
bool GenerateBuiltInSymbolTable(const TBuiltInResource* resources, TInfoSink&, TSymbolTable*, EShLanguage language = EShLangCount);
bool InitializeSymbolTable(TBuiltInStrings* BuiltInStrings, EShLanguage language, TInfoSink& infoSink, const TBuiltInResource *resources, TSymbolTable*);
const char* GetPreprocessorBuiltinString();
extern "C" int InitPreprocessor(void);
extern "C" int FinalizePreprocessor(void);

#endif // _INITIALIZE_INCLUDED_
