//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/OutputGLSL.h"
#include "compiler/debug.h"

namespace
{
TString getTypeName(const TType& type)
{
    TInfoSinkBase out;
    if (type.isMatrix())
    {
        out << "mat";
        out << type.getNominalSize();
    }
    else if (type.isVector())
    {
        switch (type.getBasicType())
        {
            case EbtFloat: out << "vec"; break;
            case EbtInt: out << "ivec"; break;
            case EbtBool: out << "bvec"; break;
            default: UNREACHABLE(); break;
        }
        out << type.getNominalSize();
    }
    else
    {
        if (type.getBasicType() == EbtStruct)
            out << type.getTypeName();
        else
            out << type.getBasicString();
    }
    return TString(out.c_str());
}

TString arrayBrackets(const TType& type)
{
    ASSERT(type.isArray());
    TInfoSinkBase out;
    out << "[" << type.getArraySize() << "]";
    return TString(out.c_str());
}

bool isSingleStatement(TIntermNode* node) {
    if (const TIntermAggregate* aggregate = node->getAsAggregate())
    {
        return (aggregate->getOp() != EOpFunction) &&
               (aggregate->getOp() != EOpSequence);
    }
    else if (const TIntermSelection* selection = node->getAsSelectionNode())
    {
        // Ternary operators are usually part of an assignment operator.
        // This handles those rare cases in which they are all by themselves.
        return selection->usesTernaryOperator();
    }
    else if (node->getAsLoopNode())
    {
        return false;
    }
    return true;
}
}  // namespace

TOutputGLSL::TOutputGLSL(TInfoSinkBase& objSink)
    : TIntermTraverser(true, true, true),
      mObjSink(objSink),
      mDeclaringVariables(false)
{
}

void TOutputGLSL::writeTriplet(Visit visit, const char* preStr, const char* inStr, const char* postStr)
{
    TInfoSinkBase& out = objSink();
    if (visit == PreVisit && preStr)
    {
        out << preStr;
    }
    else if (visit == InVisit && inStr)
    {
        out << inStr;
    }
    else if (visit == PostVisit && postStr)
    {
        out << postStr;
    }
}

void TOutputGLSL::writeVariableType(const TType& type)
{
    TInfoSinkBase& out = objSink();
    TQualifier qualifier = type.getQualifier();
    // TODO(alokp): Validate qualifier for variable declarations.
    if ((qualifier != EvqTemporary) && (qualifier != EvqGlobal))
        out << type.getQualifierString() << " ";

    // Declare the struct if we have not done so already.
    if ((type.getBasicType() == EbtStruct) &&
        (mDeclaredStructs.find(type.getTypeName()) == mDeclaredStructs.end()))
    {
        out << "struct " << type.getTypeName() << "{\n";
        const TTypeList* structure = type.getStruct();
        ASSERT(structure != NULL);
        for (size_t i = 0; i < structure->size(); ++i)
        {
            const TType* fieldType = (*structure)[i].type;
            ASSERT(fieldType != NULL);
            out << getTypeName(*fieldType) << " " << fieldType->getFieldName();
            if (fieldType->isArray())
                out << arrayBrackets(*fieldType);
            out << ";\n";
        }
        out << "}";
        mDeclaredStructs.insert(type.getTypeName());
    }
    else
    {
        out << getTypeName(type);
    }
}

void TOutputGLSL::writeFunctionParameters(const TIntermSequence& args)
{
    TInfoSinkBase& out = objSink();
    for (TIntermSequence::const_iterator iter = args.begin();
         iter != args.end(); ++iter)
    {
        const TIntermSymbol* arg = (*iter)->getAsSymbolNode();
        ASSERT(arg != NULL);

        const TType& type = arg->getType();
        TQualifier qualifier = type.getQualifier();
        // TODO(alokp): Validate qualifier for function arguments.
        if ((qualifier != EvqTemporary) && (qualifier != EvqGlobal))
            out << type.getQualifierString() << " ";

        out << getTypeName(type);

        const TString& name = arg->getSymbol();
        if (!name.empty())
            out << " " << name;
        if (type.isArray())
            out << arrayBrackets(type);

        // Put a comma if this is not the last argument.
        if (iter != args.end() - 1)
            out << ", ";
    }
}

