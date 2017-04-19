//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/IntermNode.h"
#include "compiler/translator/InfoSink.h"
#include "compiler/translator/SymbolTable.h"

namespace sh
{

void TIntermSymbol::traverse(TIntermTraverser *it)
{
    it->traverseSymbol(this);
}

void TIntermRaw::traverse(TIntermTraverser *it)
{
    it->traverseRaw(this);
}

void TIntermConstantUnion::traverse(TIntermTraverser *it)
{
    it->traverseConstantUnion(this);
}

void TIntermSwizzle::traverse(TIntermTraverser *it)
{
    it->traverseSwizzle(this);
}

void TIntermBinary::traverse(TIntermTraverser *it)
{
    it->traverseBinary(this);
}

void TIntermUnary::traverse(TIntermTraverser *it)
{
    it->traverseUnary(this);
}

void TIntermTernary::traverse(TIntermTraverser *it)
{
    it->traverseTernary(this);
}

void TIntermIfElse::traverse(TIntermTraverser *it)
{
    it->traverseIfElse(this);
}

void TIntermSwitch::traverse(TIntermTraverser *it)
{
    it->traverseSwitch(this);
}

void TIntermCase::traverse(TIntermTraverser *it)
{
    it->traverseCase(this);
}

void TIntermFunctionDefinition::traverse(TIntermTraverser *it)
{
    it->traverseFunctionDefinition(this);
}

void TIntermBlock::traverse(TIntermTraverser *it)
{
    it->traverseBlock(this);
}

void TIntermInvariantDeclaration::traverse(TIntermTraverser *it)
{
    it->traverseInvariantDeclaration(this);
}

void TIntermDeclaration::traverse(TIntermTraverser *it)
{
    it->traverseDeclaration(this);
}

void TIntermFunctionPrototype::traverse(TIntermTraverser *it)
{
    it->traverseFunctionPrototype(this);
}

void TIntermAggregate::traverse(TIntermTraverser *it)
{
    it->traverseAggregate(this);
}

void TIntermLoop::traverse(TIntermTraverser *it)
{
    it->traverseLoop(this);
}

void TIntermBranch::traverse(TIntermTraverser *it)
{
    it->traverseBranch(this);
}

TIntermTraverser::TIntermTraverser(bool preVisit, bool inVisit, bool postVisit)
    : preVisit(preVisit),
      inVisit(inVisit),
      postVisit(postVisit),
      mDepth(-1),
      mMaxDepth(0),
      mInGlobalScope(true),
      mTemporaryIndex(nullptr)
{
}

TIntermTraverser::~TIntermTraverser()
{
}

void TIntermTraverser::pushParentBlock(TIntermBlock *node)
{
    mParentBlockStack.push_back(ParentBlock(node, 0));
}

void TIntermTraverser::incrementParentBlockPos()
{
    ++mParentBlockStack.back().pos;
}

void TIntermTraverser::popParentBlock()
{
    ASSERT(!mParentBlockStack.empty());
    mParentBlockStack.pop_back();
}

void TIntermTraverser::insertStatementsInParentBlock(const TIntermSequence &insertions)
{
    TIntermSequence emptyInsertionsAfter;
    insertStatementsInParentBlock(insertions, emptyInsertionsAfter);
}

void TIntermTraverser::insertStatementsInParentBlock(const TIntermSequence &insertionsBefore,
                                                     const TIntermSequence &insertionsAfter)
{
    ASSERT(!mParentBlockStack.empty());
    ParentBlock &parentBlock = mParentBlockStack.back();
    if (mPath.back() == parentBlock.node)
    {
        ASSERT(mParentBlockStack.size() >= 2u);
        // The current node is a block node, so the parent block is not the topmost one in the block
        // stack, but the one below that.
        parentBlock = mParentBlockStack.at(mParentBlockStack.size() - 2u);
    }
    NodeInsertMultipleEntry insert(parentBlock.node, parentBlock.pos, insertionsBefore,
                                   insertionsAfter);
    mInsertions.push_back(insert);
}

void TIntermTraverser::insertStatementInParentBlock(TIntermNode *statement)
{
    TIntermSequence insertions;
    insertions.push_back(statement);
    insertStatementsInParentBlock(insertions);
}

TIntermSymbol *TIntermTraverser::createTempSymbol(const TType &type, TQualifier qualifier)
{
    // Each traversal uses at most one temporary variable, so the index stays the same within a
    // single traversal.
    TInfoSinkBase symbolNameOut;
    ASSERT(mTemporaryIndex != nullptr);
    symbolNameOut << "s" << (*mTemporaryIndex);
    TString symbolName = symbolNameOut.c_str();

    TIntermSymbol *node = new TIntermSymbol(0, symbolName, type);
    node->setInternal(true);

    ASSERT(qualifier == EvqTemporary || qualifier == EvqConst || qualifier == EvqGlobal);
    node->getTypePointer()->setQualifier(qualifier);
    // TODO(oetuaho): Might be useful to sanitize layout qualifier etc. on the type of the created
    // symbol. This might need to be done in other places as well.
    return node;
}

TIntermSymbol *TIntermTraverser::createTempSymbol(const TType &type)
{
    return createTempSymbol(type, EvqTemporary);
}

TIntermDeclaration *TIntermTraverser::createTempDeclaration(const TType &type)
{
    TIntermDeclaration *tempDeclaration = new TIntermDeclaration();
    tempDeclaration->appendDeclarator(createTempSymbol(type));
    return tempDeclaration;
}

TIntermDeclaration *TIntermTraverser::createTempInitDeclaration(TIntermTyped *initializer,
                                                                TQualifier qualifier)
{
    ASSERT(initializer != nullptr);
    TIntermSymbol *tempSymbol           = createTempSymbol(initializer->getType(), qualifier);
    TIntermDeclaration *tempDeclaration = new TIntermDeclaration();
    TIntermBinary *tempInit             = new TIntermBinary(EOpInitialize, tempSymbol, initializer);
    tempDeclaration->appendDeclarator(tempInit);
    return tempDeclaration;
}

TIntermDeclaration *TIntermTraverser::createTempInitDeclaration(TIntermTyped *initializer)
{
    return createTempInitDeclaration(initializer, EvqTemporary);
}

TIntermBinary *TIntermTraverser::createTempAssignment(TIntermTyped *rightNode)
{
    ASSERT(rightNode != nullptr);
    TIntermSymbol *tempSymbol = createTempSymbol(rightNode->getType());
    TIntermBinary *assignment = new TIntermBinary(EOpAssign, tempSymbol, rightNode);
    return assignment;
}

void TIntermTraverser::useTemporaryIndex(unsigned int *temporaryIndex)
{
    mTemporaryIndex = temporaryIndex;
}

void TIntermTraverser::nextTemporaryIndex()
{
    ASSERT(mTemporaryIndex != nullptr);
    ++(*mTemporaryIndex);
}

void TLValueTrackingTraverser::addToFunctionMap(const TSymbolUniqueId &id,
                                                TIntermSequence *paramSequence)
{
    mFunctionMap[id.get()] = paramSequence;
}

bool TLValueTrackingTraverser::isInFunctionMap(const TIntermAggregate *callNode) const
{
    ASSERT(callNode->getOp() == EOpCallFunctionInAST);
    return (mFunctionMap.find(callNode->getFunctionSymbolInfo()->getId().get()) !=
            mFunctionMap.end());
}

TIntermSequence *TLValueTrackingTraverser::getFunctionParameters(const TIntermAggregate *callNode)
{
    ASSERT(isInFunctionMap(callNode));
    return mFunctionMap[callNode->getFunctionSymbolInfo()->getId().get()];
}

void TLValueTrackingTraverser::setInFunctionCallOutParameter(bool inOutParameter)
{
    mInFunctionCallOutParameter = inOutParameter;
}

bool TLValueTrackingTraverser::isInFunctionCallOutParameter() const
{
    return mInFunctionCallOutParameter;
}

//
// Traverse the intermediate representation tree, and
// call a node type specific function for each node.
// Done recursively through the member function Traverse().
// Node types can be skipped if their function to call is 0,
// but their subtree will still be traversed.
// Nodes with children can have their whole subtree skipped
// if preVisit is turned on and the type specific function
// returns false.
//

//
// Traversal functions for terminals are straighforward....
//
void TIntermTraverser::traverseSymbol(TIntermSymbol *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);
    visitSymbol(node);
}

