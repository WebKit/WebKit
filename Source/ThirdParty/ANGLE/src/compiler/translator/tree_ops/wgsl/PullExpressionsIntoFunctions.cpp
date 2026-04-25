//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PullExpressionsIntoFunctions: certains patterns must be pulled into
// functions.
//

#include "compiler/translator/tree_ops/wgsl/PullExpressionsIntoFunctions.h"

#include "common/log_utils.h"
#include "compiler/translator/BaseTypes.h"
#include "compiler/translator/Common.h"
#include "compiler/translator/Compiler.h"
#include "compiler/translator/ImmutableString.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/Operator_autogen.h"
#include "compiler/translator/OutputTree.h"
#include "compiler/translator/SymbolUniqueId.h"
#include "compiler/translator/Types.h"
#include "compiler/translator/tree_ops/wgsl/RewriteMultielementSwizzleAssignment.h"
#include "compiler/translator/tree_util/DriverUniform.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"
#include "compiler/translator/tree_util/Visit.h"
#include "compiler/translator/util.h"

namespace sh
{
namespace
{

const TType *GetHelperType(const TType &type, std::optional<TQualifier> qualifier)
{
    // If the type does not have a precision, it typically means that none of the values that
    // comprise the typed expression have precision (for example because they are constants, or
    // bool), and there isn't any precision propagation happening from nearby operands.  In that
    // case, assign a highp precision to them; the driver will probably inline and eliminate the
    // call anyway, and the precision does not affect anything.
    constexpr TPrecision kDefaultPrecision = EbpHigh;

    TType *newType = new TType(type);
    if (IsPrecisionApplicableToType(type.getBasicType()))
    {
        newType->setPrecision(type.getPrecision() != EbpUndefined ? type.getPrecision()
                                                                  : kDefaultPrecision);
    }
    if (qualifier.has_value())
    {
        newType->setQualifier(qualifier.value());
    }

    return newType;
}

class PullExpressionsIntoFunctionsTraverser : public TIntermTraverser
{
  public:
    PullExpressionsIntoFunctionsTraverser(TCompiler *compiler, TSymbolTable *symbolTable)
        : TIntermTraverser(true, false, true, symbolTable), mCompiler(compiler)
    {}

    // Just used to keep track of the current function.
    bool visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *func) override
    {
        mCurrentFunction = func;
        return true;
    }

    // Records a usage of a symbol if traversing an untranslatable construct. Only records usage of
    // temporaries and parameters.
    void visitSymbol(TIntermSymbol *symbol) override
    {
        TQualifier q                 = symbol->getType().getQualifier();
        const bool symbolIsNotGlobal = q == EvqTemporary || IsParam(q);
        if (mUntranslatableConstructDepth > 0 && symbolIsNotGlobal)
        {
            mSymbolsInsideUntranslatableConstructs[symbol->variable().uniqueId()] =
                &symbol->variable();
        }
    }

    // Caches all variable declarations, in case they need to be moved into the global scope (in
    // case the variable is used in an untranslatable construct, which are moved into other
    // functions entirely).
    bool visitDeclaration(Visit visit, TIntermDeclaration *decl) override
    {
        if (visit != PreVisit)
        {
            return true;
        }

        // No need to replace variables declared inside the untranslatable construct.
        Declaration declView = ViewDeclaration(*decl);
        if (mUntranslatableConstructDepth == 0 &&
            declView.symbol.getType().getQualifier() == EvqTemporary)
        {
            // Declarations should always be split into individual declarations before
            ASSERT(decl->getChildCount() == 1);

            mDeclarationCache[declView.symbol.variable().uniqueId()] = {decl, getParentNode()};
        }

        return true;
    }

    // --------------------------------------------------------------------
    // The rest of the traverser detects untranslatable constructs:

    bool visitTernary(Visit visit, TIntermTernary *ternary) override
    {
        handleUntranslatableConstruct(visit, {{ternary}, getParentNode(), mCurrentFunction});
        return true;
    }

