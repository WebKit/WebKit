//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The SeparateDeclarations function processes declarations, so that in the end each declaration
// contains only one declarator.
// This is useful as an intermediate step when initialization needs to be separated from
// declaration, or when things need to be unfolded out of the initializer.
// Example:
//     int a[1] = int[1](1), b[1] = int[1](2);
// gets transformed when run through this class into the AST equivalent of:
//     int a[1] = int[1](1);
//     int b[1] = int[1](2);

#include "compiler/translator/tree_ops/SeparateDeclarations.h"

#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"

namespace sh
{

namespace
{

class SeparateDeclarationsTraverser : private TIntermTraverser
{
  public:
    ANGLE_NO_DISCARD static bool apply(TCompiler *compiler,
                                       TIntermNode *root,
                                       TSymbolTable *symbolTable);

  private:
    SeparateDeclarationsTraverser(TSymbolTable *symbolTable);
    bool visitDeclaration(Visit, TIntermDeclaration *node) override;
    void visitSymbol(TIntermSymbol *symbol) override;

    void separateDeclarator(TIntermSequence *sequence,
                            size_t index,
                            TIntermSequence *replacementDeclarations,
                            const TStructure **replacementStructure);

    VariableReplacementMap mVariableMap;
};

bool SeparateDeclarationsTraverser::apply(TCompiler *compiler,
                                          TIntermNode *root,
                                          TSymbolTable *symbolTable)
{
    SeparateDeclarationsTraverser separateDecl(symbolTable);
    root->traverse(&separateDecl);
    return separateDecl.updateTree(compiler, root);
}

SeparateDeclarationsTraverser::SeparateDeclarationsTraverser(TSymbolTable *symbolTable)
    : TIntermTraverser(true, false, false, symbolTable)
{}

bool SeparateDeclarationsTraverser::visitDeclaration(Visit, TIntermDeclaration *node)
{
    TIntermSequence *sequence = node->getSequence();
    if (sequence->size() <= 1)
    {
        return true;
    }

    TIntermBlock *parentBlock = getParentNode()->getAsBlock();
    ASSERT(parentBlock != nullptr);

    TIntermSequence replacementDeclarations;
    const TStructure *replacementStructure = nullptr;
    for (size_t ii = 0; ii < sequence->size(); ++ii)
    {
        separateDeclarator(sequence, ii, &replacementDeclarations, &replacementStructure);
    }

    mMultiReplacements.emplace_back(parentBlock, node, std::move(replacementDeclarations));
    return false;
}

void SeparateDeclarationsTraverser::visitSymbol(TIntermSymbol *symbol)
{
    const TVariable *variable = &symbol->variable();
    if (mVariableMap.count(variable) > 0)
    {
        queueAccessChainReplacement(mVariableMap[variable]->deepCopy());
    }
}

void SeparateDeclarationsTraverser::separateDeclarator(TIntermSequence *sequence,
                                                       size_t index,
                                                       TIntermSequence *replacementDeclarations,
                                                       const TStructure **replacementStructure)
{
    TIntermTyped *declarator    = sequence->at(index)->getAsTyped();
    const TType &declaratorType = declarator->getType();

    // If the declaration is not simultaneously declaring a struct, can use the same declarator.
    // Otherwise, the first declarator is taken as-is if the struct has a name.
    const TStructure *structure  = declaratorType.getStruct();
    const bool isStructSpecifier = declaratorType.isStructSpecifier();
    if (!isStructSpecifier || (index == 0 && structure->symbolType() != SymbolType::Empty))
    {
        TIntermDeclaration *replacementDeclaration = new TIntermDeclaration;

        // Make sure to update the declarator's initializers if any.
        declarator->traverse(this);

        replacementDeclaration->appendDeclarator(declarator);
        replacementDeclaration->setLine(declarator->getLine());
        replacementDeclarations->push_back(replacementDeclaration);
        return;
    }

    // If the struct is nameless, split it out first.
    if (structure->symbolType() == SymbolType::Empty)
    {
        if (*replacementStructure == nullptr)
        {
            TStructure *newStructure =
                new TStructure(mSymbolTable, kEmptyImmutableString, &structure->fields(),
                               SymbolType::AngleInternal);
            newStructure->setAtGlobalScope(structure->atGlobalScope());
            *replacementStructure = structure = newStructure;

            TType *namedType = new TType(structure, true);
            namedType->setQualifier(EvqGlobal);

            TVariable *structVariable =
                new TVariable(mSymbolTable, kEmptyImmutableString, namedType, SymbolType::Empty);

            TIntermDeclaration *structDeclaration = new TIntermDeclaration;
            structDeclaration->appendDeclarator(new TIntermSymbol(structVariable));
            structDeclaration->setLine(declarator->getLine());
            replacementDeclarations->push_back(structDeclaration);
        }
        else
        {
            structure = *replacementStructure;
        }
    }

    // Redeclare the declarator but not as a struct specifier.
    TIntermSymbol *asSymbol   = declarator->getAsSymbolNode();
    TIntermTyped *initializer = nullptr;
    if (asSymbol == nullptr)
    {
        TIntermBinary *asBinary = declarator->getAsBinaryNode();
        ASSERT(asBinary->getOp() == EOpInitialize);
        asSymbol    = asBinary->getLeft()->getAsSymbolNode();
        initializer = asBinary->getRight();

        // Make sure the initializer itself has its variables replaced if necessary.
        if (initializer->getAsSymbolNode())
        {
            const TVariable *initializerVariable = &initializer->getAsSymbolNode()->variable();
            if (mVariableMap.count(initializerVariable) > 0)
            {
                initializer = mVariableMap[initializerVariable]->deepCopy();
            }
        }
        else
        {
            initializer->traverse(this);
        }
    }

    ASSERT(asSymbol && asSymbol->variable().symbolType() != SymbolType::Empty);

    TType *newType = new TType(structure, false);
    newType->setQualifier(asSymbol->getType().getQualifier());
    newType->makeArrays(asSymbol->getType().getArraySizes());

    TVariable *replacementVar        = new TVariable(mSymbolTable, asSymbol->getName(), newType,
                                              asSymbol->variable().symbolType());
    TIntermSymbol *replacementSymbol = new TIntermSymbol(replacementVar);
    TIntermTyped *replacement        = replacementSymbol;
    if (initializer)
    {
        replacement = new TIntermBinary(EOpInitialize, replacement, initializer);
    }

    TIntermDeclaration *replacementDeclaration = new TIntermDeclaration;
    replacementDeclaration->appendDeclarator(replacement);
    replacementDeclaration->setLine(declarator->getLine());
    replacementDeclarations->push_back(replacementDeclaration);

    mVariableMap[&asSymbol->variable()] = replacementSymbol;
}
}  // namespace

bool SeparateDeclarations(TCompiler *compiler, TIntermNode *root, TSymbolTable *symbolTable)
{
    return SeparateDeclarationsTraverser::apply(compiler, root, symbolTable);
}

}  // namespace sh
