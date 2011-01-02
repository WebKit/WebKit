//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

//
// Implement the top-level of interface to the compiler,
// as defined in ShaderLang.h
//

#include "GLSLANG/ShaderLang.h"

#include "compiler/Initialize.h"
#include "compiler/InitializeDll.h"
#include "compiler/ParseHelper.h"
#include "compiler/ShHandle.h"
#include "compiler/SymbolTable.h"

static bool InitializeSymbolTable(
        const TBuiltInStrings& builtInStrings,
        EShLanguage language, EShSpec spec, const TBuiltInResource& resources,
        TInfoSink& infoSink, TSymbolTable& symbolTable)
{
    TIntermediate intermediate(infoSink);
    TParseContext parseContext(symbolTable, intermediate, language, spec, infoSink);

    GlobalParseContext = &parseContext;

    setInitialState();

    assert(symbolTable.isEmpty());       
    //
    // Parse the built-ins.  This should only happen once per
    // language symbol table.
    //
    // Push the symbol table to give it an initial scope.  This
    // push should not have a corresponding pop, so that built-ins
    // are preserved, and the test for an empty table fails.
    //
    symbolTable.push();
    
    //Initialize the Preprocessor
    if (InitPreprocessor())
    {
        infoSink.info.message(EPrefixInternalError,  "Unable to intialize the Preprocessor");
        return false;
    }

    for (TBuiltInStrings::const_iterator i = builtInStrings.begin(); i != builtInStrings.end(); ++i)
    {
        const char* builtInShaders[1];
        int builtInLengths[1];

        builtInShaders[0] = (*i).c_str();
        builtInLengths[0] = (int) (*i).size();

        if (PaParseStrings(const_cast<char**>(builtInShaders), builtInLengths, 1, parseContext) != 0)
        {
            infoSink.info.message(EPrefixInternalError, "Unable to parse built-ins");
            return false;
        }
    }

    IdentifyBuiltIns(language, spec, resources, symbolTable);

    FinalizePreprocessor();

    return true;
}

static bool GenerateBuiltInSymbolTable(
        EShLanguage language, EShSpec spec, const TBuiltInResource& resources,
        TInfoSink& infoSink, TSymbolTable& symbolTable)
{
    TBuiltIns builtIns;

    builtIns.initialize(language, spec, resources);
    return InitializeSymbolTable(builtIns.getBuiltInStrings(), language, spec, resources, infoSink, symbolTable);
}

//
// This is the platform independent interface between an OGL driver
// and the shading language compiler.
//

//
// Driver must call this first, once, before doing any other
// compiler operations.
//
int ShInitialize()
{
    if (!InitProcess())
        return 0;

    return 1;
}

//
// Cleanup symbol tables
//
int ShFinalize()
{
    if (!DetachProcess())
        return 0;

    return 1;
}

//
// Initialize built-in resources with minimum expected values.
//
void ShInitBuiltInResource(TBuiltInResource* resources)
{
    // Constants.
    resources->MaxVertexAttribs = 8;
    resources->MaxVertexUniformVectors = 128;
    resources->MaxVaryingVectors = 8;
    resources->MaxVertexTextureImageUnits = 0;
    resources->MaxCombinedTextureImageUnits = 8;
    resources->MaxTextureImageUnits = 8;
    resources->MaxFragmentUniformVectors = 16;
    resources->MaxDrawBuffers = 1;

    // Extensions.
    resources->OES_standard_derivatives = 0;
}

//
// Driver calls these to create and destroy compiler objects.
//
ShHandle ShConstructCompiler(EShLanguage language, EShSpec spec, const TBuiltInResource* resources)
{
    if (!InitThread())
        return 0;

    TShHandleBase* base = static_cast<TShHandleBase*>(ConstructCompiler(language, spec));
    TCompiler* compiler = base->getAsCompiler();
    if (compiler == 0)
        return 0;

    // Generate built-in symbol table.
    if (!GenerateBuiltInSymbolTable(language, spec, *resources, compiler->getInfoSink(), compiler->getSymbolTable())) {
        ShDestruct(base);
        return 0;
    }

    return reinterpret_cast<void*>(base);
}

void ShDestruct(ShHandle handle)
{
    if (handle == 0)
        return;

    TShHandleBase* base = static_cast<TShHandleBase*>(handle);

    if (base->getAsCompiler())
        DeleteCompiler(base->getAsCompiler());
}

