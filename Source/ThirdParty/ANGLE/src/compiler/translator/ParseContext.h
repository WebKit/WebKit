//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef COMPILER_TRANSLATOR_PARSECONTEXT_H_
#define COMPILER_TRANSLATOR_PARSECONTEXT_H_

#include "compiler/translator/Compiler.h"
#include "compiler/translator/Diagnostics.h"
#include "compiler/translator/DirectiveHandler.h"
#include "compiler/translator/Intermediate.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/QualifierTypes.h"
#include "compiler/preprocessor/Preprocessor.h"

struct TMatrixFields
{
    bool wholeRow;
    bool wholeCol;
    int row;
    int col;
};

//
// The following are extra variables needed during parsing, grouped together so
// they can be passed to the parser without needing a global.
//
class TParseContext : angle::NonCopyable
{
  public:
    TParseContext(TSymbolTable &symt,
                  TExtensionBehavior &ext,
                  sh::GLenum type,
                  ShShaderSpec spec,
                  ShCompileOptions options,
                  bool checksPrecErrors,
                  TInfoSink &is,
                  const ShBuiltInResources &resources)
        : intermediate(),
          symbolTable(symt),
          mDeferredSingleDeclarationErrorCheck(false),
          mShaderType(type),
          mShaderSpec(spec),
          mCompileOptions(options),
          mShaderVersion(100),
          mTreeRoot(nullptr),
          mLoopNestingLevel(0),
          mStructNestingLevel(0),
          mSwitchNestingLevel(0),
          mCurrentFunctionType(nullptr),
          mFunctionReturnsValue(false),
          mChecksPrecisionErrors(checksPrecErrors),
          mFragmentPrecisionHighOnESSL1(false),
          mDefaultMatrixPacking(EmpColumnMajor),
          mDefaultBlockStorage(IsWebGLBasedSpec(spec) ? EbsStd140 : EbsShared),
          mDiagnostics(is),
          mDirectiveHandler(ext,
                            mDiagnostics,
                            mShaderVersion,
                            mShaderType,
                            resources.WEBGL_debug_shader_precision == 1),
          mPreprocessor(&mDiagnostics, &mDirectiveHandler),
          mScanner(nullptr),
          mUsesFragData(false),
          mUsesFragColor(false),
          mUsesSecondaryOutputs(false),
          mMinProgramTexelOffset(resources.MinProgramTexelOffset),
          mMaxProgramTexelOffset(resources.MaxProgramTexelOffset),
          mComputeShaderLocalSizeDeclared(false),
          mDeclaringFunction(false)
    {
        mComputeShaderLocalSize.fill(-1);
    }

    const pp::Preprocessor &getPreprocessor() const { return mPreprocessor; }
    pp::Preprocessor &getPreprocessor() { return mPreprocessor; }
    void *getScanner() const { return mScanner; }
    void setScanner(void *scanner) { mScanner = scanner; }
    int getShaderVersion() const { return mShaderVersion; }
    sh::GLenum getShaderType() const { return mShaderType; }
    ShShaderSpec getShaderSpec() const { return mShaderSpec; }
    int numErrors() const { return mDiagnostics.numErrors(); }
    TInfoSink &infoSink() { return mDiagnostics.infoSink(); }
    void error(const TSourceLoc &loc, const char *reason, const char *token,
               const char *extraInfo="");
    void warning(const TSourceLoc &loc, const char *reason, const char *token,
                 const char *extraInfo="");

    // If isError is false, a warning will be reported instead.
    void outOfRangeError(bool isError,
                         const TSourceLoc &loc,
                         const char *reason,
                         const char *token,
                         const char *extraInfo = "");

    TIntermBlock *getTreeRoot() const { return mTreeRoot; }
    void setTreeRoot(TIntermBlock *treeRoot) { mTreeRoot = treeRoot; }

    bool getFragmentPrecisionHigh() const
    {
        return mFragmentPrecisionHighOnESSL1 || mShaderVersion >= 300;
    }
    void setFragmentPrecisionHighOnESSL1(bool fragmentPrecisionHigh)
    {
        mFragmentPrecisionHighOnESSL1 = fragmentPrecisionHigh;
    }

    void setLoopNestingLevel(int loopNestintLevel)
    {
        mLoopNestingLevel = loopNestintLevel;
    }

