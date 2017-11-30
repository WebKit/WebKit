//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FindSymbol.cpp:
//     Utility for finding a symbol node inside an AST tree.

#include "compiler/translator/FindSymbolNode.h"

#include "compiler/translator/IntermTraverse.h"

namespace sh
{

namespace
{

class SymbolFinder : public TIntermTraverser
{
  public:
    SymbolFinder(const TString &symbolName, TBasicType basicType)
        : TIntermTraverser(true, false, false),
          mSymbolName(symbolName),
          mNodeFound(nullptr),
          mBasicType(basicType)
    {
    }

    void visitSymbol(TIntermSymbol *node)
    {
        if (node->getBasicType() == mBasicType && node->getSymbol() == mSymbolName)
        {
            mNodeFound = node;
        }
    }

    bool isFound() const { return mNodeFound != nullptr; }
    const TIntermSymbol *getNode() const { return mNodeFound; }

  private:
    TString mSymbolName;
    TIntermSymbol *mNodeFound;
    TBasicType mBasicType;
};

}  // anonymous namespace

const TIntermSymbol *FindSymbolNode(TIntermNode *root,
                                    const TString &symbolName,
                                    TBasicType basicType)
{
    SymbolFinder finder(symbolName, basicType);
    root->traverse(&finder);
    return finder.getNode();
}

}  // namespace sh