//
// Do an actual compile on the given strings.  The result is left 
// in the given compile object.
//
// Return:  The return value of ShCompile is really boolean, indicating
// success or failure.
//
int ShCompile(
    const ShHandle handle,
    const char* const shaderStrings[],
    const int numStrings,
    const EShOptimizationLevel optLevel,
    int debugOptions
    )
{
    if (!InitThread())
        return 0;

    if (handle == 0)
        return 0;

    TShHandleBase* base = reinterpret_cast<TShHandleBase*>(handle);
    TCompiler* compiler = base->getAsCompiler();
    if (compiler == 0)
        return 0;
    
    GlobalPoolAllocator.push();
    TInfoSink& infoSink = compiler->getInfoSink();
    infoSink.info.erase();
    infoSink.debug.erase();
    infoSink.obj.erase();

    if (numStrings == 0)
        return 1;

    TIntermediate intermediate(infoSink);
    TSymbolTable& symbolTable = compiler->getSymbolTable();

    TParseContext parseContext(symbolTable, intermediate, compiler->getLanguage(), compiler->getSpec(), infoSink);
    parseContext.initializeExtensionBehavior();
    GlobalParseContext = &parseContext;
 
    setInitialState();

    InitPreprocessor();
    //
    // Parse the application's shaders.  All the following symbol table
    // work will be throw-away, so push a new allocation scope that can
    // be thrown away, then push a scope for the current shader's globals.
    //
    bool success = true;

    symbolTable.push();
    if (!symbolTable.atGlobalLevel())
        parseContext.infoSink.info.message(EPrefixInternalError, "Wrong symbol table level");

    int ret = PaParseStrings(const_cast<char**>(shaderStrings), 0, numStrings, parseContext);
    if (ret)
        success = false;

    if (success && parseContext.treeRoot) {
        if (optLevel == EShOptNoGeneration)
            parseContext.infoSink.info.message(EPrefixNone, "No errors.  No code generation was requested.");
        else {
            success = intermediate.postProcess(parseContext.treeRoot, parseContext.language);

            if (success) {

                if (debugOptions & EDebugOpIntermediate)
                    intermediate.outputTree(parseContext.treeRoot);

                //
                // Call the machine dependent compiler
                //
                if (!compiler->compile(parseContext.treeRoot))
                    success = false;
            }
        }
    } else if (!success) {
        parseContext.infoSink.info.prefix(EPrefixError);
        parseContext.infoSink.info << parseContext.numErrors << " compilation errors.  No code generated.\n\n";
        success = false;
        if (debugOptions & EDebugOpIntermediate)
            intermediate.outputTree(parseContext.treeRoot);
    } else if (!parseContext.treeRoot) {
        parseContext.error(1, "Unexpected end of file.", "", "");
        parseContext.infoSink.info << parseContext.numErrors << " compilation errors.  No code generated.\n\n";
        success = false;
        if (debugOptions & EDebugOpIntermediate)
            intermediate.outputTree(parseContext.treeRoot);
    }

    intermediate.remove(parseContext.treeRoot);

    //
    // Ensure symbol table is returned to the built-in level,
    // throwing away all but the built-ins.
    //
    while (!symbolTable.atBuiltInLevel())
        symbolTable.pop();

    FinalizePreprocessor();
    //
    // Throw away all the temporary memory used by the compilation process.
    //
    GlobalPoolAllocator.pop();

    return success ? 1 : 0;
}

//
// Return any compiler log of messages for the application.
//
const char* ShGetInfoLog(const ShHandle handle)
{
    if (!InitThread())
        return 0;

    if (handle == 0)
        return 0;

    TShHandleBase* base = static_cast<TShHandleBase*>(handle);
    TInfoSink* infoSink = 0;

    if (base->getAsCompiler())
        infoSink = &(base->getAsCompiler()->getInfoSink());

    infoSink->info << infoSink->debug.c_str();
    return infoSink->info.c_str();
}

//
// Return any object code.
//
const char* ShGetObjectCode(const ShHandle handle)
{
    if (!InitThread())
        return 0;

    if (handle == 0)
        return 0;

    TShHandleBase* base = static_cast<TShHandleBase*>(handle);
    TInfoSink* infoSink;

    if (base->getAsCompiler())
        infoSink = &(base->getAsCompiler()->getInfoSink());

    return infoSink->obj.c_str();
}