    bool visitBinary(Visit visit, TIntermBinary *binary) override
    {
        if (binary->getOp() == EOpComma)
        {
            handleUntranslatableConstruct(
                visit, UntranslatableConstructAndMetadata{UntranslatableCommaOperator{binary},
                                                          getParentNode(), mCurrentFunction});
        }
        if (IsMultielementSwizzleAssignment(binary->getOp(), binary->getLeft()) &&
            !CanRewriteMultiElementSwizzleAssignmentEasily(binary, getParentNode()))
        {

            handleUntranslatableConstruct(
                visit, UntranslatableConstructAndMetadata{UntranslatableMultiElementSwizzle{binary},
                                                          getParentNode(), mCurrentFunction});
        }

        return true;
    }

    bool visitAggregate(Visit visit, TIntermAggregate *aggregate) override
    {
        const TFunction *calledFunction = aggregate->getFunction();
        if (aggregate->getOp() != EOpCallFunctionInAST || !calledFunction)
        {
            return true;
        }

        TSet<const TVariable *> outparamVars;
        bool foundIncompatibleOutparam = false;
        for (size_t i = 0; i < calledFunction->getParamCount(); ++i)
        {
            TQualifier paramQualifier = calledFunction->getParam(i)->getType().getQualifier();
            if (IsParamOut(paramQualifier))
            {
                const TVariable *argRootVariable = FindRootVariable(aggregate->getChildNode(i));
                // Any global vars as outparams can conflict (in terms of WGSL's pointer alias
                // analysis) with accesses to the actual global var, so to be safe, any
                // non-temporary vars as outparams are consider to be incompatible.
                // WGSL also requires pointer to specify whether they are pointers to temporaries
                // or module-scope variables, which makes WGSL output more complicated unless we
                // only ever allow temporaries as outparams.
                // This makes an exception for parameters as well, which can be treated as
                // temporaries.
                TQualifier q = argRootVariable->getType().getQualifier();
                if (q != EvqTemporary && !IsParam(q))
                {
                    foundIncompatibleOutparam = true;
                    break;
                }
                // Different temporary variables can all be used as outparams to the same function.
                // In fact this is what the translation will do for incompatible calls.
                if (!outparamVars.insert(argRootVariable).second)
                {
                    foundIncompatibleOutparam = true;
                    break;
                }
            }
        }
        if (!foundIncompatibleOutparam)
        {
            return true;
        }

        handleUntranslatableConstruct(visit, {aggregate, getParentNode(), mCurrentFunction});

        return true;
    }

    bool foundUntranslatableConstruct() const { return !mUntranslatableConstructs.empty(); }

    bool update(TIntermBlock *root)
    {
        return replaceTempVarsWithGlobals(root) &&
               pullUntranslatableConstructsIntoFunctions(root) && updateTree(mCompiler, root);
    }

  private:
    using UntranslatableTernary = TIntermTernary *;
    struct UntranslatableCommaOperator
    {
        TIntermBinary *commaOperator;
    };
    using UntranslatableFunctionCallWithOutparams = TIntermAggregate *;
    struct UntranslatableMultiElementSwizzle
    {
        TIntermBinary *multielementSwizzle;
    };

    using UntranslatableConstruct = std::variant<UntranslatableTernary,
                                                 UntranslatableCommaOperator,
                                                 UntranslatableFunctionCallWithOutparams,
                                                 UntranslatableMultiElementSwizzle>;

    struct UntranslatableConstructAndMetadata
    {
        UntranslatableConstruct construct;
        TIntermNode *parent;
        const TIntermFunctionDefinition *parentFunction;
    };

    void handleUntranslatableConstruct(Visit visit, UntranslatableConstructAndMetadata construct)
    {
        if (mInGlobalScope)
        {
            UNREACHABLE();
            return;
        }

        // We are currently visiting an untranslatable construct.
        if (visit == PostVisit)
        {
            // After visiting children, decrement our depth.
            mUntranslatableConstructDepth--;
            return;
        }

        ASSERT(visit == PreVisit);

        // If inside another untranslatable construct, continue to traverse to find symbols and
        // declarations but do not record more untranslatable constructs. One layer at a time!
        if (mUntranslatableConstructDepth++ > 0)
        {
            return;
        }

        mUntranslatableConstructs.push_back(std::move(construct));
    }

