//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/IntermTraverse.h"
#include "compiler/translator/SymbolTable.h"

namespace sh
{

namespace
{

void OutputFunction(TInfoSinkBase &out, const char *str, TFunctionSymbolInfo *info)
{
    const char *internal = info->getNameObj().isInternal() ? " (internal function)" : "";
    out << str << internal << ": " << info->getNameObj().getString() << " (symbol id "
        << info->getId().get() << ")";
}

// Two purposes:
// 1.  Show an example of how to iterate tree.  Functions can also directly call traverse() on
//     children themselves to have finer grained control over the process than shown here, though
//     that's not recommended if it can be avoided.
// 2.  Print out a text based description of the tree.

// The traverser subclass is used to carry along data from node to node in the traversal.
class TOutputTraverser : public TIntermTraverser
{
  public:
    TOutputTraverser(TInfoSinkBase &out) : TIntermTraverser(true, false, false), mOut(out) {}

  protected:
    void visitSymbol(TIntermSymbol *) override;
    void visitConstantUnion(TIntermConstantUnion *) override;
    bool visitSwizzle(Visit visit, TIntermSwizzle *node) override;
    bool visitBinary(Visit visit, TIntermBinary *) override;
    bool visitUnary(Visit visit, TIntermUnary *) override;
    bool visitTernary(Visit visit, TIntermTernary *node) override;
    bool visitIfElse(Visit visit, TIntermIfElse *node) override;
    bool visitSwitch(Visit visit, TIntermSwitch *node) override;
    bool visitCase(Visit visit, TIntermCase *node) override;
    bool visitFunctionPrototype(Visit visit, TIntermFunctionPrototype *node) override;
    bool visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *node) override;
    bool visitAggregate(Visit visit, TIntermAggregate *) override;
    bool visitBlock(Visit visit, TIntermBlock *) override;
    bool visitInvariantDeclaration(Visit visit, TIntermInvariantDeclaration *node) override;
    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override;
    bool visitLoop(Visit visit, TIntermLoop *) override;
    bool visitBranch(Visit visit, TIntermBranch *) override;

