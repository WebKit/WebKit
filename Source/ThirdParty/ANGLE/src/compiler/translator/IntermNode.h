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
#include "compiler/translator/SymbolUniqueId.h"
#include "compiler/translator/Types.h"

namespace sh
{

class TDiagnostics;

class TIntermTraverser;
class TIntermAggregate;
class TIntermBlock;
class TIntermInvariantDeclaration;
class TIntermDeclaration;
class TIntermFunctionPrototype;
class TIntermFunctionDefinition;
class TIntermSwizzle;
class TIntermBinary;
class TIntermUnary;
class TIntermConstantUnion;
class TIntermTernary;
class TIntermIfElse;
class TIntermSwitch;
class TIntermCase;
class TIntermTyped;
class TIntermSymbol;
class TIntermLoop;
class TInfoSink;
class TInfoSinkBase;
class TIntermRaw;
class TIntermBranch;

class TSymbolTable;
class TFunction;

// Encapsulate an identifier string and track whether it is coming from the original shader code
// (not internal) or from ANGLE (internal). Usually internal names shouldn't be decorated or hashed.
class TName
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    explicit TName(const TString &name) : mName(name), mIsInternal(false) {}
    TName() : mName(), mIsInternal(false) {}
    TName(const TName &) = default;
    TName &operator=(const TName &) = default;

    const TString &getString() const { return mName; }
    void setString(const TString &string) { mName = string; }
    bool isInternal() const { return mIsInternal; }
    void setInternal(bool isInternal) { mIsInternal = isInternal; }

  private:
    TString mName;
    bool mIsInternal;
};

//
// Base class for the tree nodes
//
class TIntermNode : angle::NonCopyable
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
    virtual ~TIntermNode() {}

    const TSourceLoc &getLine() const { return mLine; }
    void setLine(const TSourceLoc &l) { mLine = l; }

    virtual void traverse(TIntermTraverser *) = 0;
    virtual TIntermTyped *getAsTyped() { return 0; }
    virtual TIntermConstantUnion *getAsConstantUnion() { return 0; }
    virtual TIntermFunctionDefinition *getAsFunctionDefinition() { return nullptr; }
    virtual TIntermAggregate *getAsAggregate() { return 0; }
    virtual TIntermBlock *getAsBlock() { return nullptr; }
    virtual TIntermFunctionPrototype *getAsFunctionPrototypeNode() { return nullptr; }
    virtual TIntermInvariantDeclaration *getAsInvariantDeclarationNode() { return nullptr; }
    virtual TIntermDeclaration *getAsDeclarationNode() { return nullptr; }
    virtual TIntermSwizzle *getAsSwizzleNode() { return nullptr; }
    virtual TIntermBinary *getAsBinaryNode() { return 0; }
    virtual TIntermUnary *getAsUnaryNode() { return 0; }
    virtual TIntermTernary *getAsTernaryNode() { return nullptr; }
    virtual TIntermIfElse *getAsIfElseNode() { return nullptr; }
    virtual TIntermSwitch *getAsSwitchNode() { return 0; }
    virtual TIntermCase *getAsCaseNode() { return 0; }
    virtual TIntermSymbol *getAsSymbolNode() { return 0; }
    virtual TIntermLoop *getAsLoopNode() { return 0; }
    virtual TIntermRaw *getAsRawNode() { return 0; }
    virtual TIntermBranch *getAsBranchNode() { return 0; }

    // Replace a child node. Return true if |original| is a child
    // node and it is replaced; otherwise, return false.
    virtual bool replaceChildNode(TIntermNode *original, TIntermNode *replacement) = 0;

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
    TIntermTyped(const TType &t) : mType(t) {}

    virtual TIntermTyped *deepCopy() const = 0;

    TIntermTyped *getAsTyped() override { return this; }

    // True if executing the expression represented by this node affects state, like values of
    // variables. False if the executing the expression only computes its return value without
    // affecting state. May return true conservatively.
    virtual bool hasSideEffects() const = 0;

    void setType(const TType &t) { mType = t; }
    void setTypePreservePrecision(const TType &t);
    const TType &getType() const { return mType; }
    TType *getTypePointer() { return &mType; }

    TBasicType getBasicType() const { return mType.getBasicType(); }
    TQualifier getQualifier() const { return mType.getQualifier(); }
    TPrecision getPrecision() const { return mType.getPrecision(); }
    TMemoryQualifier getMemoryQualifier() const { return mType.getMemoryQualifier(); }
    int getCols() const { return mType.getCols(); }
    int getRows() const { return mType.getRows(); }
    int getNominalSize() const { return mType.getNominalSize(); }
    int getSecondarySize() const { return mType.getSecondarySize(); }

    bool isInterfaceBlock() const { return mType.isInterfaceBlock(); }
    bool isMatrix() const { return mType.isMatrix(); }
    bool isArray() const { return mType.isArray(); }
    bool isVector() const { return mType.isVector(); }
    bool isScalar() const { return mType.isScalar(); }
    bool isScalarInt() const { return mType.isScalarInt(); }
    const char *getBasicString() const { return mType.getBasicString(); }
    TString getCompleteString() const { return mType.getCompleteString(); }

    unsigned int getOutermostArraySize() const { return mType.getOutermostArraySize(); }

    bool isConstructorWithOnlyConstantUnionParameters();

  protected:
    TType mType;

    TIntermTyped(const TIntermTyped &node);
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
                TIntermNode *init,
                TIntermTyped *cond,
                TIntermTyped *expr,
                TIntermBlock *body);

    TIntermLoop *getAsLoopNode() override { return this; }
    void traverse(TIntermTraverser *it) override;
    bool replaceChildNode(TIntermNode *original, TIntermNode *replacement) override;

    TLoopType getType() const { return mType; }
    TIntermNode *getInit() { return mInit; }
    TIntermTyped *getCondition() { return mCond; }
    TIntermTyped *getExpression() { return mExpr; }
    TIntermBlock *getBody() { return mBody; }

    void setInit(TIntermNode *init) { mInit = init; }
    void setCondition(TIntermTyped *condition) { mCond = condition; }
    void setExpression(TIntermTyped *expression) { mExpr = expression; }
    void setBody(TIntermBlock *body) { mBody = body; }

  protected:
    TLoopType mType;
    TIntermNode *mInit;   // for-loop initialization
    TIntermTyped *mCond;  // loop exit condition
    TIntermTyped *mExpr;  // for-loop expression
    TIntermBlock *mBody;  // loop body
};