    bool addParamsFromOtherFunctionAndReplace(TFunction *substituteFunction,
                                              TIntermFunctionDefinition *substituteFunctionDef,
                                              const TIntermFunctionDefinition *oldFunction)
    {
        // NOTE: don't always need to forward every parameter, but it's easiest.
        VariableReplacementMap argumentMap;
        for (size_t paramIndex = 0; paramIndex < oldFunction->getFunction()->getParamCount();
             ++paramIndex)
        {

            const TVariable *originalParam = oldFunction->getFunction()->getParam(paramIndex);
            TVariable *substituteArgument =
                new TVariable(mSymbolTable, originalParam->name(), &originalParam->getType(),
                              originalParam->symbolType());
            // Not replaced, add an identical parameter.
            substituteFunction->addParameter(substituteArgument);
            argumentMap[originalParam->uniqueId()] = new TIntermSymbol(substituteArgument);
        }

        if (!ReplaceVariables(mCompiler, substituteFunctionDef, argumentMap))
        {
            return false;
        }

        return true;
    }

    // Converts a ternary into an if/else block within a new function.
    // Adds a new function prototype and a new function definition to the respective
    // TIntermSequences.
    TFunction *replaceTernary(TIntermTernary *ternary,
                              const TIntermFunctionDefinition *parentFunction,
                              TIntermSequence &newFunctionPrototypes,
                              TIntermSequence &newFunctionDefinitions)
    {
        // Pull into function with if/else, should work because all global vars.
        // Can just use ternary->getTrueExpression() and ternary->getCondition() etc. directly
        // because they should not reference any temporaries, and they do not need to be
        // deepCopied because they are moving rather than being copied.
        TIntermBranch *retTrueCase =
            new TIntermBranch(TOperator::EOpReturn, ternary->getTrueExpression());
        TIntermBranch *retFalseCase =
            new TIntermBranch(TOperator::EOpReturn, ternary->getFalseExpression());
        TIntermIfElse *ifElse =
            new TIntermIfElse(ternary->getCondition(), new TIntermBlock({retTrueCase}),
                              new TIntermBlock({retFalseCase}));
        TIntermBlock *substituteFunctionBody = new TIntermBlock({ifElse});

        TFunction *substituteFunction = new TFunction(
            mSymbolTable, kEmptyImmutableString, SymbolType::AngleInternal,
            GetHelperType(ternary->getType(), EvqTemporary), !ternary->hasSideEffects());

        // Make sure to insert new function definitions and prototypes.
        newFunctionPrototypes.push_back(new TIntermFunctionPrototype(substituteFunction));
        TIntermFunctionDefinition *substituteFunctionDef = new TIntermFunctionDefinition(
            new TIntermFunctionPrototype(substituteFunction), substituteFunctionBody);
        newFunctionDefinitions.push_back(substituteFunctionDef);

        addParamsFromOtherFunctionAndReplace(substituteFunction, substituteFunctionDef,
                                             parentFunction);

        return substituteFunction;
    }

