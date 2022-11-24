//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The ValidateClipCullDistance function checks if the sum of array sizes for gl_ClipDistance and
// gl_CullDistance exceeds gl_MaxCombinedClipAndCullDistances
//

#include "ValidateClipCullDistance.h"

#include "compiler/translator/Diagnostics.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{

void error(const TIntermSymbol &symbol, const char *reason, TDiagnostics *diagnostics)
{
    diagnostics->error(symbol.getLine(), reason, symbol.getName().data());
}

class ValidateClipCullDistanceTraverser : public TIntermTraverser
{
  public:
    ValidateClipCullDistanceTraverser();
    void validate(TDiagnostics *diagnostics,
                  const unsigned int maxCombinedClipAndCullDistances,
                  const bool limitSimultaneousClipAndCullDistanceUsage);

  private:
    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override;
    bool visitBinary(Visit visit, TIntermBinary *node) override;

    unsigned int mClipDistanceSize;
    unsigned int mCullDistanceSize;

    unsigned int mMaxClipDistanceIndex;
    unsigned int mMaxCullDistanceIndex;

    const TIntermSymbol *mClipDistance;
    const TIntermSymbol *mCullDistance;
};

ValidateClipCullDistanceTraverser::ValidateClipCullDistanceTraverser()
    : TIntermTraverser(true, false, false),
      mClipDistanceSize(0),
      mCullDistanceSize(0),
      mMaxClipDistanceIndex(0),
      mMaxCullDistanceIndex(0),
      mClipDistance(nullptr),
      mCullDistance(nullptr)
{}

bool ValidateClipCullDistanceTraverser::visitDeclaration(Visit visit, TIntermDeclaration *node)
{
    const TIntermSequence &sequence = *(node->getSequence());

    if (sequence.size() != 1)
    {
        return true;
    }

    const TIntermSymbol *symbol = sequence.front()->getAsSymbolNode();
    if (symbol == nullptr)
    {
        return true;
    }

    if (symbol->getName() == "gl_ClipDistance")
    {
        mClipDistanceSize = symbol->getOutermostArraySize();
        mClipDistance     = symbol;
    }
    else if (symbol->getName() == "gl_CullDistance")
    {
        mCullDistanceSize = symbol->getOutermostArraySize();
        mCullDistance     = symbol;
    }

    return true;
}

bool ValidateClipCullDistanceTraverser::visitBinary(Visit visit, TIntermBinary *node)
{
    TOperator op = node->getOp();
    if (op != EOpIndexDirect && op != EOpIndexIndirect)
    {
        return true;
    }

    TIntermSymbol *left = node->getLeft()->getAsSymbolNode();
    if (!left)
    {
        return true;
    }

    ImmutableString varName(left->getName());
    if (varName != "gl_ClipDistance" && varName != "gl_CullDistance")
    {
        return true;
    }

    const TConstantUnion *constIdx = node->getRight()->getConstantValue();
    if (constIdx)
    {
        unsigned int idx = 0;
        switch (constIdx->getType())
        {
            case EbtInt:
                idx = constIdx->getIConst();
                break;
            case EbtUInt:
                idx = constIdx->getUConst();
                break;
            case EbtFloat:
                idx = static_cast<unsigned int>(constIdx->getFConst());
                break;
            case EbtBool:
                idx = constIdx->getBConst() ? 1 : 0;
                break;
            default:
                UNREACHABLE();
                break;
        }

        if (varName == "gl_ClipDistance")
        {
            if (idx > mMaxClipDistanceIndex)
            {
                mMaxClipDistanceIndex = idx;
                if (!mClipDistance)
                {
                    mClipDistance = left;
                }
            }
        }
        else
        {
            ASSERT(varName == "gl_CullDistance");
            if (idx > mMaxCullDistanceIndex)
            {
                mMaxCullDistanceIndex = idx;
                if (!mCullDistance)
                {
                    mCullDistance = left;
                }
            }
        }
    }

    return true;
}

void ValidateClipCullDistanceTraverser::validate(
    TDiagnostics *diagnostics,
    const unsigned int maxCombinedClipAndCullDistances,
    const bool limitSimultaneousClipAndCullDistanceUsage)
{
    ASSERT(diagnostics);

    unsigned int enabledClipDistances =
        (mClipDistanceSize > 0 ? mClipDistanceSize
                               : (mClipDistance ? mMaxClipDistanceIndex + 1 : 0));
    unsigned int enabledCullDistances =
        (mCullDistanceSize > 0 ? mCullDistanceSize
                               : (mCullDistance ? mMaxCullDistanceIndex + 1 : 0));
    unsigned int combinedClipAndCullDistances =
        (enabledClipDistances > 0 && enabledCullDistances > 0
             ? enabledClipDistances + enabledCullDistances
             : 0);

    if (combinedClipAndCullDistances > maxCombinedClipAndCullDistances)
    {
        const TIntermSymbol *greaterSymbol =
            (enabledClipDistances >= enabledCullDistances ? mClipDistance : mCullDistance);

        std::stringstream strstr = sh::InitializeStream<std::stringstream>();
        strstr << "The sum of 'gl_ClipDistance' and 'gl_CullDistance' size is greater than "
                  "gl_MaxCombinedClipAndCullDistances ("
               << combinedClipAndCullDistances << " > " << maxCombinedClipAndCullDistances << ")";
        error(*greaterSymbol, strstr.str().c_str(), diagnostics);
    }

    if (limitSimultaneousClipAndCullDistanceUsage &&
        (enabledClipDistances && enabledCullDistances) &&
        (enabledClipDistances > 4 || enabledCullDistances > 4))
    {
        error(enabledClipDistances > 4 ? *mClipDistance : *mCullDistance,
              "When both 'gl_ClipDistance' and 'gl_CullDistance' are used, each size must "
              "not be greater than 4.",
              diagnostics);
    }
}

}  // anonymous namespace

bool ValidateClipCullDistance(TIntermBlock *root,
                              TDiagnostics *diagnostics,
                              const unsigned int maxCombinedClipAndCullDistances,
                              const bool limitSimultaneousClipAndCullDistanceUsage)
{
    ValidateClipCullDistanceTraverser varyingValidator;
    root->traverse(&varyingValidator);
    int numErrorsBefore = diagnostics->numErrors();
    varyingValidator.validate(diagnostics, maxCombinedClipAndCullDistances,
                              limitSimultaneousClipAndCullDistanceUsage);
    return (diagnostics->numErrors() == numErrorsBefore);
}

}  // namespace sh