    void incrLoopNestingLevel() { ++mLoopNestingLevel; }
    void decrLoopNestingLevel() { --mLoopNestingLevel; }

    void incrSwitchNestingLevel() { ++mSwitchNestingLevel; }
    void decrSwitchNestingLevel() { --mSwitchNestingLevel; }

    bool isComputeShaderLocalSizeDeclared() const { return mComputeShaderLocalSizeDeclared; }
    sh::WorkGroupSize getComputeShaderLocalSize() const;

    void enterFunctionDeclaration() { mDeclaringFunction = true; }

    void exitFunctionDeclaration() { mDeclaringFunction = false; }

    bool declaringFunction() const { return mDeclaringFunction; }

    // This method is guaranteed to succeed, even if no variable with 'name' exists.
    const TVariable *getNamedVariable(const TSourceLoc &location, const TString *name, const TSymbol *symbol);
    TIntermTyped *parseVariableIdentifier(const TSourceLoc &location,
                                          const TString *name,
                                          const TSymbol *symbol);

    bool parseVectorFields(const TString&, int vecSize, TVectorFields&, const TSourceLoc &line);

    void assignError(const TSourceLoc &line, const char *op, TString left, TString right);
    void unaryOpError(const TSourceLoc &line, const char *op, TString operand);
    void binaryOpError(const TSourceLoc &line, const char *op, TString left, TString right);

    // Check functions - the ones that return bool return false if an error was generated.

    bool checkIsNotReserved(const TSourceLoc &line, const TString &identifier);
    void checkPrecisionSpecified(const TSourceLoc &line, TPrecision precision, TBasicType type);
    bool checkCanBeLValue(const TSourceLoc &line, const char *op, TIntermTyped *node);
    void checkIsConst(TIntermTyped *node);
    void checkIsScalarInteger(TIntermTyped *node, const char *token);
    bool checkIsAtGlobalLevel(const TSourceLoc &line, const char *token);
    bool checkConstructorArguments(const TSourceLoc &line,
                                   TIntermNode *argumentsNode,
                                   const TFunction &function,
                                   TOperator op,
                                   const TType &type);

    // Returns a sanitized array size to use (the size is at least 1).
    unsigned int checkIsValidArraySize(const TSourceLoc &line, TIntermTyped *expr);
    bool checkIsValidQualifierForArray(const TSourceLoc &line, const TPublicType &elementQualifier);
    bool checkIsValidTypeForArray(const TSourceLoc &line, const TPublicType &elementType);
    bool checkIsNonVoid(const TSourceLoc &line, const TString &identifier, const TBasicType &type);
    void checkIsScalarBool(const TSourceLoc &line, const TIntermTyped *type);
    void checkIsScalarBool(const TSourceLoc &line, const TPublicType &pType);
    bool checkIsNotSampler(const TSourceLoc &line,
                           const TTypeSpecifierNonArray &pType,
                           const char *reason);
    void checkDeclaratorLocationIsNotSpecified(const TSourceLoc &line, const TPublicType &pType);
    void checkLocationIsNotSpecified(const TSourceLoc &location,
                                     const TLayoutQualifier &layoutQualifier);
    void checkOutParameterIsNotSampler(const TSourceLoc &line,
                                       TQualifier qualifier,
                                       const TType &type);
    void checkIsParameterQualifierValid(const TSourceLoc &line,
                                        const TTypeQualifierBuilder &typeQualifierBuilder,
                                        TType *type);
    bool checkCanUseExtension(const TSourceLoc &line, const TString &extension);
    void singleDeclarationErrorCheck(const TPublicType &publicType,
                                     const TSourceLoc &identifierLocation);
    void checkLayoutQualifierSupported(const TSourceLoc &location,
                                       const TString &layoutQualifierName,
                                       int versionRequired);
    bool checkWorkGroupSizeIsNotSpecified(const TSourceLoc &location,
                                          const TLayoutQualifier &layoutQualifier);

    void functionCallLValueErrorCheck(const TFunction *fnCandidate, TIntermAggregate *fnCall);
    void checkInvariantVariableQualifier(bool invariant,
                                         const TQualifier qualifier,
                                         const TSourceLoc &invariantLocation);
    void checkInputOutputTypeIsValidES3(const TQualifier qualifier,
                                        const TPublicType &type,
                                        const TSourceLoc &qualifierLocation);

