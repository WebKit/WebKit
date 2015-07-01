//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

//
// Definition of the in-memory high-level intermediate representation
// of shaders.  This is a tree that parser creates.
//
// Nodes in the tree are defined as a hierarchy of classes derived from
// TIntermNode. Each is a node in a tree.  There is no preset branching factor;
// each node can have it's own type of list of children.
//

#ifndef COMPILER_TRANSLATOR_INTERMNODE_H_
#define COMPILER_TRANSLATOR_INTERMNODE_H_

#include "GLSLANG/ShaderLang.h"

#include <algorithm>
#include <queue>

#include "common/angleutils.h"
#include "compiler/translator/Common.h"
#include "compiler/translator/ConstantUnion.h"
#include "compiler/translator/Operator.h"
#include "compiler/translator/Types.h"

class TIntermTraverser;
class TIntermAggregate;
class TIntermBinary;
class TIntermUnary;
class TIntermConstantUnion;
class TIntermSelection;
class TIntermSwitch;
class TIntermCase;
class TIntermTyped;
class TIntermSymbol;
class TIntermLoop;
class TInfoSink;
class TInfoSinkBase;
class TIntermRaw;

//
// Base class for the tree nodes
//
class TIntermNode
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    TIntermNode()
    {
        // TODO: Move this to TSourceLoc constructor
        // after getting rid of TPublicType.
        mLine.first_file = mLine.last_file = 0;
        mLine.first_line = mLine.last_line = 0;
    }
    virtual ~TIntermNode() { }

    const TSourceLoc &getLine() const { return mLine; }
    void setLine(const TSourceLoc &l) { mLine = l; }

    virtual void traverse(TIntermTraverser *) = 0;
    virtual TIntermTyped *getAsTyped() { return 0; }
    virtual TIntermConstantUnion *getAsConstantUnion() { return 0; }
    virtual TIntermAggregate *getAsAggregate() { return 0; }
    virtual TIntermBinary *getAsBinaryNode() { return 0; }
    virtual TIntermUnary *getAsUnaryNode() { return 0; }
    virtual TIntermSelection *getAsSelectionNode() { return 0; }
    virtual TIntermSwitch *getAsSwitchNode() { return 0; }
    virtual TIntermCase *getAsCaseNode() { return 0; }
    virtual TIntermSymbol *getAsSymbolNode() { return 0; }
    virtual TIntermLoop *getAsLoopNode() { return 0; }
    virtual TIntermRaw *getAsRawNode() { return 0; }

    // Replace a child node. Return true if |original| is a child
    // node and it is replaced; otherwise, return false.
    virtual bool replaceChildNode(
        TIntermNode *original, TIntermNode *replacement) = 0;

  protected:
    TSourceLoc mLine;
};

//
// This is just to help yacc.
//
struct TIntermNodePair
{
    TIntermNode *node1;
    TIntermNode *node2;
};

//
// Intermediate class for nodes that have a type.
//
class TIntermTyped : public TIntermNode
{
  public:
    TIntermTyped(const TType &t) : mType(t)  { }
    virtual TIntermTyped *getAsTyped() { return this; }

    virtual bool hasSideEffects() const = 0;

    void setType(const TType &t) { mType = t; }
    void setTypePreservePrecision(const TType &t);
    const TType &getType() const { return mType; }
    TType *getTypePointer() { return &mType; }

    TBasicType getBasicType() const { return mType.getBasicType(); }
    TQualifier getQualifier() const { return mType.getQualifier(); }
    TPrecision getPrecision() const { return mType.getPrecision(); }
    int getCols() const { return mType.getCols(); }
    int getRows() const { return mType.getRows(); }
    int getNominalSize() const { return mType.getNominalSize(); }
    int getSecondarySize() const { return mType.getSecondarySize(); }

    bool isInterfaceBlock() const { return mType.isInterfaceBlock(); }
    bool isMatrix() const { return mType.isMatrix(); }
    bool isArray()  const { return mType.isArray(); }
    bool isVector() const { return mType.isVector(); }
    bool isScalar() const { return mType.isScalar(); }
    bool isScalarInt() const { return mType.isScalarInt(); }
    const char *getBasicString() const { return mType.getBasicString(); }
    const char *getQualifierString() const { return mType.getQualifierString(); }
    TString getCompleteString() const { return mType.getCompleteString(); }

    int getArraySize() const { return mType.getArraySize(); }

  protected:
    TType mType;
};

//
// Handle for, do-while, and while loops.
//
enum TLoopType
{
    ELoopFor,
    ELoopWhile,
    ELoopDoWhile
};