    TInfoSinkBase &mOut;
};

//
// Helper functions for printing, not part of traversing.
//
void OutputTreeText(TInfoSinkBase &out, TIntermNode *node, const int depth)
{
    int i;

    out.location(node->getLine().first_file, node->getLine().first_line);

    for (i = 0; i < depth; ++i)
        out << "  ";
}

//
// The rest of the file are the traversal functions.  The last one
// is the one that starts the traversal.
//
// Return true from interior nodes to have the external traversal
// continue on to children.  If you process children yourself,
// return false.
//

void TOutputTraverser::visitSymbol(TIntermSymbol *node)
{
    OutputTreeText(mOut, node, mDepth);

    mOut << "'" << node->getSymbol() << "' ";
    mOut << "(symbol id " << node->getId() << ") ";
    mOut << "(" << node->getCompleteString() << ")";
    mOut << "\n";
}

bool TOutputTraverser::visitSwizzle(Visit visit, TIntermSwizzle *node)
{
    OutputTreeText(mOut, node, mDepth);
    mOut << "vector swizzle (";
    node->writeOffsetsAsXYZW(&mOut);
    mOut << ")";

    mOut << " (" << node->getCompleteString() << ")";
    mOut << "\n";
    return true;
}

bool TOutputTraverser::visitBinary(Visit visit, TIntermBinary *node)
{
    OutputTreeText(mOut, node, mDepth);

    switch (node->getOp())
    {
        case EOpComma:
            mOut << "comma";
            break;
        case EOpAssign:
            mOut << "move second child to first child";
            break;
        case EOpInitialize:
            mOut << "initialize first child with second child";
            break;
        case EOpAddAssign:
            mOut << "add second child into first child";
            break;
        case EOpSubAssign:
            mOut << "subtract second child into first child";
            break;
        case EOpMulAssign:
            mOut << "multiply second child into first child";
            break;
        case EOpVectorTimesMatrixAssign:
            mOut << "matrix mult second child into first child";
            break;
        case EOpVectorTimesScalarAssign:
            mOut << "vector scale second child into first child";
            break;
        case EOpMatrixTimesScalarAssign:
            mOut << "matrix scale second child into first child";
            break;
        case EOpMatrixTimesMatrixAssign:
            mOut << "matrix mult second child into first child";
            break;
        case EOpDivAssign:
            mOut << "divide second child into first child";
            break;
        case EOpIModAssign:
            mOut << "modulo second child into first child";
            break;
        case EOpBitShiftLeftAssign:
            mOut << "bit-wise shift first child left by second child";
            break;
        case EOpBitShiftRightAssign:
            mOut << "bit-wise shift first child right by second child";
            break;
        case EOpBitwiseAndAssign:
            mOut << "bit-wise and second child into first child";
            break;
        case EOpBitwiseXorAssign:
            mOut << "bit-wise xor second child into first child";
            break;
        case EOpBitwiseOrAssign:
            mOut << "bit-wise or second child into first child";
            break;

        case EOpIndexDirect:
            mOut << "direct index";
            break;
        case EOpIndexIndirect:
            mOut << "indirect index";
            break;
        case EOpIndexDirectStruct:
            mOut << "direct index for structure";
            break;
        case EOpIndexDirectInterfaceBlock:
            mOut << "direct index for interface block";
            break;

        case EOpAdd:
            mOut << "add";
            break;
        case EOpSub:
            mOut << "subtract";
            break;
        case EOpMul:
            mOut << "component-wise multiply";
            break;
        case EOpDiv:
            mOut << "divide";
            break;
        case EOpIMod:
            mOut << "modulo";
            break;
        case EOpBitShiftLeft:
            mOut << "bit-wise shift left";
            break;
        case EOpBitShiftRight:
            mOut << "bit-wise shift right";
            break;
        case EOpBitwiseAnd:
            mOut << "bit-wise and";
            break;
        case EOpBitwiseXor:
            mOut << "bit-wise xor";
            break;
        case EOpBitwiseOr:
            mOut << "bit-wise or";
            break;

        case EOpEqual:
            mOut << "Compare Equal";
            break;
        case EOpNotEqual:
            mOut << "Compare Not Equal";
            break;
        case EOpLessThan:
            mOut << "Compare Less Than";
            break;
        case EOpGreaterThan:
            mOut << "Compare Greater Than";
            break;
        case EOpLessThanEqual:
            mOut << "Compare Less Than or Equal";
            break;
        case EOpGreaterThanEqual:
            mOut << "Compare Greater Than or Equal";
            break;

        case EOpVectorTimesScalar:
            mOut << "vector-scale";
            break;
        case EOpVectorTimesMatrix:
            mOut << "vector-times-matrix";
            break;
        case EOpMatrixTimesVector:
            mOut << "matrix-times-vector";
            break;
        case EOpMatrixTimesScalar:
            mOut << "matrix-scale";
            break;
        case EOpMatrixTimesMatrix:
            mOut << "matrix-multiply";
            break;

        case EOpLogicalOr:
            mOut << "logical-or";
            break;
        case EOpLogicalXor:
            mOut << "logical-xor";
            break;
        case EOpLogicalAnd:
            mOut << "logical-and";
            break;
        default:
            mOut << "<unknown op>";
    }

    mOut << " (" << node->getCompleteString() << ")";

    mOut << "\n";

    // Special handling for direct indexes. Because constant
    // unions are not aware they are struct indexes, treat them
    // here where we have that contextual knowledge.
    if (node->getOp() == EOpIndexDirectStruct || node->getOp() == EOpIndexDirectInterfaceBlock)
    {
        node->getLeft()->traverse(this);

        TIntermConstantUnion *intermConstantUnion = node->getRight()->getAsConstantUnion();
        ASSERT(intermConstantUnion);

        OutputTreeText(mOut, intermConstantUnion, mDepth + 1);

        // The following code finds the field name from the constant union
        const TConstantUnion *constantUnion   = intermConstantUnion->getUnionArrayPointer();
        const TStructure *structure           = node->getLeft()->getType().getStruct();
        const TInterfaceBlock *interfaceBlock = node->getLeft()->getType().getInterfaceBlock();
        ASSERT(structure || interfaceBlock);

        const TFieldList &fields = structure ? structure->fields() : interfaceBlock->fields();

        const TField *field = fields[constantUnion->getIConst()];

        mOut << constantUnion->getIConst() << " (field '" << field->name() << "')";

        mOut << "\n";

        return false;
    }

    return true;
}

bool TOutputTraverser::visitUnary(Visit visit, TIntermUnary *node)
{
    OutputTreeText(mOut, node, mDepth);

    switch (node->getOp())
    {
        // Give verbose names for ops that have special syntax and some built-in functions that are
        // easy to confuse with others, but mostly use GLSL names for functions.
        case EOpNegative:
            mOut << "Negate value";
            break;
        case EOpPositive:
            mOut << "Positive sign";
            break;
        case EOpLogicalNot:
            mOut << "negation";
            break;
        case EOpBitwiseNot:
            mOut << "bit-wise not";
            break;

        case EOpPostIncrement:
            mOut << "Post-Increment";
            break;
        case EOpPostDecrement:
            mOut << "Post-Decrement";
            break;
        case EOpPreIncrement:
            mOut << "Pre-Increment";
            break;
        case EOpPreDecrement:
            mOut << "Pre-Decrement";
            break;

        case EOpArrayLength:
            mOut << "Array length";
            break;

        case EOpLogicalNotComponentWise:
            mOut << "component-wise not";
            break;

        default:
            mOut << GetOperatorString(node->getOp());
            break;
    }

    mOut << " (" << node->getCompleteString() << ")";

    mOut << "\n";

    return true;
}

bool TOutputTraverser::visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *node)
{
    OutputTreeText(mOut, node, mDepth);
    mOut << "Function Definition:\n";
    mOut << "\n";
    return true;
}