//
// Handle break, continue, return, and kill.
//
class TIntermBranch : public TIntermNode
{
  public:
    TIntermBranch(TOperator op, TIntermTyped *e) : mFlowOp(op), mExpression(e) {}

    void traverse(TIntermTraverser *it) override;
    TIntermBranch *getAsBranchNode() override { return this; }
    bool replaceChildNode(TIntermNode *original, TIntermNode *replacement) override;

    TOperator getFlowOp() { return mFlowOp; }
    TIntermTyped *getExpression() { return mExpression; }

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
    TIntermSymbol(const TSymbolUniqueId &id, const TString &symbol, const TType &type)
        : TIntermTyped(type), mId(id), mSymbol(symbol)
    {
    }

    TIntermTyped *deepCopy() const override { return new TIntermSymbol(*this); }

    bool hasSideEffects() const override { return false; }

    int getId() const { return mId.get(); }
    const TString &getSymbol() const { return mSymbol.getString(); }
    const TName &getName() const { return mSymbol; }
    TName &getName() { return mSymbol; }

    void setInternal(bool internal) { mSymbol.setInternal(internal); }

    void traverse(TIntermTraverser *it) override;
    TIntermSymbol *getAsSymbolNode() override { return this; }
    bool replaceChildNode(TIntermNode *, TIntermNode *) override { return false; }

  protected:
    const TSymbolUniqueId mId;
    TName mSymbol;

  private:
    TIntermSymbol(const TIntermSymbol &) = default;  // Note: not deleted, just private!
};

// A Raw node stores raw code, that the translator will insert verbatim
// into the output stream. Useful for transformation operations that make
// complex code that might not fit naturally into the GLSL model.
class TIntermRaw : public TIntermTyped
{
  public:
    TIntermRaw(const TType &type, const TString &rawText) : TIntermTyped(type), mRawText(rawText) {}
    TIntermRaw(const TIntermRaw &) = delete;