    const TPragma &pragma() const { return mDirectiveHandler.pragma(); }
    const TExtensionBehavior &extensionBehavior() const { return mDirectiveHandler.extensionBehavior(); }
    bool supportsExtension(const char *extension);
    bool isExtensionEnabled(const char *extension) const;
    void handleExtensionDirective(const TSourceLoc &loc, const char *extName, const char *behavior);
    void handlePragmaDirective(const TSourceLoc &loc, const char *name, const char *value, bool stdgl);

    bool containsSampler(const TType &type);
    const TFunction* findFunction(
        const TSourceLoc &line, TFunction *pfnCall, int inputShaderVersion, bool *builtIn = 0);
    bool executeInitializer(const TSourceLoc &line,
                            const TString &identifier,
                            const TPublicType &pType,
                            TIntermTyped *initializer,
                            TIntermNode **intermNode);

    TPublicType addFullySpecifiedType(const TTypeQualifierBuilder &typeQualifierBuilder,
                                      const TPublicType &typeSpecifier);

    TIntermAggregate *parseSingleDeclaration(TPublicType &publicType,
                                             const TSourceLoc &identifierOrTypeLocation,
                                             const TString &identifier);
    TIntermAggregate *parseSingleArrayDeclaration(TPublicType &publicType,
                                                  const TSourceLoc &identifierLocation,
                                                  const TString &identifier,
                                                  const TSourceLoc &indexLocation,
                                                  TIntermTyped *indexExpression);
    TIntermAggregate *parseSingleInitDeclaration(const TPublicType &publicType,
                                                 const TSourceLoc &identifierLocation,
                                                 const TString &identifier,
                                                 const TSourceLoc &initLocation,
                                                 TIntermTyped *initializer);

    // Parse a declaration like "type a[n] = initializer"
    // Note that this does not apply to declarations like "type[n] a = initializer"
    TIntermAggregate *parseSingleArrayInitDeclaration(TPublicType &publicType,
                                                      const TSourceLoc &identifierLocation,
                                                      const TString &identifier,
                                                      const TSourceLoc &indexLocation,
                                                      TIntermTyped *indexExpression,
                                                      const TSourceLoc &initLocation,
                                                      TIntermTyped *initializer);

    TIntermAggregate *parseInvariantDeclaration(const TTypeQualifierBuilder &typeQualifierBuilder,
                                                const TSourceLoc &identifierLoc,
                                                const TString *identifier,
                                                const TSymbol *symbol);

    TIntermAggregate *parseDeclarator(TPublicType &publicType,
                                      TIntermAggregate *aggregateDeclaration,
                                      const TSourceLoc &identifierLocation,
                                      const TString &identifier);
    TIntermAggregate *parseArrayDeclarator(TPublicType &publicType,
                                           TIntermAggregate *aggregateDeclaration,
                                           const TSourceLoc &identifierLocation,
                                           const TString &identifier,
                                           const TSourceLoc &arrayLocation,
                                           TIntermTyped *indexExpression);
    TIntermAggregate *parseInitDeclarator(const TPublicType &publicType,
                                          TIntermAggregate *aggregateDeclaration,
                                          const TSourceLoc &identifierLocation,
                                          const TString &identifier,
                                          const TSourceLoc &initLocation,
                                          TIntermTyped *initializer);

    // Parse a declarator like "a[n] = initializer"
    TIntermAggregate *parseArrayInitDeclarator(const TPublicType &publicType,
                                               TIntermAggregate *aggregateDeclaration,
                                               const TSourceLoc &identifierLocation,
                                               const TString &identifier,
                                               const TSourceLoc &indexLocation,
                                               TIntermTyped *indexExpression,
                                               const TSourceLoc &initLocation,
                                               TIntermTyped *initializer);

    void parseGlobalLayoutQualifier(const TTypeQualifierBuilder &typeQualifierBuilder);
    TIntermAggregate *addFunctionPrototypeDeclaration(const TFunction &parsedFunction,
                                                      const TSourceLoc &location);
    TIntermFunctionDefinition *addFunctionDefinition(const TFunction &function,
                                                     TIntermAggregate *functionParameters,
                                                     TIntermBlock *functionBody,
                                                     const TSourceLoc &location);
    void parseFunctionDefinitionHeader(const TSourceLoc &location,
                                       TFunction **function,
                                       TIntermAggregate **aggregateOut);
    TFunction *parseFunctionDeclarator(const TSourceLoc &location,
                                       TFunction *function);
    TFunction *parseFunctionHeader(const TPublicType &type,
                                   const TString *name,
                                   const TSourceLoc &location);
    TFunction *addConstructorFunc(const TPublicType &publicType);
    TIntermTyped *addConstructor(TIntermNode *arguments,
                                 TOperator op,
                                 TFunction *fnCall,
                                 const TSourceLoc &line);