class TIntermLoop : public TIntermNode
{
  public:
    TIntermLoop(TLoopType type,
                TIntermNode *init, TIntermTyped *cond, TIntermTyped *expr,
                TIntermNode *body)
        : mType(type),
          mInit(init),
          mCond(cond),
          mExpr(expr),
          mBody(body),
          mUnrollFlag(false) { }

    virtual TIntermLoop *getAsLoopNode() { return this; }
    virtual void traverse(TIntermTraverser *);
    virtual bool replaceChildNode(
        TIntermNode *original, TIntermNode *replacement);

    TLoopType getType() const { return mType; }
    TIntermNode *getInit() { return mInit; }
    TIntermTyped *getCondition() { return mCond; }
    TIntermTyped *getExpression() { return mExpr; }
    TIntermNode *getBody() { return mBody; }

    void setUnrollFlag(bool flag) { mUnrollFlag = flag; }
    bool getUnrollFlag() const { return mUnrollFlag; }

  protected:
    TLoopType mType;
    TIntermNode *mInit;  // for-loop initialization
    TIntermTyped *mCond; // loop exit condition
    TIntermTyped *mExpr; // for-loop expression
    TIntermNode *mBody;  // loop body

    bool mUnrollFlag; // Whether the loop should be unrolled or not.
};

//
// Handle break, continue, return, and kill.
//
class TIntermBranch : public TIntermNode
{
  public:
    TIntermBranch(TOperator op, TIntermTyped *e)
        : mFlowOp(op),
          mExpression(e) { }

    virtual void traverse(TIntermTraverser *);
    virtual bool replaceChildNode(
        TIntermNode *original, TIntermNode *replacement);

    TOperator getFlowOp() { return mFlowOp; }
    TIntermTyped* getExpression() { return mExpression; }

protected:
    TOperator mFlowOp;
    TIntermTyped *mExpression;  // non-zero except for "return exp;" statements
};

//
// Nodes that correspond to symbols or constants in the source code.
//
class TIntermSymbol : public TIntermTyped
{
  public:
    // if symbol is initialized as symbol(sym), the memory comes from the poolallocator of sym.
    // If sym comes from per process globalpoolallocator, then it causes increased memory usage
    // per compile it is essential to use "symbol = sym" to assign to symbol
    TIntermSymbol(int id, const TString &symbol, const TType &type)
        : TIntermTyped(type),
          mId(id),
          mInternal(false)
    {
        mSymbol = symbol;
    }

    virtual bool hasSideEffects() const { return false; }

    int getId() const { return mId; }
    const TString &getSymbol() const { return mSymbol; }

    void setId(int newId) { mId = newId; }

    bool isInternal() const { return mInternal; }
    void setInternal(bool isInternal) { mInternal = isInternal; }

    virtual void traverse(TIntermTraverser *);
    virtual TIntermSymbol *getAsSymbolNode() { return this; }
    virtual bool replaceChildNode(TIntermNode *, TIntermNode *) { return false; }

  protected:
    int mId;
    bool mInternal;
    TString mSymbol;
};

// A Raw node stores raw code, that the translator will insert verbatim
// into the output stream. Useful for transformation operations that make
// complex code that might not fit naturally into the GLSL model.
class TIntermRaw : public TIntermTyped
{
  public:
    TIntermRaw(const TType &type, const TString &rawText)
        : TIntermTyped(type),
          mRawText(rawText) { }

    virtual bool hasSideEffects() const { return false; }

    TString getRawText() const { return mRawText; }

    virtual void traverse(TIntermTraverser *);

    virtual TIntermRaw *getAsRawNode() { return this; }
    virtual bool replaceChildNode(TIntermNode *, TIntermNode *) { return false; }

  protected:
    TString mRawText;
};

class TIntermConstantUnion : public TIntermTyped
{
  public:
    TIntermConstantUnion(TConstantUnion *unionPointer, const TType &type)
        : TIntermTyped(type),
          mUnionArrayPointer(unionPointer) { }

    virtual bool hasSideEffects() const { return false; }

    const TConstantUnion *getUnionArrayPointer() const { return mUnionArrayPointer; }
    TConstantUnion *getUnionArrayPointer() { return mUnionArrayPointer; }

    int getIConst(size_t index) const
    {
        return mUnionArrayPointer ? mUnionArrayPointer[index].getIConst() : 0;
    }
    unsigned int getUConst(size_t index) const
    {
        return mUnionArrayPointer ? mUnionArrayPointer[index].getUConst() : 0;
    }
    float getFConst(size_t index) const
    {
        return mUnionArrayPointer ? mUnionArrayPointer[index].getFConst() : 0.0f;
    }
    bool getBConst(size_t index) const
    {
        return mUnionArrayPointer ? mUnionArrayPointer[index].getBConst() : false;
    }

