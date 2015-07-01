//
// Copyright (c) 2002-2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The SeparateArrayDeclarations function processes declarations that contain array declarators. Each declarator in
// such declarations gets its own declaration.
// This is useful as an intermediate step when initialization needs to be separated from declaration.
// Example:
//     int a[1] = int[1](1), b[1] = int[1](2);
// gets transformed when run through this class into the AST equivalent of:
//     int a[1] = int[1](1);
//     int b[1] = int[1](2);

#include "compiler/translator/SeparateDeclarations.h"

#include "compiler/translator/IntermNode.h"

namespace
{

class SeparateDeclarations : private TIntermTraverser
{
  public:
    static void apply(TIntermNode *root);
  private:
    SeparateDeclarations();
    bool visitAggregate(Visit, TIntermAggregate *node) override;
};

void SeparateDeclarations::apply(TIntermNode *root)
{
    SeparateDeclarations separateDecl;
    root->traverse(&separateDecl);
    separateDecl.updateTree();
}

SeparateDeclarations::SeparateDeclarations()
    : TIntermTraverser(true, false, false)
{
}

bool SeparateDeclarations::visitAggregate(Visit, TIntermAggregate *node)
{
    if (node->getOp() == EOpDeclaration)
    {
        TIntermSequence *sequence = node->getSequence();
        bool sequenceContainsArrays = false;
        for (size_t ii = 0; ii < sequence->size(); ++ii)
        {
            TIntermTyped *typed = sequence->at(ii)->getAsTyped();
            if (typed != nullptr && typed->isArray())
            {
                sequenceContainsArrays = true;
                break;
            }
        }
        if (sequence->size() > 1 && sequenceContainsArrays)
        {
            TIntermAggregate *parentAgg = getParentNode()->getAsAggregate();
            ASSERT(parentAgg != nullptr);

            TIntermSequence replacementDeclarations;
            for (size_t ii = 0; ii < sequence->size(); ++ii)
            {
                TIntermAggregate *replacementDeclaration = new TIntermAggregate;

                replacementDeclaration->setOp(EOpDeclaration);
                replacementDeclaration->getSequence()->push_back(sequence->at(ii));
                replacementDeclaration->setLine(sequence->at(ii)->getLine());
                replacementDeclarations.push_back(replacementDeclaration);
            }

            mMultiReplacements.push_back(NodeReplaceWithMultipleEntry(parentAgg, node, replacementDeclarations));
        }
        return false;
    }
    return true;
}

} // namespace

void SeparateArrayDeclarations(TIntermNode *root)
{
    SeparateDeclarations::apply(root);
}
