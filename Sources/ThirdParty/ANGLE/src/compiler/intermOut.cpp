//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/localintermediate.h"

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
class TOutputTraverser : public TIntermTraverser {
public:
    TOutputTraverser(TInfoSink& i) : infoSink(i) { }
    TInfoSink& infoSink;

protected:
    void visitSymbol(TIntermSymbol*);
    void visitConstantUnion(TIntermConstantUnion*);
    bool visitBinary(Visit visit, TIntermBinary*);
    bool visitUnary(Visit visit, TIntermUnary*);
    bool visitSelection(Visit visit, TIntermSelection*);
    bool visitAggregate(Visit visit, TIntermAggregate*);
    bool visitLoop(Visit visit, TIntermLoop*);
    bool visitBranch(Visit visit, TIntermBranch*);
};

TString TType::getCompleteString() const
{
    TStringStream stream;

    if (qualifier != EvqTemporary && qualifier != EvqGlobal)
        stream << getQualifierString() << " " << getPrecisionString() << " ";
    if (array)
        stream << "array of ";
    if (matrix)
        stream << size << "X" << size << " matrix of ";
    else if (size > 1)
        stream << size << "-component vector of ";

    stream << getBasicString();
    return stream.str();
}

//
// Helper functions for printing, not part of traversing.
//

void OutputTreeText(TInfoSink& infoSink, TIntermNode* node, const int depth)
{
    int i;

    infoSink.debug.location(node->getLine());

    for (i = 0; i < depth; ++i)
        infoSink.debug << "  ";
}

//
// The rest of the file are the traversal functions.  The last one
// is the one that starts the traversal.
//
// Return true from interior nodes to have the external traversal
// continue on to children.  If you process children yourself,
// return false.
//

void TOutputTraverser::visitSymbol(TIntermSymbol* node)
{
    OutputTreeText(infoSink, node, depth);

    infoSink.debug << "'" << node->getSymbol() << "' ";
    infoSink.debug << "(" << node->getCompleteString() << ")\n";
}

bool TOutputTraverser::visitBinary(Visit visit, TIntermBinary* node)
{
    TInfoSink& out = infoSink;

    OutputTreeText(out, node, depth);

    switch (node->getOp()) {
        case EOpAssign:                   out.debug << "move second child to first child";           break;
        case EOpInitialize:               out.debug << "initialize first child with second child";   break;
        case EOpAddAssign:                out.debug << "add second child into first child";          break;
        case EOpSubAssign:                out.debug << "subtract second child into first child";     break;
        case EOpMulAssign:                out.debug << "multiply second child into first child";     break;
        case EOpVectorTimesMatrixAssign:  out.debug << "matrix mult second child into first child";  break;
        case EOpVectorTimesScalarAssign:  out.debug << "vector scale second child into first child"; break;
        case EOpMatrixTimesScalarAssign:  out.debug << "matrix scale second child into first child"; break;
        case EOpMatrixTimesMatrixAssign:  out.debug << "matrix mult second child into first child"; break;
        case EOpDivAssign:                out.debug << "divide second child into first child";       break;
        case EOpIndexDirect:   out.debug << "direct index";   break;
        case EOpIndexIndirect: out.debug << "indirect index"; break;
        case EOpIndexDirectStruct:   out.debug << "direct index for structure";   break;
        case EOpVectorSwizzle: out.debug << "vector swizzle"; break;

        case EOpAdd:    out.debug << "add";                     break;
        case EOpSub:    out.debug << "subtract";                break;
        case EOpMul:    out.debug << "component-wise multiply"; break;
        case EOpDiv:    out.debug << "divide";                  break;
        case EOpEqual:            out.debug << "Compare Equal";                 break;
        case EOpNotEqual:         out.debug << "Compare Not Equal";             break;
        case EOpLessThan:         out.debug << "Compare Less Than";             break;
        case EOpGreaterThan:      out.debug << "Compare Greater Than";          break;
        case EOpLessThanEqual:    out.debug << "Compare Less Than or Equal";    break;
        case EOpGreaterThanEqual: out.debug << "Compare Greater Than or Equal"; break;

        case EOpVectorTimesScalar: out.debug << "vector-scale";          break;
        case EOpVectorTimesMatrix: out.debug << "vector-times-matrix";   break;
        case EOpMatrixTimesVector: out.debug << "matrix-times-vector";   break;
        case EOpMatrixTimesScalar: out.debug << "matrix-scale";          break;
        case EOpMatrixTimesMatrix: out.debug << "matrix-multiply";       break;

        case EOpLogicalOr:  out.debug << "logical-or";   break;
        case EOpLogicalXor: out.debug << "logical-xor"; break;
        case EOpLogicalAnd: out.debug << "logical-and"; break;
        default: out.debug << "<unknown op>";
    }

    out.debug << " (" << node->getCompleteString() << ")";

    out.debug << "\n";

    return true;
}

