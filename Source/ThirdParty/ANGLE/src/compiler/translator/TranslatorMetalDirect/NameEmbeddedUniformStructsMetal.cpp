//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// NameEmbeddedUniformStructsMetal: Gives nameless uniform struct internal names.
//

#include "compiler/translator/TranslatorMetalDirect/NameEmbeddedUniformStructsMetal.h"

#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{
namespace
{
// This traverser translates embedded uniform structs into a specifier and declaration.
// This makes the declarations easier to move into uniform blocks.
class Traverser : public TIntermTraverser
{

  public:
    std::unordered_map<int, TIntermSymbol *> replacements;
    explicit Traverser(TSymbolTable *symbolTable)
        : TIntermTraverser(true, false, false, symbolTable)
    {}

    bool visitDeclaration(Visit visit, TIntermDeclaration *decl) override
    {
        ASSERT(visit == PreVisit);

        if (!mInGlobalScope)
        {
            return false;
        }

        const TIntermSequence &sequence = *(decl->getSequence());
        ASSERT(sequence.size() == 1);
        TIntermTyped *declarator = sequence.front()->getAsTyped();
        const TType &type        = declarator->getType();

        if (type.isStructSpecifier())
        {
            const TStructure *structure = type.getStruct();

            if (structure->symbolType() == SymbolType::Empty)
            {
                doReplacement(decl, declarator, structure);
            }
        }

        return false;
    }
    void visitSymbol(TIntermSymbol *decl) override
    {
        auto symbol = replacements.find(decl->uniqueId().get());
        if (symbol != replacements.end())
        {
            queueReplacement(symbol->second->deepCopy(), OriginalNode::IS_DROPPED);
        }
    }

  private:
    void doReplacement(TIntermDeclaration *decl,
                       TIntermTyped *declarator,
                       const TStructure *oldStructure)
    {
        // struct <structName> { ... };
        TStructure *structure = new TStructure(mSymbolTable, kEmptyImmutableString,
                                               &oldStructure->fields(), SymbolType::AngleInternal);
        TType *namedType      = new TType(structure, true);
        namedType->setQualifier(EvqGlobal);

        TVariable *structVariable =
            new TVariable(mSymbolTable, kEmptyImmutableString, namedType, SymbolType::Empty);
        TIntermSymbol *structDeclarator       = new TIntermSymbol(structVariable);
        TIntermDeclaration *structDeclaration = new TIntermDeclaration;
        structDeclaration->appendDeclarator(structDeclarator);

        TIntermSequence *newSequence = new TIntermSequence;
        newSequence->push_back(structDeclaration);

        // uniform <structName> <structUniformName>;
        TIntermSymbol *asSymbol = declarator->getAsSymbolNode();
        if (asSymbol && asSymbol->variable().symbolType() != SymbolType::Empty)
        {
            TIntermDeclaration *namedDecl = new TIntermDeclaration;
            TType *uniformType            = new TType(structure, false);
            uniformType->setQualifier(EvqUniform);

            TVariable *newVar        = new TVariable(mSymbolTable, asSymbol->getName(), uniformType,
                                              asSymbol->variable().symbolType());
            TIntermSymbol *newSymbol = new TIntermSymbol(newVar);
            replacements[asSymbol->uniqueId().get()] = newSymbol;
            namedDecl->appendDeclarator(newSymbol);

            newSequence->push_back(namedDecl);
        }

        mMultiReplacements.emplace_back(getParentNode()->getAsBlock(), decl,
                                        std::move(*newSequence));
    }
};
}  // anonymous namespace

bool NameEmbeddedStructUniformsMetal(TCompiler *compiler,
                                     TIntermBlock *root,
                                     TSymbolTable *symbolTable)
{
    Traverser nameStructs(symbolTable);
    root->traverse(&nameStructs);
    return nameStructs.updateTree(compiler, root);
}
}  // namespace sh
