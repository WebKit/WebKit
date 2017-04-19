//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/OutputGLSLBase.h"

#include "common/debug.h"
#include "common/mathutil.h"

#include <cfloat>

namespace sh
{

namespace
{
TString arrayBrackets(const TType &type)
{
    ASSERT(type.isArray());
    TInfoSinkBase out;
    out << "[" << type.getArraySize() << "]";
    return TString(out.c_str());
}

bool isSingleStatement(TIntermNode *node)
{
    if (node->getAsFunctionDefinition())
    {
        return false;
    }
    else if (node->getAsBlock())
    {
        return false;
    }
    else if (node->getAsIfElseNode())
    {
        return false;
    }
    else if (node->getAsLoopNode())
    {
        return false;
    }
    else if (node->getAsSwitchNode())
    {
        return false;
    }
    else if (node->getAsCaseNode())
    {
        return false;
    }
    return true;
}

// If SH_SCALARIZE_VEC_AND_MAT_CONSTRUCTOR_ARGS is enabled, layout qualifiers are spilled whenever
// variables with specified layout qualifiers are copied. Additional checks are needed against the
// type and storage qualifier of the variable to verify that layout qualifiers have to be outputted.
// TODO (mradev): Fix layout qualifier spilling in ScalarizeVecAndMatConstructorArgs and remove
// NeedsToWriteLayoutQualifier.
bool NeedsToWriteLayoutQualifier(const TType &type)
{
    if (type.getBasicType() == EbtInterfaceBlock)
    {
        return false;
    }

    const TLayoutQualifier &layoutQualifier = type.getLayoutQualifier();

    if ((type.getQualifier() == EvqFragmentOut || type.getQualifier() == EvqVertexIn) &&
        layoutQualifier.location >= 0)
    {
        return true;
    }

    if (type.getQualifier() == EvqFragmentOut && layoutQualifier.yuv == true)
    {
        return true;
    }

    if (IsOpaqueType(type.getBasicType()) && layoutQualifier.binding != -1)
    {
        return true;
    }

    if (IsImage(type.getBasicType()) && layoutQualifier.imageInternalFormat != EiifUnspecified)
    {
        return true;
    }
    return false;
}

class CommaSeparatedListItemPrefixGenerator
{
  public:
    CommaSeparatedListItemPrefixGenerator() : mFirst(true) {}
  private:
    bool mFirst;

