//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RewriteDoWhile.cpp: rewrites do-while loops using another equivalent
// construct.

#include "compiler/translator/RewriteDoWhile.h"

#include "compiler/translator/IntermNode.h"

namespace
{

// An AST traverser that rewrites loops of the form
//   do {
//     CODE;
//   } while (CONDITION)
//
// to loops of the form
//   bool temp = false;
//   while (true) {
//     if (temp) {
//       if (!CONDITION) {
//         break;
//       }
//     }
//     temp = true;
//     CODE;
//   }
//
// The reason we don't use a simpler form, with for example just (temp && !CONDITION) in the
// while condition, is that short-circuit is often badly supported by driver shader compiler.
// The double if has the same effect, but forces shader compilers to behave.
//
// TODO(cwallez) when UnfoldShortCircuitIntoIf handles loops correctly, revisit this as we might
// be able to use while (temp || CONDITION) with temp initially set to true then run
// UnfoldShortCircuitIntoIf
class DoWhileRewriter : public TIntermTraverser
{
  public:
    DoWhileRewriter() : TIntermTraverser(true, false, false) {}

    bool visitAggregate(Visit, TIntermAggregate *node) override
    {
        // A well-formed AST can only have do-while in EOpSequence which represent lists of
        // statements. By doing a prefix traversal we are able to replace the do-while in the
        // sequence directly as the content of the do-while will be traversed later.
        if (node->getOp() != EOpSequence)
        {
            return true;
        }

        TIntermSequence *statements = node->getSequence();

        // The statements vector will have new statements inserted when we encounter a do-while,
        // which prevents us from using a range-based for loop. Using the usual i++ works, as
        // the (two) new statements inserted replace the statement at the current position.
        for (size_t i = 0; i < statements->size(); i++)
        {
            TIntermNode *statement = (*statements)[i];
            TIntermLoop *loop      = statement->getAsLoopNode();

            if (loop == nullptr || loop->getType() != ELoopDoWhile)
            {
                continue;
            }

            TType boolType = TType(EbtBool);

            // bool temp = false;
            TIntermAggregate *tempDeclaration = nullptr;
            {
                TConstantUnion *falseConstant = new TConstantUnion();
                falseConstant->setBConst(false);
                TIntermTyped *falseValue = new TIntermConstantUnion(falseConstant, boolType);

                tempDeclaration = createTempInitDeclaration(falseValue);
            }

            // temp = true;
            TIntermBinary *assignTrue = nullptr;
            {
                TConstantUnion *trueConstant = new TConstantUnion();
                trueConstant->setBConst(true);
                TIntermTyped *trueValue = new TIntermConstantUnion(trueConstant, boolType);

                assignTrue = createTempAssignment(trueValue);
            }

            // if (temp) {
            //   if (!CONDITION) {
            //     break;
            //   }
            // }
            TIntermSelection *breakIf = nullptr;
            {
                TIntermBranch *breakStatement = new TIntermBranch(EOpBreak, nullptr);

                TIntermAggregate *breakBlock = new TIntermAggregate(EOpSequence);
                breakBlock->getSequence()->push_back(breakStatement);

                TIntermUnary *negatedCondition = new TIntermUnary(EOpLogicalNot);
                negatedCondition->setOperand(loop->getCondition());

                TIntermSelection *innerIf =
                    new TIntermSelection(negatedCondition, breakBlock, nullptr);

                TIntermAggregate *innerIfBlock = new TIntermAggregate(EOpSequence);
                innerIfBlock->getSequence()->push_back(innerIf);

                breakIf = new TIntermSelection(createTempSymbol(boolType), innerIfBlock, nullptr);
            }

            // Assemble the replacement loops, reusing the do-while loop's body and inserting our
            // statements at the front.
            TIntermLoop *newLoop = nullptr;
            {
                TConstantUnion *trueConstant = new TConstantUnion();
                trueConstant->setBConst(true);
                TIntermTyped *trueValue = new TIntermConstantUnion(trueConstant, boolType);

                TIntermAggregate *body = nullptr;
                if (loop->getBody() != nullptr)
                {
                    body = loop->getBody()->getAsAggregate();
                }
                else
                {
                    body = new TIntermAggregate(EOpSequence);
                }
                auto sequence = body->getSequence();
                sequence->insert(sequence->begin(), assignTrue);
                sequence->insert(sequence->begin(), breakIf);

                newLoop = new TIntermLoop(ELoopWhile, nullptr, trueValue, nullptr, body);
            }

            TIntermSequence replacement;
            replacement.push_back(tempDeclaration);
            replacement.push_back(newLoop);

            node->replaceChildNodeWithMultiple(loop, replacement);

            nextTemporaryIndex();
        }
        return true;
    }
};

}  // anonymous namespace

void RewriteDoWhile(TIntermNode *root, unsigned int *temporaryIndex)
{
    ASSERT(temporaryIndex != 0);

    DoWhileRewriter rewriter;
    rewriter.useTemporaryIndex(temporaryIndex);

    root->traverse(&rewriter);
}
