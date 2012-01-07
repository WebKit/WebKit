//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef _PARSER_HELPER_INCLUDED_
#define _PARSER_HELPER_INCLUDED_

#include "compiler/ExtensionBehavior.h"
#include "compiler/localintermediate.h"
#include "compiler/ShHandle.h"
#include "compiler/SymbolTable.h"

struct TMatrixFields {
    bool wholeRow;
    bool wholeCol;
    int row;
    int col;
};

struct TPragma {
    TPragma(bool o, bool d) : optimize(o), debug(d) { }
    bool optimize;
    bool debug;
    TPragmaTable pragmaTable;
};

//
// The following are extra variables needed during parsing, grouped together so
// they can be passed to the parser without needing a global.
//
struct TParseContext {
    TParseContext(TSymbolTable& symt, TExtensionBehavior& ext, TIntermediate& interm, ShShaderType type, ShShaderSpec spec, int options, bool checksPrecErrors, const char* sourcePath, TInfoSink& is) :
            intermediate(interm),
            symbolTable(symt),
            extensionBehavior(ext),
            infoSink(is),
            shaderType(type),
            shaderSpec(spec),
            compileOptions(options),
            sourcePath(sourcePath),
            treeRoot(0),
            numErrors(0),
            lexAfterType(false),
            loopNestingLevel(0),
            structNestingLevel(0),
            inTypeParen(false),
            currentFunctionType(NULL),
            functionReturnsValue(false),
            checksPrecisionErrors(checksPrecErrors),
            contextPragma(true, false),
            scanner(NULL) {  }
    TIntermediate& intermediate; // to hold and build a parse tree
    TSymbolTable& symbolTable;   // symbol table that goes with the language currently being parsed
    TExtensionBehavior& extensionBehavior;  // mapping between supported extensions and current behavior.
    TInfoSink& infoSink;
    ShShaderType shaderType;              // vertex or fragment language (future: pack or unpack)
    ShShaderSpec shaderSpec;              // The language specification compiler conforms to - GLES2 or WebGL.
    int compileOptions;
    const char* sourcePath;      // Path of source file or NULL.
    TIntermNode* treeRoot;       // root of parse tree being created
    int numErrors;
    bool lexAfterType;           // true if we've recognized a type, so can only be looking for an identifier
    int loopNestingLevel;        // 0 if outside all loops
    int structNestingLevel;      // incremented while parsing a struct declaration
    bool inTypeParen;            // true if in parentheses, looking only for an identifier
    const TType* currentFunctionType;  // the return type of the function that's currently being parsed
    bool functionReturnsValue;   // true if a non-void function has a return
    bool checksPrecisionErrors;  // true if an error will be generated when a variable is declared without precision, explicit or implicit.
    struct TPragma contextPragma;
    TString HashErrMsg;
    bool AfterEOF;
    void* scanner;

    void error(TSourceLoc loc, const char *reason, const char* token,
               const char* extraInfoFormat, ...);
    void warning(TSourceLoc loc, const char* reason, const char* token,
                 const char* extraInfoFormat, ...);
    void recover();

    bool parseVectorFields(const TString&, int vecSize, TVectorFields&, int line);
    bool parseMatrixFields(const TString&, int matSize, TMatrixFields&, int line);

    bool reservedErrorCheck(int line, const TString& identifier);
    void assignError(int line, const char* op, TString left, TString right);
    void unaryOpError(int line, const char* op, TString operand);
    void binaryOpError(int line, const char* op, TString left, TString right);
    bool precisionErrorCheck(int line, TPrecision precision, TBasicType type);
    bool lValueErrorCheck(int line, const char* op, TIntermTyped*);
    bool constErrorCheck(TIntermTyped* node);
    bool integerErrorCheck(TIntermTyped* node, const char* token);
    bool globalErrorCheck(int line, bool global, const char* token);
    bool constructorErrorCheck(int line, TIntermNode*, TFunction&, TOperator, TType*);
    bool arraySizeErrorCheck(int line, TIntermTyped* expr, int& size);
    bool arrayQualifierErrorCheck(int line, TPublicType type);
    bool arrayTypeErrorCheck(int line, TPublicType type);
    bool arrayErrorCheck(int line, TString& identifier, TPublicType type, TVariable*& variable);
    bool voidErrorCheck(int, const TString&, const TPublicType&);
    bool boolErrorCheck(int, const TIntermTyped*);
    bool boolErrorCheck(int, const TPublicType&);
    bool samplerErrorCheck(int line, const TPublicType& pType, const char* reason);
    bool structQualifierErrorCheck(int line, const TPublicType& pType);
    bool parameterSamplerErrorCheck(int line, TQualifier qualifier, const TType& type);
    bool nonInitConstErrorCheck(int line, TString& identifier, TPublicType& type);
    bool nonInitErrorCheck(int line, TString& identifier, TPublicType& type, TVariable*& variable);
    bool paramErrorCheck(int line, TQualifier qualifier, TQualifier paramQualifier, TType* type);
    bool extensionErrorCheck(int line, const TString&);
    bool supportsExtension(const char* extension);

    bool containsSampler(TType& type);
    bool areAllChildConst(TIntermAggregate* aggrNode);
    const TFunction* findFunction(int line, TFunction* pfnCall, bool *builtIn = 0);
    bool executeInitializer(TSourceLoc line, TString& identifier, TPublicType& pType,
                            TIntermTyped* initializer, TIntermNode*& intermNode, TVariable* variable = 0);
    bool arraySetMaxSize(TIntermSymbol*, TType*, int, bool, TSourceLoc);

    TIntermTyped* addConstructor(TIntermNode*, const TType*, TOperator, TFunction*, TSourceLoc);
    TIntermTyped* foldConstConstructor(TIntermAggregate* aggrNode, const TType& type);
    TIntermTyped* constructStruct(TIntermNode*, TType*, int, TSourceLoc, bool subset);
    TIntermTyped* constructBuiltIn(const TType*, TOperator, TIntermNode*, TSourceLoc, bool subset);
    TIntermTyped* addConstVectorNode(TVectorFields&, TIntermTyped*, TSourceLoc);
    TIntermTyped* addConstMatrixNode(int , TIntermTyped*, TSourceLoc);
    TIntermTyped* addConstArrayNode(int index, TIntermTyped* node, TSourceLoc line);
    TIntermTyped* addConstStruct(TString& , TIntermTyped*, TSourceLoc);

    // Performs an error check for embedded struct declarations.
    // Returns true if an error was raised due to the declaration of
    // this struct.
    bool enterStructDeclaration(TSourceLoc line, const TString& identifier);
    void exitStructDeclaration();

    bool structNestingErrorCheck(TSourceLoc line, const TType& fieldType);
};

int PaParseStrings(int count, const char* const string[], const int length[],
                   TParseContext* context);

typedef TParseContext* TParseContextPointer;
extern TParseContextPointer& GetGlobalParseContext();
#define GlobalParseContext GetGlobalParseContext()

typedef struct TThreadParseContextRec
{
    TParseContext *lpGlobalParseContext;
} TThreadParseContext;

#endif // _PARSER_HELPER_INCLUDED_