const ConstantUnion* TOutputGLSL::writeConstantUnion(const TType& type,
                                                     const ConstantUnion* pConstUnion)
{
    TInfoSinkBase& out = objSink();

    if (type.getBasicType() == EbtStruct)
    {
        out << type.getTypeName() << "(";
        const TTypeList* structure = type.getStruct();
        ASSERT(structure != NULL);
        for (size_t i = 0; i < structure->size(); ++i)
        {
            const TType* fieldType = (*structure)[i].type;
            ASSERT(fieldType != NULL);
            pConstUnion = writeConstantUnion(*fieldType, pConstUnion);
            if (i != structure->size() - 1) out << ", ";
        }
        out << ")";
    }
    else
    {
        int size = type.getObjectSize();
        bool writeType = size > 1;
        if (writeType) out << getTypeName(type) << "(";
        for (int i = 0; i < size; ++i, ++pConstUnion)
        {
            switch (pConstUnion->getType())
            {
                case EbtFloat: out << pConstUnion->getFConst(); break;
                case EbtInt: out << pConstUnion->getIConst(); break;
                case EbtBool: out << pConstUnion->getBConst(); break;
                default: UNREACHABLE();
            }
            if (i != size - 1) out << ", ";
        }
        if (writeType) out << ")";
    }
    return pConstUnion;
}

void TOutputGLSL::visitSymbol(TIntermSymbol* node)
{
    TInfoSinkBase& out = objSink();
    out << node->getSymbol();

    if (mDeclaringVariables && node->getType().isArray())
        out << arrayBrackets(node->getType());
}

void TOutputGLSL::visitConstantUnion(TIntermConstantUnion* node)
{
    writeConstantUnion(node->getType(), node->getUnionArrayPointer());
}

bool TOutputGLSL::visitBinary(Visit visit, TIntermBinary* node)
{
    bool visitChildren = true;
    TInfoSinkBase& out = objSink();
    switch (node->getOp())
    {
        case EOpInitialize:
            if (visit == InVisit)
            {
                out << " = ";
                // RHS of initialize is not being declared.
                mDeclaringVariables = false;
            }
            break;
        case EOpAssign: writeTriplet(visit, "(", " = ", ")"); break;
        case EOpAddAssign: writeTriplet(visit, "(", " += ", ")"); break;
        case EOpSubAssign: writeTriplet(visit, "(", " -= ", ")"); break;
        case EOpDivAssign: writeTriplet(visit, "(", " /= ", ")"); break;
        // Notice the fall-through.
        case EOpMulAssign: 
        case EOpVectorTimesMatrixAssign:
        case EOpVectorTimesScalarAssign:
        case EOpMatrixTimesScalarAssign:
        case EOpMatrixTimesMatrixAssign:
            writeTriplet(visit, "(", " *= ", ")");
            break;

        case EOpIndexDirect:
        case EOpIndexIndirect:
            writeTriplet(visit, NULL, "[", "]");
            break;
        case EOpIndexDirectStruct:
            if (visit == InVisit)
            {
                out << ".";
                // TODO(alokp): ASSERT
                out << node->getType().getFieldName();
                visitChildren = false;
            }
            break;
        case EOpVectorSwizzle:
            if (visit == InVisit)
            {
                out << ".";
                TIntermAggregate* rightChild = node->getRight()->getAsAggregate();
                TIntermSequence& sequence = rightChild->getSequence();
                for (TIntermSequence::iterator sit = sequence.begin(); sit != sequence.end(); ++sit)
                {
                    TIntermConstantUnion* element = (*sit)->getAsConstantUnion();
                    ASSERT(element->getBasicType() == EbtInt);
                    ASSERT(element->getNominalSize() == 1);
                    const ConstantUnion& data = element->getUnionArrayPointer()[0];
                    ASSERT(data.getType() == EbtInt);
                    switch (data.getIConst())
                    {
                        case 0: out << "x"; break;
                        case 1: out << "y"; break;
                        case 2: out << "z"; break;
                        case 3: out << "w"; break;
                        default: UNREACHABLE(); break;
                    }
                }
                visitChildren = false;
            }
            break;

        case EOpAdd: writeTriplet(visit, "(", " + ", ")"); break;
        case EOpSub: writeTriplet(visit, "(", " - ", ")"); break;
        case EOpMul: writeTriplet(visit, "(", " * ", ")"); break;
        case EOpDiv: writeTriplet(visit, "(", " / ", ")"); break;
        case EOpMod: UNIMPLEMENTED(); break;
        case EOpEqual: writeTriplet(visit, "(", " == ", ")"); break;
        case EOpNotEqual: writeTriplet(visit, "(", " != ", ")"); break;
        case EOpLessThan: writeTriplet(visit, "(", " < ", ")"); break;
        case EOpGreaterThan: writeTriplet(visit, "(", " > ", ")"); break;
        case EOpLessThanEqual: writeTriplet(visit, "(", " <= ", ")"); break;
        case EOpGreaterThanEqual: writeTriplet(visit, "(", " >= ", ")"); break;

        // Notice the fall-through.
        case EOpVectorTimesScalar:
        case EOpVectorTimesMatrix:
        case EOpMatrixTimesVector:
        case EOpMatrixTimesScalar:
        case EOpMatrixTimesMatrix:
            writeTriplet(visit, "(", " * ", ")");
            break;

        case EOpLogicalOr: writeTriplet(visit, "(", " || ", ")"); break;
        case EOpLogicalXor: writeTriplet(visit, "(", " ^^ ", ")"); break;
        case EOpLogicalAnd: writeTriplet(visit, "(", " && ", ")"); break;
        default: UNREACHABLE(); break;
    }

    return visitChildren;
}