    TFunction *replaceCallToFuncWithOutparams(TIntermAggregate *funcCall,
                                              const TIntermFunctionDefinition *parentFunction,
                                              TIntermSequence &newFunctionPrototypes,
                                              TIntermSequence &newFunctionDefinitions)
    {
        TIntermSequence initSequence;
        TIntermSequence finishSequence;

        TIntermSequence newCallArgs;

        // Create temporaries for all the parameters. Arguments must be evaluated in order.
        const TFunction *callee = funcCall->getFunction();
        for (size_t i = 0; i < callee->getParamCount(); i++)
        {
            TIntermTyped *arg = funcCall->getChildNode(i)->getAsTyped();
            ASSERT(arg);
            const TVariable *param = callee->getParam(i);
            TQualifier paramQ      = param->getType().getQualifier();

            // Create temp variable for each argument to the original function.
            TVariable *newArg = CreateTempVariable(mSymbolTable, &param->getType(), EvqTemporary);
            // We will use temp as the argument for the new function call.
            newCallArgs.push_back(new TIntermSymbol(newArg));

            if (paramQ == EvqParamInOut || paramQ == EvqParamOut)
            {
                // If inout-param, save pointer to argument. Otherwise we may evaluate arg twice,
                // and even though arg must be an l-value, it can still have side effects (e.g. in
                // x[i++] = ...);
                TVariable *newArgPtr =
                    CreateTempVariable(mSymbolTable, &param->getType(), EvqTemporary);
                TFunction *getPointerFunc =
                    new TFunction(mSymbolTable, ImmutableString("ANGLE_takePointer"),
                                  SymbolType::AngleInternal, &param->getType(), false);
                getPointerFunc->addParameter(
                    CreateTempVariable(mSymbolTable, &param->getType(), EvqParamInOut));
                TIntermSequence *getPointerCallArgs = new TIntermSequence({arg});
                TIntermAggregate *getPointerCall =
                    TIntermAggregate::CreateRawFunctionCall(*getPointerFunc, getPointerCallArgs);
                // temp_ptr = &arg; (argument should only reference global variables)
                initSequence.push_back(CreateTempInitDeclarationNode(newArgPtr, getPointerCall));
                if (paramQ == EvqParamInOut)
                {
                    // temp = *temp_ptr; (The traverser will see the pointer variable and
                    // automatically dereference it.)
                    initSequence.push_back(
                        CreateTempInitDeclarationNode(newArg, new TIntermSymbol(newArgPtr)));
                }
                else
                {
                    ASSERT(paramQ == EvqParamOut);
                    // Before the function call, just create empty var for outparam purposes. E.g.:
                    // temp : f32;
                    initSequence.push_back(CreateTempDeclarationNode(newArg));
                }

                // After the function call:
                // *temp_ptr = temp;
                finishSequence.push_back(
                    CreateTempAssignmentNode(newArgPtr, new TIntermSymbol(newArg)));
            }
            else if (paramQ == EvqParamIn || paramQ == EvqParamConst)
            {
                // temp = argument; (argument should only reference global variables),
                initSequence.push_back(CreateTempInitDeclarationNode(newArg, arg));
            }
            else
            {
                UNREACHABLE();
            }
        }

        // Start the callSequence with the initSequence.
        TIntermSequence callSequence = std::move(initSequence);

        // Create a call to the function with outparams.
        TIntermAggregate *newCall = TIntermAggregate::CreateFunctionCall(*callee, &newCallArgs);
        // If necessary, save the return value of the call.
        TVariable *retVal = nullptr;
        const bool needsToSaveRetVal =
            funcCall->getFunction()->getReturnType().getBasicType() != EbtVoid;
        if (needsToSaveRetVal)
        {
            retVal = CreateTempVariable(mSymbolTable, &funcCall->getFunction()->getReturnType(),
                                        EvqTemporary);
            TIntermDeclaration *savedRetVal = CreateTempInitDeclarationNode(retVal, newCall);
            callSequence.push_back(savedRetVal);
        }
        else
        {
            callSequence.push_back(newCall);
        }

        // Finish with the finishSequence.
        callSequence.insert(callSequence.end(), finishSequence.begin(), finishSequence.end());

        // Return a value if necessary.
        if (needsToSaveRetVal)
        {
            callSequence.push_back(
                new TIntermBranch(TOperator::EOpReturn, new TIntermSymbol(retVal)));
        }

        TIntermBlock *substituteFunctionBody = new TIntermBlock(std::move(callSequence));

        TFunction *substituteFunction =
            new TFunction(mSymbolTable, kEmptyImmutableString, SymbolType::AngleInternal,
                          GetHelperType(funcCall->getFunction()->getReturnType(), std::nullopt),
                          funcCall->getFunction()->isKnownToNotHaveSideEffects());

        // Make sure to insert new function definitions and prototypes.
        newFunctionPrototypes.push_back(new TIntermFunctionPrototype(substituteFunction));
        TIntermFunctionDefinition *substituteFunctionDef = new TIntermFunctionDefinition(
            new TIntermFunctionPrototype(substituteFunction), substituteFunctionBody);
        newFunctionDefinitions.push_back(substituteFunctionDef);

        addParamsFromOtherFunctionAndReplace(substituteFunction, substituteFunctionDef,
                                             parentFunction);

        return substituteFunction;
    }

