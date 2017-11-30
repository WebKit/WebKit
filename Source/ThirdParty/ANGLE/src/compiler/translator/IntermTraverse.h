//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IntermTraverse.h : base classes for AST traversers that walk the AST and
//   also have the ability to transform it by replacing nodes.

#ifndef COMPILER_TRANSLATOR_INTERMTRAVERSE_H_
#define COMPILER_TRANSLATOR_INTERMTRAVERSE_H_

#include "compiler/translator/IntermNode.h"

namespace sh
{

class TSymbolTable;
class TSymbolUniqueId;

enum Visit
{
    PreVisit,
    InVisit,
    PostVisit
};

// For traversing the tree.  User should derive from this class overriding the visit functions,
// and then pass an object of the subclass to a traverse method of a node.
//
// The traverse*() functions may also be overridden to do other bookkeeping on the tree to provide
// contextual information to the visit functions, such as whether the node is the target of an
// assignment. This is complex to maintain and so should only be done in special cases.
//
// When using this, just fill in the methods for nodes you want visited.
// Return false from a pre-visit to skip visiting that node's subtree.
class TIntermTraverser : angle::NonCopyable
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    TIntermTraverser(bool preVisit,
                     bool inVisit,
                     bool postVisit,
                     TSymbolTable *symbolTable = nullptr);
    virtual ~TIntermTraverser();