    friend TInfoSinkBase &operator<<(TInfoSinkBase &out,
                                     CommaSeparatedListItemPrefixGenerator &gen);
};

TInfoSinkBase &operator<<(TInfoSinkBase &out, CommaSeparatedListItemPrefixGenerator &gen)
{
    if (gen.mFirst)
    {
        gen.mFirst = false;
    }
    else
    {
        out << ", ";
    }
    return out;
}

}  // namespace

TOutputGLSLBase::TOutputGLSLBase(TInfoSinkBase &objSink,
                                 ShArrayIndexClampingStrategy clampingStrategy,
                                 ShHashFunction64 hashFunction,
                                 NameMap &nameMap,
                                 TSymbolTable &symbolTable,
                                 sh::GLenum shaderType,
                                 int shaderVersion,
                                 ShShaderOutput output,
                                 ShCompileOptions compileOptions)
    : TIntermTraverser(true, true, true),
      mObjSink(objSink),
      mDeclaringVariables(false),
      mClampingStrategy(clampingStrategy),
      mHashFunction(hashFunction),
      mNameMap(nameMap),
      mSymbolTable(symbolTable),
      mShaderType(shaderType),
      mShaderVersion(shaderVersion),
      mOutput(output),
      mCompileOptions(compileOptions)
{
}

void TOutputGLSLBase::writeInvariantQualifier(const TType &type)
{
    if (!sh::RemoveInvariant(mShaderType, mShaderVersion, mOutput, mCompileOptions))
    {
        TInfoSinkBase &out = objSink();
        out << "invariant ";
    }
}

void TOutputGLSLBase::writeFloat(TInfoSinkBase &out, float f)
{
    if ((gl::isInf(f) || gl::isNaN(f)) && mShaderVersion >= 300)
    {
        out << "uintBitsToFloat(" << gl::bitCast<uint32_t>(f) << "u)";
    }
    else
    {
        out << std::min(FLT_MAX, std::max(-FLT_MAX, f));
    }
}

void TOutputGLSLBase::writeTriplet(Visit visit,
                                   const char *preStr,
                                   const char *inStr,
                                   const char *postStr)
{
    TInfoSinkBase &out = objSink();
    if (visit == PreVisit && preStr)
        out << preStr;
    else if (visit == InVisit && inStr)
        out << inStr;
    else if (visit == PostVisit && postStr)
        out << postStr;
}

void TOutputGLSLBase::writeBuiltInFunctionTriplet(Visit visit,
                                                  TOperator op,
                                                  bool useEmulatedFunction)
{
    TInfoSinkBase &out = objSink();
    if (visit == PreVisit)
    {
        const char *opStr(GetOperatorString(op));
        if (useEmulatedFunction)
        {
            BuiltInFunctionEmulator::WriteEmulatedFunctionName(out, opStr);
        }
        else
        {
            out << opStr;
        }
        out << "(";
    }
    else
    {
        writeTriplet(visit, nullptr, ", ", ")");
    }
}

void TOutputGLSLBase::writeLayoutQualifier(const TType &type)
{
    if (!NeedsToWriteLayoutQualifier(type))
    {
        return;
    }

    TInfoSinkBase &out                      = objSink();
    const TLayoutQualifier &layoutQualifier = type.getLayoutQualifier();
    out << "layout(";

    CommaSeparatedListItemPrefixGenerator listItemPrefix;

    if (type.getQualifier() == EvqFragmentOut || type.getQualifier() == EvqVertexIn)
    {
        if (layoutQualifier.location >= 0)
        {
            out << listItemPrefix << "location = " << layoutQualifier.location;
        }
    }

    if (type.getQualifier() == EvqFragmentOut)
    {
        if (layoutQualifier.yuv == true)
        {
            out << listItemPrefix << "yuv";
        }
    }

    if (IsOpaqueType(type.getBasicType()))
    {
        if (layoutQualifier.binding >= 0)
        {
            out << listItemPrefix << "binding = " << layoutQualifier.binding;
        }
    }

    if (IsImage(type.getBasicType()))
    {
        if (layoutQualifier.imageInternalFormat != EiifUnspecified)
        {
            ASSERT(type.getQualifier() == EvqTemporary || type.getQualifier() == EvqUniform);
            out << listItemPrefix
                << getImageInternalFormatString(layoutQualifier.imageInternalFormat);
        }
    }

    out << ") ";
}

const char *TOutputGLSLBase::mapQualifierToString(TQualifier qualifier)
{
    if (sh::IsGLSL410OrOlder(mOutput) && mShaderVersion >= 300 &&
        (mCompileOptions & SH_REMOVE_INVARIANT_AND_CENTROID_FOR_ESSL3) != 0)
    {
        switch (qualifier)
        {
            // The return string is consistent with sh::getQualifierString() from
            // BaseTypes.h minus the "centroid" keyword.
            case EvqCentroid:
                return "";
            case EvqCentroidIn:
                return "smooth in";
            case EvqCentroidOut:
                return "smooth out";
            default:
                break;
        }
    }
    if (sh::IsGLSL130OrNewer(mOutput))
    {
        switch (qualifier)
        {
            case EvqAttribute:
                return "in";
            case EvqVaryingIn:
                return "in";
            case EvqVaryingOut:
                return "out";
            default:
                break;
        }
    }
    return sh::getQualifierString(qualifier);
}

void TOutputGLSLBase::writeVariableType(const TType &type)
{
    TQualifier qualifier = type.getQualifier();
    TInfoSinkBase &out   = objSink();
    if (type.isInvariant())
    {
        writeInvariantQualifier(type);
    }
    if (type.getBasicType() == EbtInterfaceBlock)
    {
        TInterfaceBlock *interfaceBlock = type.getInterfaceBlock();
        declareInterfaceBlockLayout(interfaceBlock);
    }
    if (qualifier != EvqTemporary && qualifier != EvqGlobal)
    {
        const char *qualifierString = mapQualifierToString(qualifier);
        if (qualifierString && qualifierString[0] != '\0')
        {
            out << qualifierString << " ";
        }
    }

    const TMemoryQualifier &memoryQualifier = type.getMemoryQualifier();
    if (memoryQualifier.readonly)
    {
        ASSERT(IsImage(type.getBasicType()));
        out << "readonly ";
    }

    if (memoryQualifier.writeonly)
    {
        ASSERT(IsImage(type.getBasicType()));
        out << "writeonly ";
    }

    if (memoryQualifier.coherent)
    {
        ASSERT(IsImage(type.getBasicType()));
        out << "coherent ";
    }

    if (memoryQualifier.restrictQualifier)
    {
        ASSERT(IsImage(type.getBasicType()));
        out << "restrict ";
    }

    if (memoryQualifier.volatileQualifier)
    {
        ASSERT(IsImage(type.getBasicType()));
        out << "volatile ";
    }

    // Declare the struct if we have not done so already.
    if (type.getBasicType() == EbtStruct && !structDeclared(type.getStruct()))
    {
        TStructure *structure = type.getStruct();

        declareStruct(structure);

        if (!structure->name().empty())
        {
            mDeclaredStructs.insert(structure->uniqueId());
        }
    }
    else if (type.getBasicType() == EbtInterfaceBlock)
    {
        TInterfaceBlock *interfaceBlock = type.getInterfaceBlock();
        declareInterfaceBlock(interfaceBlock);
    }
    else
    {
        if (writeVariablePrecision(type.getPrecision()))
            out << " ";
        out << getTypeName(type);
    }
}

void TOutputGLSLBase::writeFunctionParameters(const TIntermSequence &args)
{
    TInfoSinkBase &out = objSink();
    for (TIntermSequence::const_iterator iter = args.begin(); iter != args.end(); ++iter)
    {
        const TIntermSymbol *arg = (*iter)->getAsSymbolNode();
        ASSERT(arg != NULL);

        const TType &type = arg->getType();
        writeVariableType(type);

        if (!arg->getName().getString().empty())
            out << " " << hashName(arg->getName());
        if (type.isArray())
            out << arrayBrackets(type);

        // Put a comma if this is not the last argument.
        if (iter != args.end() - 1)
            out << ", ";
    }
}

const TConstantUnion *TOutputGLSLBase::writeConstantUnion(const TType &type,
                                                          const TConstantUnion *pConstUnion)
{
    TInfoSinkBase &out = objSink();

    if (type.getBasicType() == EbtStruct)
    {
        const TStructure *structure = type.getStruct();
        out << hashName(TName(structure->name())) << "(";

        const TFieldList &fields = structure->fields();
        for (size_t i = 0; i < fields.size(); ++i)
        {
            const TType *fieldType = fields[i]->type();
            ASSERT(fieldType != NULL);
            pConstUnion = writeConstantUnion(*fieldType, pConstUnion);
            if (i != fields.size() - 1)
                out << ", ";
        }
        out << ")";
    }
    else
    {
        size_t size    = type.getObjectSize();
        bool writeType = size > 1;
        if (writeType)
            out << getTypeName(type) << "(";
        for (size_t i = 0; i < size; ++i, ++pConstUnion)
        {
            switch (pConstUnion->getType())
            {
                case EbtFloat:
                    writeFloat(out, pConstUnion->getFConst());
                    break;
                case EbtInt:
                    out << pConstUnion->getIConst();
                    break;
                case EbtUInt:
                    out << pConstUnion->getUConst() << "u";
                    break;
                case EbtBool:
                    out << pConstUnion->getBConst();
                    break;
                case EbtYuvCscStandardEXT:
                    out << getYuvCscStandardEXTString(pConstUnion->getYuvCscStandardEXTConst());
                    break;
                default:
                    UNREACHABLE();
            }
            if (i != size - 1)
                out << ", ";
        }
        if (writeType)
            out << ")";
    }
    return pConstUnion;
}

void TOutputGLSLBase::writeConstructorTriplet(Visit visit, const TType &type)
{
    TInfoSinkBase &out = objSink();
    if (visit == PreVisit)
    {
        if (type.isArray())
        {
            out << getTypeName(type);
            out << arrayBrackets(type);
            out << "(";
        }
        else
        {
            out << getTypeName(type) << "(";
        }
    }
    else
    {
        writeTriplet(visit, nullptr, ", ", ")");
    }
}

void TOutputGLSLBase::visitSymbol(TIntermSymbol *node)
{
    TInfoSinkBase &out = objSink();
    out << hashVariableName(node->getName());

    if (mDeclaringVariables && node->getType().isArray())
        out << arrayBrackets(node->getType());
}

void TOutputGLSLBase::visitConstantUnion(TIntermConstantUnion *node)
{
    writeConstantUnion(node->getType(), node->getUnionArrayPointer());
}

bool TOutputGLSLBase::visitSwizzle(Visit visit, TIntermSwizzle *node)
{
    TInfoSinkBase &out = objSink();
    if (visit == PostVisit)
    {
        out << ".";
        node->writeOffsetsAsXYZW(&out);
    }
    return true;
}

bool TOutputGLSLBase::visitBinary(Visit visit, TIntermBinary *node)
{
    bool visitChildren = true;
    TInfoSinkBase &out = objSink();
    switch (node->getOp())
    {
        case EOpComma:
            writeTriplet(visit, "(", ", ", ")");
            break;
        case EOpInitialize:
            if (visit == InVisit)
            {
                out << " = ";
                // RHS of initialize is not being declared.
                mDeclaringVariables = false;
            }
            break;
        case EOpAssign:
            writeTriplet(visit, "(", " = ", ")");
            break;
        case EOpAddAssign:
            writeTriplet(visit, "(", " += ", ")");
            break;
        case EOpSubAssign:
            writeTriplet(visit, "(", " -= ", ")");
            break;
        case EOpDivAssign:
            writeTriplet(visit, "(", " /= ", ")");
            break;
        case EOpIModAssign:
            writeTriplet(visit, "(", " %= ", ")");
            break;
        // Notice the fall-through.
        case EOpMulAssign:
        case EOpVectorTimesMatrixAssign:
        case EOpVectorTimesScalarAssign:
        case EOpMatrixTimesScalarAssign:
        case EOpMatrixTimesMatrixAssign:
            writeTriplet(visit, "(", " *= ", ")");
            break;
        case EOpBitShiftLeftAssign:
            writeTriplet(visit, "(", " <<= ", ")");
            break;
        case EOpBitShiftRightAssign:
            writeTriplet(visit, "(", " >>= ", ")");
            break;
        case EOpBitwiseAndAssign:
            writeTriplet(visit, "(", " &= ", ")");
            break;
        case EOpBitwiseXorAssign:
            writeTriplet(visit, "(", " ^= ", ")");
            break;
        case EOpBitwiseOrAssign:
            writeTriplet(visit, "(", " |= ", ")");
            break;

        case EOpIndexDirect:
            writeTriplet(visit, NULL, "[", "]");
            break;
        case EOpIndexIndirect:
            if (node->getAddIndexClamp())
            {
                if (visit == InVisit)
                {
                    if (mClampingStrategy == SH_CLAMP_WITH_CLAMP_INTRINSIC)
                        out << "[int(clamp(float(";
                    else
                        out << "[webgl_int_clamp(";
                }
                else if (visit == PostVisit)
                {
                    int maxSize;
                    TIntermTyped *left = node->getLeft();
                    TType leftType     = left->getType();

                    if (left->isArray())
                    {
                        // The shader will fail validation if the array length is not > 0.
                        maxSize = static_cast<int>(leftType.getArraySize()) - 1;
                    }
                    else
                    {
                        maxSize = leftType.getNominalSize() - 1;
                    }

                    if (mClampingStrategy == SH_CLAMP_WITH_CLAMP_INTRINSIC)
                        out << "), 0.0, float(" << maxSize << ")))]";
                    else
                        out << ", 0, " << maxSize << ")]";
                }
            }
            else
            {
                writeTriplet(visit, NULL, "[", "]");
            }
            break;
        case EOpIndexDirectStruct:
            if (visit == InVisit)
            {
                // Here we are writing out "foo.bar", where "foo" is struct
                // and "bar" is field. In AST, it is represented as a binary
                // node, where left child represents "foo" and right child "bar".
                // The node itself represents ".". The struct field "bar" is
                // actually stored as an index into TStructure::fields.
                out << ".";
                const TStructure *structure       = node->getLeft()->getType().getStruct();
                const TIntermConstantUnion *index = node->getRight()->getAsConstantUnion();
                const TField *field               = structure->fields()[index->getIConst(0)];

                TString fieldName = field->name();
                if (!mSymbolTable.findBuiltIn(structure->name(), mShaderVersion))
                    fieldName = hashName(TName(fieldName));

                out << fieldName;
                visitChildren = false;
            }
            break;
        case EOpIndexDirectInterfaceBlock:
            if (visit == InVisit)
            {
                out << ".";
                const TInterfaceBlock *interfaceBlock =
                    node->getLeft()->getType().getInterfaceBlock();
                const TIntermConstantUnion *index = node->getRight()->getAsConstantUnion();
                const TField *field               = interfaceBlock->fields()[index->getIConst(0)];

                TString fieldName = field->name();
                ASSERT(!mSymbolTable.findBuiltIn(interfaceBlock->name(), mShaderVersion));
                fieldName = hashName(TName(fieldName));

                out << fieldName;
                visitChildren = false;
            }
            break;

        case EOpAdd:
            writeTriplet(visit, "(", " + ", ")");
            break;
        case EOpSub:
            writeTriplet(visit, "(", " - ", ")");
            break;
        case EOpMul:
            writeTriplet(visit, "(", " * ", ")");
            break;
        case EOpDiv:
            writeTriplet(visit, "(", " / ", ")");
            break;
        case EOpIMod:
            writeTriplet(visit, "(", " % ", ")");
            break;
        case EOpBitShiftLeft:
            writeTriplet(visit, "(", " << ", ")");
            break;
        case EOpBitShiftRight:
            writeTriplet(visit, "(", " >> ", ")");
            break;
        case EOpBitwiseAnd:
            writeTriplet(visit, "(", " & ", ")");
            break;
        case EOpBitwiseXor:
            writeTriplet(visit, "(", " ^ ", ")");
            break;
        case EOpBitwiseOr:
            writeTriplet(visit, "(", " | ", ")");
            break;

        case EOpEqual:
            writeTriplet(visit, "(", " == ", ")");
            break;
        case EOpNotEqual:
            writeTriplet(visit, "(", " != ", ")");
            break;
        case EOpLessThan:
            writeTriplet(visit, "(", " < ", ")");
            break;
        case EOpGreaterThan:
            writeTriplet(visit, "(", " > ", ")");
            break;
        case EOpLessThanEqual:
            writeTriplet(visit, "(", " <= ", ")");
            break;
        case EOpGreaterThanEqual:
            writeTriplet(visit, "(", " >= ", ")");
            break;

        // Notice the fall-through.
        case EOpVectorTimesScalar:
        case EOpVectorTimesMatrix:
        case EOpMatrixTimesVector:
        case EOpMatrixTimesScalar:
        case EOpMatrixTimesMatrix:
            writeTriplet(visit, "(", " * ", ")");
            break;

        case EOpLogicalOr:
            writeTriplet(visit, "(", " || ", ")");
            break;
        case EOpLogicalXor:
            writeTriplet(visit, "(", " ^^ ", ")");
            break;
        case EOpLogicalAnd:
            writeTriplet(visit, "(", " && ", ")");
            break;
        default:
            UNREACHABLE();
    }

    return visitChildren;
}

bool TOutputGLSLBase::visitUnary(Visit visit, TIntermUnary *node)
{
    TString preString;
    TString postString = ")";

    switch (node->getOp())
    {
        case EOpNegative:
            preString = "(-";
            break;
        case EOpPositive:
            preString = "(+";
            break;
        case EOpLogicalNot:
            preString = "(!";
            break;
        case EOpBitwiseNot:
            preString = "(~";
            break;

        case EOpPostIncrement:
            preString  = "(";
            postString = "++)";
            break;
        case EOpPostDecrement:
            preString  = "(";
            postString = "--)";
            break;
        case EOpPreIncrement:
            preString = "(++";
            break;
        case EOpPreDecrement:
            preString = "(--";
            break;

        case EOpRadians:
        case EOpDegrees:
        case EOpSin:
        case EOpCos:
        case EOpTan:
        case EOpAsin:
        case EOpAcos:
        case EOpAtan:
        case EOpSinh:
        case EOpCosh:
        case EOpTanh:
        case EOpAsinh:
        case EOpAcosh:
        case EOpAtanh:
        case EOpExp:
        case EOpLog:
        case EOpExp2:
        case EOpLog2:
        case EOpSqrt:
        case EOpInverseSqrt:
        case EOpAbs:
        case EOpSign:
        case EOpFloor:
        case EOpTrunc:
        case EOpRound:
        case EOpRoundEven:
        case EOpCeil:
        case EOpFract:
        case EOpIsNan:
        case EOpIsInf:
        case EOpFloatBitsToInt:
        case EOpFloatBitsToUint:
        case EOpIntBitsToFloat:
        case EOpUintBitsToFloat:
        case EOpPackSnorm2x16:
        case EOpPackUnorm2x16:
        case EOpPackHalf2x16:
        case EOpUnpackSnorm2x16:
        case EOpUnpackUnorm2x16:
        case EOpUnpackHalf2x16:
        case EOpPackUnorm4x8:
        case EOpPackSnorm4x8:
        case EOpUnpackUnorm4x8:
        case EOpUnpackSnorm4x8:
        case EOpLength:
        case EOpNormalize:
        case EOpDFdx:
        case EOpDFdy:
        case EOpFwidth:
        case EOpTranspose:
        case EOpDeterminant:
        case EOpInverse:
        case EOpAny:
        case EOpAll:
        case EOpLogicalNotComponentWise:
        case EOpBitfieldReverse:
        case EOpBitCount:
        case EOpFindLSB:
        case EOpFindMSB:
            writeBuiltInFunctionTriplet(visit, node->getOp(), node->getUseEmulatedFunction());
            return true;
        default:
            UNREACHABLE();
    }

    writeTriplet(visit, preString.c_str(), NULL, postString.c_str());

    return true;
}

bool TOutputGLSLBase::visitTernary(Visit visit, TIntermTernary *node)
{
    TInfoSinkBase &out = objSink();
    // Notice two brackets at the beginning and end. The outer ones
    // encapsulate the whole ternary expression. This preserves the
    // order of precedence when ternary expressions are used in a
    // compound expression, i.e., c = 2 * (a < b ? 1 : 2).
    out << "((";
    node->getCondition()->traverse(this);
    out << ") ? (";
    node->getTrueExpression()->traverse(this);
    out << ") : (";
    node->getFalseExpression()->traverse(this);
    out << "))";
    return false;
}

bool TOutputGLSLBase::visitIfElse(Visit visit, TIntermIfElse *node)
{
    TInfoSinkBase &out = objSink();

    out << "if (";
    node->getCondition()->traverse(this);
    out << ")\n";

    visitCodeBlock(node->getTrueBlock());

    if (node->getFalseBlock())
    {
        out << "else\n";
        visitCodeBlock(node->getFalseBlock());
    }
    return false;
}

bool TOutputGLSLBase::visitSwitch(Visit visit, TIntermSwitch *node)
{
    if (node->getStatementList())
    {
        writeTriplet(visit, "switch (", ") ", nullptr);
        // The curly braces get written when visiting the statementList aggregate
    }
    else
    {
        // No statementList, so it won't output curly braces
        writeTriplet(visit, "switch (", ") {", "}\n");
    }
    return true;
}

bool TOutputGLSLBase::visitCase(Visit visit, TIntermCase *node)
{
    if (node->hasCondition())
    {
        writeTriplet(visit, "case (", nullptr, "):\n");
        return true;
    }
    else
    {
        TInfoSinkBase &out = objSink();
        out << "default:\n";
        return false;
    }
}

bool TOutputGLSLBase::visitBlock(Visit visit, TIntermBlock *node)
{
    TInfoSinkBase &out = objSink();
    // Scope the blocks except when at the global scope.
    if (mDepth > 0)
    {
        out << "{\n";
    }

    for (TIntermSequence::const_iterator iter = node->getSequence()->begin();
         iter != node->getSequence()->end(); ++iter)
    {
        TIntermNode *curNode = *iter;
        ASSERT(curNode != nullptr);
        curNode->traverse(this);

        if (isSingleStatement(curNode))
            out << ";\n";
    }

    // Scope the blocks except when at the global scope.
    if (mDepth > 0)
    {
        out << "}\n";
    }
    return false;
}

bool TOutputGLSLBase::visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *node)
{
    TIntermFunctionPrototype *prototype = node->getFunctionPrototype();
    prototype->traverse(this);
    visitCodeBlock(node->getBody());

    // Fully processed; no need to visit children.
    return false;
}