    TFunction *replaceSequenceOperator(TIntermBinary *sequenceOperator,
                                       const TIntermFunctionDefinition *parentFunction,
                                       TIntermSequence &newFunctionPrototypes,
                                       TIntermSequence &newFunctionDefinitions)
    {
        ASSERT(sequenceOperator->getOp() == EOpComma);

        // Pull into function that just puts one statement after the other.

        TIntermSequence extractedStmts;

        // Flatten the nested comma operators into a sequence of statements.
        std::stack<TIntermTyped *, TVector<TIntermTyped *>> stmts;
        stmts.push(sequenceOperator->getRight());
        stmts.push(sequenceOperator->getLeft());
        while (!stmts.empty())
        {
            TIntermTyped *stmt = stmts.top();
            stmts.pop();

            if (TIntermBinary *nestedSequenceOperator = stmt->getAsBinaryNode();
                nestedSequenceOperator && nestedSequenceOperator->getOp() == EOpComma)
            {
                stmts.push(nestedSequenceOperator->getRight());
                stmts.push(nestedSequenceOperator->getLeft());
                continue;
            }

            extractedStmts.push_back(stmt);
        }

        // The last statement needs a return, if it is not of type void (i.e. the type of a function
        // call to a void-returning function).
        TIntermNode *lastStmt = extractedStmts.back();
        if (lastStmt->getAsTyped()->getBasicType() != EbtVoid)
        {
            extractedStmts.back() = new TIntermBranch(TOperator::EOpReturn, lastStmt->getAsTyped());
        }

        TIntermBlock *substituteFunctionBody = new TIntermBlock(std::move(extractedStmts));

        TFunction *substituteFunction =
            new TFunction(mSymbolTable, kEmptyImmutableString, SymbolType::AngleInternal,
                          GetHelperType(sequenceOperator->getType(), EvqTemporary),
                          !sequenceOperator->hasSideEffects());

        // Make sure to insert new function definitions and prototypes.
        newFunctionPrototypes.push_back(new TIntermFunctionPrototype(substituteFunction));
        TIntermFunctionDefinition *substituteFunctionDef = new TIntermFunctionDefinition(
            new TIntermFunctionPrototype(substituteFunction), substituteFunctionBody);
        newFunctionDefinitions.push_back(substituteFunctionDef);

        addParamsFromOtherFunctionAndReplace(substituteFunction, substituteFunctionDef,
                                             parentFunction);

        return substituteFunction;
    }

    TFunction *replaceDifficultMultielementSwizzle(TIntermBinary *swizzleAssignment,
                                                   const TIntermFunctionDefinition *parentFunction,
                                                   TIntermSequence &newFunctionPrototypes,
                                                   TIntermSequence &newFunctionDefinitions)
    {
        // Pull into a function that takes the swizzle operand as an outparam, which will then be
        // handled by future passes of this AST traverser if necessary.
        TIntermSwizzle *oldSwizzle = swizzleAssignment->getLeft()->getAsSwizzleNode();
        ASSERT(oldSwizzle);

        TType *paramType = new TType(oldSwizzle->getOperand()->getType());
        paramType->setQualifier(swizzleAssignment->getOp() == EOpAssign ? EvqParamOut
                                                                        : EvqParamInOut);
        TVariable *operandParam = new TVariable(mSymbolTable, kEmptyImmutableString, paramType,
                                                SymbolType::AngleInternal);

        // Swizzle the outparam:
        TIntermSwizzle *swizzledParam =
            new TIntermSwizzle(new TIntermSymbol(operandParam), oldSwizzle->getSwizzleOffsets());

        // Assign to the swizzled outparam instead of the original
        TIntermBinary *newSwizzleAssignment = new TIntermBinary(
            swizzleAssignment->getOp(), swizzledParam, swizzleAssignment->getRight());

        // The swizzle assignment has a result, so return it.
        TIntermBranch *result = new TIntermBranch(TOperator::EOpReturn, swizzledParam->deepCopy());
        TIntermBlock *substituteFunctionBody = new TIntermBlock({newSwizzleAssignment, result});

        TFunction *substituteFunction =
            new TFunction(mSymbolTable, kEmptyImmutableString, SymbolType::AngleInternal,
                          GetHelperType(swizzleAssignment->getType(), EvqTemporary),
                          !swizzleAssignment->getRight()->hasSideEffects());

        substituteFunction->addParameter(operandParam);

        // Make sure to insert new function definitions and prototypes.
        newFunctionPrototypes.push_back(new TIntermFunctionPrototype(substituteFunction));
        TIntermFunctionDefinition *substituteFunctionDef = new TIntermFunctionDefinition(
            new TIntermFunctionPrototype(substituteFunction), substituteFunctionBody);
        newFunctionDefinitions.push_back(substituteFunctionDef);

        addParamsFromOtherFunctionAndReplace(substituteFunction, substituteFunctionDef,
                                             parentFunction);

        return substituteFunction;
    }

