//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/Intermediate.h"
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

//
// Two purposes:
// 1.  Show an example of how to iterate tree.  Functions can
//     also directly call Traverse() on children themselves to
//     have finer grained control over the process than shown here.
//     See the last function for how to get started.
// 2.  Print out a text based description of the tree.
//

//
// Use this class to carry along data from node to node in
// the traversal
//
class TOutputTraverser : public TIntermTraverser
{
  public:
    TOutputTraverser(TInfoSinkBase &i) : TIntermTraverser(true, false, false), sink(i) {}
    TInfoSinkBase &sink;

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
};

//
// Helper functions for printing, not part of traversing.
//
void OutputTreeText(TInfoSinkBase &sink, TIntermNode *node, const int depth)
{
    int i;

    sink.location(node->getLine().first_file, node->getLine().first_line);

    for (i = 0; i < depth; ++i)
        sink << "  ";
}

}  // namespace anonymous

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
    OutputTreeText(sink, node, mDepth);

    sink << "'" << node->getSymbol() << "' ";
    sink << "(" << node->getCompleteString() << ")\n";
}

bool TOutputTraverser::visitSwizzle(Visit visit, TIntermSwizzle *node)
{
    TInfoSinkBase &out = sink;
    OutputTreeText(out, node, mDepth);
    out << "vector swizzle (";
    node->writeOffsetsAsXYZW(&out);
    out << ")";

    out << " (" << node->getCompleteString() << ")";
    out << "\n";
    return true;
}

bool TOutputTraverser::visitBinary(Visit visit, TIntermBinary *node)
{
    TInfoSinkBase &out = sink;

    OutputTreeText(out, node, mDepth);

    switch (node->getOp())
    {
        case EOpComma:
            out << "comma";
            break;
        case EOpAssign:
            out << "move second child to first child";
            break;
        case EOpInitialize:
            out << "initialize first child with second child";
            break;
        case EOpAddAssign:
            out << "add second child into first child";
            break;
        case EOpSubAssign:
            out << "subtract second child into first child";
            break;
        case EOpMulAssign:
            out << "multiply second child into first child";
            break;
        case EOpVectorTimesMatrixAssign:
            out << "matrix mult second child into first child";
            break;
        case EOpVectorTimesScalarAssign:
            out << "vector scale second child into first child";
            break;
        case EOpMatrixTimesScalarAssign:
            out << "matrix scale second child into first child";
            break;
        case EOpMatrixTimesMatrixAssign:
            out << "matrix mult second child into first child";
            break;
        case EOpDivAssign:
            out << "divide second child into first child";
            break;
        case EOpIModAssign:
            out << "modulo second child into first child";
            break;
        case EOpBitShiftLeftAssign:
            out << "bit-wise shift first child left by second child";
            break;
        case EOpBitShiftRightAssign:
            out << "bit-wise shift first child right by second child";
            break;
        case EOpBitwiseAndAssign:
            out << "bit-wise and second child into first child";
            break;
        case EOpBitwiseXorAssign:
            out << "bit-wise xor second child into first child";
            break;
        case EOpBitwiseOrAssign:
            out << "bit-wise or second child into first child";
            break;

        case EOpIndexDirect:
            out << "direct index";
            break;
        case EOpIndexIndirect:
            out << "indirect index";
            break;
        case EOpIndexDirectStruct:
            out << "direct index for structure";
            break;
        case EOpIndexDirectInterfaceBlock:
            out << "direct index for interface block";
            break;

        case EOpAdd:
            out << "add";
            break;
        case EOpSub:
            out << "subtract";
            break;
        case EOpMul:
            out << "component-wise multiply";
            break;
        case EOpDiv:
            out << "divide";
            break;
        case EOpIMod:
            out << "modulo";
            break;
        case EOpBitShiftLeft:
            out << "bit-wise shift left";
            break;
        case EOpBitShiftRight:
            out << "bit-wise shift right";
            break;
        case EOpBitwiseAnd:
            out << "bit-wise and";
            break;
        case EOpBitwiseXor:
            out << "bit-wise xor";
            break;
        case EOpBitwiseOr:
            out << "bit-wise or";
            break;

        case EOpEqual:
            out << "Compare Equal";
            break;
        case EOpNotEqual:
            out << "Compare Not Equal";
            break;
        case EOpLessThan:
            out << "Compare Less Than";
            break;
        case EOpGreaterThan:
            out << "Compare Greater Than";
            break;
        case EOpLessThanEqual:
            out << "Compare Less Than or Equal";
            break;
        case EOpGreaterThanEqual:
            out << "Compare Greater Than or Equal";
            break;

        case EOpVectorTimesScalar:
            out << "vector-scale";
            break;
        case EOpVectorTimesMatrix:
            out << "vector-times-matrix";
            break;
        case EOpMatrixTimesVector:
            out << "matrix-times-vector";
            break;
        case EOpMatrixTimesScalar:
            out << "matrix-scale";
            break;
        case EOpMatrixTimesMatrix:
            out << "matrix-multiply";
            break;

        case EOpLogicalOr:
            out << "logical-or";
            break;
        case EOpLogicalXor:
            out << "logical-xor";
            break;
        case EOpLogicalAnd:
            out << "logical-and";
            break;
        default:
            out << "<unknown op>";
    }

    out << " (" << node->getCompleteString() << ")";

    out << "\n";

    // Special handling for direct indexes. Because constant
    // unions are not aware they are struct indexes, treat them
    // here where we have that contextual knowledge.
    if (node->getOp() == EOpIndexDirectStruct || node->getOp() == EOpIndexDirectInterfaceBlock)
    {
        mDepth++;
        node->getLeft()->traverse(this);
        mDepth--;

        TIntermConstantUnion *intermConstantUnion = node->getRight()->getAsConstantUnion();
        ASSERT(intermConstantUnion);

        OutputTreeText(out, intermConstantUnion, mDepth + 1);

        // The following code finds the field name from the constant union
        const TConstantUnion *constantUnion   = intermConstantUnion->getUnionArrayPointer();
        const TStructure *structure           = node->getLeft()->getType().getStruct();
        const TInterfaceBlock *interfaceBlock = node->getLeft()->getType().getInterfaceBlock();
        ASSERT(structure || interfaceBlock);

        const TFieldList &fields = structure ? structure->fields() : interfaceBlock->fields();

        const TField *field = fields[constantUnion->getIConst()];

        out << constantUnion->getIConst() << " (field '" << field->name() << "')";

        return false;
    }

    return true;
}