bool TOutputGLSLBase::visitInvariantDeclaration(Visit visit, TIntermInvariantDeclaration *node)
{
    TInfoSinkBase &out = objSink();
    ASSERT(visit == PreVisit);
    const TIntermSymbol *symbol = node->getSymbol();
    out << "invariant " << hashVariableName(symbol->getName());
    return false;
}

bool TOutputGLSLBase::visitFunctionPrototype(Visit visit, TIntermFunctionPrototype *node)
{
    TInfoSinkBase &out = objSink();
    ASSERT(visit == PreVisit);

    const TType &type = node->getType();
    writeVariableType(type);
    if (type.isArray())
        out << arrayBrackets(type);

    out << " " << hashFunctionNameIfNeeded(*node->getFunctionSymbolInfo());

    out << "(";
    writeFunctionParameters(*(node->getSequence()));
    out << ")";

    return false;
}

bool TOutputGLSLBase::visitAggregate(Visit visit, TIntermAggregate *node)
{
    bool visitChildren       = true;
    TInfoSinkBase &out       = objSink();
    switch (node->getOp())
    {
        case EOpCallFunctionInAST:
        case EOpCallInternalRawFunction:
        case EOpCallBuiltInFunction:
            // Function call.
            if (visit == PreVisit)
            {
                if (node->getOp() == EOpCallBuiltInFunction)
                {
                    out << translateTextureFunction(node->getFunctionSymbolInfo()->getName());
                }
                else
                {
                    out << hashFunctionNameIfNeeded(*node->getFunctionSymbolInfo());
                }
                out << "(";
            }
            else if (visit == InVisit)
                out << ", ";
            else
                out << ")";
            break;
        case EOpConstructFloat:
        case EOpConstructVec2:
        case EOpConstructVec3:
        case EOpConstructVec4:
        case EOpConstructBool:
        case EOpConstructBVec2:
        case EOpConstructBVec3:
        case EOpConstructBVec4:
        case EOpConstructInt:
        case EOpConstructIVec2:
        case EOpConstructIVec3:
        case EOpConstructIVec4:
        case EOpConstructUInt:
        case EOpConstructUVec2:
        case EOpConstructUVec3:
        case EOpConstructUVec4:
        case EOpConstructMat2:
        case EOpConstructMat2x3:
        case EOpConstructMat2x4:
        case EOpConstructMat3x2:
        case EOpConstructMat3:
        case EOpConstructMat3x4:
        case EOpConstructMat4x2:
        case EOpConstructMat4x3:
        case EOpConstructMat4:
        case EOpConstructStruct:
            writeConstructorTriplet(visit, node->getType());
            break;

        case EOpEqualComponentWise:
        case EOpNotEqualComponentWise:
        case EOpLessThanComponentWise:
        case EOpGreaterThanComponentWise:
        case EOpLessThanEqualComponentWise:
        case EOpGreaterThanEqualComponentWise:
        case EOpMod:
        case EOpModf:
        case EOpPow:
        case EOpAtan:
        case EOpMin:
        case EOpMax:
        case EOpClamp:
        case EOpMix:
        case EOpStep:
        case EOpSmoothStep:
        case EOpFrexp:
        case EOpLdexp:
        case EOpDistance:
        case EOpDot:
        case EOpCross:
        case EOpFaceForward:
        case EOpReflect:
        case EOpRefract:
        case EOpMulMatrixComponentWise:
        case EOpOuterProduct:
        case EOpBitfieldExtract:
        case EOpBitfieldInsert:
        case EOpUaddCarry:
        case EOpUsubBorrow:
        case EOpUmulExtended:
        case EOpImulExtended:
        case EOpBarrier:
        case EOpMemoryBarrier:
        case EOpMemoryBarrierAtomicCounter:
        case EOpMemoryBarrierBuffer:
        case EOpMemoryBarrierImage:
        case EOpMemoryBarrierShared:
        case EOpGroupMemoryBarrier:
            writeBuiltInFunctionTriplet(visit, node->getOp(), node->getUseEmulatedFunction());
            break;
        default:
            UNREACHABLE();
    }
    return visitChildren;
}