void TIntermTraverser::traverseConstantUnion(TIntermConstantUnion *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);
    visitConstantUnion(node);
}

void TIntermTraverser::traverseSwizzle(TIntermSwizzle *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);

    bool visit = true;

    if (preVisit)
        visit = visitSwizzle(PreVisit, node);

    if (visit)
    {
        node->getOperand()->traverse(this);
    }

    if (visit && postVisit)
        visitSwizzle(PostVisit, node);
}

//
// Traverse a binary node.
//
void TIntermTraverser::traverseBinary(TIntermBinary *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);

    bool visit = true;

    //
    // visit the node before children if pre-visiting.
    //
    if (preVisit)
        visit = visitBinary(PreVisit, node);

    //
    // Visit the children, in the right order.
    //
    if (visit)
    {
        if (node->getLeft())
            node->getLeft()->traverse(this);

        if (inVisit)
            visit = visitBinary(InVisit, node);

        if (visit && node->getRight())
            node->getRight()->traverse(this);
    }

    //
    // Visit the node after the children, if requested and the traversal
    // hasn't been cancelled yet.
    //
    if (visit && postVisit)
        visitBinary(PostVisit, node);
}

void TLValueTrackingTraverser::traverseBinary(TIntermBinary *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);

    bool visit = true;

    //
    // visit the node before children if pre-visiting.
    //
    if (preVisit)
        visit = visitBinary(PreVisit, node);

    //
    // Visit the children, in the right order.
    //
    if (visit)
    {
        // Some binary operations like indexing can be inside an expression which must be an
        // l-value.
        bool parentOperatorRequiresLValue     = operatorRequiresLValue();
        bool parentInFunctionCallOutParameter = isInFunctionCallOutParameter();
        if (node->isAssignment())
        {
            ASSERT(!isLValueRequiredHere());
            setOperatorRequiresLValue(true);
        }

        if (node->getLeft())
            node->getLeft()->traverse(this);

        if (inVisit)
            visit = visitBinary(InVisit, node);

        if (node->isAssignment())
            setOperatorRequiresLValue(false);

        // Index is not required to be an l-value even when the surrounding expression is required
        // to be an l-value.
        TOperator op = node->getOp();
        if (op == EOpIndexDirect || op == EOpIndexDirectInterfaceBlock ||
            op == EOpIndexDirectStruct || op == EOpIndexIndirect)
        {
            setOperatorRequiresLValue(false);
            setInFunctionCallOutParameter(false);
        }

        if (visit && node->getRight())
            node->getRight()->traverse(this);

        setOperatorRequiresLValue(parentOperatorRequiresLValue);
        setInFunctionCallOutParameter(parentInFunctionCallOutParameter);
    }

    //
    // Visit the node after the children, if requested and the traversal
    // hasn't been cancelled yet.
    //
    if (visit && postVisit)
        visitBinary(PostVisit, node);
}