    virtual void visitSymbol(TIntermSymbol *node) {}
    virtual void visitRaw(TIntermRaw *node) {}
    virtual void visitConstantUnion(TIntermConstantUnion *node) {}
    virtual bool visitSwizzle(Visit visit, TIntermSwizzle *node) { return true; }
    virtual bool visitBinary(Visit visit, TIntermBinary *node) { return true; }
    virtual bool visitUnary(Visit visit, TIntermUnary *node) { return true; }
    virtual bool visitTernary(Visit visit, TIntermTernary *node) { return true; }
    virtual bool visitIfElse(Visit visit, TIntermIfElse *node) { return true; }
    virtual bool visitSwitch(Visit visit, TIntermSwitch *node) { return true; }
    virtual bool visitCase(Visit visit, TIntermCase *node) { return true; }
    virtual bool visitFunctionPrototype(Visit visit, TIntermFunctionPrototype *node)
    {
        return true;
    }
    virtual bool visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *node)
    {
        return true;
    }
    virtual bool visitAggregate(Visit visit, TIntermAggregate *node) { return true; }
    virtual bool visitBlock(Visit visit, TIntermBlock *node) { return true; }
    virtual bool visitInvariantDeclaration(Visit visit, TIntermInvariantDeclaration *node)
    {
        return true;
    }
    virtual bool visitDeclaration(Visit visit, TIntermDeclaration *node) { return true; }
    virtual bool visitLoop(Visit visit, TIntermLoop *node) { return true; }
    virtual bool visitBranch(Visit visit, TIntermBranch *node) { return true; }

    // The traverse functions contain logic for iterating over the children of the node
    // and calling the visit functions in the appropriate places. They also track some
    // context that may be used by the visit functions.
    virtual void traverseSymbol(TIntermSymbol *node);
    virtual void traverseRaw(TIntermRaw *node);
    virtual void traverseConstantUnion(TIntermConstantUnion *node);
    virtual void traverseSwizzle(TIntermSwizzle *node);
    virtual void traverseBinary(TIntermBinary *node);
    virtual void traverseUnary(TIntermUnary *node);
    virtual void traverseTernary(TIntermTernary *node);
    virtual void traverseIfElse(TIntermIfElse *node);
    virtual void traverseSwitch(TIntermSwitch *node);
    virtual void traverseCase(TIntermCase *node);
    virtual void traverseFunctionPrototype(TIntermFunctionPrototype *node);
    virtual void traverseFunctionDefinition(TIntermFunctionDefinition *node);
    virtual void traverseAggregate(TIntermAggregate *node);
    virtual void traverseBlock(TIntermBlock *node);
    virtual void traverseInvariantDeclaration(TIntermInvariantDeclaration *node);
    virtual void traverseDeclaration(TIntermDeclaration *node);
    virtual void traverseLoop(TIntermLoop *node);
    virtual void traverseBranch(TIntermBranch *node);

    int getMaxDepth() const { return mMaxDepth; }

    // If traversers need to replace nodes, they can add the replacements in
    // mReplacements/mMultiReplacements during traversal and the user of the traverser should call
    // this function after traversal to perform them.
    void updateTree();

  protected:
    // Should only be called from traverse*() functions
    void incrementDepth(TIntermNode *current)
    {
        mDepth++;
        mMaxDepth = std::max(mMaxDepth, mDepth);
        mPath.push_back(current);
    }

    // Should only be called from traverse*() functions
    void decrementDepth()
    {
        mDepth--;
        mPath.pop_back();
    }

    // RAII helper for incrementDepth/decrementDepth
    class ScopedNodeInTraversalPath
    {
      public:
        ScopedNodeInTraversalPath(TIntermTraverser *traverser, TIntermNode *current)
            : mTraverser(traverser)
        {
            mTraverser->incrementDepth(current);
        }
        ~ScopedNodeInTraversalPath() { mTraverser->decrementDepth(); }

      private:
        TIntermTraverser *mTraverser;
    };

    TIntermNode *getParentNode() { return mPath.size() <= 1 ? nullptr : mPath[mPath.size() - 2u]; }

    // Return the nth ancestor of the node being traversed. getAncestorNode(0) == getParentNode()
    TIntermNode *getAncestorNode(unsigned int n)
    {
        if (mPath.size() > n + 1u)
        {
            return mPath[mPath.size() - n - 2u];
        }
        return nullptr;
    }

    const TIntermBlock *getParentBlock() const;

    void pushParentBlock(TIntermBlock *node);
    void incrementParentBlockPos();
    void popParentBlock();

    // To replace a single node with multiple nodes in the parent aggregate. May be used with blocks
    // but also with other nodes like declarations.
    struct NodeReplaceWithMultipleEntry
    {
        NodeReplaceWithMultipleEntry(TIntermAggregateBase *_parent,
                                     TIntermNode *_original,
                                     TIntermSequence _replacements)
            : parent(_parent), original(_original), replacements(_replacements)
        {
        }

        TIntermAggregateBase *parent;
        TIntermNode *original;
        TIntermSequence replacements;
    };

    // Helper to insert statements in the parent block of the node currently being traversed.
    // The statements will be inserted before the node being traversed once updateTree is called.
    // Should only be called during PreVisit or PostVisit if called from block nodes.
    // Note that two insertions to the same position in the same block are not supported.
    void insertStatementsInParentBlock(const TIntermSequence &insertions);

    // Same as above, but supports simultaneous insertion of statements before and after the node
    // currently being traversed.
    void insertStatementsInParentBlock(const TIntermSequence &insertionsBefore,
                                       const TIntermSequence &insertionsAfter);

    // Helper to insert a single statement.
    void insertStatementInParentBlock(TIntermNode *statement);

    // Helper to create a temporary symbol node with the given qualifier.
    TIntermSymbol *createTempSymbol(const TType &type, TQualifier qualifier);
    // Helper to create a temporary symbol node.
    TIntermSymbol *createTempSymbol(const TType &type);
    // Create a node that declares but doesn't initialize a temporary symbol.
    TIntermDeclaration *createTempDeclaration(const TType &type);
    // Create a node that initializes the current temporary symbol with initializer. The symbol will
    // have the given qualifier.
    TIntermDeclaration *createTempInitDeclaration(TIntermTyped *initializer, TQualifier qualifier);
    // Create a node that initializes the current temporary symbol with initializer.
    TIntermDeclaration *createTempInitDeclaration(TIntermTyped *initializer);
    // Create a node that assigns rightNode to the current temporary symbol.
    TIntermBinary *createTempAssignment(TIntermTyped *rightNode);
    // Increment temporary symbol index.
    void nextTemporaryId();

    enum class OriginalNode
    {
        BECOMES_CHILD,
        IS_DROPPED
    };

    void clearReplacementQueue();

    // Replace the node currently being visited with replacement.
    void queueReplacement(TIntermNode *replacement, OriginalNode originalStatus);
    // Explicitly specify a node to replace with replacement.
    void queueReplacementWithParent(TIntermNode *parent,
                                    TIntermNode *original,
                                    TIntermNode *replacement,
                                    OriginalNode originalStatus);

    const bool preVisit;
    const bool inVisit;
    const bool postVisit;

    int mDepth;
    int mMaxDepth;

    bool mInGlobalScope;

    // During traversing, save all the changes that need to happen into
    // mReplacements/mMultiReplacements, then do them by calling updateTree().
    // Multi replacements are processed after single replacements.
    std::vector<NodeReplaceWithMultipleEntry> mMultiReplacements;

    TSymbolTable *mSymbolTable;

  private:
    // To insert multiple nodes into the parent block.
    struct NodeInsertMultipleEntry
    {
        NodeInsertMultipleEntry(TIntermBlock *_parent,
                                TIntermSequence::size_type _position,
                                TIntermSequence _insertionsBefore,
                                TIntermSequence _insertionsAfter)
            : parent(_parent),
              position(_position),
              insertionsBefore(_insertionsBefore),
              insertionsAfter(_insertionsAfter)
        {
        }

        TIntermBlock *parent;
        TIntermSequence::size_type position;
        TIntermSequence insertionsBefore;
        TIntermSequence insertionsAfter;
    };

    static bool CompareInsertion(const NodeInsertMultipleEntry &a,
                                 const NodeInsertMultipleEntry &b);

    // To replace a single node with another on the parent node
    struct NodeUpdateEntry
    {
        NodeUpdateEntry(TIntermNode *_parent,
                        TIntermNode *_original,
                        TIntermNode *_replacement,
                        bool _originalBecomesChildOfReplacement)
            : parent(_parent),
              original(_original),
              replacement(_replacement),
              originalBecomesChildOfReplacement(_originalBecomesChildOfReplacement)
        {
        }

        TIntermNode *parent;
        TIntermNode *original;
        TIntermNode *replacement;
        bool originalBecomesChildOfReplacement;
    };

    struct ParentBlock
    {
        ParentBlock(TIntermBlock *nodeIn, TIntermSequence::size_type posIn)
            : node(nodeIn), pos(posIn)
        {
        }

        TIntermBlock *node;
        TIntermSequence::size_type pos;
    };

    std::vector<NodeInsertMultipleEntry> mInsertions;
    std::vector<NodeUpdateEntry> mReplacements;

    // All the nodes from root to the current node during traversing.
    TVector<TIntermNode *> mPath;

    // All the code blocks from the root to the current node's parent during traversal.
    std::vector<ParentBlock> mParentBlockStack;

    TSymbolUniqueId *mTemporaryId;
};

