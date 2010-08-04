//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef _SHHANDLE_INCLUDED_
#define _SHHANDLE_INCLUDED_

//
// Machine independent part of the compiler private objects
// sent as ShHandle to the driver.
//
// This should not be included by driver code.
//

#include "GLSLANG/ShaderLang.h"

#include "compiler/InfoSink.h"
#include "compiler/SymbolTable.h"

class TCompiler;
class TIntermNode;

//
// The base class used to back handles returned to the driver.
//
class TShHandleBase {
public:
    TShHandleBase() { }
    virtual ~TShHandleBase() { }
    virtual TCompiler* getAsCompiler() { return 0; }
};

//
// The base class for the machine dependent compiler to derive from
// for managing object code from the compile.
//
class TCompiler : public TShHandleBase {
public:
    TCompiler(EShLanguage l, EShSpec s) : language(l), spec(s) { }
    virtual ~TCompiler() { }

    EShLanguage getLanguage() const { return language; }
    EShSpec getSpec() const { return spec; }
    TSymbolTable& getSymbolTable() { return symbolTable; }
    TInfoSink& getInfoSink() { return infoSink; }

    virtual bool compile(TIntermNode* root) = 0;

    virtual TCompiler* getAsCompiler() { return this; }

protected:
    EShLanguage language;
    EShSpec spec;

    // Built-in symbol table for the given language, spec, and resources.
    // It is preserved from compile-to-compile.
    TSymbolTable symbolTable;
    // Output sink.
    TInfoSink infoSink;
};

//
// This is the interface between the machine independent code
// and the machine dependent code.
//
// The machine dependent code should derive from the classes
// above. Then Construct*() and Delete*() will create and 
// destroy the machine dependent objects, which contain the
// above machine independent information.
//
TCompiler* ConstructCompiler(EShLanguage, EShSpec);
void DeleteCompiler(TCompiler*);

#endif // _SHHANDLE_INCLUDED_