    TIntermTyped *deepCopy() const override
    {
        UNREACHABLE();
        return nullptr;
    }

    bool hasSideEffects() const override { return false; }

    TString getRawText() const { return mRawText; }

    void traverse(TIntermTraverser *it) override;

    TIntermRaw *getAsRawNode() override { return this; }
    bool replaceChildNode(TIntermNode *, TIntermNode *) override { return false; }

  protected:
    TString mRawText;
};

// Constant folded node.
// Note that nodes may be constant folded and not be constant expressions with the EvqConst
// qualifier. This happens for example when the following expression is processed:
// "true ? 1.0 : non_constant"
// Other nodes than TIntermConstantUnion may also be constant expressions.
//
class TIntermConstantUnion : public TIntermTyped
{
  public:
    TIntermConstantUnion(const TConstantUnion *unionPointer, const TType &type)
        : TIntermTyped(type), mUnionArrayPointer(unionPointer)
    {
        ASSERT(unionPointer);
    }

    TIntermTyped *deepCopy() const override { return new TIntermConstantUnion(*this); }

    bool hasSideEffects() const override { return false; }

    const TConstantUnion *getUnionArrayPointer() const { return mUnionArrayPointer; }

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

    void replaceConstantUnion(const TConstantUnion *safeConstantUnion)
    {
        ASSERT(safeConstantUnion);
        // Previous union pointer freed on pool deallocation.
        mUnionArrayPointer = safeConstantUnion;
    }

    TIntermConstantUnion *getAsConstantUnion() override { return this; }
    void traverse(TIntermTraverser *it) override;
    bool replaceChildNode(TIntermNode *, TIntermNode *) override { return false; }

    TConstantUnion *foldBinary(TOperator op,
                               TIntermConstantUnion *rightNode,
                               TDiagnostics *diagnostics,
                               const TSourceLoc &line);
    const TConstantUnion *foldIndexing(int index);
    TConstantUnion *foldUnaryNonComponentWise(TOperator op);
    TConstantUnion *foldUnaryComponentWise(TOperator op, TDiagnostics *diagnostics);

    static TConstantUnion *FoldAggregateConstructor(TIntermAggregate *aggregate);
    static TConstantUnion *FoldAggregateBuiltIn(TIntermAggregate *aggregate,
                                                TDiagnostics *diagnostics);

  protected:
    // Same data may be shared between multiple constant unions, so it can't be modified.
    const TConstantUnion *mUnionArrayPointer;

  private:
    typedef float (*FloatTypeUnaryFunc)(float);
    void foldFloatTypeUnary(const TConstantUnion &parameter,
                            FloatTypeUnaryFunc builtinFunc,
                            TConstantUnion *result) const;

    TIntermConstantUnion(const TIntermConstantUnion &node);  // Note: not deleted, just private!
};

//
// Intermediate class for node types that hold operators.
//
class TIntermOperator : public TIntermTyped
{
  public:
    TOperator getOp() const { return mOp; }

    bool isAssignment() const;
    bool isMultiplication() const;
    bool isConstructor() const;

    // Returns true for calls mapped to EOpCall*, false for built-ins that have their own specific
    // ops.
    bool isFunctionCall() const;

    bool hasSideEffects() const override { return isAssignment(); }

  protected:
    TIntermOperator(TOperator op) : TIntermTyped(TType(EbtFloat, EbpUndefined)), mOp(op) {}
    TIntermOperator(TOperator op, const TType &type) : TIntermTyped(type), mOp(op) {}

    TIntermOperator(const TIntermOperator &) = default;

    const TOperator mOp;
};

// Node for vector swizzles.
class TIntermSwizzle : public TIntermTyped
{
  public:
    // This constructor determines the type of the node based on the operand.
    TIntermSwizzle(TIntermTyped *operand, const TVector<int> &swizzleOffsets);

    TIntermTyped *deepCopy() const override { return new TIntermSwizzle(*this); }

    TIntermSwizzle *getAsSwizzleNode() override { return this; };
    void traverse(TIntermTraverser *it) override;
    bool replaceChildNode(TIntermNode *original, TIntermNode *replacement) override;