bool TOutputTraverser::visitUnary(Visit visit, TIntermUnary *node)
{
    TInfoSinkBase &out = sink;

    OutputTreeText(out, node, mDepth);

    switch (node->getOp())
    {
        // Give verbose names for ops that have special syntax and some built-in functions that are
        // easy to confuse with others, but mostly use GLSL names for functions.
        case EOpNegative:
            out << "Negate value";
            break;
        case EOpPositive:
            out << "Positive sign";
            break;
        case EOpLogicalNot:
            out << "negation";
            break;
        case EOpBitwiseNot:
            out << "bit-wise not";
            break;

        case EOpPostIncrement:
            out << "Post-Increment";
            break;
        case EOpPostDecrement:
            out << "Post-Decrement";
            break;
        case EOpPreIncrement:
            out << "Pre-Increment";
            break;
        case EOpPreDecrement:
            out << "Pre-Decrement";
            break;

        case EOpLogicalNotComponentWise:
            out << "component-wise not";
            break;

        default:
            out << GetOperatorString(node->getOp());
            break;
    }

    out << " (" << node->getCompleteString() << ")";

    out << "\n";

    return true;
}

bool TOutputTraverser::visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *node)
{
    TInfoSinkBase &out = sink;
    OutputTreeText(out, node, mDepth);
    out << "Function Definition:\n";
    out << "\n";
    return true;
}

bool TOutputTraverser::visitInvariantDeclaration(Visit visit, TIntermInvariantDeclaration *node)
{
    TInfoSinkBase &out = sink;
    OutputTreeText(out, node, mDepth);
    out << "Invariant Declaration:\n";
    return true;
}