bool TOutputGLSL::visitUnary(Visit visit, TIntermUnary* node)
{
    switch (node->getOp())
    {
        case EOpNegative: writeTriplet(visit, "(-", NULL, ")"); break;
        case EOpVectorLogicalNot: writeTriplet(visit, "not(", NULL, ")"); break;
        case EOpLogicalNot: writeTriplet(visit, "(!", NULL, ")"); break;

        case EOpPostIncrement: writeTriplet(visit, "(", NULL, "++)"); break;
        case EOpPostDecrement: writeTriplet(visit, "(", NULL, "--)"); break;
        case EOpPreIncrement: writeTriplet(visit, "(++", NULL, ")"); break;
        case EOpPreDecrement: writeTriplet(visit, "(--", NULL, ")"); break;

        case EOpConvIntToBool:
        case EOpConvFloatToBool:
            switch (node->getOperand()->getType().getNominalSize())
            {
                case 1: writeTriplet(visit, "bool(", NULL, ")");  break;
                case 2: writeTriplet(visit, "bvec2(", NULL, ")"); break;
                case 3: writeTriplet(visit, "bvec3(", NULL, ")"); break;
                case 4: writeTriplet(visit, "bvec4(", NULL, ")"); break;
                default: UNREACHABLE();
            }
            break;
        case EOpConvBoolToFloat:
        case EOpConvIntToFloat:
            switch (node->getOperand()->getType().getNominalSize())
            {
                case 1: writeTriplet(visit, "float(", NULL, ")");  break;
                case 2: writeTriplet(visit, "vec2(", NULL, ")"); break;
                case 3: writeTriplet(visit, "vec3(", NULL, ")"); break;
                case 4: writeTriplet(visit, "vec4(", NULL, ")"); break;
                default: UNREACHABLE();
            }
            break;
        case EOpConvFloatToInt:
        case EOpConvBoolToInt:
            switch (node->getOperand()->getType().getNominalSize())
            {
                case 1: writeTriplet(visit, "int(", NULL, ")");  break;
                case 2: writeTriplet(visit, "ivec2(", NULL, ")"); break;
                case 3: writeTriplet(visit, "ivec3(", NULL, ")"); break;
                case 4: writeTriplet(visit, "ivec4(", NULL, ")"); break;
                default: UNREACHABLE();
            }
            break;

        case EOpRadians: writeTriplet(visit, "radians(", NULL, ")"); break;
        case EOpDegrees: writeTriplet(visit, "degrees(", NULL, ")"); break;
        case EOpSin: writeTriplet(visit, "sin(", NULL, ")"); break;
        case EOpCos: writeTriplet(visit, "cos(", NULL, ")"); break;
        case EOpTan: writeTriplet(visit, "tan(", NULL, ")"); break;
        case EOpAsin: writeTriplet(visit, "asin(", NULL, ")"); break;
        case EOpAcos: writeTriplet(visit, "acos(", NULL, ")"); break;
        case EOpAtan: writeTriplet(visit, "atan(", NULL, ")"); break;

        case EOpExp: writeTriplet(visit, "exp(", NULL, ")"); break;
        case EOpLog: writeTriplet(visit, "log(", NULL, ")"); break;
        case EOpExp2: writeTriplet(visit, "exp2(", NULL, ")"); break;
        case EOpLog2: writeTriplet(visit, "log2(", NULL, ")"); break;
        case EOpSqrt: writeTriplet(visit, "sqrt(", NULL, ")"); break;
        case EOpInverseSqrt: writeTriplet(visit, "inversesqrt(", NULL, ")"); break;

        case EOpAbs: writeTriplet(visit, "abs(", NULL, ")"); break;
        case EOpSign: writeTriplet(visit, "sign(", NULL, ")"); break;
        case EOpFloor: writeTriplet(visit, "floor(", NULL, ")"); break;
        case EOpCeil: writeTriplet(visit, "ceil(", NULL, ")"); break;
        case EOpFract: writeTriplet(visit, "fract(", NULL, ")"); break;

        case EOpLength: writeTriplet(visit, "length(", NULL, ")"); break;
        case EOpNormalize: writeTriplet(visit, "normalize(", NULL, ")"); break;

        case EOpDFdx: writeTriplet(visit, "dFdx(", NULL, ")"); break;
        case EOpDFdy: writeTriplet(visit, "dFdy(", NULL, ")"); break;
        case EOpFwidth: writeTriplet(visit, "fwidth(", NULL, ")"); break;

        case EOpAny: writeTriplet(visit, "any(", NULL, ")"); break;
        case EOpAll: writeTriplet(visit, "all(", NULL, ")"); break;

        default: UNREACHABLE(); break;
    }

    return true;
}