//
// Traverse a unary node.  Same comments in binary node apply here.
//
void TIntermTraverser::traverseUnary(TIntermUnary *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);

    bool visit = true;

    if (preVisit)
        visit = visitUnary(PreVisit, node);

    if (visit)
    {
        node->getOperand()->traverse(this);
    }

    if (visit && postVisit)
        visitUnary(PostVisit, node);
}

void TLValueTrackingTraverser::traverseUnary(TIntermUnary *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);

    bool visit = true;

    if (preVisit)
        visit = visitUnary(PreVisit, node);

    if (visit)
    {
        ASSERT(!operatorRequiresLValue());
        switch (node->getOp())
        {
            case EOpPostIncrement:
            case EOpPostDecrement:
            case EOpPreIncrement:
            case EOpPreDecrement:
                setOperatorRequiresLValue(true);
                break;
            default:
                break;
        }

        node->getOperand()->traverse(this);

        setOperatorRequiresLValue(false);
    }

    if (visit && postVisit)
        visitUnary(PostVisit, node);
}

// Traverse a function definition node.
void TIntermTraverser::traverseFunctionDefinition(TIntermFunctionDefinition *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);

    bool visit = true;

    if (preVisit)
        visit = visitFunctionDefinition(PreVisit, node);

    if (visit)
    {
        mInGlobalScope = false;

        node->getFunctionPrototype()->traverse(this);
        if (inVisit)
            visit = visitFunctionDefinition(InVisit, node);
        node->getBody()->traverse(this);

        mInGlobalScope = true;
    }

    if (visit && postVisit)
        visitFunctionDefinition(PostVisit, node);
}