    bool replaceTempVarsWithGlobals(TIntermBlock *root)
    {
        TIntermSequence globalDeclarations;
        VariableReplacementMap tempToGlobal;
        for (const auto &[varId, var] : mSymbolsInsideUntranslatableConstructs)
        {
            auto declIt = mDeclarationCache.find(varId);
            if (declIt == mDeclarationCache.end())
            {
                // If the declaration is inside the untranslatable construct, it won't be in the map
                // because it does not need to be replaced with a global.
                continue;
            }

            std::pair<TIntermDeclaration *, TIntermNode *> &declAndParent = declIt->second;
            TIntermDeclaration *decl                                      = declAndParent.first;
            TIntermNode *parentOfDecl                                     = declAndParent.second;

            ASSERT(var->getType().getQualifier() == EvqTemporary);

            TType *globalType = new TType(var->getType());
            globalType->setQualifier(EvqGlobal);

            const TVariable *replacementVariable =
                new TVariable(mSymbolTable, var->name(), globalType, var->symbolType());

            // Make sure to declare global variable replacement. Ignore the init expression, that
            // will be done in the same place as the temporary declaration.
            globalDeclarations.push_back(new TIntermDeclaration({replacementVariable}));

            if (TIntermBinary *binaryInitExpr = decl->getChildNode(0)->getAsBinaryNode())
            {
                ASSERT(binaryInitExpr->getOp() == EOpInitialize);
                TIntermBinary *newAssignment = new TIntermBinary(
                    EOpAssign, new TIntermSymbol(replacementVariable), binaryInitExpr->getRight());
                // Replace the declaration with the binary init expression.
                parentOfDecl->replaceChildNode(decl, newAssignment);

                // The untranslatable constructs that were the RHS of this binary initialization
                // expression now have a new parent, the new binary assignment expression.
                for (UntranslatableConstructAndMetadata &constructAndMetadata :
                     mUntranslatableConstructs)
                {
                    if (constructAndMetadata.parent == binaryInitExpr)
                    {
                        constructAndMetadata.parent = newAssignment;
                    }
                }
            }
            else
            {
                // TODO(anglebug.com/42267100): there can be declarations inside loops, not just
                // blocks. Need to remove them from there probably. Or not, and this code should
                // know how to remove declarations from while loops.
                if (!parentOfDecl->getAsBlock())
                {
                    UNIMPLEMENTED();
                    continue;
                }
                // Delete the declaration, the global one already exists.
                parentOfDecl->getAsBlock()->replaceChildNodeWithMultiple(decl, TIntermSequence());
            }

            // For ReplaceVariables() to replace the temp variable with a reference to the global.
            tempToGlobal[var->uniqueId()] = new TIntermSymbol(replacementVariable);
        }

        // Insert the global declarations.
        const size_t firstFunctionIndex = FindFirstFunctionDefinitionIndex(root);
        root->insertChildNodes(firstFunctionIndex, std::move(globalDeclarations));

        // Replace the variables with references to the new global ones.
        if (!ReplaceVariables(mCompiler, root, tempToGlobal))
        {
            return false;
        }

        return true;
    }