bool TOutputGLSL::visitSelection(Visit visit, TIntermSelection* node)
{
    TInfoSinkBase& out = objSink();

    if (node->usesTernaryOperator())
    {
        // Notice two brackets at the beginning and end. The outer ones
        // encapsulate the whole ternary expression. This preserves the
        // order of precedence when ternary expressions are used in a
        // compound expression, i.e., c = 2 * (a < b ? 1 : 2).
        out << "((";
        node->getCondition()->traverse(this);
        out << ") ? (";
        node->getTrueBlock()->traverse(this);
        out << ") : (";
        node->getFalseBlock()->traverse(this);
        out << "))";
    }
    else
    {
        out << "if (";
        node->getCondition()->traverse(this);
        out << ")\n";

        incrementDepth();
        visitCodeBlock(node->getTrueBlock());

        if (node->getFalseBlock())
        {
            out << "else\n";
            visitCodeBlock(node->getFalseBlock());
        }
        decrementDepth();
    }
    return false;
}

bool TOutputGLSL::visitAggregate(Visit visit, TIntermAggregate* node)
{
    bool visitChildren = true;
    TInfoSinkBase& out = objSink();
    switch (node->getOp())
    {
        case EOpSequence: {
            // Scope the sequences except when at the global scope.
            if (depth > 0) out << "{\n";

            incrementDepth();
            const TIntermSequence& sequence = node->getSequence();
            for (TIntermSequence::const_iterator iter = sequence.begin();
                 iter != sequence.end(); ++iter)
            {
                TIntermNode* node = *iter;
                ASSERT(node != NULL);
                node->traverse(this);

                if (isSingleStatement(node))
                    out << ";\n";
            }
            decrementDepth();

            // Scope the sequences except when at the global scope.
            if (depth > 0) out << "}\n";
            visitChildren = false;
            break;
        }
        case EOpPrototype: {
            // Function declaration.
            ASSERT(visit == PreVisit);
            TString returnType = getTypeName(node->getType());
            out << returnType << " " << node->getName();

            out << "(";
            writeFunctionParameters(node->getSequence());
            out << ")";

            visitChildren = false;
            break;
        }
        case EOpFunction: {
            // Function definition.
            ASSERT(visit == PreVisit);
            TString returnType = getTypeName(node->getType());
            TString functionName = TFunction::unmangleName(node->getName());
            out << returnType << " " << functionName;

            incrementDepth();
            // Function definition node contains one or two children nodes
            // representing function parameters and function body. The latter
            // is not present in case of empty function bodies.
            const TIntermSequence& sequence = node->getSequence();
            ASSERT((sequence.size() == 1) || (sequence.size() == 2));
            TIntermSequence::const_iterator seqIter = sequence.begin();

            // Traverse function parameters.
            TIntermAggregate* params = (*seqIter)->getAsAggregate();
            ASSERT(params != NULL);
            ASSERT(params->getOp() == EOpParameters);
            params->traverse(this);

            // Traverse function body.
            TIntermAggregate* body = ++seqIter != sequence.end() ?
                (*seqIter)->getAsAggregate() : NULL;
            visitCodeBlock(body);
            decrementDepth();
 
            // Fully processed; no need to visit children.
            visitChildren = false;
            break;
        }
        case EOpFunctionCall:
            // Function call.
            if (visit == PreVisit)
            {
                TString functionName = TFunction::unmangleName(node->getName());
                out << functionName << "(";
            }
            else if (visit == InVisit)
            {
                out << ", ";
            }
            else
            {
                out << ")";
            }
            break;
        case EOpParameters: {
            // Function parameters.
            ASSERT(visit == PreVisit);
            out << "(";
            writeFunctionParameters(node->getSequence());
            out << ")";
            visitChildren = false;
            break;
        }
        case EOpDeclaration: {
            // Variable declaration.
            if (visit == PreVisit)
            {
                const TIntermSequence& sequence = node->getSequence();
                const TIntermTyped* variable = sequence.front()->getAsTyped();
                writeVariableType(variable->getType());
                out << " ";
                mDeclaringVariables = true;
            }
            else if (visit == InVisit)
            {
                out << ", ";
                mDeclaringVariables = true;
            }
            else
            {
                mDeclaringVariables = false;
            }
            break;
        }
        case EOpConstructFloat: writeTriplet(visit, "float(", NULL, ")"); break;
        case EOpConstructVec2: writeTriplet(visit, "vec2(", ", ", ")"); break;
        case EOpConstructVec3: writeTriplet(visit, "vec3(", ", ", ")"); break;
        case EOpConstructVec4: writeTriplet(visit, "vec4(", ", ", ")"); break;
        case EOpConstructBool: writeTriplet(visit, "bool(", NULL, ")"); break;
        case EOpConstructBVec2: writeTriplet(visit, "bvec2(", ", ", ")"); break;
        case EOpConstructBVec3: writeTriplet(visit, "bvec3(", ", ", ")"); break;
        case EOpConstructBVec4: writeTriplet(visit, "bvec4(", ", ", ")"); break;
        case EOpConstructInt: writeTriplet(visit, "int(", NULL, ")"); break;
        case EOpConstructIVec2: writeTriplet(visit, "ivec2(", ", ", ")"); break;
        case EOpConstructIVec3: writeTriplet(visit, "ivec3(", ", ", ")"); break;
        case EOpConstructIVec4: writeTriplet(visit, "ivec4(", ", ", ")"); break;
        case EOpConstructMat2: writeTriplet(visit, "mat2(", ", ", ")"); break;
        case EOpConstructMat3: writeTriplet(visit, "mat3(", ", ", ")"); break;
        case EOpConstructMat4: writeTriplet(visit, "mat4(", ", ", ")"); break;
        case EOpConstructStruct:
            if (visit == PreVisit)
            {
                const TType& type = node->getType();
                ASSERT(type.getBasicType() == EbtStruct);
                out << type.getTypeName() << "(";
            }
            else if (visit == InVisit)
            {
                out << ", ";
            }
            else
            {
                out << ")";
            }
            break;

        case EOpLessThan: writeTriplet(visit, "lessThan(", ", ", ")"); break;
        case EOpGreaterThan: writeTriplet(visit, "greaterThan(", ", ", ")"); break;
        case EOpLessThanEqual: writeTriplet(visit, "lessThanEqual(", ", ", ")"); break;
        case EOpGreaterThanEqual: writeTriplet(visit, "greaterThanEqual(", ", ", ")"); break;
        case EOpVectorEqual: writeTriplet(visit, "equal(", ", ", ")"); break;
        case EOpVectorNotEqual: writeTriplet(visit, "notEqual(", ", ", ")"); break;
        case EOpComma: writeTriplet(visit, NULL, ", ", NULL); break;

        case EOpMod: writeTriplet(visit, "mod(", ", ", ")"); break;
        case EOpPow: writeTriplet(visit, "pow(", ", ", ")"); break;
        case EOpAtan: writeTriplet(visit, "atan(", ", ", ")"); break;
        case EOpMin: writeTriplet(visit, "min(", ", ", ")"); break;
        case EOpMax: writeTriplet(visit, "max(", ", ", ")"); break;
        case EOpClamp: writeTriplet(visit, "clamp(", ", ", ")"); break;
        case EOpMix: writeTriplet(visit, "mix(", ", ", ")"); break;
        case EOpStep: writeTriplet(visit, "step(", ", ", ")"); break;
        case EOpSmoothStep: writeTriplet(visit, "smoothstep(", ", ", ")"); break;

        case EOpDistance: writeTriplet(visit, "distance(", ", ", ")"); break;
        case EOpDot: writeTriplet(visit, "dot(", ", ", ")"); break;
        case EOpCross: writeTriplet(visit, "cross(", ", ", ")"); break;
        case EOpFaceForward: writeTriplet(visit, "faceforward(", ", ", ")"); break;
        case EOpReflect: writeTriplet(visit, "reflect(", ", ", ")"); break;
        case EOpRefract: writeTriplet(visit, "refract(", ", ", ")"); break;
        case EOpMul: writeTriplet(visit, "matrixCompMult(", ", ", ")"); break;

        default: UNREACHABLE(); break;
    }
    return visitChildren;
}

