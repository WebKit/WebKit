//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef _LOCAL_INTERMEDIATE_INCLUDED_
#define _LOCAL_INTERMEDIATE_INCLUDED_

#include "GLSLANG/ShaderLang.h"
#include "compiler/intermediate.h"
#include "compiler/SymbolTable.h"

struct TVectorFields {
    int offsets[4];
    int num;
};

//
// Set of helper functions to help parse and build the tree.
//
class TInfoSink;
class TIntermediate {
public:    
    POOL_ALLOCATOR_NEW_DELETE(GlobalPoolAllocator)

    TIntermediate(TInfoSink& i) : infoSink(i) { }
    TIntermSymbol* addSymbol(int Id, const TString&, const TType&, TSourceLoc);
    TIntermTyped* addConversion(TOperator, const TType&, TIntermTyped*);
    TIntermTyped* addBinaryMath(TOperator op, TIntermTyped* left, TIntermTyped* right, TSourceLoc, TSymbolTable&);
    TIntermTyped* addAssign(TOperator op, TIntermTyped* left, TIntermTyped* right, TSourceLoc);
    TIntermTyped* addIndex(TOperator op, TIntermTyped* base, TIntermTyped* index, TSourceLoc);
    TIntermTyped* addUnaryMath(TOperator op, TIntermNode* child, TSourceLoc, TSymbolTable&);
    TIntermAggregate* growAggregate(TIntermNode* left, TIntermNode* right, TSourceLoc);
    TIntermAggregate* makeAggregate(TIntermNode* node, TSourceLoc);
    TIntermAggregate* setAggregateOperator(TIntermNode*, TOperator, TSourceLoc);
    TIntermNode*  addSelection(TIntermTyped* cond, TIntermNodePair code, TSourceLoc);
    TIntermTyped* addSelection(TIntermTyped* cond, TIntermTyped* trueBlock, TIntermTyped* falseBlock, TSourceLoc);
    TIntermTyped* addComma(TIntermTyped* left, TIntermTyped* right, TSourceLoc);
    TIntermConstantUnion* addConstantUnion(ConstantUnion*, const TType&, TSourceLoc);
    TIntermTyped* promoteConstantUnion(TBasicType, TIntermConstantUnion*) ;
    bool parseConstTree(TSourceLoc, TIntermNode*, ConstantUnion*, TOperator, TSymbolTable&, TType, bool singleConstantParam = false);        
    TIntermNode* addLoop(TLoopType, TIntermNode*, TIntermTyped*, TIntermTyped*, TIntermNode*, TSourceLoc);
    TIntermBranch* addBranch(TOperator, TSourceLoc);
    TIntermBranch* addBranch(TOperator, TIntermTyped*, TSourceLoc);
    TIntermTyped* addSwizzle(TVectorFields&, TSourceLoc);
    bool postProcess(TIntermNode*);
	void remove(TIntermNode*);
    void outputTree(TIntermNode*);
    
protected:
    TInfoSink& infoSink;

private:
    void operator=(TIntermediate&); // prevent assignments
};

#endif // _LOCAL_INTERMEDIATE_INCLUDED_