    void replaceConstantUnion(TConstantUnion *safeConstantUnion)
    {
        // Previous union pointer freed on pool deallocation.
        mUnionArrayPointer = safeConstantUnion;
    }

    virtual TIntermConstantUnion *getAsConstantUnion()  { return this; }
    virtual void traverse(TIntermTraverser *);
    virtual bool replaceChildNode(TIntermNode *, TIntermNode *) { return false; }

    TIntermTyped *fold(TOperator op, TIntermConstantUnion *rightNode, TInfoSink &infoSink);

  protected:
    TConstantUnion *mUnionArrayPointer;

  private:
    typedef float(*FloatTypeUnaryFunc) (float);
    bool foldFloatTypeUnary(const TConstantUnion &parameter, FloatTypeUnaryFunc builtinFunc, TInfoSink &infoSink, TConstantUnion *result) const;
};

//
// Intermediate class for node types that hold operators.
//
class TIntermOperator : public TIntermTyped
{
  public:
    TOperator getOp() const { return mOp; }
    void setOp(TOperator op) { mOp = op; }

    bool isAssignment() const;
    bool isConstructor() const;

    virtual bool hasSideEffects() const { return isAssignment(); }

  protected:
    TIntermOperator(TOperator op)
        : TIntermTyped(TType(EbtFloat, EbpUndefined)),
          mOp(op) {}
    TIntermOperator(TOperator op, const TType &type)
        : TIntermTyped(type),
          mOp(op) {}

    TOperator mOp;
};

//
// Nodes for all the basic binary math operators.
//
class TIntermBinary : public TIntermOperator
{
  public:
    TIntermBinary(TOperator op)
        : TIntermOperator(op),
          mAddIndexClamp(false) {}

    virtual TIntermBinary *getAsBinaryNode() { return this; }
    virtual void traverse(TIntermTraverser *);
    virtual bool replaceChildNode(
        TIntermNode *original, TIntermNode *replacement);

    virtual bool hasSideEffects() const
    {
        return isAssignment() || mLeft->hasSideEffects() || mRight->hasSideEffects();
    }

    void setLeft(TIntermTyped *node) { mLeft = node; }
    void setRight(TIntermTyped *node) { mRight = node; }
    TIntermTyped *getLeft() const { return mLeft; }
    TIntermTyped *getRight() const { return mRight; }
    bool promote(TInfoSink &);

    void setAddIndexClamp() { mAddIndexClamp = true; }
    bool getAddIndexClamp() { return mAddIndexClamp; }

  protected:
    TIntermTyped* mLeft;
    TIntermTyped* mRight;

    // If set to true, wrap any EOpIndexIndirect with a clamp to bounds.
    bool mAddIndexClamp;
};

//
// Nodes for unary math operators.
//
class TIntermUnary : public TIntermOperator
{
  public:
    TIntermUnary(TOperator op, const TType &type)
        : TIntermOperator(op, type),
          mOperand(NULL),
          mUseEmulatedFunction(false) {}
    TIntermUnary(TOperator op)
        : TIntermOperator(op),
          mOperand(NULL),
          mUseEmulatedFunction(false) {}

    virtual void traverse(TIntermTraverser *);
    virtual TIntermUnary *getAsUnaryNode() { return this; }
    virtual bool replaceChildNode(
        TIntermNode *original, TIntermNode *replacement);

    virtual bool hasSideEffects() const
    {
        return isAssignment() || mOperand->hasSideEffects();
    }

    void setOperand(TIntermTyped *operand) { mOperand = operand; }
    TIntermTyped *getOperand() { return mOperand; }
    void promote(const TType *funcReturnType);

    void setUseEmulatedFunction() { mUseEmulatedFunction = true; }
    bool getUseEmulatedFunction() { return mUseEmulatedFunction; }

  protected:
    TIntermTyped *mOperand;

    // If set to true, replace the built-in function call with an emulated one
    // to work around driver bugs.
    bool mUseEmulatedFunction;
};

typedef TVector<TIntermNode *> TIntermSequence;
typedef TVector<int> TQualifierList;

//
// Nodes that operate on an arbitrary sized set of children.
//
class TIntermAggregate : public TIntermOperator
{
  public:
    TIntermAggregate()
        : TIntermOperator(EOpNull),
          mUserDefined(false),
          mUseEmulatedFunction(false) { }
    TIntermAggregate(TOperator op)
        : TIntermOperator(op),
          mUseEmulatedFunction(false) { }
    ~TIntermAggregate() { }