bool TOutputTraverser::visitInvariantDeclaration(Visit visit, TIntermInvariantDeclaration *node)
{
    OutputTreeText(mOut, node, mDepth);
    mOut << "Invariant Declaration:\n";
    return true;
}

bool TOutputTraverser::visitFunctionPrototype(Visit visit, TIntermFunctionPrototype *node)
{
    OutputTreeText(mOut, node, mDepth);
    OutputFunction(mOut, "Function Prototype", node->getFunctionSymbolInfo());
    mOut << " (" << node->getCompleteString() << ")";
    mOut << "\n";

    return true;
}

bool TOutputTraverser::visitAggregate(Visit visit, TIntermAggregate *node)
{
    OutputTreeText(mOut, node, mDepth);

    if (node->getOp() == EOpNull)
    {
        mOut.prefix(SH_ERROR);
        mOut << "node is still EOpNull!\n";
        return true;
    }

    // Give verbose names for some built-in functions that are easy to confuse with others, but
    // mostly use GLSL names for functions.
    switch (node->getOp())
    {
        case EOpCallFunctionInAST:
            OutputFunction(mOut, "Call an user-defined function", node->getFunctionSymbolInfo());
            break;
        case EOpCallInternalRawFunction:
            OutputFunction(mOut, "Call an internal function with raw implementation",
                           node->getFunctionSymbolInfo());
            break;
        case EOpCallBuiltInFunction:
            OutputFunction(mOut, "Call a built-in function", node->getFunctionSymbolInfo());
            break;

        case EOpConstruct:
            // The type of the constructor will be printed below.
            mOut << "Construct";
            break;

        case EOpEqualComponentWise:
            mOut << "component-wise equal";
            break;
        case EOpNotEqualComponentWise:
            mOut << "component-wise not equal";
            break;
        case EOpLessThanComponentWise:
            mOut << "component-wise less than";
            break;
        case EOpGreaterThanComponentWise:
            mOut << "component-wise greater than";
            break;
        case EOpLessThanEqualComponentWise:
            mOut << "component-wise less than or equal";
            break;
        case EOpGreaterThanEqualComponentWise:
            mOut << "component-wise greater than or equal";
            break;

        case EOpDot:
            mOut << "dot product";
            break;
        case EOpCross:
            mOut << "cross product";
            break;
        case EOpMulMatrixComponentWise:
            mOut << "component-wise multiply";
            break;

        default:
            mOut << GetOperatorString(node->getOp());
            break;
    }

    mOut << " (" << node->getCompleteString() << ")";

    mOut << "\n";

    return true;
}

bool TOutputTraverser::visitBlock(Visit visit, TIntermBlock *node)
{
    OutputTreeText(mOut, node, mDepth);
    mOut << "Code block\n";

    return true;
}

bool TOutputTraverser::visitDeclaration(Visit visit, TIntermDeclaration *node)
{
    OutputTreeText(mOut, node, mDepth);
    mOut << "Declaration\n";

    return true;
}

bool TOutputTraverser::visitTernary(Visit visit, TIntermTernary *node)
{
    OutputTreeText(mOut, node, mDepth);

    mOut << "Ternary selection";
    mOut << " (" << node->getCompleteString() << ")\n";

    ++mDepth;

    OutputTreeText(mOut, node, mDepth);
    mOut << "Condition\n";
    node->getCondition()->traverse(this);

    OutputTreeText(mOut, node, mDepth);
    if (node->getTrueExpression())
    {
        mOut << "true case\n";
        node->getTrueExpression()->traverse(this);
    }
    if (node->getFalseExpression())
    {
        OutputTreeText(mOut, node, mDepth);
        mOut << "false case\n";
        node->getFalseExpression()->traverse(this);
    }

    --mDepth;

    return false;
}