    bool hasSideEffects() const override { return mOperand->hasSideEffects(); }

    TIntermTyped *getOperand() { return mOperand; }
    void writeOffsetsAsXYZW(TInfoSinkBase *out) const;

    bool hasDuplicateOffsets() const;
    bool offsetsMatch(int offset) const;

    TIntermTyped *fold();

  protected:
    TIntermTyped *mOperand;
    TVector<int> mSwizzleOffsets;

  private:
    void promote();

    TIntermSwizzle(const TIntermSwizzle &node);  // Note: not deleted, just private!
};

//
// Nodes for all the basic binary math operators.
//
class TIntermBinary : public TIntermOperator
{
  public:
    // This constructor determines the type of the binary node based on the operands and op.
    TIntermBinary(TOperator op, TIntermTyped *left, TIntermTyped *right);

    TIntermTyped *deepCopy() const override { return new TIntermBinary(*this); }

    static TOperator GetMulOpBasedOnOperands(const TType &left, const TType &right);
    static TOperator GetMulAssignOpBasedOnOperands(const TType &left, const TType &right);
    static TQualifier GetCommaQualifier(int shaderVersion,
                                        const TIntermTyped *left,
                                        const TIntermTyped *right);

    TIntermBinary *getAsBinaryNode() override { return this; };
    void traverse(TIntermTraverser *it) override;
    bool replaceChildNode(TIntermNode *original, TIntermNode *replacement) override;

    bool hasSideEffects() const override
    {
        return isAssignment() || mLeft->hasSideEffects() || mRight->hasSideEffects();
    }

    TIntermTyped *getLeft() const { return mLeft; }
    TIntermTyped *getRight() const { return mRight; }
    TIntermTyped *fold(TDiagnostics *diagnostics);

    void setAddIndexClamp() { mAddIndexClamp = true; }
    bool getAddIndexClamp() { return mAddIndexClamp; }

  protected:
    TIntermTyped *mLeft;
    TIntermTyped *mRight;

    // If set to true, wrap any EOpIndexIndirect with a clamp to bounds.
    bool mAddIndexClamp;

  private:
    void promote();

    TIntermBinary(const TIntermBinary &node);  // Note: not deleted, just private!
};

//
// Nodes for unary math operators.
//
class TIntermUnary : public TIntermOperator
{
  public:
    TIntermUnary(TOperator op, TIntermTyped *operand);

    TIntermTyped *deepCopy() const override { return new TIntermUnary(*this); }

    void traverse(TIntermTraverser *it) override;
    TIntermUnary *getAsUnaryNode() override { return this; }
    bool replaceChildNode(TIntermNode *original, TIntermNode *replacement) override;

    bool hasSideEffects() const override { return isAssignment() || mOperand->hasSideEffects(); }

    TIntermTyped *getOperand() { return mOperand; }
    TIntermTyped *fold(TDiagnostics *diagnostics);

    void setUseEmulatedFunction() { mUseEmulatedFunction = true; }
    bool getUseEmulatedFunction() { return mUseEmulatedFunction; }

  protected:
    TIntermTyped *mOperand;

    // If set to true, replace the built-in function call with an emulated one
    // to work around driver bugs.
    bool mUseEmulatedFunction;

  private:
    void promote();

    TIntermUnary(const TIntermUnary &node);  // note: not deleted, just private!
};

class TFunctionSymbolInfo
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    TFunctionSymbolInfo(const TSymbolUniqueId &id);
    TFunctionSymbolInfo() : mId(nullptr), mKnownToNotHaveSideEffects(false) {}

    TFunctionSymbolInfo(const TFunctionSymbolInfo &info);
    TFunctionSymbolInfo &operator=(const TFunctionSymbolInfo &info);

    void setFromFunction(const TFunction &function);

    void setNameObj(const TName &name) { mName = name; }
    const TName &getNameObj() const { return mName; }

    const TString &getName() const { return mName.getString(); }
    void setName(const TString &name) { mName.setString(name); }
    bool isMain() const { return mName.getString() == "main"; }

    void setKnownToNotHaveSideEffects(bool knownToNotHaveSideEffects)
    {
        mKnownToNotHaveSideEffects = knownToNotHaveSideEffects;
    }
    bool isKnownToNotHaveSideEffects() const { return mKnownToNotHaveSideEffects; }

    void setId(const TSymbolUniqueId &functionId);
    const TSymbolUniqueId &getId() const;

    bool isImageFunction() const
    {
        return getName() == "imageSize" || getName() == "imageLoad" || getName() == "imageStore";
    }

  private:
    TName mName;
    TSymbolUniqueId *mId;
    bool mKnownToNotHaveSideEffects;
};

