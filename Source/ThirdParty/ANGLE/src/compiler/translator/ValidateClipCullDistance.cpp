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
                  const unsigned int maxCullDistances,
                  const unsigned int maxCombinedClipAndCullDistances,
                  uint8_t *clipDistanceSizeOut,
                  uint8_t *cullDistanceSizeOut,
                  bool *clipDistanceRedeclaredOut,
                  bool *cullDistanceRedeclaredOut,
                  bool *clipDistanceUsedOut);

  private:
    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override;
    bool visitBinary(Visit visit, TIntermBinary *node) override;

    uint8_t mClipDistanceSize;
    uint8_t mCullDistanceSize;

    int8_t mMaxClipDistanceIndex;
    int8_t mMaxCullDistanceIndex;

    bool mHasNonConstClipDistanceIndex;
    bool mHasNonConstCullDistanceIndex;

    const TIntermSymbol *mClipDistance;
    const TIntermSymbol *mCullDistance;
};

ValidateClipCullDistanceTraverser::ValidateClipCullDistanceTraverser()
    : TIntermTraverser(true, false, false),
      mClipDistanceSize(0),
      mCullDistanceSize(0),
      mMaxClipDistanceIndex(-1),
      mMaxCullDistanceIndex(-1),
      mHasNonConstClipDistanceIndex(false),
      mHasNonConstCullDistanceIndex(false),
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
        mClipDistanceSize = static_cast<uint8_t>(symbol->getOutermostArraySize());
        mClipDistance     = symbol;
    }
    else if (symbol->getName() == "gl_CullDistance")
    {
        mCullDistanceSize = static_cast<uint8_t>(symbol->getOutermostArraySize());
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
        int idx = 0;
        switch (constIdx->getType())
        {
            case EbtInt:
                idx = constIdx->getIConst();
                break;
            case EbtUInt:
                idx = constIdx->getUConst();
                break;
            case EbtFloat:
                idx = static_cast<int>(constIdx->getFConst());
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
                mMaxClipDistanceIndex = static_cast<int8_t>(idx);
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
                mMaxCullDistanceIndex = static_cast<int8_t>(idx);
                if (!mCullDistance)
                {
                    mCullDistance = left;
                }
            }
        }
    }
    else
    {
        if (varName == "gl_ClipDistance")
        {
            mHasNonConstClipDistanceIndex = true;
            if (!mClipDistance)
            {
                mClipDistance = left;
            }
        }
        else
        {
            ASSERT(varName == "gl_CullDistance");
            mHasNonConstCullDistanceIndex = true;
            if (!mCullDistance)
            {
                mCullDistance = left;
            }
        }
    }

    return true;
}

void ValidateClipCullDistanceTraverser::validate(TDiagnostics *diagnostics,
                                                 const unsigned int maxCullDistances,
                                                 const unsigned int maxCombinedClipAndCullDistances,
                                                 uint8_t *clipDistanceSizeOut,
                                                 uint8_t *cullDistanceSizeOut,
                                                 bool *clipDistanceRedeclaredOut,
                                                 bool *cullDistanceRedeclaredOut,
                                                 bool *clipDistanceUsedOut)
{
    ASSERT(diagnostics);

    if (mClipDistanceSize == 0 && mHasNonConstClipDistanceIndex)
    {
        error(*mClipDistance,
              "The array must be sized by the shader either redeclaring it with a size or "
              "indexing it only with constant integral expressions",
              diagnostics);
    }

    if (mCullDistanceSize == 0 && mHasNonConstCullDistanceIndex)
    {
        error(*mCullDistance,
              "The array must be sized by the shader either redeclaring it with a size or "
              "indexing it only with constant integral expressions",
              diagnostics);
    }

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

    if (enabledCullDistances > 0 && maxCullDistances == 0)
    {
        error(*mCullDistance, "Cull distance functionality is not available", diagnostics);
    }

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

    // Update the compiler state
    *clipDistanceSizeOut = mClipDistanceSize ? mClipDistanceSize : (mMaxClipDistanceIndex + 1);
    *cullDistanceSizeOut = mCullDistanceSize ? mCullDistanceSize : (mMaxCullDistanceIndex + 1);
    *clipDistanceRedeclaredOut = mClipDistanceSize != 0;
    *cullDistanceRedeclaredOut = mCullDistanceSize != 0;
    *clipDistanceUsedOut       = (mMaxClipDistanceIndex != -1) || mHasNonConstClipDistanceIndex;
}

}  // anonymous namespace

bool ValidateClipCullDistance(TIntermBlock *root,
                              TDiagnostics *diagnostics,
                              const unsigned int maxCullDistances,
                              const unsigned int maxCombinedClipAndCullDistances,
                              uint8_t *clipDistanceSizeOut,
                              uint8_t *cullDistanceSizeOut,
                              bool *clipDistanceRedeclaredOut,
                              bool *cullDistanceRedeclaredOut,
                              bool *clipDistanceUsedOut)
{
    ValidateClipCullDistanceTraverser varyingValidator;
    root->traverse(&varyingValidator);
    int numErrorsBefore = diagnostics->numErrors();
    varyingValidator.validate(diagnostics, maxCullDistances, maxCombinedClipAndCullDistances,
                              clipDistanceSizeOut, cullDistanceSizeOut, clipDistanceRedeclaredOut,
                              cullDistanceRedeclaredOut, clipDistanceUsedOut);
    return (diagnostics->numErrors() == numErrorsBefore);
}

}  // namespace sh