bool TOutputGLSLBase::visitDeclaration(Visit visit, TIntermDeclaration *node)
{
    TInfoSinkBase &out = objSink();

    // Variable declaration.
    if (visit == PreVisit)
    {
        const TIntermSequence &sequence = *(node->getSequence());
        const TIntermTyped *variable    = sequence.front()->getAsTyped();
        writeLayoutQualifier(variable->getType());
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
    return true;
}

bool TOutputGLSLBase::visitLoop(Visit visit, TIntermLoop *node)
{
    TInfoSinkBase &out = objSink();

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

        visitCodeBlock(node->getBody());
    }
    else if (loopType == ELoopWhile)  // while loop
    {
        out << "while (";
        ASSERT(node->getCondition() != NULL);
        node->getCondition()->traverse(this);
        out << ")\n";

        visitCodeBlock(node->getBody());
    }
    else  // do-while loop
    {
        ASSERT(loopType == ELoopDoWhile);
        out << "do\n";

        visitCodeBlock(node->getBody());

        out << "while (";
        ASSERT(node->getCondition() != NULL);
        node->getCondition()->traverse(this);
        out << ");\n";
    }

    // No need to visit children. They have been already processed in
    // this function.
    return false;
}

bool TOutputGLSLBase::visitBranch(Visit visit, TIntermBranch *node)
{
    switch (node->getFlowOp())
    {
        case EOpKill:
            writeTriplet(visit, "discard", NULL, NULL);
            break;
        case EOpBreak:
            writeTriplet(visit, "break", NULL, NULL);
            break;
        case EOpContinue:
            writeTriplet(visit, "continue", NULL, NULL);
            break;
        case EOpReturn:
            writeTriplet(visit, "return ", NULL, NULL);
            break;
        default:
            UNREACHABLE();
    }

    return true;
}