bool TOutputTraverser::visitUnary(Visit visit, TIntermUnary* node)
{
    TInfoSink& out = infoSink;

    OutputTreeText(out, node, depth);

    switch (node->getOp()) {
        case EOpNegative:       out.debug << "Negate value";         break;
        case EOpVectorLogicalNot:
        case EOpLogicalNot:     out.debug << "Negate conditional";   break;

        case EOpPostIncrement:  out.debug << "Post-Increment";       break;
        case EOpPostDecrement:  out.debug << "Post-Decrement";       break;
        case EOpPreIncrement:   out.debug << "Pre-Increment";        break;
        case EOpPreDecrement:   out.debug << "Pre-Decrement";        break;

        case EOpConvIntToBool:  out.debug << "Convert int to bool";  break;
        case EOpConvFloatToBool:out.debug << "Convert float to bool";break;
        case EOpConvBoolToFloat:out.debug << "Convert bool to float";break;
        case EOpConvIntToFloat: out.debug << "Convert int to float"; break;
        case EOpConvFloatToInt: out.debug << "Convert float to int"; break;
        case EOpConvBoolToInt:  out.debug << "Convert bool to int";  break;

        case EOpRadians:        out.debug << "radians";              break;
        case EOpDegrees:        out.debug << "degrees";              break;
        case EOpSin:            out.debug << "sine";                 break;
        case EOpCos:            out.debug << "cosine";               break;
        case EOpTan:            out.debug << "tangent";              break;
        case EOpAsin:           out.debug << "arc sine";             break;
        case EOpAcos:           out.debug << "arc cosine";           break;
        case EOpAtan:           out.debug << "arc tangent";          break;

        case EOpExp:            out.debug << "exp";                  break;
        case EOpLog:            out.debug << "log";                  break;
        case EOpExp2:           out.debug << "exp2";                 break;
        case EOpLog2:           out.debug << "log2";                 break;
        case EOpSqrt:           out.debug << "sqrt";                 break;
        case EOpInverseSqrt:    out.debug << "inverse sqrt";         break;

        case EOpAbs:            out.debug << "Absolute value";       break;
        case EOpSign:           out.debug << "Sign";                 break;
        case EOpFloor:          out.debug << "Floor";                break;
        case EOpCeil:           out.debug << "Ceiling";              break;
        case EOpFract:          out.debug << "Fraction";             break;

        case EOpLength:         out.debug << "length";               break;
        case EOpNormalize:      out.debug << "normalize";            break;
            //	case EOpDPdx:           out.debug << "dPdx";                 break;               
            //	case EOpDPdy:           out.debug << "dPdy";                 break;   
            //	case EOpFwidth:         out.debug << "fwidth";               break;                   

        case EOpAny:            out.debug << "any";                  break;
        case EOpAll:            out.debug << "all";                  break;

        default: out.debug.message(EPrefixError, "Bad unary op");
    }

    out.debug << " (" << node->getCompleteString() << ")";

    out.debug << "\n";

    return true;
}

bool TOutputTraverser::visitAggregate(Visit visit, TIntermAggregate* node)
{
    TInfoSink& out = infoSink;

    if (node->getOp() == EOpNull) {
        out.debug.message(EPrefixError, "node is still EOpNull!");
        return true;
    }

    OutputTreeText(out, node, depth);

    switch (node->getOp()) {
        case EOpSequence:      out.debug << "Sequence\n"; return true;
        case EOpComma:         out.debug << "Comma\n"; return true;
        case EOpFunction:      out.debug << "Function Definition: " << node->getName(); break;
        case EOpFunctionCall:  out.debug << "Function Call: " << node->getName(); break;
        case EOpParameters:    out.debug << "Function Parameters: ";              break;

        case EOpConstructFloat: out.debug << "Construct float"; break;
        case EOpConstructVec2:  out.debug << "Construct vec2";  break;
        case EOpConstructVec3:  out.debug << "Construct vec3";  break;
        case EOpConstructVec4:  out.debug << "Construct vec4";  break;
        case EOpConstructBool:  out.debug << "Construct bool";  break;
        case EOpConstructBVec2: out.debug << "Construct bvec2"; break;
        case EOpConstructBVec3: out.debug << "Construct bvec3"; break;
        case EOpConstructBVec4: out.debug << "Construct bvec4"; break;
        case EOpConstructInt:   out.debug << "Construct int";   break;
        case EOpConstructIVec2: out.debug << "Construct ivec2"; break;
        case EOpConstructIVec3: out.debug << "Construct ivec3"; break;
        case EOpConstructIVec4: out.debug << "Construct ivec4"; break;
        case EOpConstructMat2:  out.debug << "Construct mat2";  break;
        case EOpConstructMat3:  out.debug << "Construct mat3";  break;
        case EOpConstructMat4:  out.debug << "Construct mat4";  break;
        case EOpConstructStruct:  out.debug << "Construct structure";  break;

        case EOpLessThan:         out.debug << "Compare Less Than";             break;
        case EOpGreaterThan:      out.debug << "Compare Greater Than";          break;
        case EOpLessThanEqual:    out.debug << "Compare Less Than or Equal";    break;
        case EOpGreaterThanEqual: out.debug << "Compare Greater Than or Equal"; break;
        case EOpVectorEqual:      out.debug << "Equal";                         break;
        case EOpVectorNotEqual:   out.debug << "NotEqual";                      break;

        case EOpMod:           out.debug << "mod";         break;
        case EOpPow:           out.debug << "pow";         break;

        case EOpAtan:          out.debug << "arc tangent"; break;

        case EOpMin:           out.debug << "min";         break;
        case EOpMax:           out.debug << "max";         break;
        case EOpClamp:         out.debug << "clamp";       break;
        case EOpMix:           out.debug << "mix";         break;
        case EOpStep:          out.debug << "step";        break;
        case EOpSmoothStep:    out.debug << "smoothstep";  break;

        case EOpDistance:      out.debug << "distance";                break;
        case EOpDot:           out.debug << "dot-product";             break;
        case EOpCross:         out.debug << "cross-product";           break;
        case EOpFaceForward:   out.debug << "face-forward";            break;
        case EOpReflect:       out.debug << "reflect";                 break;
        case EOpRefract:       out.debug << "refract";                 break;
        case EOpMul:           out.debug << "component-wise multiply"; break;

        default: out.debug.message(EPrefixError, "Bad aggregation op");
    }

    if (node->getOp() != EOpSequence && node->getOp() != EOpParameters)
        out.debug << " (" << node->getCompleteString() << ")";

    out.debug << "\n";

    return true;
}