bool TOutputTraverser::visitFunctionPrototype(Visit visit, TIntermFunctionPrototype *node)
{
    TInfoSinkBase &out = sink;

    OutputTreeText(out, node, mDepth);
    OutputFunction(out, "Function Prototype", node->getFunctionSymbolInfo());
    out << " (" << node->getCompleteString() << ")";
    out << "\n";

    return true;
}

bool TOutputTraverser::visitAggregate(Visit visit, TIntermAggregate *node)
{
    TInfoSinkBase &out = sink;

    OutputTreeText(out, node, mDepth);

    if (node->getOp() == EOpNull)
    {
        out.prefix(SH_ERROR);
        out << "node is still EOpNull!\n";
        return true;
    }

    if (node->isConstructor())
    {
        if (node->getOp() == EOpConstructStruct)
        {
            out << "Construct structure";
        }
        else
        {
            out << "Construct " << GetOperatorString(node->getOp());
        }
    }
    else
    {
        // Give verbose names for some built-in functions that are easy to confuse with others, but
        // mostly use GLSL names for functions.
        switch (node->getOp())
        {
            case EOpCallFunctionInAST:
                OutputFunction(out, "Call an user-defined function", node->getFunctionSymbolInfo());
                break;
            case EOpCallInternalRawFunction:
                OutputFunction(out, "Call an internal function with raw implementation",
                               node->getFunctionSymbolInfo());
                break;
            case EOpCallBuiltInFunction:
                OutputFunction(out, "Call a built-in function", node->getFunctionSymbolInfo());
                break;

            case EOpEqualComponentWise:
                out << "component-wise equal";
                break;
            case EOpNotEqualComponentWise:
                out << "component-wise not equal";
                break;
            case EOpLessThanComponentWise:
                out << "component-wise less than";
                break;
            case EOpGreaterThanComponentWise:
                out << "component-wise greater than";
                break;
            case EOpLessThanEqualComponentWise:
                out << "component-wise less than or equal";
                break;
            case EOpGreaterThanEqualComponentWise:
                out << "component-wise greater than or equal";
                break;

            case EOpDot:
                out << "dot product";
                break;
            case EOpCross:
                out << "cross product";
                break;
            case EOpMulMatrixComponentWise:
                out << "component-wise multiply";
                break;

            default:
                out << GetOperatorString(node->getOp());
                break;
        }
    }

    out << " (" << node->getCompleteString() << ")";

    out << "\n";

    return true;
}

bool TOutputTraverser::visitBlock(Visit visit, TIntermBlock *node)
{
    TInfoSinkBase &out = sink;

    OutputTreeText(out, node, mDepth);
    out << "Code block\n";

    return true;
}

bool TOutputTraverser::visitDeclaration(Visit visit, TIntermDeclaration *node)
{
    TInfoSinkBase &out = sink;

    OutputTreeText(out, node, mDepth);
    out << "Declaration\n";

    return true;
}

bool TOutputTraverser::visitTernary(Visit visit, TIntermTernary *node)
{
    TInfoSinkBase &out = sink;

    OutputTreeText(out, node, mDepth);

    out << "Ternary selection";
    out << " (" << node->getCompleteString() << ")\n";

    ++mDepth;

    OutputTreeText(sink, node, mDepth);
    out << "Condition\n";
    node->getCondition()->traverse(this);

    OutputTreeText(sink, node, mDepth);
    if (node->getTrueExpression())
    {
        out << "true case\n";
        node->getTrueExpression()->traverse(this);
    }
    if (node->getFalseExpression())
    {
        OutputTreeText(sink, node, mDepth);
        out << "false case\n";
        node->getFalseExpression()->traverse(this);
    }

    --mDepth;

    return false;
}

bool TOutputTraverser::visitIfElse(Visit visit, TIntermIfElse *node)
{
    TInfoSinkBase &out = sink;

    OutputTreeText(out, node, mDepth);

    out << "If test\n";

    ++mDepth;

    OutputTreeText(sink, node, mDepth);
    out << "Condition\n";
    node->getCondition()->traverse(this);

    OutputTreeText(sink, node, mDepth);
    if (node->getTrueBlock())
    {
        out << "true case\n";
        node->getTrueBlock()->traverse(this);
    }
    else
    {
        out << "true case is null\n";
    }

    if (node->getFalseBlock())
    {
        OutputTreeText(sink, node, mDepth);
        out << "false case\n";
        node->getFalseBlock()->traverse(this);
    }

    --mDepth;

    return false;
}