bool TOutputGLSL::visitLoop(Visit visit, TIntermLoop* node)
{
    TInfoSinkBase& out = objSink();

    incrementDepth();
    // Loop header.
    TLoopType loopType = node->getType();
    if (loopType == ELoopFor)  // for loop
    {
        out << "for (";
        if (node->getInit())
            node->getInit()->traverse(this);
        out << "; ";

        if (node->getCondition())
            node->getCondition()->traverse(this);
        out << "; ";

        if (node->getExpression())
            node->getExpression()->traverse(this);
        out << ")\n";
    }
    else if (loopType == ELoopWhile)  // while loop
    {
        out << "while (";
        ASSERT(node->getCondition() != NULL);
        node->getCondition()->traverse(this);
        out << ")\n";
    }
    else  // do-while loop
    {
        ASSERT(loopType == ELoopDoWhile);
        out << "do\n";
    }

    // Loop body.
    visitCodeBlock(node->getBody());

    // Loop footer.
    if (loopType == ELoopDoWhile)  // do-while loop
    {
        out << "while (";
        ASSERT(node->getCondition() != NULL);
        node->getCondition()->traverse(this);
        out << ");\n";
    }
    decrementDepth();

    // No need to visit children. They have been already processed in
    // this function.
    return false;
}

bool TOutputGLSL::visitBranch(Visit visit, TIntermBranch* node)
{
    switch (node->getFlowOp())
    {
        case EOpKill: writeTriplet(visit, "discard", NULL, NULL); break;
        case EOpBreak: writeTriplet(visit, "break", NULL, NULL); break;
        case EOpContinue: writeTriplet(visit, "continue", NULL, NULL); break;
        case EOpReturn: writeTriplet(visit, "return ", NULL, NULL); break;
        default: UNREACHABLE(); break;
    }

    return true;
}

void TOutputGLSL::visitCodeBlock(TIntermNode* node) {
    TInfoSinkBase &out = objSink();
    if (node != NULL)
    {
        node->traverse(this);
        // Single statements not part of a sequence need to be terminated
        // with semi-colon.
        if (isSingleStatement(node))
            out << ";\n";
    }
    else
    {
        out << "{\n}\n";  // Empty code block.
    }
}