typedef TVector<TIntermNode *> TIntermSequence;
typedef TVector<int> TQualifierList;

//
// This is just to help yacc.
//
struct TIntermFunctionCallOrMethod
{
    TIntermSequence *arguments;
    TIntermNode *thisNode;
};

// Interface for node classes that have an arbitrarily sized set of children.
class TIntermAggregateBase
{
  public:
    virtual ~TIntermAggregateBase() {}

    virtual TIntermSequence *getSequence()             = 0;
    virtual const TIntermSequence *getSequence() const = 0;

    bool replaceChildNodeWithMultiple(TIntermNode *original, const TIntermSequence &replacements);
    bool insertChildNodes(TIntermSequence::size_type position, const TIntermSequence &insertions);

  protected:
    TIntermAggregateBase() {}

    bool replaceChildNodeInternal(TIntermNode *original, TIntermNode *replacement);
};

//
// Nodes that operate on an arbitrary sized set of children.
//
class TIntermAggregate : public TIntermOperator, public TIntermAggregateBase
{
  public:
    static TIntermAggregate *CreateFunctionCall(const TFunction &func, TIntermSequence *arguments);

    // If using this, ensure that there's a consistent function definition with the same symbol id
    // added to the AST.
    static TIntermAggregate *CreateFunctionCall(const TType &type,
                                                const TSymbolUniqueId &id,
                                                const TName &name,
                                                TIntermSequence *arguments);

    static TIntermAggregate *CreateBuiltInFunctionCall(const TFunction &func,
                                                       TIntermSequence *arguments);
    static TIntermAggregate *CreateConstructor(const TType &type,
                                               TIntermSequence *arguments);
    static TIntermAggregate *Create(const TType &type, TOperator op, TIntermSequence *arguments);
    ~TIntermAggregate() {}

    // Note: only supported for nodes that can be a part of an expression.
    TIntermTyped *deepCopy() const override { return new TIntermAggregate(*this); }

    TIntermAggregate *shallowCopy() const;

    TIntermAggregate *getAsAggregate() override { return this; }
    void traverse(TIntermTraverser *it) override;
    bool replaceChildNode(TIntermNode *original, TIntermNode *replacement) override;

    bool hasSideEffects() const override;

    static bool CanFoldAggregateBuiltInOp(TOperator op);
    TIntermTyped *fold(TDiagnostics *diagnostics);

    TIntermSequence *getSequence() override { return &mArguments; }
    const TIntermSequence *getSequence() const override { return &mArguments; }

    TString getSymbolTableMangledName() const;

    void setUseEmulatedFunction() { mUseEmulatedFunction = true; }
    bool getUseEmulatedFunction() { return mUseEmulatedFunction; }

    // Returns true if changing parameter precision may affect the return value.
    bool gotPrecisionFromChildren() const { return mGotPrecisionFromChildren; }

    TFunctionSymbolInfo *getFunctionSymbolInfo() { return &mFunctionInfo; }
    const TFunctionSymbolInfo *getFunctionSymbolInfo() const { return &mFunctionInfo; }

  protected:
    TIntermSequence mArguments;

    // If set to true, replace the built-in function call with an emulated one
    // to work around driver bugs. Only for calls mapped to ops other than EOpCall*.
    bool mUseEmulatedFunction;

    bool mGotPrecisionFromChildren;

    TFunctionSymbolInfo mFunctionInfo;

  private:
    TIntermAggregate(const TType &type, TOperator op, TIntermSequence *arguments);

    TIntermAggregate(const TIntermAggregate &node);  // note: not deleted, just private!

    void setTypePrecisionAndQualifier(const TType &type);

    bool areChildrenConstQualified();

    void setPrecisionFromChildren();

    void setPrecisionForBuiltInOp();