void TOutputGLSLBase::visitCodeBlock(TIntermBlock *node)
{
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

TString TOutputGLSLBase::getTypeName(const TType &type)
{
    if (type.getBasicType() == EbtStruct)
        return hashName(TName(type.getStruct()->name()));
    else
        return type.getBuiltInTypeNameString();
}

TString TOutputGLSLBase::hashName(const TName &name)
{
    if (name.getString().empty())
    {
        ASSERT(!name.isInternal());
        return name.getString();
    }
    if (name.isInternal())
    {
        // TODO(oetuaho): Would be nicer to prefix non-internal names with "_" instead, like is
        // done in the HLSL output, but that requires fairly complex changes elsewhere in the code
        // as well.
        // We need to use a prefix that is reserved in WebGL in order to guarantee that the internal
        // names don't conflict with user-defined names from WebGL.
        return "webgl_angle_" + name.getString();
    }
    if (mHashFunction == nullptr)
    {
        return name.getString();
    }
    NameMap::const_iterator it = mNameMap.find(name.getString().c_str());
    if (it != mNameMap.end())
        return it->second.c_str();
    TString hashedName                 = TIntermTraverser::hash(name.getString(), mHashFunction);
    mNameMap[name.getString().c_str()] = hashedName.c_str();
    return hashedName;
}

TString TOutputGLSLBase::hashVariableName(const TName &name)
{
    if (mSymbolTable.findBuiltIn(name.getString(), mShaderVersion) != NULL)
    {
        if (mCompileOptions & SH_TRANSLATE_VIEWID_OVR_TO_UNIFORM &&
            name.getString() == "gl_ViewID_OVR")
        {
            TName uniformName(TString("ViewID_OVR"));
            uniformName.setInternal(true);
            return hashName(uniformName);
        }
        return name.getString();
    }
    return hashName(name);
}

TString TOutputGLSLBase::hashFunctionNameIfNeeded(const TFunctionSymbolInfo &info)
{
    if (info.isMain() || info.getNameObj().isInternal())
    {
        // Internal function names are outputted as-is - they may refer to functions manually added
        // to the output shader source that are not included in the AST at all.
        return info.getName();
    }
    else
    {
        return hashName(info.getNameObj());
    }
}

bool TOutputGLSLBase::structDeclared(const TStructure *structure) const
{
    ASSERT(structure);
    if (structure->name().empty())
    {
        return false;
    }

    return (mDeclaredStructs.count(structure->uniqueId()) > 0);
}

void TOutputGLSLBase::declareStruct(const TStructure *structure)
{
    TInfoSinkBase &out = objSink();

    out << "struct " << hashName(TName(structure->name())) << "{\n";
    const TFieldList &fields = structure->fields();
    for (size_t i = 0; i < fields.size(); ++i)
    {
        const TField *field = fields[i];
        if (writeVariablePrecision(field->type()->getPrecision()))
            out << " ";
        out << getTypeName(*field->type()) << " " << hashName(TName(field->name()));
        if (field->type()->isArray())
            out << arrayBrackets(*field->type());
        out << ";\n";
    }
    out << "}";
}

void TOutputGLSLBase::declareInterfaceBlockLayout(const TInterfaceBlock *interfaceBlock)
{
    TInfoSinkBase &out = objSink();

    out << "layout(";

    switch (interfaceBlock->blockStorage())
    {
        case EbsUnspecified:
        case EbsShared:
            // Default block storage is shared.
            out << "shared";
            break;

        case EbsPacked:
            out << "packed";
            break;

        case EbsStd140:
            out << "std140";
            break;

        default:
            UNREACHABLE();
            break;
    }

    out << ", ";

    switch (interfaceBlock->matrixPacking())
    {
        case EmpUnspecified:
        case EmpColumnMajor:
            // Default matrix packing is column major.
            out << "column_major";
            break;

        case EmpRowMajor:
            out << "row_major";
            break;

        default:
            UNREACHABLE();
            break;
    }

    out << ") ";
}

void TOutputGLSLBase::declareInterfaceBlock(const TInterfaceBlock *interfaceBlock)
{
    TInfoSinkBase &out = objSink();

    out << hashName(TName(interfaceBlock->name())) << "{\n";
    const TFieldList &fields = interfaceBlock->fields();
    for (size_t i = 0; i < fields.size(); ++i)
    {
        const TField *field = fields[i];
        if (writeVariablePrecision(field->type()->getPrecision()))
            out << " ";
        out << getTypeName(*field->type()) << " " << hashName(TName(field->name()));
        if (field->type()->isArray())
            out << arrayBrackets(*field->type());
        out << ";\n";
    }
    out << "}";
}

}  // namespace sh