// Traverser parent class that tracks where a node is a destination of a write operation and so is
// required to be an l-value.
class TLValueTrackingTraverser : public TIntermTraverser
{
  public:
    TLValueTrackingTraverser(bool preVisit,
                             bool inVisit,
                             bool postVisit,
                             TSymbolTable *symbolTable,
                             int shaderVersion);
    virtual ~TLValueTrackingTraverser() {}

    void traverseBinary(TIntermBinary *node) final;
    void traverseUnary(TIntermUnary *node) final;
    void traverseFunctionPrototype(TIntermFunctionPrototype *node) final;
    void traverseAggregate(TIntermAggregate *node) final;

  protected:
    bool isLValueRequiredHere() const
    {
        return mOperatorRequiresLValue || mInFunctionCallOutParameter;
    }

  private:
    // Track whether an l-value is required in the node that is currently being traversed by the
    // surrounding operator.
    // Use isLValueRequiredHere to check all conditions which require an l-value.
    void setOperatorRequiresLValue(bool lValueRequired)
    {
        mOperatorRequiresLValue = lValueRequired;
    }
    bool operatorRequiresLValue() const { return mOperatorRequiresLValue; }

    // Add a function encountered during traversal to the function map.
    void addToFunctionMap(const TSymbolUniqueId &id, TIntermSequence *paramSequence);

    // Return true if the prototype or definition of the function being called has been encountered
    // during traversal.
    bool isInFunctionMap(const TIntermAggregate *callNode) const;

    // Return the parameters sequence from the function definition or prototype.
    TIntermSequence *getFunctionParameters(const TIntermAggregate *callNode);

    // Track whether an l-value is required inside a function call.
    void setInFunctionCallOutParameter(bool inOutParameter);
    bool isInFunctionCallOutParameter() const;

    bool mOperatorRequiresLValue;
    bool mInFunctionCallOutParameter;

    // Map from function symbol id values to their parameter sequences
    TMap<int, TIntermSequence *> mFunctionMap;

    const int mShaderVersion;
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_INTERMTRAVERSE_H_