    // Returns true if precision was set according to special rules for this built-in.
    bool setPrecisionForSpecialBuiltInOp();

    // Used for built-in functions under EOpCallBuiltInFunction. The function name in the symbol
    // info needs to be set before calling this.
    void setBuiltInFunctionPrecision();
};

// A list of statements. Either the root node which contains declarations and function definitions,
// or a block that can be marked with curly braces {}.
class TIntermBlock : public TIntermNode, public TIntermAggregateBase
{
  public:
    TIntermBlock() : TIntermNode() {}
    ~TIntermBlock() {}

    TIntermBlock *getAsBlock() override { return this; }
    void traverse(TIntermTraverser *it) override;
    bool replaceChildNode(TIntermNode *original, TIntermNode *replacement) override;

    // Only intended for initially building the block.
    void appendStatement(TIntermNode *statement);

    TIntermSequence *getSequence() override { return &mStatements; }
    const TIntermSequence *getSequence() const override { return &mStatements; }

  protected:
    TIntermSequence mStatements;
};

// Function prototype. May be in the AST either as a function prototype declaration or as a part of
// a function definition. The type of the node is the function return type.
class TIntermFunctionPrototype : public TIntermTyped, public TIntermAggregateBase
{
  public:
    // TODO(oetuaho@nvidia.com): See if TFunctionSymbolInfo could be added to constructor
    // parameters.
    TIntermFunctionPrototype(const TType &type, const TSymbolUniqueId &id)
        : TIntermTyped(type), mFunctionInfo(id)
    {
    }
    ~TIntermFunctionPrototype() {}

    TIntermFunctionPrototype *getAsFunctionPrototypeNode() override { return this; }
    void traverse(TIntermTraverser *it) override;
    bool replaceChildNode(TIntermNode *original, TIntermNode *replacement) override;

    TIntermTyped *deepCopy() const override
    {
        UNREACHABLE();
        return nullptr;
    }
    bool hasSideEffects() const override
    {
        UNREACHABLE();
        return true;
    }

    // Only intended for initially building the declaration.
    void appendParameter(TIntermSymbol *parameter);

    TIntermSequence *getSequence() override { return &mParameters; }
    const TIntermSequence *getSequence() const override { return &mParameters; }

    TFunctionSymbolInfo *getFunctionSymbolInfo() { return &mFunctionInfo; }
    const TFunctionSymbolInfo *getFunctionSymbolInfo() const { return &mFunctionInfo; }

  protected:
    TIntermSequence mParameters;

    TFunctionSymbolInfo mFunctionInfo;
};

// Node for function definitions. The prototype child node stores the function header including
// parameters, and the body child node stores the function body.
class TIntermFunctionDefinition : public TIntermNode
{
  public:
    TIntermFunctionDefinition(TIntermFunctionPrototype *prototype, TIntermBlock *body)
        : TIntermNode(), mPrototype(prototype), mBody(body)
    {
        ASSERT(prototype != nullptr);
        ASSERT(body != nullptr);
    }

    TIntermFunctionDefinition *getAsFunctionDefinition() override { return this; }
    void traverse(TIntermTraverser *it) override;
    bool replaceChildNode(TIntermNode *original, TIntermNode *replacement) override;

    TIntermFunctionPrototype *getFunctionPrototype() const { return mPrototype; }
    TIntermBlock *getBody() const { return mBody; }

    const TFunctionSymbolInfo *getFunctionSymbolInfo() const
    {
        return mPrototype->getFunctionSymbolInfo();
    }

  private:
    TIntermFunctionPrototype *mPrototype;
    TIntermBlock *mBody;
};

// Struct, interface block or variable declaration. Can contain multiple variable declarators.
class TIntermDeclaration : public TIntermNode, public TIntermAggregateBase
{
  public:
    TIntermDeclaration() : TIntermNode() {}
    ~TIntermDeclaration() {}

    TIntermDeclaration *getAsDeclarationNode() override { return this; }
    void traverse(TIntermTraverser *it) override;
    bool replaceChildNode(TIntermNode *original, TIntermNode *replacement) override;

    // Only intended for initially building the declaration.
    // The declarator node should be either TIntermSymbol or TIntermBinary with op set to
    // EOpInitialize.
    void appendDeclarator(TIntermTyped *declarator);