bool TOutputTraverser::visitSelection(Visit visit, TIntermSelection* node)
{
    TInfoSink& out = infoSink;

    OutputTreeText(out, node, depth);

    out.debug << "Test condition and select";
    out.debug << " (" << node->getCompleteString() << ")\n";

    ++depth;

    OutputTreeText(infoSink, node, depth);
    out.debug << "Condition\n";
    node->getCondition()->traverse(this);

    OutputTreeText(infoSink, node, depth);
    if (node->getTrueBlock()) {
        out.debug << "true case\n";
        node->getTrueBlock()->traverse(this);
    } else
        out.debug << "true case is null\n";

    if (node->getFalseBlock()) {
        OutputTreeText(infoSink, node, depth);
        out.debug << "false case\n";
        node->getFalseBlock()->traverse(this);
    }

    --depth;

    return false;
}

void TOutputTraverser::visitConstantUnion(TIntermConstantUnion* node)
{
    TInfoSink& out = infoSink;

    int size = node->getType().getObjectSize();

    for (int i = 0; i < size; i++) {
        OutputTreeText(out, node, depth);
        switch (node->getUnionArrayPointer()[i].getType()) {
            case EbtBool:
                if (node->getUnionArrayPointer()[i].getBConst())
                    out.debug << "true";
                else
                    out.debug << "false";

                out.debug << " (" << "const bool" << ")";
                out.debug << "\n";
                break;
            case EbtFloat:
                out.debug << node->getUnionArrayPointer()[i].getFConst();
                out.debug << " (const float)\n";
                break;
            case EbtInt:
                out.debug << node->getUnionArrayPointer()[i].getIConst();
                out.debug << " (const int)\n";
                break;
            default:
                out.info.message(EPrefixInternalError, "Unknown constant", node->getLine());
                break;
        }
    }
}

bool TOutputTraverser::visitLoop(Visit visit, TIntermLoop* node)
{
    TInfoSink& out = infoSink;

    OutputTreeText(out, node, depth);

    out.debug << "Loop with condition ";
    if (! node->testFirst())
        out.debug << "not ";
    out.debug << "tested first\n";

    ++depth;

    OutputTreeText(infoSink, node, depth);
    if (node->getTest()) {
        out.debug << "Loop Condition\n";
        node->getTest()->traverse(this);
    } else
        out.debug << "No loop condition\n";

    OutputTreeText(infoSink, node, depth);
    if (node->getBody()) {
        out.debug << "Loop Body\n";
        node->getBody()->traverse(this);
    } else
        out.debug << "No loop body\n";

    if (node->getTerminal()) {
        OutputTreeText(infoSink, node, depth);
        out.debug << "Loop Terminal Expression\n";
        node->getTerminal()->traverse(this);
    }

    --depth;

    return false;
}

bool TOutputTraverser::visitBranch(Visit visit, TIntermBranch* node)
{
    TInfoSink& out = infoSink;

    OutputTreeText(out, node, depth);

    switch (node->getFlowOp()) {
        case EOpKill:      out.debug << "Branch: Kill";           break;
        case EOpBreak:     out.debug << "Branch: Break";          break;
        case EOpContinue:  out.debug << "Branch: Continue";       break;
        case EOpReturn:    out.debug << "Branch: Return";         break;
        default:           out.debug << "Branch: Unknown Branch"; break;
    }

    if (node->getExpression()) {
        out.debug << " with expression\n";
        ++depth;
        node->getExpression()->traverse(this);
        --depth;
    } else
        out.debug << "\n";

    return false;
}

//
// This function is the one to call externally to start the traversal.
// Individual functions can be initialized to 0 to skip processing of that
// type of node.  It's children will still be processed.
//
void TIntermediate::outputTree(TIntermNode* root)
{
    if (root == 0)
        return;

    TOutputTraverser it(infoSink);

    root->traverse(&it);
}