bool TOutputTraverser::visitSwitch(Visit visit, TIntermSwitch *node)
{
    TInfoSinkBase &out = sink;

    OutputTreeText(out, node, mDepth);

    out << "Switch\n";

    return true;
}

bool TOutputTraverser::visitCase(Visit visit, TIntermCase *node)
{
    TInfoSinkBase &out = sink;

    OutputTreeText(out, node, mDepth);

    if (node->getCondition() == nullptr)
    {
        out << "Default\n";
    }
    else
    {
        out << "Case\n";
    }

    return true;
}

void TOutputTraverser::visitConstantUnion(TIntermConstantUnion *node)
{
    TInfoSinkBase &out = sink;

    size_t size = node->getType().getObjectSize();

    for (size_t i = 0; i < size; i++)
    {
        OutputTreeText(out, node, mDepth);
        switch (node->getUnionArrayPointer()[i].getType())
        {
            case EbtBool:
                if (node->getUnionArrayPointer()[i].getBConst())
                    out << "true";
                else
                    out << "false";

                out << " ("
                    << "const bool"
                    << ")";
                out << "\n";
                break;
            case EbtFloat:
                out << node->getUnionArrayPointer()[i].getFConst();
                out << " (const float)\n";
                break;
            case EbtInt:
                out << node->getUnionArrayPointer()[i].getIConst();
                out << " (const int)\n";
                break;
            case EbtUInt:
                out << node->getUnionArrayPointer()[i].getUConst();
                out << " (const uint)\n";
                break;
            case EbtYuvCscStandardEXT:
                out << getYuvCscStandardEXTString(
                    node->getUnionArrayPointer()[i].getYuvCscStandardEXTConst());
                out << " (const yuvCscStandardEXT)\n";
                break;
            default:
                out.prefix(SH_ERROR);
                out << "Unknown constant\n";
                break;
        }
    }
}

bool TOutputTraverser::visitLoop(Visit visit, TIntermLoop *node)
{
    TInfoSinkBase &out = sink;

    OutputTreeText(out, node, mDepth);

    out << "Loop with condition ";
    if (node->getType() == ELoopDoWhile)
        out << "not ";
    out << "tested first\n";

    ++mDepth;

    OutputTreeText(sink, node, mDepth);
    if (node->getCondition())
    {
        out << "Loop Condition\n";
        node->getCondition()->traverse(this);
    }
    else
    {
        out << "No loop condition\n";
    }

    OutputTreeText(sink, node, mDepth);
    if (node->getBody())
    {
        out << "Loop Body\n";
        node->getBody()->traverse(this);
    }
    else
    {
        out << "No loop body\n";
    }

    if (node->getExpression())
    {
        OutputTreeText(sink, node, mDepth);
        out << "Loop Terminal Expression\n";
        node->getExpression()->traverse(this);
    }

    --mDepth;

    return false;
}

bool TOutputTraverser::visitBranch(Visit visit, TIntermBranch *node)
{
    TInfoSinkBase &out = sink;

    OutputTreeText(out, node, mDepth);

    switch (node->getFlowOp())
    {
        case EOpKill:
            out << "Branch: Kill";
            break;
        case EOpBreak:
            out << "Branch: Break";
            break;
        case EOpContinue:
            out << "Branch: Continue";
            break;
        case EOpReturn:
            out << "Branch: Return";
            break;
        default:
            out << "Branch: Unknown Branch";
            break;
    }

    if (node->getExpression())
    {
        out << " with expression\n";
        ++mDepth;
        node->getExpression()->traverse(this);
        --mDepth;
    }
    else
    {
        out << "\n";
    }

    return false;
}

//
// This function is the one to call externally to start the traversal.
// Individual functions can be initialized to 0 to skip processing of that
// type of node. Its children will still be processed.
//
void TIntermediate::outputTree(TIntermNode *root, TInfoSinkBase &infoSink)
{
    TOutputTraverser it(infoSink);

    ASSERT(root);

    root->traverse(&it);
}

}  // namespace sh