    TIntermSequence *getSequence() override { return &mDeclarators; }
    const TIntermSequence *getSequence() const override { return &mDeclarators; }
  protected:
    TIntermSequence mDeclarators;
};

// Specialized declarations for attributing invariance.
class TIntermInvariantDeclaration : public TIntermNode
{
  public:
    TIntermInvariantDeclaration(TIntermSymbol *symbol, const TSourceLoc &line);

    virtual TIntermInvariantDeclaration *getAsInvariantDeclarationNode() override { return this; }

    TIntermSymbol *getSymbol() { return mSymbol; }

    void traverse(TIntermTraverser *it) override;
    bool replaceChildNode(TIntermNode *original, TIntermNode *replacement) override;

  private:
    TIntermSymbol *mSymbol;
};

// For ternary operators like a ? b : c.
class TIntermTernary : public TIntermTyped
{
  public:
    TIntermTernary(TIntermTyped *cond, TIntermTyped *trueExpression, TIntermTyped *falseExpression);

    void traverse(TIntermTraverser *it) override;
    bool replaceChildNode(TIntermNode *original, TIntermNode *replacement) override;

    TIntermTyped *getCondition() const { return mCondition; }
    TIntermTyped *getTrueExpression() const { return mTrueExpression; }
    TIntermTyped *getFalseExpression() const { return mFalseExpression; }
    TIntermTernary *getAsTernaryNode() override { return this; }

    TIntermTyped *deepCopy() const override { return new TIntermTernary(*this); }

    bool hasSideEffects() const override
    {
        return mCondition->hasSideEffects() || mTrueExpression->hasSideEffects() ||
               mFalseExpression->hasSideEffects();
    }

    TIntermTyped *fold();

  private:
    TIntermTernary(const TIntermTernary &node);  // Note: not deleted, just private!

    static TQualifier DetermineQualifier(TIntermTyped *cond,
                                         TIntermTyped *trueExpression,
                                         TIntermTyped *falseExpression);

    TIntermTyped *mCondition;
    TIntermTyped *mTrueExpression;
    TIntermTyped *mFalseExpression;
};

class TIntermIfElse : public TIntermNode
{
  public:
    TIntermIfElse(TIntermTyped *cond, TIntermBlock *trueB, TIntermBlock *falseB);

    void traverse(TIntermTraverser *it) override;
    bool replaceChildNode(TIntermNode *original, TIntermNode *replacement) override;

    TIntermTyped *getCondition() const { return mCondition; }
    TIntermBlock *getTrueBlock() const { return mTrueBlock; }
    TIntermBlock *getFalseBlock() const { return mFalseBlock; }
    TIntermIfElse *getAsIfElseNode() override { return this; }

  protected:
    TIntermTyped *mCondition;
    TIntermBlock *mTrueBlock;
    TIntermBlock *mFalseBlock;
};

//
// Switch statement.
//
class TIntermSwitch : public TIntermNode
{
  public:
    TIntermSwitch(TIntermTyped *init, TIntermBlock *statementList);

    void traverse(TIntermTraverser *it) override;
    bool replaceChildNode(TIntermNode *original, TIntermNode *replacement) override;

    TIntermSwitch *getAsSwitchNode() override { return this; }

    TIntermTyped *getInit() { return mInit; }
    TIntermBlock *getStatementList() { return mStatementList; }

    // Must be called with a non-null statementList.
    void setStatementList(TIntermBlock *statementList);

  protected:
    TIntermTyped *mInit;
    TIntermBlock *mStatementList;
};

//
// Case label.
//
class TIntermCase : public TIntermNode
{
  public:
    TIntermCase(TIntermTyped *condition) : TIntermNode(), mCondition(condition) {}

    void traverse(TIntermTraverser *it) override;
    bool replaceChildNode(TIntermNode *original, TIntermNode *replacement) override;

    TIntermCase *getAsCaseNode() override { return this; }

    bool hasCondition() const { return mCondition != nullptr; }
    TIntermTyped *getCondition() const { return mCondition; }

  protected:
    TIntermTyped *mCondition;
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_INTERMNODE_H_