    TIntermTyped *addIndexExpression(TIntermTyped *baseExpression,
                                     const TSourceLoc& location,
                                     TIntermTyped *indexExpression);
    TIntermTyped* addFieldSelectionExpression(TIntermTyped *baseExpression,
                                              const TSourceLoc &dotLocation,
                                              const TString &fieldString,
                                              const TSourceLoc &fieldLocation);

    TFieldList *addStructDeclaratorListWithQualifiers(
        const TTypeQualifierBuilder &typeQualifierBuilder,
        TPublicType *typeSpecifier,
        TFieldList *fieldList);
    TFieldList *addStructDeclaratorList(const TPublicType &typeSpecifier, TFieldList *fieldList);
    TTypeSpecifierNonArray addStructure(const TSourceLoc &structLine,
                                        const TSourceLoc &nameLine,
                                        const TString *structName,
                                        TFieldList *fieldList);

    TIntermAggregate *addInterfaceBlock(const TTypeQualifierBuilder &typeQualifierBuilder,
                                        const TSourceLoc &nameLine,
                                        const TString &blockName,
                                        TFieldList *fieldList,
                                        const TString *instanceName,
                                        const TSourceLoc &instanceLine,
                                        TIntermTyped *arrayIndex,
                                        const TSourceLoc &arrayIndexLine);

    void parseLocalSize(const TString &qualifierType,
                        const TSourceLoc &qualifierTypeLine,
                        int intValue,
                        const TSourceLoc &intValueLine,
                        const std::string &intValueString,
                        size_t index,
                        sh::WorkGroupSize *localSize);
    TLayoutQualifier parseLayoutQualifier(
        const TString &qualifierType, const TSourceLoc &qualifierTypeLine);
    TLayoutQualifier parseLayoutQualifier(const TString &qualifierType,
                                          const TSourceLoc &qualifierTypeLine,
                                          int intValue,
                                          const TSourceLoc &intValueLine);
    TTypeQualifierBuilder *createTypeQualifierBuilder(const TSourceLoc &loc);
    TLayoutQualifier joinLayoutQualifiers(TLayoutQualifier leftQualifier,
                                          TLayoutQualifier rightQualifier,
                                          const TSourceLoc &rightQualifierLocation);

    // Performs an error check for embedded struct declarations.
    void enterStructDeclaration(const TSourceLoc &line, const TString &identifier);
    void exitStructDeclaration();

    void checkIsBelowStructNestingLimit(const TSourceLoc &line, const TField &field);

    TIntermSwitch *addSwitch(TIntermTyped *init,
                             TIntermBlock *statementList,
                             const TSourceLoc &loc);
    TIntermCase *addCase(TIntermTyped *condition, const TSourceLoc &loc);
    TIntermCase *addDefault(const TSourceLoc &loc);

    TIntermTyped *addUnaryMath(TOperator op, TIntermTyped *child, const TSourceLoc &loc);
    TIntermTyped *addUnaryMathLValue(TOperator op, TIntermTyped *child, const TSourceLoc &loc);
    TIntermTyped *addBinaryMath(
        TOperator op, TIntermTyped *left, TIntermTyped *right, const TSourceLoc &loc);
    TIntermTyped *addBinaryMathBooleanResult(
        TOperator op, TIntermTyped *left, TIntermTyped *right, const TSourceLoc &loc);
    TIntermTyped *addAssign(
        TOperator op, TIntermTyped *left, TIntermTyped *right, const TSourceLoc &loc);

    TIntermTyped *addComma(TIntermTyped *left, TIntermTyped *right, const TSourceLoc &loc);

    TIntermBranch *addBranch(TOperator op, const TSourceLoc &loc);
    TIntermBranch *addBranch(TOperator op, TIntermTyped *returnValue, const TSourceLoc &loc);