// Traverse a block node.
void TIntermTraverser::traverseBlock(TIntermBlock *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);
    pushParentBlock(node);

    bool visit = true;

    TIntermSequence *sequence = node->getSequence();

    if (preVisit)
        visit = visitBlock(PreVisit, node);

    if (visit)
    {
        for (auto *child : *sequence)
        {
            child->traverse(this);
            if (visit && inVisit)
            {
                if (child != sequence->back())
                    visit = visitBlock(InVisit, node);
            }

            incrementParentBlockPos();
        }
    }

    if (visit && postVisit)
        visitBlock(PostVisit, node);

    popParentBlock();
}

void TIntermTraverser::traverseInvariantDeclaration(TIntermInvariantDeclaration *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);

    bool visit = true;

    if (preVisit)
    {
        visit = visitInvariantDeclaration(PreVisit, node);
    }

    if (visit)
    {
        node->getSymbol()->traverse(this);
        if (postVisit)
        {
            visitInvariantDeclaration(PostVisit, node);
        }
    }
}

// Traverse a declaration node.
void TIntermTraverser::traverseDeclaration(TIntermDeclaration *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);

    bool visit = true;

    TIntermSequence *sequence = node->getSequence();

    if (preVisit)
        visit = visitDeclaration(PreVisit, node);

    if (visit)
    {
        for (auto *child : *sequence)
        {
            child->traverse(this);
            if (visit && inVisit)
            {
                if (child != sequence->back())
                    visit = visitDeclaration(InVisit, node);
            }
        }
    }

    if (visit && postVisit)
        visitDeclaration(PostVisit, node);
}

void TIntermTraverser::traverseFunctionPrototype(TIntermFunctionPrototype *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);

    bool visit = true;

    TIntermSequence *sequence = node->getSequence();

    if (preVisit)
        visit = visitFunctionPrototype(PreVisit, node);

    if (visit)
    {
        for (auto *child : *sequence)
        {
            child->traverse(this);
            if (visit && inVisit)
            {
                if (child != sequence->back())
                    visit = visitFunctionPrototype(InVisit, node);
            }
        }
    }

    if (visit && postVisit)
        visitFunctionPrototype(PostVisit, node);
}

// Traverse an aggregate node.  Same comments in binary node apply here.
void TIntermTraverser::traverseAggregate(TIntermAggregate *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);

    bool visit = true;

    TIntermSequence *sequence = node->getSequence();

    if (preVisit)
        visit = visitAggregate(PreVisit, node);

    if (visit)
    {
        for (auto *child : *sequence)
        {
            child->traverse(this);
            if (visit && inVisit)
            {
                if (child != sequence->back())
                    visit = visitAggregate(InVisit, node);
            }
        }
    }

    if (visit && postVisit)
        visitAggregate(PostVisit, node);
}

void TLValueTrackingTraverser::traverseFunctionPrototype(TIntermFunctionPrototype *node)
{
    TIntermSequence *sequence = node->getSequence();
    addToFunctionMap(node->getFunctionSymbolInfo()->getId(), sequence);

    TIntermTraverser::traverseFunctionPrototype(node);
}

void TLValueTrackingTraverser::traverseAggregate(TIntermAggregate *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);

    bool visit = true;

    TIntermSequence *sequence = node->getSequence();

    if (preVisit)
        visit = visitAggregate(PreVisit, node);

    if (visit)
    {
        if (node->getOp() == EOpCallFunctionInAST)
        {
            if (isInFunctionMap(node))
            {
                TIntermSequence *params             = getFunctionParameters(node);
                TIntermSequence::iterator paramIter = params->begin();
                for (auto *child : *sequence)
                {
                    ASSERT(paramIter != params->end());
                    TQualifier qualifier = (*paramIter)->getAsTyped()->getQualifier();
                    setInFunctionCallOutParameter(qualifier == EvqOut || qualifier == EvqInOut);

                    child->traverse(this);
                    if (visit && inVisit)
                    {
                        if (child != sequence->back())
                            visit = visitAggregate(InVisit, node);
                    }

                    ++paramIter;
                }
            }
            else
            {
                // The node might not be in the function map in case we're in the middle of
                // transforming the AST, and have inserted function call nodes without inserting the
                // function definitions yet.
                setInFunctionCallOutParameter(false);
                for (auto *child : *sequence)
                {
                    child->traverse(this);
                    if (visit && inVisit)
                    {
                        if (child != sequence->back())
                            visit = visitAggregate(InVisit, node);
                    }
                }
            }

            setInFunctionCallOutParameter(false);
        }
        else
        {
            // Find the built-in function corresponding to this op so that we can determine the
            // in/out qualifiers of its parameters.
            TFunction *builtInFunc = nullptr;
            if (!node->isFunctionCall() && !node->isConstructor())
            {
                builtInFunc = static_cast<TFunction *>(
                    mSymbolTable.findBuiltIn(node->getSymbolTableMangledName(), mShaderVersion));
            }

            size_t paramIndex = 0;

            for (auto *child : *sequence)
            {
                // This assumes that raw functions called with
                // EOpCallInternalRawFunction don't have out parameters.
                TQualifier qualifier = EvqIn;
                if (builtInFunc != nullptr)
                    qualifier = builtInFunc->getParam(paramIndex).type->getQualifier();
                setInFunctionCallOutParameter(qualifier == EvqOut || qualifier == EvqInOut);
                child->traverse(this);

                if (visit && inVisit)
                {
                    if (child != sequence->back())
                        visit = visitAggregate(InVisit, node);
                }

                ++paramIndex;
            }

            setInFunctionCallOutParameter(false);
        }
    }

    if (visit && postVisit)
        visitAggregate(PostVisit, node);
}

