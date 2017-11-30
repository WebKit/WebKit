//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/InitializeVariables.h"

#include "angle_gl.h"
#include "common/debug.h"
#include "compiler/translator/FindMain.h"
#include "compiler/translator/IntermNode_util.h"
#include "compiler/translator/IntermTraverse.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{

void AddArrayZeroInitSequence(const TIntermTyped *initializedNode,
                              bool canUseLoopsToInitialize,
                              TIntermSequence *initSequenceOut,
                              TSymbolTable *symbolTable);

void AddStructZeroInitSequence(const TIntermTyped *initializedNode,
                               bool canUseLoopsToInitialize,
                               TIntermSequence *initSequenceOut,
                               TSymbolTable *symbolTable);

TIntermBinary *CreateZeroInitAssignment(const TIntermTyped *initializedNode)
{
    TIntermTyped *zero = CreateZeroNode(initializedNode->getType());
    return new TIntermBinary(EOpAssign, initializedNode->deepCopy(), zero);
}

void AddZeroInitSequence(const TIntermTyped *initializedNode,
                         bool canUseLoopsToInitialize,
                         TIntermSequence *initSequenceOut,
                         TSymbolTable *symbolTable)
{
    if (initializedNode->isArray())
    {
        AddArrayZeroInitSequence(initializedNode, canUseLoopsToInitialize, initSequenceOut,
                                 symbolTable);
    }
    else if (initializedNode->getType().isStructureContainingArrays() ||
             initializedNode->getType().isNamelessStruct())
    {
        AddStructZeroInitSequence(initializedNode, canUseLoopsToInitialize, initSequenceOut,
                                  symbolTable);
    }
    else
    {
        initSequenceOut->push_back(CreateZeroInitAssignment(initializedNode));
    }
}

void AddStructZeroInitSequence(const TIntermTyped *initializedNode,
                               bool canUseLoopsToInitialize,
                               TIntermSequence *initSequenceOut,
                               TSymbolTable *symbolTable)
{
    ASSERT(initializedNode->getBasicType() == EbtStruct);
    const TStructure *structType = initializedNode->getType().getStruct();
    for (int i = 0; i < static_cast<int>(structType->fields().size()); ++i)
    {
        TIntermBinary *element = new TIntermBinary(EOpIndexDirectStruct,
                                                   initializedNode->deepCopy(), CreateIndexNode(i));
        // Structs can't be defined inside structs, so the type of a struct field can't be a
        // nameless struct.
        ASSERT(!element->getType().isNamelessStruct());
        AddZeroInitSequence(element, canUseLoopsToInitialize, initSequenceOut, symbolTable);
    }
}

void AddArrayZeroInitStatementList(const TIntermTyped *initializedNode,
                                   bool canUseLoopsToInitialize,
                                   TIntermSequence *initSequenceOut,
                                   TSymbolTable *symbolTable)
{
    for (unsigned int i = 0; i < initializedNode->getOutermostArraySize(); ++i)
    {
        TIntermBinary *element =
            new TIntermBinary(EOpIndexDirect, initializedNode->deepCopy(), CreateIndexNode(i));
        AddZeroInitSequence(element, canUseLoopsToInitialize, initSequenceOut, symbolTable);
    }
}

void AddArrayZeroInitForLoop(const TIntermTyped *initializedNode,
                             TIntermSequence *initSequenceOut,
                             TSymbolTable *symbolTable)
{
    ASSERT(initializedNode->isArray());
    TSymbolUniqueId indexSymbol(symbolTable);

    TIntermSymbol *indexSymbolNode = CreateTempSymbolNode(indexSymbol, TType(EbtInt), EvqTemporary);
    TIntermDeclaration *indexInit =
        CreateTempInitDeclarationNode(indexSymbol, CreateZeroNode(TType(EbtInt)), EvqTemporary);
    TIntermConstantUnion *arraySizeNode = CreateIndexNode(initializedNode->getOutermostArraySize());
    TIntermBinary *indexSmallerThanSize =
        new TIntermBinary(EOpLessThan, indexSymbolNode->deepCopy(), arraySizeNode);
    TIntermUnary *indexIncrement = new TIntermUnary(EOpPreIncrement, indexSymbolNode->deepCopy());

    TIntermBlock *forLoopBody       = new TIntermBlock();
    TIntermSequence *forLoopBodySeq = forLoopBody->getSequence();

    TIntermBinary *element = new TIntermBinary(EOpIndexIndirect, initializedNode->deepCopy(),
                                               indexSymbolNode->deepCopy());
    AddZeroInitSequence(element, true, forLoopBodySeq, symbolTable);

    TIntermLoop *forLoop =
        new TIntermLoop(ELoopFor, indexInit, indexSmallerThanSize, indexIncrement, forLoopBody);
    initSequenceOut->push_back(forLoop);
}

void AddArrayZeroInitSequence(const TIntermTyped *initializedNode,
                              bool canUseLoopsToInitialize,
                              TIntermSequence *initSequenceOut,
                              TSymbolTable *symbolTable)
{
    // The array elements are assigned one by one to keep the AST compatible with ESSL 1.00 which
    // doesn't have array assignment. We'll do this either with a for loop or just a list of
    // statements assigning to each array index. Note that it is important to have the array init in
    // the right order to workaround http://crbug.com/709317
    bool isSmallArray = initializedNode->getOutermostArraySize() <= 1u ||
                        (initializedNode->getBasicType() != EbtStruct &&
                         !initializedNode->getType().isArrayOfArrays() &&
                         initializedNode->getOutermostArraySize() <= 3u);
    if (initializedNode->getQualifier() == EvqFragData ||
        initializedNode->getQualifier() == EvqFragmentOut || isSmallArray ||
        !canUseLoopsToInitialize)
    {
        // Fragment outputs should not be indexed by non-constant indices.
        // Also it doesn't make sense to use loops to initialize very small arrays.
        AddArrayZeroInitStatementList(initializedNode, canUseLoopsToInitialize, initSequenceOut,
                                      symbolTable);
    }
    else
    {
        AddArrayZeroInitForLoop(initializedNode, initSequenceOut, symbolTable);
    }
}