    void checkTextureOffsetConst(TIntermAggregate *functionCall);
    TIntermTyped *addFunctionCallOrMethod(TFunction *fnCall,
                                          TIntermNode *paramNode,
                                          TIntermNode *thisNode,
                                          const TSourceLoc &loc,
                                          bool *fatalError);

    TIntermTyped *addTernarySelection(TIntermTyped *cond,
                                      TIntermTyped *trueExpression,
                                      TIntermTyped *falseExpression,
                                      const TSourceLoc &line);

    // TODO(jmadill): make these private
    TIntermediate intermediate;  // to build a parse tree
    TSymbolTable &symbolTable;   // symbol table that goes with the language currently being parsed

  private:
    // Returns a clamped index.
    int checkIndexOutOfRange(bool outOfRangeIndexIsError,
                             const TSourceLoc &location,
                             int index,
                             int arraySize,
                             const char *reason,
                             const char *token);

    bool declareVariable(const TSourceLoc &line, const TString &identifier, const TType &type, TVariable **variable);

    void checkCanBeDeclaredWithoutInitializer(const TSourceLoc &line,
                                              const TString &identifier,
                                              TPublicType *type);

    bool checkIsValidTypeAndQualifierForArray(const TSourceLoc &indexLocation,
                                              const TPublicType &elementType);

    // Assumes that multiplication op has already been set based on the types.
    bool isMultiplicationTypeCombinationValid(TOperator op, const TType &left, const TType &right);

    TIntermTyped *addBinaryMathInternal(
        TOperator op, TIntermTyped *left, TIntermTyped *right, const TSourceLoc &loc);
    TIntermTyped *createAssign(
        TOperator op, TIntermTyped *left, TIntermTyped *right, const TSourceLoc &loc);
    // The funcReturnType parameter is expected to be non-null when the operation is a built-in function.
    // It is expected to be null for other unary operators.
    TIntermTyped *createUnaryMath(
        TOperator op, TIntermTyped *child, const TSourceLoc &loc, const TType *funcReturnType);

    // Return true if the checks pass
    bool binaryOpCommonCheck(
        TOperator op, TIntermTyped *left, TIntermTyped *right, const TSourceLoc &loc);

    // Set to true when the last/current declarator list was started with an empty declaration.
    bool mDeferredSingleDeclarationErrorCheck;

    sh::GLenum mShaderType;              // vertex or fragment language (future: pack or unpack)
    ShShaderSpec mShaderSpec;  // The language specification compiler conforms to - GLES2 or WebGL.
    ShCompileOptions mCompileOptions;  // Options passed to TCompiler
    int mShaderVersion;
    TIntermBlock *mTreeRoot;     // root of parse tree being created
    int mLoopNestingLevel;       // 0 if outside all loops
    int mStructNestingLevel;     // incremented while parsing a struct declaration
    int mSwitchNestingLevel;     // 0 if outside all switch statements
    const TType
        *mCurrentFunctionType;   // the return type of the function that's currently being parsed
    bool mFunctionReturnsValue;  // true if a non-void function has a return
    bool mChecksPrecisionErrors;  // true if an error will be generated when a variable is declared
                                  // without precision, explicit or implicit.
    bool mFragmentPrecisionHighOnESSL1;  // true if highp precision is supported when compiling
                                         // ESSL1.
    TLayoutMatrixPacking mDefaultMatrixPacking;
    TLayoutBlockStorage mDefaultBlockStorage;
    TString mHashErrMsg;
    TDiagnostics mDiagnostics;
    TDirectiveHandler mDirectiveHandler;
    pp::Preprocessor mPreprocessor;
    void *mScanner;
    bool mUsesFragData; // track if we are using both gl_FragData and gl_FragColor
    bool mUsesFragColor;
    bool mUsesSecondaryOutputs;  // Track if we are using either gl_SecondaryFragData or
                                 // gl_Secondary FragColor or both.
    int mMinProgramTexelOffset;
    int mMaxProgramTexelOffset;

    // keep track of local group size declared in layout. It should be declared only once.
    bool mComputeShaderLocalSizeDeclared;
    sh::WorkGroupSize mComputeShaderLocalSize;
    // keeps track whether we are declaring / defining a function
    bool mDeclaringFunction;
};

int PaParseStrings(
    size_t count, const char *const string[], const int length[], TParseContext *context);

#endif // COMPILER_TRANSLATOR_PARSECONTEXT_H_