bool TOutputTraverser::visitIfElse(Visit visit, TIntermIfElse *node)
{
    OutputTreeText(mOut, node, mDepth);

    mOut << "If test\n";

    ++mDepth;

    OutputTreeText(mOut, node, mDepth);
    mOut << "Condition\n";
    node->getCondition()->traverse(this);

    OutputTreeText(mOut, node, mDepth);
    if (node->getTrueBlock())
    {
        mOut << "true case\n";
        node->getTrueBlock()->traverse(this);
    }
    else
    {
        mOut << "true case is null\n";
    }

    if (node->getFalseBlock())
    {
        OutputTreeText(mOut, node, mDepth);
        mOut << "false case\n";
        node->getFalseBlock()->traverse(this);
    }

    --mDepth;

    return false;
}

bool TOutputTraverser::visitSwitch(Visit visit, TIntermSwitch *node)
{
    OutputTreeText(mOut, node, mDepth);

    mOut << "Switch\n";

    return true;
}

bool TOutputTraverser::visitCase(Visit visit, TIntermCase *node)
{
    OutputTreeText(mOut, node, mDepth);

    if (node->getCondition() == nullptr)
    {
        mOut << "Default\n";
    }
    else
    {
        mOut << "Case\n";
    }

    return true;
}

void TOutputTraverser::visitConstantUnion(TIntermConstantUnion *node)
{
    size_t size = node->getType().getObjectSize();

    for (size_t i = 0; i < size; i++)
    {
        OutputTreeText(mOut, node, mDepth);
        switch (node->getUnionArrayPointer()[i].getType())
        {
            case EbtBool:
                if (node->getUnionArrayPointer()[i].getBConst())
                    mOut << "true";
                else
                    mOut << "false";

                mOut << " ("
                     << "const bool"
                     << ")";
                mOut << "\n";
                break;
            case EbtFloat:
                mOut << node->getUnionArrayPointer()[i].getFConst();
                mOut << " (const float)\n";
                break;
            case EbtInt:
                mOut << node->getUnionArrayPointer()[i].getIConst();
                mOut << " (const int)\n";
                break;
            case EbtUInt:
                mOut << node->getUnionArrayPointer()[i].getUConst();
                mOut << " (const uint)\n";
                break;
            case EbtYuvCscStandardEXT:
                mOut << getYuvCscStandardEXTString(
                    node->getUnionArrayPointer()[i].getYuvCscStandardEXTConst());
                mOut << " (const yuvCscStandardEXT)\n";
                break;
            default:
                mOut.prefix(SH_ERROR);
                mOut << "Unknown constant\n";
                break;
        }
    }
}

bool TOutputTraverser::visitLoop(Visit visit, TIntermLoop *node)
{
    OutputTreeText(mOut, node, mDepth);

    mOut << "Loop with condition ";
    if (node->getType() == ELoopDoWhile)
        mOut << "not ";
    mOut << "tested first\n";

    ++mDepth;

    OutputTreeText(mOut, node, mDepth);
    if (node->getCondition())
    {
        mOut << "Loop Condition\n";
        node->getCondition()->traverse(this);
    }
    else
    {
        mOut << "No loop condition\n";
    }

    OutputTreeText(mOut, node, mDepth);
    if (node->getBody())
    {
        mOut << "Loop Body\n";
        node->getBody()->traverse(this);
    }
    else
    {
        mOut << "No loop body\n";
    }

    if (node->getExpression())
    {
        OutputTreeText(mOut, node, mDepth);
        mOut << "Loop Terminal Expression\n";
        node->getExpression()->traverse(this);
    }

    --mDepth;

    return false;
}

bool TOutputTraverser::visitBranch(Visit visit, TIntermBranch *node)
{
    OutputTreeText(mOut, node, mDepth);

    switch (node->getFlowOp())
    {
        case EOpKill:
            mOut << "Branch: Kill";
            break;
        case EOpBreak:
            mOut << "Branch: Break";
            break;
        case EOpContinue:
            mOut << "Branch: Continue";
            break;
        case EOpReturn:
            mOut << "Branch: Return";
            break;
        default:
            mOut << "Branch: Unknown Branch";
            break;
    }

    if (node->getExpression())
    {
        mOut << " with expression\n";
        ++mDepth;
        node->getExpression()->traverse(this);
        --mDepth;
    }
    else
    {
        mOut << "\n";
    }

    return false;
}

}  // anonymous namespace

void OutputTree(TIntermNode *root, TInfoSinkBase &out)
{
    TOutputTraverser it(out);
    ASSERT(root);
    root->traverse(&it);
}

}  // namespace sh
