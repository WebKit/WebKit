//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// NameNamelessUniformBuffers: Gives nameless uniform buffer variables internal names.
//

#include "compiler/translator/tree_ops/NameNamelessUniformBuffers.h"

#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{
namespace
{
// Traverse uniform buffer declarations and give name to nameless declarations.  Keeps track of
// the interface fields which will be used in the source without the interface block variable name
// and replaces them with name.field.
class NameUniformBufferVariablesTraverser : public TIntermTraverser
{
  public:
    explicit NameUniformBufferVariablesTraverser(TSymbolTable *symbolTable)
        : TIntermTraverser(true, false, false, symbolTable)
    {}

    bool visitDeclaration(Visit visit, TIntermDeclaration *decl) override
    {
        ASSERT(visit == PreVisit);

        const TIntermSequence &sequence = *(decl->getSequence());

        TIntermTyped *variableNode = sequence.front()->getAsTyped();
        const TType &type          = variableNode->getType();

        // If it's an interface block, it may have to be converted if it contains any row-major
        // fields.
        if (!type.isInterfaceBlock())
        {
            return true;
        }

        // Multi declaration statements are already separated, so there can only be one variable
        // here.
        ASSERT(sequence.size() == 1);
        const TVariable *variable = &variableNode->getAsSymbolNode()->variable();
        if (variable->symbolType() != SymbolType::Empty)
        {
            return false;
        }

        TIntermDeclaration *newDeclaration = new TIntermDeclaration;
        TVariable *newVariable = new TVariable(mSymbolTable, kEmptyImmutableString, &type,
                                               SymbolType::AngleInternal, variable->extensions());
        newDeclaration->appendDeclarator(new TIntermSymbol(newVariable));

        queueReplacement(newDeclaration, OriginalNode::IS_DROPPED);

        // It's safe to key the map with the interface block, as there couldn't have been multiple
        // declarations with this interface block (as the variable is nameless), so for nameless
        // uniform buffers, the interface block is unique.
        mNamelessUniformBuffersMap[type.getInterfaceBlock()] = newVariable;

        return false;
    }

    void visitSymbol(TIntermSymbol *symbol) override
    {
        const TType &type = symbol->getType();

        // The symbols we are looking for have the interface block pointer set, but are not
        // interface blocks.  These are references to fields of nameless uniform buffers.
        if (type.isInterfaceBlock() || type.getInterfaceBlock() == nullptr)
        {
            return;
        }

        const TInterfaceBlock *block = type.getInterfaceBlock();

        // If block variable is not nameless, there's nothing to do.
        if (mNamelessUniformBuffersMap.count(block) == 0)
        {
            return;
        }

        const ImmutableString symbolName = symbol->getName();

        // Find which field it is
        const TVector<TField *> fields = block->fields();
        for (size_t fieldIndex = 0; fieldIndex < fields.size(); ++fieldIndex)
        {
            const TField *field = fields[fieldIndex];
            if (field->name() != symbolName)
            {
                continue;
            }

            // Replace this node with a binary node that indexes the named uniform buffer.
            TIntermSymbol *namedUniformBuffer =
                new TIntermSymbol(mNamelessUniformBuffersMap[block]);
            TIntermBinary *replacement =
                new TIntermBinary(EOpIndexDirectInterfaceBlock, namedUniformBuffer,
                                  CreateIndexNode(static_cast<uint32_t>(fieldIndex)));

            queueReplacement(replacement, OriginalNode::IS_DROPPED);

            return;
        }

        UNREACHABLE();
    }

  private:
    // A map from nameless uniform buffers to their named replacements.
    std::unordered_map<const TInterfaceBlock *, const TVariable *> mNamelessUniformBuffersMap;
};
}  // anonymous namespace

bool NameNamelessUniformBuffers(TCompiler *compiler, TIntermBlock *root, TSymbolTable *symbolTable)
{
    NameUniformBufferVariablesTraverser nameUniformBufferVariables(symbolTable);
    root->traverse(&nameUniformBufferVariables);
    return nameUniformBufferVariables.updateTree(compiler, root);
}
}  // namespace sh