    virtual TIntermAggregate *getAsAggregate() { return this; }
    virtual void traverse(TIntermTraverser *);
    virtual bool replaceChildNode(
        TIntermNode *original, TIntermNode *replacement);
    bool replaceChildNodeWithMultiple(TIntermNode *original, TIntermSequence replacements);
    // Conservatively assume function calls and other aggregate operators have side-effects
    virtual bool hasSideEffects() const { return true; }

    TIntermSequence *getSequence() { return &mSequence; }

    void setName(const TString &name) { mName = name; }
    const TString &getName() const { return mName; }

    void setUserDefined() { mUserDefined = true; }
    bool isUserDefined() const { return mUserDefined; }

    void setOptimize(bool optimize) { mOptimize = optimize; }
    bool getOptimize() const { return mOptimize; }
    void setDebug(bool debug) { mDebug = debug; }
    bool getDebug() const { return mDebug; }

    void setFunctionId(int functionId) { mFunctionId = functionId; }
    int getFunctionId() const { return mFunctionId; }

    void setUseEmulatedFunction() { mUseEmulatedFunction = true; }
    bool getUseEmulatedFunction() { return mUseEmulatedFunction; }

    void setPrecisionFromChildren();
    void setBuiltInFunctionPrecision();

  protected:
    TIntermAggregate(const TIntermAggregate &); // disallow copy constructor
    TIntermAggregate &operator=(const TIntermAggregate &); // disallow assignment operator
    TIntermSequence mSequence;
    TString mName;
    bool mUserDefined; // used for user defined function names
    int mFunctionId;

    bool mOptimize;
    bool mDebug;

    // If set to true, replace the built-in function call with an emulated one
    // to work around driver bugs.
    bool mUseEmulatedFunction;
};

//
// For if tests.
//
class TIntermSelection : public TIntermTyped
{
  public:
    TIntermSelection(TIntermTyped *cond, TIntermNode *trueB, TIntermNode *falseB)
        : TIntermTyped(TType(EbtVoid, EbpUndefined)),
          mCondition(cond),
          mTrueBlock(trueB),
          mFalseBlock(falseB) {}
    TIntermSelection(TIntermTyped *cond, TIntermNode *trueB, TIntermNode *falseB,
                     const TType &type)
        : TIntermTyped(type),
          mCondition(cond),
          mTrueBlock(trueB),
          mFalseBlock(falseB) {}

    virtual void traverse(TIntermTraverser *);
    virtual bool replaceChildNode(
        TIntermNode *original, TIntermNode *replacement);

    // Conservatively assume selections have side-effects
    virtual bool hasSideEffects() const { return true; }

    bool usesTernaryOperator() const { return getBasicType() != EbtVoid; }
    TIntermNode *getCondition() const { return mCondition; }
    TIntermNode *getTrueBlock() const { return mTrueBlock; }
    TIntermNode *getFalseBlock() const { return mFalseBlock; }
    TIntermSelection *getAsSelectionNode() { return this; }

protected:
    TIntermTyped *mCondition;
    TIntermNode *mTrueBlock;
    TIntermNode *mFalseBlock;
};

//
// Switch statement.
//
class TIntermSwitch : public TIntermNode
{
  public:
    TIntermSwitch(TIntermTyped *init, TIntermAggregate *statementList)
        : TIntermNode(),
          mInit(init),
          mStatementList(statementList)
    {
    }

    void traverse(TIntermTraverser *it) override;
    bool replaceChildNode(
        TIntermNode *original, TIntermNode *replacement) override;

    TIntermSwitch *getAsSwitchNode() override { return this; }

    TIntermAggregate *getStatementList() { return mStatementList; }
    void setStatementList(TIntermAggregate *statementList) { mStatementList = statementList; }

  protected:
    TIntermTyped *mInit;
    TIntermAggregate *mStatementList;
};

//
// Case label.
//
class TIntermCase : public TIntermNode
{
  public:
    TIntermCase(TIntermTyped *condition)
        : TIntermNode(),
          mCondition(condition)
    {
    }

    void traverse(TIntermTraverser *it) override;
    bool replaceChildNode(
        TIntermNode *original, TIntermNode *replacement) override;

    TIntermCase *getAsCaseNode() override { return this; }

    bool hasCondition() const { return mCondition != nullptr; }
    TIntermTyped *getCondition() const { return mCondition; }

  protected:
    TIntermTyped *mCondition;
};

enum Visit
{
    PreVisit,
    InVisit,
    PostVisit
};