//
// Traverse a ternary node.  Same comments in binary node apply here.
//
void TIntermTraverser::traverseTernary(TIntermTernary *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);

    bool visit = true;

    if (preVisit)
        visit = visitTernary(PreVisit, node);

    if (visit)
    {
        node->getCondition()->traverse(this);
        if (node->getTrueExpression())
            node->getTrueExpression()->traverse(this);
        if (node->getFalseExpression())
            node->getFalseExpression()->traverse(this);
    }

    if (visit && postVisit)
        visitTernary(PostVisit, node);
}

// Traverse an if-else node.  Same comments in binary node apply here.
void TIntermTraverser::traverseIfElse(TIntermIfElse *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);

    bool visit = true;

    if (preVisit)
        visit = visitIfElse(PreVisit, node);

    if (visit)
    {
        node->getCondition()->traverse(this);
        if (node->getTrueBlock())
            node->getTrueBlock()->traverse(this);
        if (node->getFalseBlock())
            node->getFalseBlock()->traverse(this);
    }

    if (visit && postVisit)
        visitIfElse(PostVisit, node);
}

//
// Traverse a switch node.  Same comments in binary node apply here.
//
void TIntermTraverser::traverseSwitch(TIntermSwitch *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);

    bool visit = true;

    if (preVisit)
        visit = visitSwitch(PreVisit, node);

    if (visit)
    {
        node->getInit()->traverse(this);
        if (inVisit)
            visit = visitSwitch(InVisit, node);
        if (visit && node->getStatementList())
            node->getStatementList()->traverse(this);
    }

    if (visit && postVisit)
        visitSwitch(PostVisit, node);
}

//
// Traverse a case node.  Same comments in binary node apply here.
//
void TIntermTraverser::traverseCase(TIntermCase *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);

    bool visit = true;

    if (preVisit)
        visit = visitCase(PreVisit, node);

    if (visit && node->getCondition())
    {
        node->getCondition()->traverse(this);
    }

    if (visit && postVisit)
        visitCase(PostVisit, node);
}

//
// Traverse a loop node.  Same comments in binary node apply here.
//
void TIntermTraverser::traverseLoop(TIntermLoop *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);

    bool visit = true;

    if (preVisit)
        visit = visitLoop(PreVisit, node);

    if (visit)
    {
        if (node->getInit())
            node->getInit()->traverse(this);

        if (node->getCondition())
            node->getCondition()->traverse(this);

        if (node->getBody())
            node->getBody()->traverse(this);

        if (node->getExpression())
            node->getExpression()->traverse(this);
    }

    if (visit && postVisit)
        visitLoop(PostVisit, node);
}

//
// Traverse a branch node.  Same comments in binary node apply here.
//
void TIntermTraverser::traverseBranch(TIntermBranch *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);

    bool visit = true;

    if (preVisit)
        visit = visitBranch(PreVisit, node);

    if (visit && node->getExpression())
    {
        node->getExpression()->traverse(this);
    }

    if (visit && postVisit)
        visitBranch(PostVisit, node);
}

void TIntermTraverser::traverseRaw(TIntermRaw *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);
    visitRaw(node);
}

}  // namespace sh
