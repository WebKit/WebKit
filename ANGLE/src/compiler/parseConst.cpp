//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/ParseHelper.h"

//
// Use this class to carry along data from node to node in 
// the traversal
//
class TConstTraverser : public TIntermTraverser {
public:
    TConstTraverser(ConstantUnion* cUnion, bool singleConstParam, TOperator constructType, TInfoSink& sink, TSymbolTable& symTable, TType& t)
        : error(false),
          index(0),
          unionArray(cUnion),
          type(t),
          constructorType(constructType),
          singleConstantParam(singleConstParam),
          infoSink(sink),
          symbolTable(symTable),
          size(0),
          isMatrix(false),
          matrixSize(0) {
    }

    bool error;

protected:
    void visitSymbol(TIntermSymbol*);
    void visitConstantUnion(TIntermConstantUnion*);
    bool visitBinary(Visit visit, TIntermBinary*);
    bool visitUnary(Visit visit, TIntermUnary*);
    bool visitSelection(Visit visit, TIntermSelection*);
    bool visitAggregate(Visit visit, TIntermAggregate*);
    bool visitLoop(Visit visit, TIntermLoop*);
    bool visitBranch(Visit visit, TIntermBranch*);

    int index;
    ConstantUnion *unionArray;
    TType type;
    TOperator constructorType;
    bool singleConstantParam;
    TInfoSink& infoSink;
    TSymbolTable& symbolTable;
    int size; // size of the constructor ( 4 for vec4)
    bool isMatrix;
    int matrixSize; // dimension of the matrix (nominal size and not the instance size)
};

//
// The rest of the file are the traversal functions.  The last one
// is the one that starts the traversal.
//
// Return true from interior nodes to have the external traversal
// continue on to children.  If you process children yourself,
// return false.
//

void TConstTraverser::visitSymbol(TIntermSymbol* node)
{
    infoSink.info.message(EPrefixInternalError, "Symbol Node found in constant constructor", node->getLine());
    return;

}

bool TConstTraverser::visitBinary(Visit visit, TIntermBinary* node)
{
    TQualifier qualifier = node->getType().getQualifier();
    
    if (qualifier != EvqConst) {
        TString buf;
        buf.append("'constructor' : assigning non-constant to ");
        buf.append(type.getCompleteString());
        infoSink.info.message(EPrefixError, buf.c_str(), node->getLine());
        error = true;
        return false;  
    }

   infoSink.info.message(EPrefixInternalError, "Binary Node found in constant constructor", node->getLine());
    
    return false;
}

bool TConstTraverser::visitUnary(Visit visit, TIntermUnary* node)
{
    TString buf;
    buf.append("'constructor' : assigning non-constant to ");
    buf.append(type.getCompleteString());
    infoSink.info.message(EPrefixError, buf.c_str(), node->getLine());
    error = true;
    return false;  
}

bool TConstTraverser::visitAggregate(Visit visit, TIntermAggregate* node)
{
    if (!node->isConstructor() && node->getOp() != EOpComma) {
        TString buf;
        buf.append("'constructor' : assigning non-constant to ");
        buf.append(type.getCompleteString());
        infoSink.info.message(EPrefixError, buf.c_str(), node->getLine());
        error = true;
        return false;  
    }

    if (node->getSequence().size() == 0) {
        error = true;
        return false;
    }

    bool flag = node->getSequence().size() == 1 && node->getSequence()[0]->getAsTyped()->getAsConstantUnion();
    if (flag) 
    {
        singleConstantParam = true; 
        constructorType = node->getOp();
        size = node->getType().getObjectSize();

        if (node->getType().isMatrix()) {
            isMatrix = true;
            matrixSize = node->getType().getNominalSize();
        }
    }       

    for (TIntermSequence::iterator p = node->getSequence().begin(); 
                                   p != node->getSequence().end(); p++) {

        if (node->getOp() == EOpComma)
            index = 0;           

        (*p)->traverse(this);
    }   
    if (flag) 
    {
        singleConstantParam = false;   
        constructorType = EOpNull;
        size = 0;
        isMatrix = false;
        matrixSize = 0;
    }
    return false;
}

bool TConstTraverser::visitSelection(Visit visit, TIntermSelection* node)
{
    infoSink.info.message(EPrefixInternalError, "Selection Node found in constant constructor", node->getLine());
    error = true;
    return false;
}

void TConstTraverser::visitConstantUnion(TIntermConstantUnion* node)
{
    ConstantUnion* leftUnionArray = unionArray;
    int instanceSize = type.getObjectSize();

    if (index >= instanceSize)
        return;

    if (!singleConstantParam) {
        int size = node->getType().getObjectSize();
    
        ConstantUnion *rightUnionArray = node->getUnionArrayPointer();
        for (int i=0; i < size; i++) {
            if (index >= instanceSize)
                return;
            leftUnionArray[index] = rightUnionArray[i];

            (index)++;
        }
    } else {
        int totalSize = index + size;
        ConstantUnion *rightUnionArray = node->getUnionArrayPointer();
        if (!isMatrix) {
            int count = 0;
            for (int i = index; i < totalSize; i++) {
                if (i >= instanceSize)
                    return;

                leftUnionArray[i] = rightUnionArray[count];

                (index)++;
                
                if (node->getType().getObjectSize() > 1)
                    count++;
            }
        } else {  // for matrix constructors
            int count = 0;
            int element = index;
            for (int i = index; i < totalSize; i++) {
                if (i >= instanceSize)
                    return;
                if (element - i == 0 || (i - element) % (matrixSize + 1) == 0 )
                    leftUnionArray[i] = rightUnionArray[count];
                else 
                    leftUnionArray[i].setFConst(0.0f);

                (index)++;

                if (node->getType().getObjectSize() > 1)
                    count++;                
            }
        }
    }
}

bool TConstTraverser::visitLoop(Visit visit, TIntermLoop* node)
{
    infoSink.info.message(EPrefixInternalError, "Loop Node found in constant constructor", node->getLine());
    error = true;
    return false;
}

bool TConstTraverser::visitBranch(Visit visit, TIntermBranch* node)
{
    infoSink.info.message(EPrefixInternalError, "Branch Node found in constant constructor", node->getLine());
    error = true;
    return false;
}

//
// This function is the one to call externally to start the traversal.
// Individual functions can be initialized to 0 to skip processing of that
// type of node.  It's children will still be processed.
//
bool TIntermediate::parseConstTree(TSourceLoc line, TIntermNode* root, ConstantUnion* unionArray, TOperator constructorType, TSymbolTable& symbolTable, TType t, bool singleConstantParam)
{
    if (root == 0)
        return false;

    TConstTraverser it(unionArray, singleConstantParam, constructorType, infoSink, symbolTable, t);

    root->traverse(&it);
    if (it.error)
        return true;
    else
        return false;
}