//
// For traversing the tree.  User should derive from this,
// put their traversal specific data in it, and then pass
// it to a Traverse method.
//
// When using this, just fill in the methods for nodes you want visited.
// Return false from a pre-visit to skip visiting that node's subtree.
//
class TIntermTraverser : angle::NonCopyable
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    // TODO(zmo): remove default values.
    TIntermTraverser(bool preVisit = true, bool inVisit = false, bool postVisit = false,
                     bool rightToLeft = false)
        : preVisit(preVisit),
          inVisit(inVisit),
          postVisit(postVisit),
          rightToLeft(rightToLeft),
          mDepth(0),
          mMaxDepth(0) {}
    virtual ~TIntermTraverser() {}

    virtual void visitSymbol(TIntermSymbol *) {}
    virtual void visitRaw(TIntermRaw *) {}
    virtual void visitConstantUnion(TIntermConstantUnion *) {}
    virtual bool visitBinary(Visit, TIntermBinary *) { return true; }
    virtual bool visitUnary(Visit, TIntermUnary *) { return true; }
    virtual bool visitSelection(Visit, TIntermSelection *) { return true; }
    virtual bool visitSwitch(Visit, TIntermSwitch *) { return true; }
    virtual bool visitCase(Visit, TIntermCase *) { return true; }
    virtual bool visitAggregate(Visit, TIntermAggregate *) { return true; }
    virtual bool visitLoop(Visit, TIntermLoop *) { return true; }
    virtual bool visitBranch(Visit, TIntermBranch *) { return true; }

    int getMaxDepth() const { return mMaxDepth; }

    void incrementDepth(TIntermNode *current)
    {
        mDepth++;
        mMaxDepth = std::max(mMaxDepth, mDepth);
        mPath.push_back(current);
    }

    void decrementDepth()
    {
        mDepth--;
        mPath.pop_back();
    }

    TIntermNode *getParentNode()
    {
        return mPath.size() == 0 ? NULL : mPath.back();
    }

    // Return the original name if hash function pointer is NULL;
    // otherwise return the hashed name.
    static TString hash(const TString& name, ShHashFunction64 hashFunction);

    const bool preVisit;
    const bool inVisit;
    const bool postVisit;
    const bool rightToLeft;

    // If traversers need to replace nodes, they can add the replacements in
    // mReplacements/mMultiReplacements during traversal and the user of the traverser should call
    // this function after traversal to perform them.
    void updateTree();

  protected:
    int mDepth;
    int mMaxDepth;

    // All the nodes from root to the current node's parent during traversing.
    TVector<TIntermNode *> mPath;

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
              originalBecomesChildOfReplacement(_originalBecomesChildOfReplacement) {}

        TIntermNode *parent;
        TIntermNode *original;
        TIntermNode *replacement;
        bool originalBecomesChildOfReplacement;
    };

    // To replace a single node with multiple nodes on the parent aggregate node
    struct NodeReplaceWithMultipleEntry
    {
        NodeReplaceWithMultipleEntry(TIntermAggregate *_parent, TIntermNode *_original, TIntermSequence _replacements)
            : parent(_parent),
              original(_original),
              replacements(_replacements)
        {
        }

        TIntermAggregate *parent;
        TIntermNode *original;
        TIntermSequence replacements;
    };

    // During traversing, save all the changes that need to happen into
    // mReplacements/mMultiReplacements, then do them by calling updateTree().
    // Multi replacements are processed after single replacements.
    std::vector<NodeUpdateEntry> mReplacements;
    std::vector<NodeReplaceWithMultipleEntry> mMultiReplacements;
};

//
// For traversing the tree, and computing max depth.
// Takes a maximum depth limit to prevent stack overflow.
//
class TMaxDepthTraverser : public TIntermTraverser
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    TMaxDepthTraverser(int depthLimit)
        : TIntermTraverser(true, true, false, false),
          mDepthLimit(depthLimit) { }

    virtual bool visitBinary(Visit, TIntermBinary *) { return depthCheck(); }
    virtual bool visitUnary(Visit, TIntermUnary *) { return depthCheck(); }
    virtual bool visitSelection(Visit, TIntermSelection *) { return depthCheck(); }
    virtual bool visitAggregate(Visit, TIntermAggregate *) { return depthCheck(); }
    virtual bool visitLoop(Visit, TIntermLoop *) { return depthCheck(); }
    virtual bool visitBranch(Visit, TIntermBranch *) { return depthCheck(); }

  protected:
    bool depthCheck() const { return mMaxDepth < mDepthLimit; }

    int mDepthLimit;
};

#endif  // COMPILER_TRANSLATOR_INTERMNODE_H_