void InsertInitCode(TIntermSequence *mainBody,
                    const InitVariableList &variables,
                    TSymbolTable *symbolTable,
                    int shaderVersion,
                    const TExtensionBehavior &extensionBehavior,
                    bool canUseLoopsToInitialize)
{
    for (const auto &var : variables)
    {
        TString name = TString(var.name.c_str());
        size_t pos   = name.find_last_of('[');
        if (pos != TString::npos)
        {
            name = name.substr(0, pos);
        }

        TIntermTyped *initializedSymbol = nullptr;
        if (var.isBuiltIn())
        {
            initializedSymbol = ReferenceBuiltInVariable(name, *symbolTable, shaderVersion);
            if (initializedSymbol->getQualifier() == EvqFragData &&
                !IsExtensionEnabled(extensionBehavior, TExtension::EXT_draw_buffers))
            {
                // If GL_EXT_draw_buffers is disabled, only the 0th index of gl_FragData can be
                // written to.
                // TODO(oetuaho): This is a bit hacky and would be better to remove, if we came up
                // with a good way to do it. Right now "gl_FragData" in symbol table is initialized
                // to have the array size of MaxDrawBuffers, and the initialization happens before
                // the shader sets the extensions it is using.
                initializedSymbol =
                    new TIntermBinary(EOpIndexDirect, initializedSymbol, CreateIndexNode(0));
            }
        }
        else
        {
            initializedSymbol = ReferenceGlobalVariable(name, *symbolTable);
        }
        ASSERT(initializedSymbol != nullptr);

        TIntermSequence *initCode =
            CreateInitCode(initializedSymbol, canUseLoopsToInitialize, symbolTable);
        mainBody->insert(mainBody->begin(), initCode->begin(), initCode->end());
    }
}

class InitializeLocalsTraverser : public TIntermTraverser
{
  public:
    InitializeLocalsTraverser(int shaderVersion,
                              TSymbolTable *symbolTable,
                              bool canUseLoopsToInitialize)
        : TIntermTraverser(true, false, false, symbolTable),
          mShaderVersion(shaderVersion),
          mCanUseLoopsToInitialize(canUseLoopsToInitialize)
    {
    }

  protected:
    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override
    {
        for (TIntermNode *declarator : *node->getSequence())
        {
            if (!mInGlobalScope && !declarator->getAsBinaryNode())
            {
                TIntermSymbol *symbol = declarator->getAsSymbolNode();
                ASSERT(symbol);
                if (symbol->getSymbol() == "")
                {
                    continue;
                }

                // Arrays may need to be initialized one element at a time, since ESSL 1.00 does not
                // support array constructors or assigning arrays.
                bool arrayConstructorUnavailable =
                    (symbol->isArray() || symbol->getType().isStructureContainingArrays()) &&
                    mShaderVersion == 100;
                // Nameless struct constructors can't be referred to, so they also need to be
                // initialized one element at a time.
                // TODO(oetuaho): Check if it makes sense to initialize using a loop, even if we
                // could use an initializer. It could at least reduce code size for very large
                // arrays, but could hurt runtime performance.
                if (arrayConstructorUnavailable || symbol->getType().isNamelessStruct())
                {
                    // SimplifyLoopConditions should have been run so the parent node of this node
                    // should not be a loop.
                    ASSERT(getParentNode()->getAsLoopNode() == nullptr);
                    // SeparateDeclarations should have already been run, so we don't need to worry
                    // about further declarators in this declaration depending on the effects of
                    // this declarator.
                    ASSERT(node->getSequence()->size() == 1);
                    insertStatementsInParentBlock(
                        TIntermSequence(),
                        *CreateInitCode(symbol, mCanUseLoopsToInitialize, mSymbolTable));
                }
                else
                {
                    TIntermBinary *init =
                        new TIntermBinary(EOpInitialize, symbol, CreateZeroNode(symbol->getType()));
                    queueReplacementWithParent(node, symbol, init, OriginalNode::BECOMES_CHILD);
                }
            }
        }
        return false;
    }

  private:
    int mShaderVersion;
    bool mCanUseLoopsToInitialize;
};

}  // namespace anonymous

TIntermSequence *CreateInitCode(const TIntermTyped *initializedSymbol,
                                bool canUseLoopsToInitialize,
                                TSymbolTable *symbolTable)
{
    TIntermSequence *initCode = new TIntermSequence();
    AddZeroInitSequence(initializedSymbol, canUseLoopsToInitialize, initCode, symbolTable);
    return initCode;
}

void InitializeUninitializedLocals(TIntermBlock *root,
                                   int shaderVersion,
                                   bool canUseLoopsToInitialize,
                                   TSymbolTable *symbolTable)
{
    InitializeLocalsTraverser traverser(shaderVersion, symbolTable, canUseLoopsToInitialize);
    root->traverse(&traverser);
    traverser.updateTree();
}

void InitializeVariables(TIntermBlock *root,
                         const InitVariableList &vars,
                         TSymbolTable *symbolTable,
                         int shaderVersion,
                         const TExtensionBehavior &extensionBehavior,
                         bool canUseLoopsToInitialize)
{
    TIntermBlock *body = FindMainBody(root);
    InsertInitCode(body->getSequence(), vars, symbolTable, shaderVersion, extensionBehavior,
                   canUseLoopsToInitialize);
}

}  // namespace sh