    bool pullUntranslatableConstructsIntoFunctions(TIntermBlock *root)
    {
        TIntermSequence newFunctionPrototypes;
        TIntermSequence newFunctionDefinitions;

        for (UntranslatableConstructAndMetadata &constructAndMetadata : mUntranslatableConstructs)
        {
            UntranslatableConstruct &construct = constructAndMetadata.construct;
            TIntermNode *untranslatableNode    = nullptr;
            TFunction *substituteFunction      = nullptr;
            TIntermSequence args;

            if (TIntermTernary **ternary = std::get_if<UntranslatableTernary>(&construct))
            {
                untranslatableNode = *ternary;
                substituteFunction = replaceTernary(*ternary, constructAndMetadata.parentFunction,
                                                    newFunctionPrototypes, newFunctionDefinitions);
            }
            else if (UntranslatableCommaOperator *commaOperator =
                         std::get_if<UntranslatableCommaOperator>(&construct))
            {
                untranslatableNode = commaOperator->commaOperator;
                substituteFunction = replaceSequenceOperator(
                    commaOperator->commaOperator, constructAndMetadata.parentFunction,
                    newFunctionPrototypes, newFunctionDefinitions);
            }
            else if (TIntermAggregate **funcCall =
                         std::get_if<UntranslatableFunctionCallWithOutparams>(&construct))
            {
                untranslatableNode = *funcCall;
                substituteFunction =
                    replaceCallToFuncWithOutparams(*funcCall, constructAndMetadata.parentFunction,
                                                   newFunctionPrototypes, newFunctionDefinitions);
            }
            else if (UntranslatableMultiElementSwizzle *multielementSwizzleAssignment =
                         std::get_if<UntranslatableMultiElementSwizzle>(&construct))
            {
                untranslatableNode = multielementSwizzleAssignment->multielementSwizzle;
                substituteFunction = replaceDifficultMultielementSwizzle(
                    multielementSwizzleAssignment->multielementSwizzle,
                    constructAndMetadata.parentFunction, newFunctionPrototypes,
                    newFunctionDefinitions);

                args.push_back(multielementSwizzleAssignment->multielementSwizzle->getLeft()
                                   ->getAsSwizzleNode()
                                   ->getOperand());
            }
            else
            {
                UNREACHABLE();
            }

            // The parameters of the parent function must be passed to the new function.
            for (size_t i = 0;
                 i < constructAndMetadata.parentFunction->getFunction()->getParamCount(); i++)
            {
                const TVariable *param =
                    constructAndMetadata.parentFunction->getFunction()->getParam(i);
                args.push_back(new TIntermSymbol(param));
            }

            queueReplacementWithParent(
                constructAndMetadata.parent, untranslatableNode,
                TIntermAggregate::CreateFunctionCall(*substituteFunction, &args),
                OriginalNode::IS_DROPPED);
        }

        // Insert new function prototypes so they are defined for all the following functions.
        const size_t firstFunctionIndex = FindFirstFunctionDefinitionIndex(root);
        root->insertChildNodes(firstFunctionIndex, newFunctionPrototypes);
        // And insert the function definitions at the end so all called functions are
        // legal.
        root->insertChildNodes(root->getChildCount(), newFunctionDefinitions);

        return true;
    }

    TCompiler *mCompiler;

    size_t mUntranslatableConstructDepth = 0;

    const TIntermFunctionDefinition *mCurrentFunction = nullptr;

    // Tracks all the untranslatable constructs found.
    TVector<UntranslatableConstructAndMetadata> mUntranslatableConstructs;

    // Keeps track of all temporary variables used in untranslatable constructs.
    TMap<TSymbolUniqueId, const TVariable *> mSymbolsInsideUntranslatableConstructs;
    // Keeps track of all declaration of temporary variables anywhere outside of untranslatable
    // constructs, as well as the parent nodes of those temp vars.
    TMap<TSymbolUniqueId, std::pair<TIntermDeclaration *, TIntermNode *>> mDeclarationCache;
};

}  // anonymous namespace

bool PullExpressionsIntoFunctions(TCompiler *compiler, TIntermBlock *root)
{
    // Correct the first level of untranslatable constructs. There may be nested untranslatable
    // constructs, and those are handled with subsequent iterations.
    do
    {
        PullExpressionsIntoFunctionsTraverser traverser(compiler, &compiler->getSymbolTable());
        root->traverse(&traverser);
        if (traverser.foundUntranslatableConstruct())
        {
            if (!traverser.update(root))
            {
                return false;
            }
        }
        else
        {
            break;
        }
    } while (true);

    return true;
}
}  // namespace sh
