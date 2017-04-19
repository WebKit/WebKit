//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/ParseContext.h"

#include <stdarg.h>
#include <stdio.h>

#include "compiler/preprocessor/SourceLocation.h"
#include "compiler/translator/Cache.h"
#include "compiler/translator/glslang.h"
#include "compiler/translator/ValidateSwitch.h"
#include "compiler/translator/ValidateGlobalInitializer.h"
#include "compiler/translator/util.h"

namespace sh
{

///////////////////////////////////////////////////////////////////////
//
// Sub- vector and matrix fields
//
////////////////////////////////////////////////////////////////////////

namespace
{

const int kWebGLMaxStructNesting = 4;

bool ContainsSampler(const TType &type)
{
    if (IsSampler(type.getBasicType()))
        return true;

    if (type.getBasicType() == EbtStruct || type.isInterfaceBlock())
    {
        const TFieldList &fields = type.getStruct()->fields();
        for (unsigned int i = 0; i < fields.size(); ++i)
        {
            if (ContainsSampler(*fields[i]->type()))
                return true;
        }
    }

    return false;
}

bool ContainsImage(const TType &type)
{
    if (IsImage(type.getBasicType()))
        return true;

    if (type.getBasicType() == EbtStruct || type.isInterfaceBlock())
    {
        const TFieldList &fields = type.getStruct()->fields();
        for (unsigned int i = 0; i < fields.size(); ++i)
        {
            if (ContainsImage(*fields[i]->type()))
                return true;
        }
    }

    return false;
}

// Get a token from an image argument to use as an error message token.
const char *GetImageArgumentToken(TIntermTyped *imageNode)
{
    ASSERT(IsImage(imageNode->getBasicType()));
    while (imageNode->getAsBinaryNode() &&
           (imageNode->getAsBinaryNode()->getOp() == EOpIndexIndirect ||
            imageNode->getAsBinaryNode()->getOp() == EOpIndexDirect))
    {
        imageNode = imageNode->getAsBinaryNode()->getLeft();
    }
    TIntermSymbol *imageSymbol = imageNode->getAsSymbolNode();
    if (imageSymbol)
    {
        return imageSymbol->getSymbol().c_str();
    }
    return "image";
}

}  // namespace

TParseContext::TParseContext(TSymbolTable &symt,
                             TExtensionBehavior &ext,
                             sh::GLenum type,
                             ShShaderSpec spec,
                             ShCompileOptions options,
                             bool checksPrecErrors,
                             TDiagnostics *diagnostics,
                             const ShBuiltInResources &resources)
    : intermediate(),
      symbolTable(symt),
      mDeferredSingleDeclarationErrorCheck(false),
      mShaderType(type),
      mShaderSpec(spec),
      mCompileOptions(options),
      mShaderVersion(100),
      mTreeRoot(nullptr),
      mLoopNestingLevel(0),
      mStructNestingLevel(0),
      mSwitchNestingLevel(0),
      mCurrentFunctionType(nullptr),
      mFunctionReturnsValue(false),
      mChecksPrecisionErrors(checksPrecErrors),
      mFragmentPrecisionHighOnESSL1(false),
      mDefaultMatrixPacking(EmpColumnMajor),
      mDefaultBlockStorage(sh::IsWebGLBasedSpec(spec) ? EbsStd140 : EbsShared),
      mDiagnostics(diagnostics),
      mDirectiveHandler(ext,
                        *mDiagnostics,
                        mShaderVersion,
                        mShaderType,
                        resources.WEBGL_debug_shader_precision == 1),
      mPreprocessor(mDiagnostics, &mDirectiveHandler, pp::PreprocessorSettings()),
      mScanner(nullptr),
      mUsesFragData(false),
      mUsesFragColor(false),
      mUsesSecondaryOutputs(false),
      mMinProgramTexelOffset(resources.MinProgramTexelOffset),
      mMaxProgramTexelOffset(resources.MaxProgramTexelOffset),
      mMultiviewAvailable(resources.OVR_multiview == 1),
      mComputeShaderLocalSizeDeclared(false),
      mNumViews(-1),
      mMaxNumViews(resources.MaxViewsOVR),
      mMaxImageUnits(resources.MaxImageUnits),
      mMaxCombinedTextureImageUnits(resources.MaxCombinedTextureImageUnits),
      mMaxUniformLocations(resources.MaxUniformLocations),
      mDeclaringFunction(false)
{
    mComputeShaderLocalSize.fill(-1);
}

//
// Look at a '.' field selector string and change it into offsets
// for a vector.
//
bool TParseContext::parseVectorFields(const TString &compString,
                                      int vecSize,
                                      TVectorFields &fields,
                                      const TSourceLoc &line)
{
    fields.num = (int)compString.size();
    if (fields.num > 4)
    {
        error(line, "illegal vector field selection", compString.c_str());
        return false;
    }

    enum
    {
        exyzw,
        ergba,
        estpq
    } fieldSet[4];

    for (int i = 0; i < fields.num; ++i)
    {
        switch (compString[i])
        {
            case 'x':
                fields.offsets[i] = 0;
                fieldSet[i]       = exyzw;
                break;
            case 'r':
                fields.offsets[i] = 0;
                fieldSet[i]       = ergba;
                break;
            case 's':
                fields.offsets[i] = 0;
                fieldSet[i]       = estpq;
                break;
            case 'y':
                fields.offsets[i] = 1;
                fieldSet[i]       = exyzw;
                break;
            case 'g':
                fields.offsets[i] = 1;
                fieldSet[i]       = ergba;
                break;
            case 't':
                fields.offsets[i] = 1;
                fieldSet[i]       = estpq;
                break;
            case 'z':
                fields.offsets[i] = 2;
                fieldSet[i]       = exyzw;
                break;
            case 'b':
                fields.offsets[i] = 2;
                fieldSet[i]       = ergba;
                break;
            case 'p':
                fields.offsets[i] = 2;
                fieldSet[i]       = estpq;
                break;

            case 'w':
                fields.offsets[i] = 3;
                fieldSet[i]       = exyzw;
                break;
            case 'a':
                fields.offsets[i] = 3;
                fieldSet[i]       = ergba;
                break;
            case 'q':
                fields.offsets[i] = 3;
                fieldSet[i]       = estpq;
                break;
            default:
                error(line, "illegal vector field selection", compString.c_str());
                return false;
        }
    }

    for (int i = 0; i < fields.num; ++i)
    {
        if (fields.offsets[i] >= vecSize)
        {
            error(line, "vector field selection out of range", compString.c_str());
            return false;
        }

        if (i > 0)
        {
            if (fieldSet[i] != fieldSet[i - 1])
            {
                error(line, "illegal - vector component fields not from the same set",
                      compString.c_str());
                return false;
            }
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////////////
//
// Errors
//
////////////////////////////////////////////////////////////////////////

//
// Used by flex/bison to output all syntax and parsing errors.
//
void TParseContext::error(const TSourceLoc &loc, const char *reason, const char *token)
{
    mDiagnostics->error(loc, reason, token);
}

void TParseContext::warning(const TSourceLoc &loc, const char *reason, const char *token)
{
    mDiagnostics->warning(loc, reason, token);
}

void TParseContext::outOfRangeError(bool isError,
                                    const TSourceLoc &loc,
                                    const char *reason,
                                    const char *token)
{
    if (isError)
    {
        error(loc, reason, token);
    }
    else
    {
        warning(loc, reason, token);
    }
}

//
// Same error message for all places assignments don't work.
//
void TParseContext::assignError(const TSourceLoc &line, const char *op, TString left, TString right)
{
    std::stringstream reasonStream;
    reasonStream << "cannot convert from '" << right << "' to '" << left << "'";
    std::string reason = reasonStream.str();
    error(line, reason.c_str(), op);
}

//
// Same error message for all places unary operations don't work.
//
void TParseContext::unaryOpError(const TSourceLoc &line, const char *op, TString operand)
{
    std::stringstream reasonStream;
    reasonStream << "wrong operand type - no operation '" << op
                 << "' exists that takes an operand of type " << operand
                 << " (or there is no acceptable conversion)";
    std::string reason = reasonStream.str();
    error(line, reason.c_str(), op);
}

//
// Same error message for all binary operations don't work.
//
void TParseContext::binaryOpError(const TSourceLoc &line,
                                  const char *op,
                                  TString left,
                                  TString right)
{
    std::stringstream reasonStream;
    reasonStream << "wrong operand types - no operation '" << op
                 << "' exists that takes a left-hand operand of type '" << left
                 << "' and a right operand of type '" << right
                 << "' (or there is no acceptable conversion)";
    std::string reason = reasonStream.str();
    error(line, reason.c_str(), op);
}

void TParseContext::checkPrecisionSpecified(const TSourceLoc &line,
                                            TPrecision precision,
                                            TBasicType type)
{
    if (!mChecksPrecisionErrors)
        return;

    if (precision != EbpUndefined && !SupportsPrecision(type))
    {
        error(line, "illegal type for precision qualifier", getBasicString(type));
    }

    if (precision == EbpUndefined)
    {
        switch (type)
        {
            case EbtFloat:
                error(line, "No precision specified for (float)", "");
                return;
            case EbtInt:
            case EbtUInt:
                UNREACHABLE();  // there's always a predeclared qualifier
                error(line, "No precision specified (int)", "");
                return;
            default:
                if (IsSampler(type))
                {
                    error(line, "No precision specified (sampler)", "");
                    return;
                }
                if (IsImage(type))
                {
                    error(line, "No precision specified (image)", "");
                    return;
                }
        }
    }
}

// Both test and if necessary, spit out an error, to see if the node is really
// an l-value that can be operated on this way.
bool TParseContext::checkCanBeLValue(const TSourceLoc &line, const char *op, TIntermTyped *node)
{
    TIntermSymbol *symNode      = node->getAsSymbolNode();
    TIntermBinary *binaryNode   = node->getAsBinaryNode();
    TIntermSwizzle *swizzleNode = node->getAsSwizzleNode();

    if (swizzleNode)
    {
        bool ok = checkCanBeLValue(line, op, swizzleNode->getOperand());
        if (ok && swizzleNode->hasDuplicateOffsets())
        {
            error(line, " l-value of swizzle cannot have duplicate components", op);
            return false;
        }
        return ok;
    }

    if (binaryNode)
    {
        switch (binaryNode->getOp())
        {
            case EOpIndexDirect:
            case EOpIndexIndirect:
            case EOpIndexDirectStruct:
            case EOpIndexDirectInterfaceBlock:
                return checkCanBeLValue(line, op, binaryNode->getLeft());
            default:
                break;
        }
        error(line, " l-value required", op);
        return false;
    }

    const char *message = 0;
    switch (node->getQualifier())
    {
        case EvqConst:
            message = "can't modify a const";
            break;
        case EvqConstReadOnly:
            message = "can't modify a const";
            break;
        case EvqAttribute:
            message = "can't modify an attribute";
            break;
        case EvqFragmentIn:
            message = "can't modify an input";
            break;
        case EvqVertexIn:
            message = "can't modify an input";
            break;
        case EvqUniform:
            message = "can't modify a uniform";
            break;
        case EvqVaryingIn:
            message = "can't modify a varying";
            break;
        case EvqFragCoord:
            message = "can't modify gl_FragCoord";
            break;
        case EvqFrontFacing:
            message = "can't modify gl_FrontFacing";
            break;
        case EvqPointCoord:
            message = "can't modify gl_PointCoord";
            break;
        case EvqNumWorkGroups:
            message = "can't modify gl_NumWorkGroups";
            break;
        case EvqWorkGroupSize:
            message = "can't modify gl_WorkGroupSize";
            break;
        case EvqWorkGroupID:
            message = "can't modify gl_WorkGroupID";
            break;
        case EvqLocalInvocationID:
            message = "can't modify gl_LocalInvocationID";
            break;
        case EvqGlobalInvocationID:
            message = "can't modify gl_GlobalInvocationID";
            break;
        case EvqLocalInvocationIndex:
            message = "can't modify gl_LocalInvocationIndex";
            break;
        case EvqComputeIn:
            message = "can't modify work group size variable";
            break;
        default:
            //
            // Type that can't be written to?
            //
            if (node->getBasicType() == EbtVoid)
            {
                message = "can't modify void";
            }
            if (IsSampler(node->getBasicType()))
            {
                message = "can't modify a sampler";
            }
            if (IsImage(node->getBasicType()))
            {
                message = "can't modify an image";
            }
    }

    if (message == 0 && binaryNode == 0 && symNode == 0)
    {
        error(line, "l-value required", op);

        return false;
    }

    //
    // Everything else is okay, no error.
    //
    if (message == 0)
        return true;

    //
    // If we get here, we have an error and a message.
    //
    if (symNode)
    {
        const char *symbol = symNode->getSymbol().c_str();
        std::stringstream reasonStream;
        reasonStream << "l-value required (" << message << " \"" << symbol << "\")";
        std::string reason = reasonStream.str();
        error(line, reason.c_str(), op);
    }
    else
    {
        std::stringstream reasonStream;
        reasonStream << "l-value required (" << message << ")";
        std::string reason = reasonStream.str();
        error(line, reason.c_str(), op);
    }

    return false;
}

// Both test, and if necessary spit out an error, to see if the node is really
// a constant.
void TParseContext::checkIsConst(TIntermTyped *node)
{
    if (node->getQualifier() != EvqConst)
    {
        error(node->getLine(), "constant expression required", "");
    }
}

// Both test, and if necessary spit out an error, to see if the node is really
// an integer.
void TParseContext::checkIsScalarInteger(TIntermTyped *node, const char *token)
{
    if (!node->isScalarInt())
    {
        error(node->getLine(), "integer expression required", token);
    }
}

// Both test, and if necessary spit out an error, to see if we are currently
// globally scoped.
bool TParseContext::checkIsAtGlobalLevel(const TSourceLoc &line, const char *token)
{
    if (!symbolTable.atGlobalLevel())
    {
        error(line, "only allowed at global scope", token);
        return false;
    }
    return true;
}

// For now, keep it simple:  if it starts "gl_", it's reserved, independent
// of scope.  Except, if the symbol table is at the built-in push-level,
// which is when we are parsing built-ins.
// Also checks for "webgl_" and "_webgl_" reserved identifiers if parsing a
// webgl shader.
bool TParseContext::checkIsNotReserved(const TSourceLoc &line, const TString &identifier)
{
    static const char *reservedErrMsg = "reserved built-in name";
    if (!symbolTable.atBuiltInLevel())
    {
        if (identifier.compare(0, 3, "gl_") == 0)
        {
            error(line, reservedErrMsg, "gl_");
            return false;
        }
        if (sh::IsWebGLBasedSpec(mShaderSpec))
        {
            if (identifier.compare(0, 6, "webgl_") == 0)
            {
                error(line, reservedErrMsg, "webgl_");
                return false;
            }
            if (identifier.compare(0, 7, "_webgl_") == 0)
            {
                error(line, reservedErrMsg, "_webgl_");
                return false;
            }
        }
        if (identifier.find("__") != TString::npos)
        {
            error(line,
                  "identifiers containing two consecutive underscores (__) are reserved as "
                  "possible future keywords",
                  identifier.c_str());
            return false;
        }
    }

    return true;
}

// Make sure there is enough data provided to the constructor to build
// something of the type of the constructor.  Also returns the type of
// the constructor.
bool TParseContext::checkConstructorArguments(const TSourceLoc &line,
                                              const TIntermSequence *arguments,
                                              TOperator op,
                                              const TType &type)
{
    bool constructingMatrix = false;
    switch (op)
    {
        case EOpConstructMat2:
        case EOpConstructMat2x3:
        case EOpConstructMat2x4:
        case EOpConstructMat3x2:
        case EOpConstructMat3:
        case EOpConstructMat3x4:
        case EOpConstructMat4x2:
        case EOpConstructMat4x3:
        case EOpConstructMat4:
            constructingMatrix = true;
            break;
        default:
            break;
    }

    //
    // Note: It's okay to have too many components available, but not okay to have unused
    // arguments.  'full' will go to true when enough args have been seen.  If we loop
    // again, there is an extra argument, so 'overfull' will become true.
    //

    size_t size         = 0;
    bool full           = false;
    bool overFull       = false;
    bool matrixInMatrix = false;
    bool arrayArg       = false;
    for (TIntermNode *arg : *arguments)
    {
        const TIntermTyped *argTyped = arg->getAsTyped();
        size += argTyped->getType().getObjectSize();

        if (constructingMatrix && argTyped->getType().isMatrix())
            matrixInMatrix = true;
        if (full)
            overFull = true;
        if (op != EOpConstructStruct && !type.isArray() && size >= type.getObjectSize())
            full = true;
        if (argTyped->getType().isArray())
            arrayArg = true;
    }

    if (type.isArray())
    {
        // The size of an unsized constructor should already have been determined.
        ASSERT(!type.isUnsizedArray());
        if (static_cast<size_t>(type.getArraySize()) != arguments->size())
        {
            error(line, "array constructor needs one argument per array element", "constructor");
            return false;
        }
    }

    if (arrayArg && op != EOpConstructStruct)
    {
        error(line, "constructing from a non-dereferenced array", "constructor");
        return false;
    }

    if (matrixInMatrix && !type.isArray())
    {
        if (arguments->size() != 1)
        {
            error(line, "constructing matrix from matrix can only take one argument",
                  "constructor");
            return false;
        }
    }

    if (overFull)
    {
        error(line, "too many arguments", "constructor");
        return false;
    }

    if (op == EOpConstructStruct && !type.isArray() &&
        type.getStruct()->fields().size() != arguments->size())
    {
        error(line,
              "Number of constructor parameters does not match the number of structure fields",
              "constructor");
        return false;
    }

    if (!type.isMatrix() || !matrixInMatrix)
    {
        if ((op != EOpConstructStruct && size != 1 && size < type.getObjectSize()) ||
            (op == EOpConstructStruct && size < type.getObjectSize()))
        {
            error(line, "not enough data provided for construction", "constructor");
            return false;
        }
    }

    if (arguments->empty())
    {
        error(line, "constructor does not have any arguments", "constructor");
        return false;
    }

    for (TIntermNode *const &argNode : *arguments)
    {
        TIntermTyped *argTyped = argNode->getAsTyped();
        ASSERT(argTyped != nullptr);
        if (op != EOpConstructStruct && IsSampler(argTyped->getBasicType()))
        {
            error(line, "cannot convert a sampler", "constructor");
            return false;
        }
        if (op != EOpConstructStruct && IsImage(argTyped->getBasicType()))
        {
            error(line, "cannot convert an image", "constructor");
            return false;
        }
        if (argTyped->getBasicType() == EbtVoid)
        {
            error(line, "cannot convert a void", "constructor");
            return false;
        }
    }

    if (type.isArray())
    {
        // GLSL ES 3.00 section 5.4.4: Each argument must be the same type as the element type of
        // the array.
        for (TIntermNode *const &argNode : *arguments)
        {
            const TType &argType = argNode->getAsTyped()->getType();
            // It has already been checked that the argument is not an array, but we can arrive
            // here due to prior error conditions.
            if (argType.isArray())
            {
                return false;
            }
            if (!argType.sameElementType(type))
            {
                error(line, "Array constructor argument has an incorrect type", "constructor");
                return false;
            }
        }
    }
    else if (op == EOpConstructStruct)
    {
        const TFieldList &fields = type.getStruct()->fields();

        for (size_t i = 0; i < fields.size(); i++)
        {
            if (i >= arguments->size() ||
                (*arguments)[i]->getAsTyped()->getType() != *fields[i]->type())
            {
                error(line, "Structure constructor arguments do not match structure fields",
                      "constructor");
                return false;
            }
        }
    }

    return true;
}

// This function checks to see if a void variable has been declared and raise an error message for
// such a case
//
// returns true in case of an error
//
bool TParseContext::checkIsNonVoid(const TSourceLoc &line,
                                   const TString &identifier,
                                   const TBasicType &type)
{
    if (type == EbtVoid)
    {
        error(line, "illegal use of type 'void'", identifier.c_str());
        return false;
    }

    return true;
}

// This function checks to see if the node (for the expression) contains a scalar boolean expression
// or not.
void TParseContext::checkIsScalarBool(const TSourceLoc &line, const TIntermTyped *type)
{
    if (type->getBasicType() != EbtBool || type->isArray() || type->isMatrix() || type->isVector())
    {
        error(line, "boolean expression expected", "");
    }
}

// This function checks to see if the node (for the expression) contains a scalar boolean expression
// or not.
void TParseContext::checkIsScalarBool(const TSourceLoc &line, const TPublicType &pType)
{
    if (pType.getBasicType() != EbtBool || pType.isAggregate())
    {
        error(line, "boolean expression expected", "");
    }
}

bool TParseContext::checkIsNotSampler(const TSourceLoc &line,
                                      const TTypeSpecifierNonArray &pType,
                                      const char *reason)
{
    if (pType.type == EbtStruct)
    {
        if (ContainsSampler(*pType.userDef))
        {
            std::stringstream reasonStream;
            reasonStream << reason << " (structure contains a sampler)";
            std::string reasonStr = reasonStream.str();
            error(line, reasonStr.c_str(), getBasicString(pType.type));
            return false;
        }

        return true;
    }
    else if (IsSampler(pType.type))
    {
        error(line, reason, getBasicString(pType.type));
        return false;
    }

    return true;
}

bool TParseContext::checkIsNotImage(const TSourceLoc &line,
                                    const TTypeSpecifierNonArray &pType,
                                    const char *reason)
{
    if (pType.type == EbtStruct)
    {
        if (ContainsImage(*pType.userDef))
        {
            std::stringstream reasonStream;
            reasonStream << reason << " (structure contains an image)";
            std::string reasonStr = reasonStream.str();
            error(line, reasonStr.c_str(), getBasicString(pType.type));

            return false;
        }

        return true;
    }
    else if (IsImage(pType.type))
    {
        error(line, reason, getBasicString(pType.type));

        return false;
    }

    return true;
}

void TParseContext::checkDeclaratorLocationIsNotSpecified(const TSourceLoc &line,
                                                          const TPublicType &pType)
{
    if (pType.layoutQualifier.location != -1)
    {
        error(line, "location must only be specified for a single input or output variable",
              "location");
    }
}

void TParseContext::checkLocationIsNotSpecified(const TSourceLoc &location,
                                                const TLayoutQualifier &layoutQualifier)
{
    if (layoutQualifier.location != -1)
    {
        const char *errorMsg = "invalid layout qualifier: only valid on program inputs and outputs";
        if (mShaderVersion >= 310)
        {
            errorMsg =
                "invalid layout qualifier: only valid on program inputs, outputs, and uniforms";
        }
        error(location, errorMsg, "location");
    }
}

void TParseContext::checkOutParameterIsNotOpaqueType(const TSourceLoc &line,
                                                     TQualifier qualifier,
                                                     const TType &type)
{
    checkOutParameterIsNotSampler(line, qualifier, type);
    checkOutParameterIsNotImage(line, qualifier, type);
}

void TParseContext::checkOutParameterIsNotSampler(const TSourceLoc &line,
                                                  TQualifier qualifier,
                                                  const TType &type)
{
    ASSERT(qualifier == EvqOut || qualifier == EvqInOut);
    if (IsSampler(type.getBasicType()))
    {
        error(line, "samplers cannot be output parameters", type.getBasicString());
    }
}

void TParseContext::checkOutParameterIsNotImage(const TSourceLoc &line,
                                                TQualifier qualifier,
                                                const TType &type)
{
    ASSERT(qualifier == EvqOut || qualifier == EvqInOut);
    if (IsImage(type.getBasicType()))
    {
        error(line, "images cannot be output parameters", type.getBasicString());
    }
}

// Do size checking for an array type's size.
unsigned int TParseContext::checkIsValidArraySize(const TSourceLoc &line, TIntermTyped *expr)
{
    TIntermConstantUnion *constant = expr->getAsConstantUnion();

    // TODO(oetuaho@nvidia.com): Get rid of the constant == nullptr check here once all constant
    // expressions can be folded. Right now we don't allow constant expressions that ANGLE can't
    // fold as array size.
    if (expr->getQualifier() != EvqConst || constant == nullptr || !constant->isScalarInt())
    {
        error(line, "array size must be a constant integer expression", "");
        return 1u;
    }

    unsigned int size = 0u;

    if (constant->getBasicType() == EbtUInt)
    {
        size = constant->getUConst(0);
    }
    else
    {
        int signedSize = constant->getIConst(0);

        if (signedSize < 0)
        {
            error(line, "array size must be non-negative", "");
            return 1u;
        }

        size = static_cast<unsigned int>(signedSize);
    }

    if (size == 0u)
    {
        error(line, "array size must be greater than zero", "");
        return 1u;
    }

    // The size of arrays is restricted here to prevent issues further down the
    // compiler/translator/driver stack. Shader Model 5 generation hardware is limited to
    // 4096 registers so this should be reasonable even for aggressively optimizable code.
    const unsigned int sizeLimit = 65536;

    if (size > sizeLimit)
    {
        error(line, "array size too large", "");
        return 1u;
    }

    return size;
}

// See if this qualifier can be an array.
bool TParseContext::checkIsValidQualifierForArray(const TSourceLoc &line,
                                                  const TPublicType &elementQualifier)
{
    if ((elementQualifier.qualifier == EvqAttribute) ||
        (elementQualifier.qualifier == EvqVertexIn) ||
        (elementQualifier.qualifier == EvqConst && mShaderVersion < 300))
    {
        error(line, "cannot declare arrays of this qualifier",
              TType(elementQualifier).getQualifierString());
        return false;
    }

    return true;
}

// See if this element type can be formed into an array.
bool TParseContext::checkIsValidTypeForArray(const TSourceLoc &line, const TPublicType &elementType)
{
    //
    // Can the type be an array?
    //
    if (elementType.array)
    {
        error(line, "cannot declare arrays of arrays",
              TType(elementType).getCompleteString().c_str());
        return false;
    }
    // In ESSL1.00 shaders, structs cannot be varying (section 4.3.5). This is checked elsewhere.
    // In ESSL3.00 shaders, struct inputs/outputs are allowed but not arrays of structs (section
    // 4.3.4).
    if (mShaderVersion >= 300 && elementType.getBasicType() == EbtStruct &&
        sh::IsVarying(elementType.qualifier))
    {
        error(line, "cannot declare arrays of structs of this qualifier",
              TType(elementType).getCompleteString().c_str());
        return false;
    }

    return true;
}

// Check if this qualified element type can be formed into an array.
bool TParseContext::checkIsValidTypeAndQualifierForArray(const TSourceLoc &indexLocation,
                                                         const TPublicType &elementType)
{
    if (checkIsValidTypeForArray(indexLocation, elementType))
    {
        return checkIsValidQualifierForArray(indexLocation, elementType);
    }
    return false;
}

// Enforce non-initializer type/qualifier rules.
void TParseContext::checkCanBeDeclaredWithoutInitializer(const TSourceLoc &line,
                                                         const TString &identifier,
                                                         TPublicType *type)
{
    ASSERT(type != nullptr);
    if (type->qualifier == EvqConst)
    {
        // Make the qualifier make sense.
        type->qualifier = EvqTemporary;

        // Generate informative error messages for ESSL1.
        // In ESSL3 arrays and structures containing arrays can be constant.
        if (mShaderVersion < 300 && type->isStructureContainingArrays())
        {
            error(line,
                  "structures containing arrays may not be declared constant since they cannot be "
                  "initialized",
                  identifier.c_str());
        }
        else
        {
            error(line, "variables with qualifier 'const' must be initialized", identifier.c_str());
        }
        return;
    }
    if (type->isUnsizedArray())
    {
        error(line, "implicitly sized arrays need to be initialized", identifier.c_str());
    }
}

// Do some simple checks that are shared between all variable declarations,
// and update the symbol table.
//
// Returns true if declaring the variable succeeded.
//
bool TParseContext::declareVariable(const TSourceLoc &line,
                                    const TString &identifier,
                                    const TType &type,
                                    TVariable **variable)
{
    ASSERT((*variable) == nullptr);

    checkBindingIsValid(line, type);

    bool needsReservedCheck = true;

    // gl_LastFragData may be redeclared with a new precision qualifier
    if (type.isArray() && identifier.compare(0, 15, "gl_LastFragData") == 0)
    {
        const TVariable *maxDrawBuffers = static_cast<const TVariable *>(
            symbolTable.findBuiltIn("gl_MaxDrawBuffers", mShaderVersion));
        if (static_cast<int>(type.getArraySize()) == maxDrawBuffers->getConstPointer()->getIConst())
        {
            if (TSymbol *builtInSymbol = symbolTable.findBuiltIn(identifier, mShaderVersion))
            {
                needsReservedCheck = !checkCanUseExtension(line, builtInSymbol->getExtension());
            }
        }
        else
        {
            error(line, "redeclaration of gl_LastFragData with size != gl_MaxDrawBuffers",
                  identifier.c_str());
            return false;
        }
    }

    if (needsReservedCheck && !checkIsNotReserved(line, identifier))
        return false;

    (*variable) = new TVariable(&identifier, type);
    if (!symbolTable.declare(*variable))
    {
        error(line, "redefinition", identifier.c_str());
        *variable = nullptr;
        return false;
    }

    if (!checkIsNonVoid(line, identifier, type.getBasicType()))
        return false;

    return true;
}

void TParseContext::checkIsParameterQualifierValid(
    const TSourceLoc &line,
    const TTypeQualifierBuilder &typeQualifierBuilder,
    TType *type)
{
    TTypeQualifier typeQualifier = typeQualifierBuilder.getParameterTypeQualifier(mDiagnostics);

    if (typeQualifier.qualifier == EvqOut || typeQualifier.qualifier == EvqInOut)
    {
        checkOutParameterIsNotOpaqueType(line, typeQualifier.qualifier, *type);
    }

    if (!IsImage(type->getBasicType()))
    {
        checkMemoryQualifierIsNotSpecified(typeQualifier.memoryQualifier, line);
    }
    else
    {
        type->setMemoryQualifier(typeQualifier.memoryQualifier);
    }

    type->setQualifier(typeQualifier.qualifier);

    if (typeQualifier.precision != EbpUndefined)
    {
        type->setPrecision(typeQualifier.precision);
    }
}

bool TParseContext::checkCanUseExtension(const TSourceLoc &line, const TString &extension)
{
    const TExtensionBehavior &extBehavior   = extensionBehavior();
    TExtensionBehavior::const_iterator iter = extBehavior.find(extension.c_str());
    if (iter == extBehavior.end())
    {
        error(line, "extension is not supported", extension.c_str());
        return false;
    }
    // In GLSL ES, an extension's default behavior is "disable".
    if (iter->second == EBhDisable || iter->second == EBhUndefined)
    {
        // TODO(oetuaho@nvidia.com): This is slightly hacky. Might be better if symbols could be
        // associated with more than one extension.
        if (extension == "GL_OVR_multiview")
        {
            return checkCanUseExtension(line, "GL_OVR_multiview2");
        }
        error(line, "extension is disabled", extension.c_str());
        return false;
    }
    if (iter->second == EBhWarn)
    {
        warning(line, "extension is being used", extension.c_str());
        return true;
    }

    return true;
}

void TParseContext::emptyDeclarationErrorCheck(const TPublicType &publicType,
                                               const TSourceLoc &location)
{
    if (publicType.isUnsizedArray())
    {
        // ESSL3 spec section 4.1.9: Array declaration which leaves the size unspecified is an
        // error. It is assumed that this applies to empty declarations as well.
        error(location, "empty array declaration needs to specify a size", "");
    }
    if (publicType.qualifier == EvqShared && !publicType.layoutQualifier.isEmpty())
    {
        error(location, "Shared memory declarations cannot have layout specified", "layout");
    }
}

// These checks are common for all declarations starting a declarator list, and declarators that
// follow an empty declaration.
void TParseContext::singleDeclarationErrorCheck(const TPublicType &publicType,
                                                const TSourceLoc &identifierLocation)
{
    switch (publicType.qualifier)
    {
        case EvqVaryingIn:
        case EvqVaryingOut:
        case EvqAttribute:
        case EvqVertexIn:
        case EvqFragmentOut:
        case EvqComputeIn:
            if (publicType.getBasicType() == EbtStruct)
            {
                error(identifierLocation, "cannot be used with a structure",
                      getQualifierString(publicType.qualifier));
                return;
            }

        default:
            break;
    }

    if (publicType.qualifier != EvqUniform &&
        !checkIsNotSampler(identifierLocation, publicType.typeSpecifierNonArray,
                           "samplers must be uniform"))
    {
        return;
    }
    if (publicType.qualifier != EvqUniform &&
        !checkIsNotImage(identifierLocation, publicType.typeSpecifierNonArray,
                         "images must be uniform"))
    {
        return;
    }

    if ((publicType.qualifier != EvqTemporary && publicType.qualifier != EvqGlobal &&
         publicType.qualifier != EvqConst) &&
        publicType.getBasicType() == EbtYuvCscStandardEXT)
    {
        error(identifierLocation, "cannot be used with a yuvCscStandardEXT",
              getQualifierString(publicType.qualifier));
        return;
    }

    // check for layout qualifier issues
    const TLayoutQualifier layoutQualifier = publicType.layoutQualifier;

    if (layoutQualifier.matrixPacking != EmpUnspecified)
    {
        error(identifierLocation, "layout qualifier only valid for interface blocks",
              getMatrixPackingString(layoutQualifier.matrixPacking));
        return;
    }

    if (layoutQualifier.blockStorage != EbsUnspecified)
    {
        error(identifierLocation, "layout qualifier only valid for interface blocks",
              getBlockStorageString(layoutQualifier.blockStorage));
        return;
    }

    bool canHaveLocation =
        publicType.qualifier == EvqVertexIn || publicType.qualifier == EvqFragmentOut;

    if (mShaderVersion >= 310 && publicType.qualifier == EvqUniform)
    {
        canHaveLocation = true;

        // Valid uniform declarations can't be unsized arrays since uniforms can't be initialized.
        // But invalid shaders may still reach here with an unsized array declaration.
        if (!publicType.isUnsizedArray())
        {
            TType type(publicType);
            checkUniformLocationInRange(identifierLocation, type.getLocationCount(),
                                        publicType.layoutQualifier);
        }
    }
    if (!canHaveLocation)
    {
        checkLocationIsNotSpecified(identifierLocation, publicType.layoutQualifier);
    }

    if (publicType.qualifier == EvqFragmentOut)
    {
        if (layoutQualifier.location != -1 && layoutQualifier.yuv == true)
        {
            error(identifierLocation, "invalid layout qualifier combination", "yuv");
            return;
        }
    }
    else
    {
        checkYuvIsNotSpecified(identifierLocation, layoutQualifier.yuv);
    }

    if (IsImage(publicType.getBasicType()))
    {

        switch (layoutQualifier.imageInternalFormat)
        {
            case EiifRGBA32F:
            case EiifRGBA16F:
            case EiifR32F:
            case EiifRGBA8:
            case EiifRGBA8_SNORM:
                if (!IsFloatImage(publicType.getBasicType()))
                {
                    error(identifierLocation,
                          "internal image format requires a floating image type",
                          getBasicString(publicType.getBasicType()));
                    return;
                }
                break;
            case EiifRGBA32I:
            case EiifRGBA16I:
            case EiifRGBA8I:
            case EiifR32I:
                if (!IsIntegerImage(publicType.getBasicType()))
                {
                    error(identifierLocation,
                          "internal image format requires an integer image type",
                          getBasicString(publicType.getBasicType()));
                    return;
                }
                break;
            case EiifRGBA32UI:
            case EiifRGBA16UI:
            case EiifRGBA8UI:
            case EiifR32UI:
                if (!IsUnsignedImage(publicType.getBasicType()))
                {
                    error(identifierLocation,
                          "internal image format requires an unsigned image type",
                          getBasicString(publicType.getBasicType()));
                    return;
                }
                break;
            case EiifUnspecified:
                error(identifierLocation, "layout qualifier", "No image internal format specified");
                return;
            default:
                error(identifierLocation, "layout qualifier", "unrecognized token");
                return;
        }

        // GLSL ES 3.10 Revision 4, 4.9 Memory Access Qualifiers
        switch (layoutQualifier.imageInternalFormat)
        {
            case EiifR32F:
            case EiifR32I:
            case EiifR32UI:
                break;
            default:
                if (!publicType.memoryQualifier.readonly && !publicType.memoryQualifier.writeonly)
                {
                    error(identifierLocation, "layout qualifier",
                          "Except for images with the r32f, r32i and r32ui format qualifiers, "
                          "image variables must be qualified readonly and/or writeonly");
                    return;
                }
                break;
        }
    }
    else
    {
        checkInternalFormatIsNotSpecified(identifierLocation, layoutQualifier.imageInternalFormat);

        checkMemoryQualifierIsNotSpecified(publicType.memoryQualifier, identifierLocation);
    }
}

void TParseContext::checkBindingIsValid(const TSourceLoc &identifierLocation, const TType &type)
{
    TLayoutQualifier layoutQualifier = type.getLayoutQualifier();
    int arraySize                    = type.isArray() ? type.getArraySize() : 1;
    if (IsImage(type.getBasicType()))
    {
        checkImageBindingIsValid(identifierLocation, layoutQualifier.binding, arraySize);
    }
    else if (IsSampler(type.getBasicType()))
    {
        checkSamplerBindingIsValid(identifierLocation, layoutQualifier.binding, arraySize);
    }
    else
    {
        ASSERT(!IsOpaqueType(type.getBasicType()));
        checkBindingIsNotSpecified(identifierLocation, layoutQualifier.binding);
    }
}

void TParseContext::checkLayoutQualifierSupported(const TSourceLoc &location,
                                                  const TString &layoutQualifierName,
                                                  int versionRequired)
{

    if (mShaderVersion < versionRequired)
    {
        error(location, "invalid layout qualifier: not supported", layoutQualifierName.c_str());
    }
}

bool TParseContext::checkWorkGroupSizeIsNotSpecified(const TSourceLoc &location,
                                                     const TLayoutQualifier &layoutQualifier)
{
    const sh::WorkGroupSize &localSize = layoutQualifier.localSize;
    for (size_t i = 0u; i < localSize.size(); ++i)
    {
        if (localSize[i] != -1)
        {
            error(location,
                  "invalid layout qualifier: only valid when used with 'in' in a compute shader "
                  "global layout declaration",
                  getWorkGroupSizeString(i));
            return false;
        }
    }

    return true;
}

void TParseContext::checkInternalFormatIsNotSpecified(const TSourceLoc &location,
                                                      TLayoutImageInternalFormat internalFormat)
{
    if (internalFormat != EiifUnspecified)
    {
        error(location, "invalid layout qualifier: only valid when used with images",
              getImageInternalFormatString(internalFormat));
    }
}

void TParseContext::checkBindingIsNotSpecified(const TSourceLoc &location, int binding)
{
    if (binding != -1)
    {
        error(location,
              "invalid layout qualifier: only valid when used with opaque types or blocks",
              "binding");
    }
}

void TParseContext::checkImageBindingIsValid(const TSourceLoc &location, int binding, int arraySize)
{
    // Expects arraySize to be 1 when setting binding for only a single variable.
    if (binding >= 0 && binding + arraySize > mMaxImageUnits)
    {
        error(location, "image binding greater than gl_MaxImageUnits", "binding");
    }
}

void TParseContext::checkSamplerBindingIsValid(const TSourceLoc &location,
                                               int binding,
                                               int arraySize)
{
    // Expects arraySize to be 1 when setting binding for only a single variable.
    if (binding >= 0 && binding + arraySize > mMaxCombinedTextureImageUnits)
    {
        error(location, "sampler binding greater than maximum texture units", "binding");
    }
}

void TParseContext::checkUniformLocationInRange(const TSourceLoc &location,
                                                int objectLocationCount,
                                                const TLayoutQualifier &layoutQualifier)
{
    int loc = layoutQualifier.location;
    if (loc >= 0 && loc + objectLocationCount > mMaxUniformLocations)
    {
        error(location, "Uniform location out of range", "location");
    }
}

void TParseContext::checkYuvIsNotSpecified(const TSourceLoc &location, bool yuv)
{
    if (yuv != false)
    {
        error(location, "invalid layout qualifier: only valid on program outputs", "yuv");
    }
}

void TParseContext::functionCallLValueErrorCheck(const TFunction *fnCandidate,
                                                 TIntermAggregate *fnCall)
{
    for (size_t i = 0; i < fnCandidate->getParamCount(); ++i)
    {
        TQualifier qual = fnCandidate->getParam(i).type->getQualifier();
        if (qual == EvqOut || qual == EvqInOut)
        {
            TIntermTyped *argument = (*(fnCall->getSequence()))[i]->getAsTyped();
            if (!checkCanBeLValue(argument->getLine(), "assign", argument))
            {
                error(argument->getLine(),
                      "Constant value cannot be passed for 'out' or 'inout' parameters.",
                      fnCall->getFunctionSymbolInfo()->getName().c_str());
                return;
            }
        }
    }
}

void TParseContext::checkInvariantVariableQualifier(bool invariant,
                                                    const TQualifier qualifier,
                                                    const TSourceLoc &invariantLocation)
{
    if (!invariant)
        return;

    if (mShaderVersion < 300)
    {
        // input variables in the fragment shader can be also qualified as invariant
        if (!sh::CanBeInvariantESSL1(qualifier))
        {
            error(invariantLocation, "Cannot be qualified as invariant.", "invariant");
        }
    }
    else
    {
        if (!sh::CanBeInvariantESSL3OrGreater(qualifier))
        {
            error(invariantLocation, "Cannot be qualified as invariant.", "invariant");
        }
    }
}

bool TParseContext::supportsExtension(const char *extension)
{
    const TExtensionBehavior &extbehavior   = extensionBehavior();
    TExtensionBehavior::const_iterator iter = extbehavior.find(extension);
    return (iter != extbehavior.end());
}

bool TParseContext::isExtensionEnabled(const char *extension) const
{
    return ::IsExtensionEnabled(extensionBehavior(), extension);
}

void TParseContext::handleExtensionDirective(const TSourceLoc &loc,
                                             const char *extName,
                                             const char *behavior)
{
    pp::SourceLocation srcLoc;
    srcLoc.file = loc.first_file;
    srcLoc.line = loc.first_line;
    mDirectiveHandler.handleExtension(srcLoc, extName, behavior);
}

void TParseContext::handlePragmaDirective(const TSourceLoc &loc,
                                          const char *name,
                                          const char *value,
                                          bool stdgl)
{
    pp::SourceLocation srcLoc;
    srcLoc.file = loc.first_file;
    srcLoc.line = loc.first_line;
    mDirectiveHandler.handlePragma(srcLoc, name, value, stdgl);
}

sh::WorkGroupSize TParseContext::getComputeShaderLocalSize() const
{
    sh::WorkGroupSize result;
    for (size_t i = 0u; i < result.size(); ++i)
    {
        if (mComputeShaderLocalSizeDeclared && mComputeShaderLocalSize[i] == -1)
        {
            result[i] = 1;
        }
        else
        {
            result[i] = mComputeShaderLocalSize[i];
        }
    }
    return result;
}

/////////////////////////////////////////////////////////////////////////////////
//
// Non-Errors.
//
/////////////////////////////////////////////////////////////////////////////////

const TVariable *TParseContext::getNamedVariable(const TSourceLoc &location,
                                                 const TString *name,
                                                 const TSymbol *symbol)
{
    const TVariable *variable = NULL;

    if (!symbol)
    {
        error(location, "undeclared identifier", name->c_str());
    }
    else if (!symbol->isVariable())
    {
        error(location, "variable expected", name->c_str());
    }
    else
    {
        variable = static_cast<const TVariable *>(symbol);

        if (symbolTable.findBuiltIn(variable->getName(), mShaderVersion) &&
            !variable->getExtension().empty())
        {
            checkCanUseExtension(location, variable->getExtension());
        }

        // Reject shaders using both gl_FragData and gl_FragColor
        TQualifier qualifier = variable->getType().getQualifier();
        if (qualifier == EvqFragData || qualifier == EvqSecondaryFragDataEXT)
        {
            mUsesFragData = true;
        }
        else if (qualifier == EvqFragColor || qualifier == EvqSecondaryFragColorEXT)
        {
            mUsesFragColor = true;
        }
        if (qualifier == EvqSecondaryFragDataEXT || qualifier == EvqSecondaryFragColorEXT)
        {
            mUsesSecondaryOutputs = true;
        }

        // This validation is not quite correct - it's only an error to write to
        // both FragData and FragColor. For simplicity, and because users shouldn't
        // be rewarded for reading from undefined varaibles, return an error
        // if they are both referenced, rather than assigned.
        if (mUsesFragData && mUsesFragColor)
        {
            const char *errorMessage = "cannot use both gl_FragData and gl_FragColor";
            if (mUsesSecondaryOutputs)
            {
                errorMessage =
                    "cannot use both output variable sets (gl_FragData, gl_SecondaryFragDataEXT)"
                    " and (gl_FragColor, gl_SecondaryFragColorEXT)";
            }
            error(location, errorMessage, name->c_str());
        }

        // GLSL ES 3.1 Revision 4, 7.1.3 Compute Shader Special Variables
        if (getShaderType() == GL_COMPUTE_SHADER && !mComputeShaderLocalSizeDeclared &&
            qualifier == EvqWorkGroupSize)
        {
            error(location,
                  "It is an error to use gl_WorkGroupSize before declaring the local group size",
                  "gl_WorkGroupSize");
        }
    }

    if (!variable)
    {
        TType type(EbtFloat, EbpUndefined);
        TVariable *fakeVariable = new TVariable(name, type);
        symbolTable.declare(fakeVariable);
        variable = fakeVariable;
    }

    return variable;
}

TIntermTyped *TParseContext::parseVariableIdentifier(const TSourceLoc &location,
                                                     const TString *name,
                                                     const TSymbol *symbol)
{
    const TVariable *variable = getNamedVariable(location, name, symbol);

    if (variable->getType().getQualifier() == EvqViewIDOVR && IsWebGLBasedSpec(mShaderSpec) &&
        mShaderType == GL_FRAGMENT_SHADER && !isExtensionEnabled("GL_OVR_multiview2"))
    {
        // WEBGL_multiview spec
        error(location, "Need to enable OVR_multiview2 to use gl_ViewID_OVR in fragment shader",
              "gl_ViewID_OVR");
    }

    if (variable->getConstPointer())
    {
        const TConstantUnion *constArray = variable->getConstPointer();
        return intermediate.addConstantUnion(constArray, variable->getType(), location);
    }
    else if (variable->getType().getQualifier() == EvqWorkGroupSize &&
             mComputeShaderLocalSizeDeclared)
    {
        // gl_WorkGroupSize can be used to size arrays according to the ESSL 3.10.4 spec, so it
        // needs to be added to the AST as a constant and not as a symbol.
        sh::WorkGroupSize workGroupSize = getComputeShaderLocalSize();
        TConstantUnion *constArray      = new TConstantUnion[3];
        for (size_t i = 0; i < 3; ++i)
        {
            constArray[i].setUConst(static_cast<unsigned int>(workGroupSize[i]));
        }

        ASSERT(variable->getType().getBasicType() == EbtUInt);
        ASSERT(variable->getType().getObjectSize() == 3);

        TType type(variable->getType());
        type.setQualifier(EvqConst);
        return intermediate.addConstantUnion(constArray, type, location);
    }
    else
    {
        return intermediate.addSymbol(variable->getUniqueId(), variable->getName(),
                                      variable->getType(), location);
    }
}

//
// Initializers show up in several places in the grammar.  Have one set of
// code to handle them here.
//
// Returns true on error, false if no error
//
bool TParseContext::executeInitializer(const TSourceLoc &line,
                                       const TString &identifier,
                                       const TPublicType &pType,
                                       TIntermTyped *initializer,
                                       TIntermBinary **initNode)
{
    ASSERT(initNode != nullptr);
    ASSERT(*initNode == nullptr);
    TType type = TType(pType);

    TVariable *variable = nullptr;
    if (type.isUnsizedArray())
    {
        // We have not checked yet whether the initializer actually is an array or not.
        if (initializer->isArray())
        {
            type.setArraySize(initializer->getArraySize());
        }
        else
        {
            // Having a non-array initializer for an unsized array will result in an error later,
            // so we don't generate an error message here.
            type.setArraySize(1u);
        }
    }
    if (!declareVariable(line, identifier, type, &variable))
    {
        return true;
    }

    bool globalInitWarning = false;
    if (symbolTable.atGlobalLevel() &&
        !ValidateGlobalInitializer(initializer, this, &globalInitWarning))
    {
        // Error message does not completely match behavior with ESSL 1.00, but
        // we want to steer developers towards only using constant expressions.
        error(line, "global variable initializers must be constant expressions", "=");
        return true;
    }
    if (globalInitWarning)
    {
        warning(
            line,
            "global variable initializers should be constant expressions "
            "(uniforms and globals are allowed in global initializers for legacy compatibility)",
            "=");
    }

    //
    // identifier must be of type constant, a global, or a temporary
    //
    TQualifier qualifier = variable->getType().getQualifier();
    if ((qualifier != EvqTemporary) && (qualifier != EvqGlobal) && (qualifier != EvqConst))
    {
        error(line, " cannot initialize this type of qualifier ",
              variable->getType().getQualifierString());
        return true;
    }
    //
    // test for and propagate constant
    //

    if (qualifier == EvqConst)
    {
        if (qualifier != initializer->getType().getQualifier())
        {
            std::stringstream reasonStream;
            reasonStream << "assigning non-constant to '" << variable->getType().getCompleteString()
                         << "'";
            std::string reason = reasonStream.str();
            error(line, reason.c_str(), "=");
            variable->getType().setQualifier(EvqTemporary);
            return true;
        }
        if (type != initializer->getType())
        {
            error(line, " non-matching types for const initializer ",
                  variable->getType().getQualifierString());
            variable->getType().setQualifier(EvqTemporary);
            return true;
        }

        // Save the constant folded value to the variable if possible. For example array
        // initializers are not folded, since that way copying the array literal to multiple places
        // in the shader is avoided.
        // TODO(oetuaho@nvidia.com): Consider constant folding array initialization in cases where
        // it would be beneficial.
        if (initializer->getAsConstantUnion())
        {
            variable->shareConstPointer(initializer->getAsConstantUnion()->getUnionArrayPointer());
            *initNode = nullptr;
            return false;
        }
        else if (initializer->getAsSymbolNode())
        {
            const TSymbol *symbol =
                symbolTable.find(initializer->getAsSymbolNode()->getSymbol(), 0);
            const TVariable *tVar = static_cast<const TVariable *>(symbol);

            const TConstantUnion *constArray = tVar->getConstPointer();
            if (constArray)
            {
                variable->shareConstPointer(constArray);
                *initNode = nullptr;
                return false;
            }
        }
    }

    TIntermSymbol *intermSymbol = intermediate.addSymbol(
        variable->getUniqueId(), variable->getName(), variable->getType(), line);
    *initNode = createAssign(EOpInitialize, intermSymbol, initializer, line);
    if (*initNode == nullptr)
    {
        assignError(line, "=", intermSymbol->getCompleteString(), initializer->getCompleteString());
        return true;
    }

    return false;
}

void TParseContext::addFullySpecifiedType(TPublicType *typeSpecifier)
{
    checkPrecisionSpecified(typeSpecifier->getLine(), typeSpecifier->precision,
                            typeSpecifier->getBasicType());

    if (mShaderVersion < 300 && typeSpecifier->array)
    {
        error(typeSpecifier->getLine(), "not supported", "first-class array");
        typeSpecifier->clearArrayness();
    }
}

TPublicType TParseContext::addFullySpecifiedType(const TTypeQualifierBuilder &typeQualifierBuilder,
                                                 const TPublicType &typeSpecifier)
{
    TTypeQualifier typeQualifier = typeQualifierBuilder.getVariableTypeQualifier(mDiagnostics);

    TPublicType returnType     = typeSpecifier;
    returnType.qualifier       = typeQualifier.qualifier;
    returnType.invariant       = typeQualifier.invariant;
    returnType.layoutQualifier = typeQualifier.layoutQualifier;
    returnType.memoryQualifier = typeQualifier.memoryQualifier;
    returnType.precision       = typeSpecifier.precision;

    if (typeQualifier.precision != EbpUndefined)
    {
        returnType.precision = typeQualifier.precision;
    }

    checkPrecisionSpecified(typeSpecifier.getLine(), returnType.precision,
                            typeSpecifier.getBasicType());

    checkInvariantVariableQualifier(returnType.invariant, returnType.qualifier,
                                    typeSpecifier.getLine());

    checkWorkGroupSizeIsNotSpecified(typeSpecifier.getLine(), returnType.layoutQualifier);

    if (mShaderVersion < 300)
    {
        if (typeSpecifier.array)
        {
            error(typeSpecifier.getLine(), "not supported", "first-class array");
            returnType.clearArrayness();
        }

        if (returnType.qualifier == EvqAttribute &&
            (typeSpecifier.getBasicType() == EbtBool || typeSpecifier.getBasicType() == EbtInt))
        {
            error(typeSpecifier.getLine(), "cannot be bool or int",
                  getQualifierString(returnType.qualifier));
        }

        if ((returnType.qualifier == EvqVaryingIn || returnType.qualifier == EvqVaryingOut) &&
            (typeSpecifier.getBasicType() == EbtBool || typeSpecifier.getBasicType() == EbtInt))
        {
            error(typeSpecifier.getLine(), "cannot be bool or int",
                  getQualifierString(returnType.qualifier));
        }
    }
    else
    {
        if (!returnType.layoutQualifier.isEmpty())
        {
            checkIsAtGlobalLevel(typeSpecifier.getLine(), "layout");
        }
        if (sh::IsVarying(returnType.qualifier) || returnType.qualifier == EvqVertexIn ||
            returnType.qualifier == EvqFragmentOut)
        {
            checkInputOutputTypeIsValidES3(returnType.qualifier, typeSpecifier,
                                           typeSpecifier.getLine());
        }
        if (returnType.qualifier == EvqComputeIn)
        {
            error(typeSpecifier.getLine(), "'in' can be only used to specify the local group size",
                  "in");
        }
    }

    return returnType;
}

void TParseContext::checkInputOutputTypeIsValidES3(const TQualifier qualifier,
                                                   const TPublicType &type,
                                                   const TSourceLoc &qualifierLocation)
{
    // An input/output variable can never be bool or a sampler. Samplers are checked elsewhere.
    if (type.getBasicType() == EbtBool)
    {
        error(qualifierLocation, "cannot be bool", getQualifierString(qualifier));
    }

    // Specific restrictions apply for vertex shader inputs and fragment shader outputs.
    switch (qualifier)
    {
        case EvqVertexIn:
            // ESSL 3.00 section 4.3.4
            if (type.array)
            {
                error(qualifierLocation, "cannot be array", getQualifierString(qualifier));
            }
            // Vertex inputs with a struct type are disallowed in singleDeclarationErrorCheck
            return;
        case EvqFragmentOut:
            // ESSL 3.00 section 4.3.6
            if (type.typeSpecifierNonArray.isMatrix())
            {
                error(qualifierLocation, "cannot be matrix", getQualifierString(qualifier));
            }
            // Fragment outputs with a struct type are disallowed in singleDeclarationErrorCheck
            return;
        default:
            break;
    }

    // Vertex shader outputs / fragment shader inputs have a different, slightly more lenient set of
    // restrictions.
    bool typeContainsIntegers =
        (type.getBasicType() == EbtInt || type.getBasicType() == EbtUInt ||
         type.isStructureContainingType(EbtInt) || type.isStructureContainingType(EbtUInt));
    if (typeContainsIntegers && qualifier != EvqFlatIn && qualifier != EvqFlatOut)
    {
        error(qualifierLocation, "must use 'flat' interpolation here",
              getQualifierString(qualifier));
    }

    if (type.getBasicType() == EbtStruct)
    {
        // ESSL 3.00 sections 4.3.4 and 4.3.6.
        // These restrictions are only implied by the ESSL 3.00 spec, but
        // the ESSL 3.10 spec lists these restrictions explicitly.
        if (type.array)
        {
            error(qualifierLocation, "cannot be an array of structures",
                  getQualifierString(qualifier));
        }
        if (type.isStructureContainingArrays())
        {
            error(qualifierLocation, "cannot be a structure containing an array",
                  getQualifierString(qualifier));
        }
        if (type.isStructureContainingType(EbtStruct))
        {
            error(qualifierLocation, "cannot be a structure containing a structure",
                  getQualifierString(qualifier));
        }
        if (type.isStructureContainingType(EbtBool))
        {
            error(qualifierLocation, "cannot be a structure containing a bool",
                  getQualifierString(qualifier));
        }
    }
}

void TParseContext::checkLocalVariableConstStorageQualifier(const TQualifierWrapperBase &qualifier)
{
    if (qualifier.getType() == QtStorage)
    {
        const TStorageQualifierWrapper &storageQualifier =
            static_cast<const TStorageQualifierWrapper &>(qualifier);
        if (!declaringFunction() && storageQualifier.getQualifier() != EvqConst &&
            !symbolTable.atGlobalLevel())
        {
            error(storageQualifier.getLine(),
                  "Local variables can only use the const storage qualifier.",
                  storageQualifier.getQualifierString().c_str());
        }
    }
}

void TParseContext::checkMemoryQualifierIsNotSpecified(const TMemoryQualifier &memoryQualifier,
                                                       const TSourceLoc &location)
{
    if (memoryQualifier.readonly)
    {
        error(location, "Only allowed with images.", "readonly");
    }
    if (memoryQualifier.writeonly)
    {
        error(location, "Only allowed with images.", "writeonly");
    }
    if (memoryQualifier.coherent)
    {
        error(location, "Only allowed with images.", "coherent");
    }
    if (memoryQualifier.restrictQualifier)
    {
        error(location, "Only allowed with images.", "restrict");
    }
    if (memoryQualifier.volatileQualifier)
    {
        error(location, "Only allowed with images.", "volatile");
    }
}

TIntermDeclaration *TParseContext::parseSingleDeclaration(
    TPublicType &publicType,
    const TSourceLoc &identifierOrTypeLocation,
    const TString &identifier)
{
    TType type(publicType);
    if ((mCompileOptions & SH_FLATTEN_PRAGMA_STDGL_INVARIANT_ALL) &&
        mDirectiveHandler.pragma().stdgl.invariantAll)
    {
        TQualifier qualifier = type.getQualifier();

        // The directive handler has already taken care of rejecting invalid uses of this pragma
        // (for example, in ESSL 3.00 fragment shaders), so at this point, flatten it into all
        // affected variable declarations:
        //
        // 1. Built-in special variables which are inputs to the fragment shader. (These are handled
        // elsewhere, in TranslatorGLSL.)
        //
        // 2. Outputs from vertex shaders in ESSL 1.00 and 3.00 (EvqVaryingOut and EvqVertexOut). It
        // is actually less likely that there will be bugs in the handling of ESSL 3.00 shaders, but
        // the way this is currently implemented we have to enable this compiler option before
        // parsing the shader and determining the shading language version it uses. If this were
        // implemented as a post-pass, the workaround could be more targeted.
        //
        // 3. Inputs in ESSL 1.00 fragment shaders (EvqVaryingIn). This is somewhat in violation of
        // the specification, but there are desktop OpenGL drivers that expect that this is the
        // behavior of the #pragma when specified in ESSL 1.00 fragment shaders.
        if (qualifier == EvqVaryingOut || qualifier == EvqVertexOut || qualifier == EvqVaryingIn)
        {
            type.setInvariant(true);
        }
    }

    TIntermSymbol *symbol = intermediate.addSymbol(0, identifier, type, identifierOrTypeLocation);

    bool emptyDeclaration = (identifier == "");

    mDeferredSingleDeclarationErrorCheck = emptyDeclaration;

    TIntermDeclaration *declaration = new TIntermDeclaration();
    declaration->setLine(identifierOrTypeLocation);

    if (emptyDeclaration)
    {
        emptyDeclarationErrorCheck(publicType, identifierOrTypeLocation);
    }
    else
    {
        singleDeclarationErrorCheck(publicType, identifierOrTypeLocation);

        checkCanBeDeclaredWithoutInitializer(identifierOrTypeLocation, identifier, &publicType);

        TVariable *variable = nullptr;
        declareVariable(identifierOrTypeLocation, identifier, type, &variable);

        if (variable && symbol)
        {
            symbol->setId(variable->getUniqueId());
        }
    }

    // We append the symbol even if the declaration is empty, mainly because of struct declarations
    // that may just declare a type.
    declaration->appendDeclarator(symbol);

    return declaration;
}

TIntermDeclaration *TParseContext::parseSingleArrayDeclaration(TPublicType &publicType,
                                                               const TSourceLoc &identifierLocation,
                                                               const TString &identifier,
                                                               const TSourceLoc &indexLocation,
                                                               TIntermTyped *indexExpression)
{
    mDeferredSingleDeclarationErrorCheck = false;

    singleDeclarationErrorCheck(publicType, identifierLocation);

    checkCanBeDeclaredWithoutInitializer(identifierLocation, identifier, &publicType);

    checkIsValidTypeAndQualifierForArray(indexLocation, publicType);

    TType arrayType(publicType);

    unsigned int size = checkIsValidArraySize(identifierLocation, indexExpression);
    // Make the type an array even if size check failed.
    // This ensures useless error messages regarding the variable's non-arrayness won't follow.
    arrayType.setArraySize(size);

    TVariable *variable = nullptr;
    declareVariable(identifierLocation, identifier, arrayType, &variable);

    TIntermDeclaration *declaration = new TIntermDeclaration();
    declaration->setLine(identifierLocation);

    TIntermSymbol *symbol = intermediate.addSymbol(0, identifier, arrayType, identifierLocation);
    if (variable && symbol)
    {
        symbol->setId(variable->getUniqueId());
        declaration->appendDeclarator(symbol);
    }

    return declaration;
}

TIntermDeclaration *TParseContext::parseSingleInitDeclaration(const TPublicType &publicType,
                                                              const TSourceLoc &identifierLocation,
                                                              const TString &identifier,
                                                              const TSourceLoc &initLocation,
                                                              TIntermTyped *initializer)
{
    mDeferredSingleDeclarationErrorCheck = false;

    singleDeclarationErrorCheck(publicType, identifierLocation);

    TIntermDeclaration *declaration = new TIntermDeclaration();
    declaration->setLine(identifierLocation);

    TIntermBinary *initNode = nullptr;
    if (!executeInitializer(identifierLocation, identifier, publicType, initializer, &initNode))
    {
        if (initNode)
        {
            declaration->appendDeclarator(initNode);
        }
    }
    return declaration;
}

TIntermDeclaration *TParseContext::parseSingleArrayInitDeclaration(
    TPublicType &publicType,
    const TSourceLoc &identifierLocation,
    const TString &identifier,
    const TSourceLoc &indexLocation,
    TIntermTyped *indexExpression,
    const TSourceLoc &initLocation,
    TIntermTyped *initializer)
{
    mDeferredSingleDeclarationErrorCheck = false;

    singleDeclarationErrorCheck(publicType, identifierLocation);

    checkIsValidTypeAndQualifierForArray(indexLocation, publicType);

    TPublicType arrayType(publicType);

    unsigned int size = 0u;
    // If indexExpression is nullptr, then the array will eventually get its size implicitly from
    // the initializer.
    if (indexExpression != nullptr)
    {
        size = checkIsValidArraySize(identifierLocation, indexExpression);
    }
    // Make the type an array even if size check failed.
    // This ensures useless error messages regarding the variable's non-arrayness won't follow.
    arrayType.setArraySize(size);

    TIntermDeclaration *declaration = new TIntermDeclaration();
    declaration->setLine(identifierLocation);

    // initNode will correspond to the whole of "type b[n] = initializer".
    TIntermBinary *initNode = nullptr;
    if (!executeInitializer(identifierLocation, identifier, arrayType, initializer, &initNode))
    {
        if (initNode)
        {
            declaration->appendDeclarator(initNode);
        }
    }

    return declaration;
}

TIntermInvariantDeclaration *TParseContext::parseInvariantDeclaration(
    const TTypeQualifierBuilder &typeQualifierBuilder,
    const TSourceLoc &identifierLoc,
    const TString *identifier,
    const TSymbol *symbol)
{
    TTypeQualifier typeQualifier = typeQualifierBuilder.getVariableTypeQualifier(mDiagnostics);

    if (!typeQualifier.invariant)
    {
        error(identifierLoc, "Expected invariant", identifier->c_str());
        return nullptr;
    }
    if (!checkIsAtGlobalLevel(identifierLoc, "invariant varying"))
    {
        return nullptr;
    }
    if (!symbol)
    {
        error(identifierLoc, "undeclared identifier declared as invariant", identifier->c_str());
        return nullptr;
    }
    if (!IsQualifierUnspecified(typeQualifier.qualifier))
    {
        error(identifierLoc, "invariant declaration specifies qualifier",
              getQualifierString(typeQualifier.qualifier));
    }
    if (typeQualifier.precision != EbpUndefined)
    {
        error(identifierLoc, "invariant declaration specifies precision",
              getPrecisionString(typeQualifier.precision));
    }
    if (!typeQualifier.layoutQualifier.isEmpty())
    {
        error(identifierLoc, "invariant declaration specifies layout", "'layout'");
    }

    const TVariable *variable = getNamedVariable(identifierLoc, identifier, symbol);
    ASSERT(variable);
    const TType &type = variable->getType();

    checkInvariantVariableQualifier(typeQualifier.invariant, type.getQualifier(),
                                    typeQualifier.line);
    checkMemoryQualifierIsNotSpecified(typeQualifier.memoryQualifier, typeQualifier.line);

    symbolTable.addInvariantVarying(std::string(identifier->c_str()));

    TIntermSymbol *intermSymbol =
        intermediate.addSymbol(variable->getUniqueId(), *identifier, type, identifierLoc);

    return new TIntermInvariantDeclaration(intermSymbol, identifierLoc);
}

void TParseContext::parseDeclarator(TPublicType &publicType,
                                    const TSourceLoc &identifierLocation,
                                    const TString &identifier,
                                    TIntermDeclaration *declarationOut)
{
    // If the declaration starting this declarator list was empty (example: int,), some checks were
    // not performed.
    if (mDeferredSingleDeclarationErrorCheck)
    {
        singleDeclarationErrorCheck(publicType, identifierLocation);
        mDeferredSingleDeclarationErrorCheck = false;
    }

    checkDeclaratorLocationIsNotSpecified(identifierLocation, publicType);

    checkCanBeDeclaredWithoutInitializer(identifierLocation, identifier, &publicType);

    TVariable *variable = nullptr;
    TType type(publicType);
    declareVariable(identifierLocation, identifier, type, &variable);

    TIntermSymbol *symbol = intermediate.addSymbol(0, identifier, type, identifierLocation);
    if (variable && symbol)
    {
        symbol->setId(variable->getUniqueId());
        declarationOut->appendDeclarator(symbol);
    }
}

void TParseContext::parseArrayDeclarator(TPublicType &publicType,
                                         const TSourceLoc &identifierLocation,
                                         const TString &identifier,
                                         const TSourceLoc &arrayLocation,
                                         TIntermTyped *indexExpression,
                                         TIntermDeclaration *declarationOut)
{
    // If the declaration starting this declarator list was empty (example: int,), some checks were
    // not performed.
    if (mDeferredSingleDeclarationErrorCheck)
    {
        singleDeclarationErrorCheck(publicType, identifierLocation);
        mDeferredSingleDeclarationErrorCheck = false;
    }

    checkDeclaratorLocationIsNotSpecified(identifierLocation, publicType);

    checkCanBeDeclaredWithoutInitializer(identifierLocation, identifier, &publicType);

    if (checkIsValidTypeAndQualifierForArray(arrayLocation, publicType))
    {
        TType arrayType   = TType(publicType);
        unsigned int size = checkIsValidArraySize(arrayLocation, indexExpression);
        arrayType.setArraySize(size);

        TVariable *variable = nullptr;
        declareVariable(identifierLocation, identifier, arrayType, &variable);

        TIntermSymbol *symbol =
            intermediate.addSymbol(0, identifier, arrayType, identifierLocation);
        if (variable && symbol)
            symbol->setId(variable->getUniqueId());

        declarationOut->appendDeclarator(symbol);
    }
}

void TParseContext::parseInitDeclarator(const TPublicType &publicType,
                                        const TSourceLoc &identifierLocation,
                                        const TString &identifier,
                                        const TSourceLoc &initLocation,
                                        TIntermTyped *initializer,
                                        TIntermDeclaration *declarationOut)
{
    // If the declaration starting this declarator list was empty (example: int,), some checks were
    // not performed.
    if (mDeferredSingleDeclarationErrorCheck)
    {
        singleDeclarationErrorCheck(publicType, identifierLocation);
        mDeferredSingleDeclarationErrorCheck = false;
    }

    checkDeclaratorLocationIsNotSpecified(identifierLocation, publicType);

    TIntermBinary *initNode = nullptr;
    if (!executeInitializer(identifierLocation, identifier, publicType, initializer, &initNode))
    {
        //
        // build the intermediate representation
        //
        if (initNode)
        {
            declarationOut->appendDeclarator(initNode);
        }
    }
}

void TParseContext::parseArrayInitDeclarator(const TPublicType &publicType,
                                             const TSourceLoc &identifierLocation,
                                             const TString &identifier,
                                             const TSourceLoc &indexLocation,
                                             TIntermTyped *indexExpression,
                                             const TSourceLoc &initLocation,
                                             TIntermTyped *initializer,
                                             TIntermDeclaration *declarationOut)
{
    // If the declaration starting this declarator list was empty (example: int,), some checks were
    // not performed.
    if (mDeferredSingleDeclarationErrorCheck)
    {
        singleDeclarationErrorCheck(publicType, identifierLocation);
        mDeferredSingleDeclarationErrorCheck = false;
    }

    checkDeclaratorLocationIsNotSpecified(identifierLocation, publicType);

    checkIsValidTypeAndQualifierForArray(indexLocation, publicType);

    TPublicType arrayType(publicType);

    unsigned int size = 0u;
    // If indexExpression is nullptr, then the array will eventually get its size implicitly from
    // the initializer.
    if (indexExpression != nullptr)
    {
        size = checkIsValidArraySize(identifierLocation, indexExpression);
    }
    // Make the type an array even if size check failed.
    // This ensures useless error messages regarding the variable's non-arrayness won't follow.
    arrayType.setArraySize(size);

    // initNode will correspond to the whole of "b[n] = initializer".
    TIntermBinary *initNode = nullptr;
    if (!executeInitializer(identifierLocation, identifier, arrayType, initializer, &initNode))
    {
        if (initNode)
        {
            declarationOut->appendDeclarator(initNode);
        }
    }
}

void TParseContext::parseGlobalLayoutQualifier(const TTypeQualifierBuilder &typeQualifierBuilder)
{
    TTypeQualifier typeQualifier = typeQualifierBuilder.getVariableTypeQualifier(mDiagnostics);
    const TLayoutQualifier layoutQualifier = typeQualifier.layoutQualifier;

    checkInvariantVariableQualifier(typeQualifier.invariant, typeQualifier.qualifier,
                                    typeQualifier.line);

    // It should never be the case, but some strange parser errors can send us here.
    if (layoutQualifier.isEmpty())
    {
        error(typeQualifier.line, "Error during layout qualifier parsing.", "?");
        return;
    }

    if (!layoutQualifier.isCombinationValid())
    {
        error(typeQualifier.line, "invalid layout qualifier combination", "layout");
        return;
    }

    checkBindingIsNotSpecified(typeQualifier.line, layoutQualifier.binding);

    checkMemoryQualifierIsNotSpecified(typeQualifier.memoryQualifier, typeQualifier.line);

    checkInternalFormatIsNotSpecified(typeQualifier.line, layoutQualifier.imageInternalFormat);

    checkYuvIsNotSpecified(typeQualifier.line, layoutQualifier.yuv);

    if (typeQualifier.qualifier == EvqComputeIn)
    {
        if (mComputeShaderLocalSizeDeclared &&
            !layoutQualifier.isLocalSizeEqual(mComputeShaderLocalSize))
        {
            error(typeQualifier.line, "Work group size does not match the previous declaration",
                  "layout");
            return;
        }

        if (mShaderVersion < 310)
        {
            error(typeQualifier.line, "in type qualifier supported in GLSL ES 3.10 only", "layout");
            return;
        }

        if (!layoutQualifier.localSize.isAnyValueSet())
        {
            error(typeQualifier.line, "No local work group size specified", "layout");
            return;
        }

        const TVariable *maxComputeWorkGroupSize = static_cast<const TVariable *>(
            symbolTable.findBuiltIn("gl_MaxComputeWorkGroupSize", mShaderVersion));

        const TConstantUnion *maxComputeWorkGroupSizeData =
            maxComputeWorkGroupSize->getConstPointer();

        for (size_t i = 0u; i < layoutQualifier.localSize.size(); ++i)
        {
            if (layoutQualifier.localSize[i] != -1)
            {
                mComputeShaderLocalSize[i]             = layoutQualifier.localSize[i];
                const int maxComputeWorkGroupSizeValue = maxComputeWorkGroupSizeData[i].getIConst();
                if (mComputeShaderLocalSize[i] < 1 ||
                    mComputeShaderLocalSize[i] > maxComputeWorkGroupSizeValue)
                {
                    std::stringstream reasonStream;
                    reasonStream << "invalid value: Value must be at least 1 and no greater than "
                                 << maxComputeWorkGroupSizeValue;
                    const std::string &reason = reasonStream.str();

                    error(typeQualifier.line, reason.c_str(), getWorkGroupSizeString(i));
                    return;
                }
            }
        }

        mComputeShaderLocalSizeDeclared = true;
    }
    else if (mMultiviewAvailable &&
             (isExtensionEnabled("GL_OVR_multiview") || isExtensionEnabled("GL_OVR_multiview2")) &&
             typeQualifier.qualifier == EvqVertexIn)
    {
        // This error is only specified in WebGL, but tightens unspecified behavior in the native
        // specification.
        if (mNumViews != -1 && layoutQualifier.numViews != mNumViews)
        {
            error(typeQualifier.line, "Number of views does not match the previous declaration",
                  "layout");
            return;
        }

        if (layoutQualifier.numViews == -1)
        {
            error(typeQualifier.line, "No num_views specified", "layout");
            return;
        }

        if (layoutQualifier.numViews > mMaxNumViews)
        {
            error(typeQualifier.line, "num_views greater than the value of GL_MAX_VIEWS_OVR",
                  "layout");
            return;
        }

        mNumViews = layoutQualifier.numViews;
    }
    else
    {
        if (!checkWorkGroupSizeIsNotSpecified(typeQualifier.line, layoutQualifier))
        {
            return;
        }

        if (typeQualifier.qualifier != EvqUniform)
        {
            error(typeQualifier.line, "invalid qualifier: global layout must be uniform",
                  getQualifierString(typeQualifier.qualifier));
            return;
        }

        if (mShaderVersion < 300)
        {
            error(typeQualifier.line, "layout qualifiers supported in GLSL ES 3.00 and above",
                  "layout");
            return;
        }

        checkLocationIsNotSpecified(typeQualifier.line, layoutQualifier);

        if (layoutQualifier.matrixPacking != EmpUnspecified)
        {
            mDefaultMatrixPacking = layoutQualifier.matrixPacking;
        }

        if (layoutQualifier.blockStorage != EbsUnspecified)
        {
            mDefaultBlockStorage = layoutQualifier.blockStorage;
        }
    }
}

TIntermFunctionPrototype *TParseContext::createPrototypeNodeFromFunction(
    const TFunction &function,
    const TSourceLoc &location,
    bool insertParametersToSymbolTable)
{
    TIntermFunctionPrototype *prototype =
        new TIntermFunctionPrototype(function.getReturnType(), TSymbolUniqueId(function));
    // TODO(oetuaho@nvidia.com): Instead of converting the function information here, the node could
    // point to the data that already exists in the symbol table.
    prototype->getFunctionSymbolInfo()->setFromFunction(function);
    prototype->setLine(location);

    for (size_t i = 0; i < function.getParamCount(); i++)
    {
        const TConstParameter &param = function.getParam(i);

        // If the parameter has no name, it's not an error, just don't add it to symbol table (could
        // be used for unused args).
        if (param.name != nullptr)
        {
            TVariable *variable = new TVariable(param.name, *param.type);

            // Insert the parameter in the symbol table.
            if (insertParametersToSymbolTable && !symbolTable.declare(variable))
            {
                error(location, "redefinition", variable->getName().c_str());
                prototype->appendParameter(intermediate.addSymbol(0, "", *param.type, location));
                continue;
            }
            TIntermSymbol *symbol = intermediate.addSymbol(
                variable->getUniqueId(), variable->getName(), variable->getType(), location);
            prototype->appendParameter(symbol);
        }
        else
        {
            prototype->appendParameter(intermediate.addSymbol(0, "", *param.type, location));
        }
    }
    return prototype;
}

TIntermFunctionPrototype *TParseContext::addFunctionPrototypeDeclaration(
    const TFunction &parsedFunction,
    const TSourceLoc &location)
{
    // Note: function found from the symbol table could be the same as parsedFunction if this is the
    // first declaration. Either way the instance in the symbol table is used to track whether the
    // function is declared multiple times.
    TFunction *function = static_cast<TFunction *>(
        symbolTable.find(parsedFunction.getMangledName(), getShaderVersion()));
    if (function->hasPrototypeDeclaration() && mShaderVersion == 100)
    {
        // ESSL 1.00.17 section 4.2.7.
        // Doesn't apply to ESSL 3.00.4: see section 4.2.3.
        error(location, "duplicate function prototype declarations are not allowed", "function");
    }
    function->setHasPrototypeDeclaration();

    TIntermFunctionPrototype *prototype =
        createPrototypeNodeFromFunction(*function, location, false);

    symbolTable.pop();

    if (!symbolTable.atGlobalLevel())
    {
        // ESSL 3.00.4 section 4.2.4.
        error(location, "local function prototype declarations are not allowed", "function");
    }

    return prototype;
}

TIntermFunctionDefinition *TParseContext::addFunctionDefinition(
    TIntermFunctionPrototype *functionPrototype,
    TIntermBlock *functionBody,
    const TSourceLoc &location)
{
    // Check that non-void functions have at least one return statement.
    if (mCurrentFunctionType->getBasicType() != EbtVoid && !mFunctionReturnsValue)
    {
        error(location, "function does not return a value:",
              functionPrototype->getFunctionSymbolInfo()->getName().c_str());
    }

    if (functionBody == nullptr)
    {
        functionBody = new TIntermBlock();
        functionBody->setLine(location);
    }
    TIntermFunctionDefinition *functionNode =
        new TIntermFunctionDefinition(functionPrototype, functionBody);
    functionNode->setLine(location);

    symbolTable.pop();
    return functionNode;
}

void TParseContext::parseFunctionDefinitionHeader(const TSourceLoc &location,
                                                  TFunction **function,
                                                  TIntermFunctionPrototype **prototypeOut)
{
    ASSERT(function);
    ASSERT(*function);
    const TSymbol *builtIn =
        symbolTable.findBuiltIn((*function)->getMangledName(), getShaderVersion());

    if (builtIn)
    {
        error(location, "built-in functions cannot be redefined", (*function)->getName().c_str());
    }
    else
    {
        TFunction *prevDec = static_cast<TFunction *>(
            symbolTable.find((*function)->getMangledName(), getShaderVersion()));

        // Note: 'prevDec' could be 'function' if this is the first time we've seen function as it
        // would have just been put in the symbol table. Otherwise, we're looking up an earlier
        // occurance.
        if (*function != prevDec)
        {
            // Swap the parameters of the previous declaration to the parameters of the function
            // definition (parameter names may differ).
            prevDec->swapParameters(**function);

            // The function definition will share the same symbol as any previous declaration.
            *function = prevDec;
        }

        if ((*function)->isDefined())
        {
            error(location, "function already has a body", (*function)->getName().c_str());
        }

        (*function)->setDefined();
    }

    // Remember the return type for later checking for return statements.
    mCurrentFunctionType  = &((*function)->getReturnType());
    mFunctionReturnsValue = false;

    *prototypeOut = createPrototypeNodeFromFunction(**function, location, true);
    setLoopNestingLevel(0);
}

TFunction *TParseContext::parseFunctionDeclarator(const TSourceLoc &location, TFunction *function)
{
    //
    // We don't know at this point whether this is a function definition or a prototype.
    // The definition production code will check for redefinitions.
    // In the case of ESSL 1.00 the prototype production code will also check for redeclarations.
    //
    // Return types and parameter qualifiers must match in all redeclarations, so those are checked
    // here.
    //
    TFunction *prevDec =
        static_cast<TFunction *>(symbolTable.find(function->getMangledName(), getShaderVersion()));

    if (getShaderVersion() >= 300 &&
        symbolTable.hasUnmangledBuiltInForShaderVersion(function->getName().c_str(),
                                                        getShaderVersion()))
    {
        // With ESSL 3.00 and above, names of built-in functions cannot be redeclared as functions.
        // Therefore overloading or redefining builtin functions is an error.
        error(location, "Name of a built-in function cannot be redeclared as function",
              function->getName().c_str());
    }
    else if (prevDec)
    {
        if (prevDec->getReturnType() != function->getReturnType())
        {
            error(location, "function must have the same return type in all of its declarations",
                  function->getReturnType().getBasicString());
        }
        for (size_t i = 0; i < prevDec->getParamCount(); ++i)
        {
            if (prevDec->getParam(i).type->getQualifier() !=
                function->getParam(i).type->getQualifier())
            {
                error(location,
                      "function must have the same parameter qualifiers in all of its declarations",
                      function->getParam(i).type->getQualifierString());
            }
        }
    }

    //
    // Check for previously declared variables using the same name.
    //
    TSymbol *prevSym = symbolTable.find(function->getName(), getShaderVersion());
    if (prevSym)
    {
        if (!prevSym->isFunction())
        {
            error(location, "redefinition of a function", function->getName().c_str());
        }
    }
    else
    {
        // Insert the unmangled name to detect potential future redefinition as a variable.
        symbolTable.getOuterLevel()->insertUnmangled(function);
    }

    // We're at the inner scope level of the function's arguments and body statement.
    // Add the function prototype to the surrounding scope instead.
    symbolTable.getOuterLevel()->insert(function);

    // Raise error message if main function takes any parameters or return anything other than void
    if (function->getName() == "main")
    {
        if (function->getParamCount() > 0)
        {
            error(location, "function cannot take any parameter(s)", "main");
        }
        if (function->getReturnType().getBasicType() != EbtVoid)
        {
            error(location, "main function cannot return a value",
                  function->getReturnType().getBasicString());
        }
    }

    //
    // If this is a redeclaration, it could also be a definition, in which case, we want to use the
    // variable names from this one, and not the one that's
    // being redeclared.  So, pass back up this declaration, not the one in the symbol table.
    //
    return function;
}

TFunction *TParseContext::parseFunctionHeader(const TPublicType &type,
                                              const TString *name,
                                              const TSourceLoc &location)
{
    if (type.qualifier != EvqGlobal && type.qualifier != EvqTemporary)
    {
        error(location, "no qualifiers allowed for function return",
              getQualifierString(type.qualifier));
    }
    if (!type.layoutQualifier.isEmpty())
    {
        error(location, "no qualifiers allowed for function return", "layout");
    }
    // make sure a sampler or an image is not involved as well...
    checkIsNotSampler(location, type.typeSpecifierNonArray,
                      "samplers can't be function return values");
    checkIsNotImage(location, type.typeSpecifierNonArray, "images can't be function return values");
    if (mShaderVersion < 300)
    {
        // Array return values are forbidden, but there's also no valid syntax for declaring array
        // return values in ESSL 1.00.
        ASSERT(type.arraySize == 0 || mDiagnostics->numErrors() > 0);

        if (type.isStructureContainingArrays())
        {
            // ESSL 1.00.17 section 6.1 Function Definitions
            error(location, "structures containing arrays can't be function return values",
                  TType(type).getCompleteString().c_str());
        }
    }

    // Add the function as a prototype after parsing it (we do not support recursion)
    return new TFunction(name, new TType(type));
}

TFunction *TParseContext::addConstructorFunc(const TPublicType &publicTypeIn)
{
    TPublicType publicType = publicTypeIn;
    if (publicType.isStructSpecifier())
    {
        error(publicType.getLine(), "constructor can't be a structure definition",
              getBasicString(publicType.getBasicType()));
    }

    TOperator op = EOpNull;
    if (publicType.getUserDef())
    {
        op = EOpConstructStruct;
    }
    else
    {
        op = sh::TypeToConstructorOperator(TType(publicType));
        if (op == EOpNull)
        {
            error(publicType.getLine(), "cannot construct this type",
                  getBasicString(publicType.getBasicType()));
            publicType.setBasicType(EbtFloat);
            op = EOpConstructFloat;
        }
    }

    const TType *type = new TType(publicType);
    return new TFunction(nullptr, type, op);
}

// This function is used to test for the correctness of the parameters passed to various constructor
// functions and also convert them to the right datatype if it is allowed and required.
//
// Returns a node to add to the tree regardless of if an error was generated or not.
//
TIntermTyped *TParseContext::addConstructor(TIntermSequence *arguments,
                                            TOperator op,
                                            TType type,
                                            const TSourceLoc &line)
{
    if (type.isUnsizedArray())
    {
        if (arguments->empty())
        {
            error(line, "implicitly sized array constructor must have at least one argument", "[]");
            type.setArraySize(1u);
            return TIntermTyped::CreateZero(type);
        }
        type.setArraySize(static_cast<unsigned int>(arguments->size()));
    }

    if (!checkConstructorArguments(line, arguments, op, type))
    {
        return TIntermTyped::CreateZero(type);
    }

    TIntermAggregate *constructorNode = TIntermAggregate::CreateConstructor(type, op, arguments);
    constructorNode->setLine(line);

    TIntermTyped *constConstructor =
        intermediate.foldAggregateBuiltIn(constructorNode, mDiagnostics);
    if (constConstructor)
    {
        return constConstructor;
    }

    return constructorNode;
}

//
// Interface/uniform blocks
//
TIntermDeclaration *TParseContext::addInterfaceBlock(
    const TTypeQualifierBuilder &typeQualifierBuilder,
    const TSourceLoc &nameLine,
    const TString &blockName,
    TFieldList *fieldList,
    const TString *instanceName,
    const TSourceLoc &instanceLine,
    TIntermTyped *arrayIndex,
    const TSourceLoc &arrayIndexLine)
{
    checkIsNotReserved(nameLine, blockName);

    TTypeQualifier typeQualifier = typeQualifierBuilder.getVariableTypeQualifier(mDiagnostics);

    if (typeQualifier.qualifier != EvqUniform)
    {
        error(typeQualifier.line, "invalid qualifier: interface blocks must be uniform",
              getQualifierString(typeQualifier.qualifier));
    }

    if (typeQualifier.invariant)
    {
        error(typeQualifier.line, "invalid qualifier on interface block member", "invariant");
    }

    checkMemoryQualifierIsNotSpecified(typeQualifier.memoryQualifier, typeQualifier.line);

    // TODO(oetuaho): Remove this and support binding for blocks.
    checkBindingIsNotSpecified(typeQualifier.line, typeQualifier.layoutQualifier.binding);

    checkYuvIsNotSpecified(typeQualifier.line, typeQualifier.layoutQualifier.yuv);

    TLayoutQualifier blockLayoutQualifier = typeQualifier.layoutQualifier;
    checkLocationIsNotSpecified(typeQualifier.line, blockLayoutQualifier);

    if (blockLayoutQualifier.matrixPacking == EmpUnspecified)
    {
        blockLayoutQualifier.matrixPacking = mDefaultMatrixPacking;
    }

    if (blockLayoutQualifier.blockStorage == EbsUnspecified)
    {
        blockLayoutQualifier.blockStorage = mDefaultBlockStorage;
    }

    checkWorkGroupSizeIsNotSpecified(nameLine, blockLayoutQualifier);

    checkInternalFormatIsNotSpecified(nameLine, blockLayoutQualifier.imageInternalFormat);

    TSymbol *blockNameSymbol = new TInterfaceBlockName(&blockName);
    if (!symbolTable.declare(blockNameSymbol))
    {
        error(nameLine, "redefinition of an interface block name", blockName.c_str());
    }

    // check for sampler types and apply layout qualifiers
    for (size_t memberIndex = 0; memberIndex < fieldList->size(); ++memberIndex)
    {
        TField *field    = (*fieldList)[memberIndex];
        TType *fieldType = field->type();
        if (IsSampler(fieldType->getBasicType()))
        {
            error(field->line(),
                  "unsupported type - sampler types are not allowed in interface blocks",
                  fieldType->getBasicString());
        }

        if (IsImage(fieldType->getBasicType()))
        {
            error(field->line(),
                  "unsupported type - image types are not allowed in interface blocks",
                  fieldType->getBasicString());
        }

        const TQualifier qualifier = fieldType->getQualifier();
        switch (qualifier)
        {
            case EvqGlobal:
            case EvqUniform:
                break;
            default:
                error(field->line(), "invalid qualifier on interface block member",
                      getQualifierString(qualifier));
                break;
        }

        if (fieldType->isInvariant())
        {
            error(field->line(), "invalid qualifier on interface block member", "invariant");
        }

        // check layout qualifiers
        TLayoutQualifier fieldLayoutQualifier = fieldType->getLayoutQualifier();
        checkLocationIsNotSpecified(field->line(), fieldLayoutQualifier);

        if (fieldLayoutQualifier.blockStorage != EbsUnspecified)
        {
            error(field->line(), "invalid layout qualifier: cannot be used here",
                  getBlockStorageString(fieldLayoutQualifier.blockStorage));
        }

        if (fieldLayoutQualifier.matrixPacking == EmpUnspecified)
        {
            fieldLayoutQualifier.matrixPacking = blockLayoutQualifier.matrixPacking;
        }
        else if (!fieldType->isMatrix() && fieldType->getBasicType() != EbtStruct)
        {
            warning(field->line(),
                    "extraneous layout qualifier: only has an effect on matrix types",
                    getMatrixPackingString(fieldLayoutQualifier.matrixPacking));
        }

        fieldType->setLayoutQualifier(fieldLayoutQualifier);
    }

    // add array index
    unsigned int arraySize = 0;
    if (arrayIndex != nullptr)
    {
        arraySize = checkIsValidArraySize(arrayIndexLine, arrayIndex);
    }

    TInterfaceBlock *interfaceBlock =
        new TInterfaceBlock(&blockName, fieldList, instanceName, arraySize, blockLayoutQualifier);
    TType interfaceBlockType(interfaceBlock, typeQualifier.qualifier, blockLayoutQualifier,
                             arraySize);

    TString symbolName = "";
    int symbolId       = 0;

    if (!instanceName)
    {
        // define symbols for the members of the interface block
        for (size_t memberIndex = 0; memberIndex < fieldList->size(); ++memberIndex)
        {
            TField *field    = (*fieldList)[memberIndex];
            TType *fieldType = field->type();

            // set parent pointer of the field variable
            fieldType->setInterfaceBlock(interfaceBlock);

            TVariable *fieldVariable = new TVariable(&field->name(), *fieldType);
            fieldVariable->setQualifier(typeQualifier.qualifier);

            if (!symbolTable.declare(fieldVariable))
            {
                error(field->line(), "redefinition of an interface block member name",
                      field->name().c_str());
            }
        }
    }
    else
    {
        checkIsNotReserved(instanceLine, *instanceName);

        // add a symbol for this interface block
        TVariable *instanceTypeDef = new TVariable(instanceName, interfaceBlockType, false);
        instanceTypeDef->setQualifier(typeQualifier.qualifier);

        if (!symbolTable.declare(instanceTypeDef))
        {
            error(instanceLine, "redefinition of an interface block instance name",
                  instanceName->c_str());
        }

        symbolId   = instanceTypeDef->getUniqueId();
        symbolName = instanceTypeDef->getName();
    }

    TIntermSymbol *blockSymbol =
        intermediate.addSymbol(symbolId, symbolName, interfaceBlockType, typeQualifier.line);
    TIntermDeclaration *declaration = new TIntermDeclaration();
    declaration->appendDeclarator(blockSymbol);
    declaration->setLine(nameLine);

    exitStructDeclaration();
    return declaration;
}

void TParseContext::enterStructDeclaration(const TSourceLoc &line, const TString &identifier)
{
    ++mStructNestingLevel;

    // Embedded structure definitions are not supported per GLSL ES spec.
    // ESSL 1.00.17 section 10.9. ESSL 3.00.6 section 12.11.
    if (mStructNestingLevel > 1)
    {
        error(line, "Embedded struct definitions are not allowed", "struct");
    }
}

void TParseContext::exitStructDeclaration()
{
    --mStructNestingLevel;
}

void TParseContext::checkIsBelowStructNestingLimit(const TSourceLoc &line, const TField &field)
{
    if (!sh::IsWebGLBasedSpec(mShaderSpec))
    {
        return;
    }

    if (field.type()->getBasicType() != EbtStruct)
    {
        return;
    }

    // We're already inside a structure definition at this point, so add
    // one to the field's struct nesting.
    if (1 + field.type()->getDeepestStructNesting() > kWebGLMaxStructNesting)
    {
        std::stringstream reasonStream;
        reasonStream << "Reference of struct type " << field.type()->getStruct()->name().c_str()
                     << " exceeds maximum allowed nesting level of " << kWebGLMaxStructNesting;
        std::string reason = reasonStream.str();
        error(line, reason.c_str(), field.name().c_str());
        return;
    }
}

//
// Parse an array index expression
//
TIntermTyped *TParseContext::addIndexExpression(TIntermTyped *baseExpression,
                                                const TSourceLoc &location,
                                                TIntermTyped *indexExpression)
{
    if (!baseExpression->isArray() && !baseExpression->isMatrix() && !baseExpression->isVector())
    {
        if (baseExpression->getAsSymbolNode())
        {
            error(location, " left of '[' is not of type array, matrix, or vector ",
                  baseExpression->getAsSymbolNode()->getSymbol().c_str());
        }
        else
        {
            error(location, " left of '[' is not of type array, matrix, or vector ", "expression");
        }

        TConstantUnion *unionArray = new TConstantUnion[1];
        unionArray->setFConst(0.0f);
        return intermediate.addConstantUnion(unionArray, TType(EbtFloat, EbpHigh, EvqConst),
                                             location);
    }

    TIntermConstantUnion *indexConstantUnion = indexExpression->getAsConstantUnion();

    // TODO(oetuaho@nvidia.com): Get rid of indexConstantUnion == nullptr below once ANGLE is able
    // to constant fold all constant expressions. Right now we don't allow indexing interface blocks
    // or fragment outputs with expressions that ANGLE is not able to constant fold, even if the
    // index is a constant expression.
    if (indexExpression->getQualifier() != EvqConst || indexConstantUnion == nullptr)
    {
        if (baseExpression->isInterfaceBlock())
        {
            error(location,
                  "array indexes for interface blocks arrays must be constant integral expressions",
                  "[");
        }
        else if (baseExpression->getQualifier() == EvqFragmentOut)
        {
            error(location,
                  "array indexes for fragment outputs must be constant integral expressions", "[");
        }
        else if (mShaderSpec == SH_WEBGL2_SPEC && baseExpression->getQualifier() == EvqFragData)
        {
            error(location, "array index for gl_FragData must be constant zero", "[");
        }
    }

    if (indexConstantUnion)
    {
        // If an out-of-range index is not qualified as constant, the behavior in the spec is
        // undefined. This applies even if ANGLE has been able to constant fold it (ANGLE may
        // constant fold expressions that are not constant expressions). The most compatible way to
        // handle this case is to report a warning instead of an error and force the index to be in
        // the correct range.
        bool outOfRangeIndexIsError = indexExpression->getQualifier() == EvqConst;
        int index                   = indexConstantUnion->getIConst(0);

        int safeIndex = -1;

        if (baseExpression->isArray())
        {
            if (baseExpression->getQualifier() == EvqFragData && index > 0)
            {
                if (mShaderSpec == SH_WEBGL2_SPEC)
                {
                    // Error has been already generated if index is not const.
                    if (indexExpression->getQualifier() == EvqConst)
                    {
                        error(location, "array index for gl_FragData must be constant zero", "[");
                    }
                    safeIndex = 0;
                }
                else if (!isExtensionEnabled("GL_EXT_draw_buffers"))
                {
                    outOfRangeError(outOfRangeIndexIsError, location,
                                    "array index for gl_FragData must be zero when "
                                    "GL_EXT_draw_buffers is disabled",
                                    "[");
                    safeIndex = 0;
                }
            }
            // Only do generic out-of-range check if similar error hasn't already been reported.
            if (safeIndex < 0)
            {
                safeIndex = checkIndexOutOfRange(outOfRangeIndexIsError, location, index,
                                                 baseExpression->getArraySize(),
                                                 "array index out of range");
            }
        }
        else if (baseExpression->isMatrix())
        {
            safeIndex = checkIndexOutOfRange(outOfRangeIndexIsError, location, index,
                                             baseExpression->getType().getCols(),
                                             "matrix field selection out of range");
        }
        else if (baseExpression->isVector())
        {
            safeIndex = checkIndexOutOfRange(outOfRangeIndexIsError, location, index,
                                             baseExpression->getType().getNominalSize(),
                                             "vector field selection out of range");
        }

        ASSERT(safeIndex >= 0);
        // Data of constant unions can't be changed, because it may be shared with other
        // constant unions or even builtins, like gl_MaxDrawBuffers. Instead use a new
        // sanitized object.
        if (safeIndex != index)
        {
            TConstantUnion *safeConstantUnion = new TConstantUnion();
            safeConstantUnion->setIConst(safeIndex);
            indexConstantUnion->replaceConstantUnion(safeConstantUnion);
        }

        return intermediate.addIndex(EOpIndexDirect, baseExpression, indexExpression, location,
                                     mDiagnostics);
    }
    else
    {
        return intermediate.addIndex(EOpIndexIndirect, baseExpression, indexExpression, location,
                                     mDiagnostics);
    }
}

int TParseContext::checkIndexOutOfRange(bool outOfRangeIndexIsError,
                                        const TSourceLoc &location,
                                        int index,
                                        int arraySize,
                                        const char *reason)
{
    if (index >= arraySize || index < 0)
    {
        std::stringstream reasonStream;
        reasonStream << reason << " '" << index << "'";
        std::string token = reasonStream.str();
        outOfRangeError(outOfRangeIndexIsError, location, reason, "[]");
        if (index < 0)
        {
            return 0;
        }
        else
        {
            return arraySize - 1;
        }
    }
    return index;
}

TIntermTyped *TParseContext::addFieldSelectionExpression(TIntermTyped *baseExpression,
                                                         const TSourceLoc &dotLocation,
                                                         const TString &fieldString,
                                                         const TSourceLoc &fieldLocation)
{
    if (baseExpression->isArray())
    {
        error(fieldLocation, "cannot apply dot operator to an array", ".");
        return baseExpression;
    }

    if (baseExpression->isVector())
    {
        TVectorFields fields;
        if (!parseVectorFields(fieldString, baseExpression->getNominalSize(), fields,
                               fieldLocation))
        {
            fields.num        = 1;
            fields.offsets[0] = 0;
        }

        return TIntermediate::AddSwizzle(baseExpression, fields, dotLocation);
    }
    else if (baseExpression->getBasicType() == EbtStruct)
    {
        const TFieldList &fields = baseExpression->getType().getStruct()->fields();
        if (fields.empty())
        {
            error(dotLocation, "structure has no fields", "Internal Error");
            return baseExpression;
        }
        else
        {
            bool fieldFound = false;
            unsigned int i;
            for (i = 0; i < fields.size(); ++i)
            {
                if (fields[i]->name() == fieldString)
                {
                    fieldFound = true;
                    break;
                }
            }
            if (fieldFound)
            {
                TIntermTyped *index = TIntermTyped::CreateIndexNode(i);
                index->setLine(fieldLocation);
                return intermediate.addIndex(EOpIndexDirectStruct, baseExpression, index,
                                             dotLocation, mDiagnostics);
            }
            else
            {
                error(dotLocation, " no such field in structure", fieldString.c_str());
                return baseExpression;
            }
        }
    }
    else if (baseExpression->isInterfaceBlock())
    {
        const TFieldList &fields = baseExpression->getType().getInterfaceBlock()->fields();
        if (fields.empty())
        {
            error(dotLocation, "interface block has no fields", "Internal Error");
            return baseExpression;
        }
        else
        {
            bool fieldFound = false;
            unsigned int i;
            for (i = 0; i < fields.size(); ++i)
            {
                if (fields[i]->name() == fieldString)
                {
                    fieldFound = true;
                    break;
                }
            }
            if (fieldFound)
            {
                TIntermTyped *index = TIntermTyped::CreateIndexNode(i);
                index->setLine(fieldLocation);
                return intermediate.addIndex(EOpIndexDirectInterfaceBlock, baseExpression, index,
                                             dotLocation, mDiagnostics);
            }
            else
            {
                error(dotLocation, " no such field in interface block", fieldString.c_str());
                return baseExpression;
            }
        }
    }
    else
    {
        if (mShaderVersion < 300)
        {
            error(dotLocation, " field selection requires structure or vector on left hand side",
                  fieldString.c_str());
        }
        else
        {
            error(dotLocation,
                  " field selection requires structure, vector, or interface block on left hand "
                  "side",
                  fieldString.c_str());
        }
        return baseExpression;
    }
}

TLayoutQualifier TParseContext::parseLayoutQualifier(const TString &qualifierType,
                                                     const TSourceLoc &qualifierTypeLine)
{
    TLayoutQualifier qualifier = TLayoutQualifier::create();

    if (qualifierType == "shared")
    {
        if (sh::IsWebGLBasedSpec(mShaderSpec))
        {
            error(qualifierTypeLine, "Only std140 layout is allowed in WebGL", "shared");
        }
        qualifier.blockStorage = EbsShared;
    }
    else if (qualifierType == "packed")
    {
        if (sh::IsWebGLBasedSpec(mShaderSpec))
        {
            error(qualifierTypeLine, "Only std140 layout is allowed in WebGL", "packed");
        }
        qualifier.blockStorage = EbsPacked;
    }
    else if (qualifierType == "std140")
    {
        qualifier.blockStorage = EbsStd140;
    }
    else if (qualifierType == "row_major")
    {
        qualifier.matrixPacking = EmpRowMajor;
    }
    else if (qualifierType == "column_major")
    {
        qualifier.matrixPacking = EmpColumnMajor;
    }
    else if (qualifierType == "location")
    {
        error(qualifierTypeLine, "invalid layout qualifier: location requires an argument",
              qualifierType.c_str());
    }
    else if (qualifierType == "yuv" && isExtensionEnabled("GL_EXT_YUV_target") &&
             mShaderType == GL_FRAGMENT_SHADER)
    {
        qualifier.yuv = true;
    }
    else if (qualifierType == "rgba32f")
    {
        checkLayoutQualifierSupported(qualifierTypeLine, qualifierType, 310);
        qualifier.imageInternalFormat = EiifRGBA32F;
    }
    else if (qualifierType == "rgba16f")
    {
        checkLayoutQualifierSupported(qualifierTypeLine, qualifierType, 310);
        qualifier.imageInternalFormat = EiifRGBA16F;
    }
    else if (qualifierType == "r32f")
    {
        checkLayoutQualifierSupported(qualifierTypeLine, qualifierType, 310);
        qualifier.imageInternalFormat = EiifR32F;
    }
    else if (qualifierType == "rgba8")
    {
        checkLayoutQualifierSupported(qualifierTypeLine, qualifierType, 310);
        qualifier.imageInternalFormat = EiifRGBA8;
    }
    else if (qualifierType == "rgba8_snorm")
    {
        checkLayoutQualifierSupported(qualifierTypeLine, qualifierType, 310);
        qualifier.imageInternalFormat = EiifRGBA8_SNORM;
    }
    else if (qualifierType == "rgba32i")
    {
        checkLayoutQualifierSupported(qualifierTypeLine, qualifierType, 310);
        qualifier.imageInternalFormat = EiifRGBA32I;
    }
    else if (qualifierType == "rgba16i")
    {
        checkLayoutQualifierSupported(qualifierTypeLine, qualifierType, 310);
        qualifier.imageInternalFormat = EiifRGBA16I;
    }
    else if (qualifierType == "rgba8i")
    {
        checkLayoutQualifierSupported(qualifierTypeLine, qualifierType, 310);
        qualifier.imageInternalFormat = EiifRGBA8I;
    }
    else if (qualifierType == "r32i")
    {
        checkLayoutQualifierSupported(qualifierTypeLine, qualifierType, 310);
        qualifier.imageInternalFormat = EiifR32I;
    }
    else if (qualifierType == "rgba32ui")
    {
        checkLayoutQualifierSupported(qualifierTypeLine, qualifierType, 310);
        qualifier.imageInternalFormat = EiifRGBA32UI;
    }
    else if (qualifierType == "rgba16ui")
    {
        checkLayoutQualifierSupported(qualifierTypeLine, qualifierType, 310);
        qualifier.imageInternalFormat = EiifRGBA16UI;
    }
    else if (qualifierType == "rgba8ui")
    {
        checkLayoutQualifierSupported(qualifierTypeLine, qualifierType, 310);
        qualifier.imageInternalFormat = EiifRGBA8UI;
    }
    else if (qualifierType == "r32ui")
    {
        checkLayoutQualifierSupported(qualifierTypeLine, qualifierType, 310);
        qualifier.imageInternalFormat = EiifR32UI;
    }

    else
    {
        error(qualifierTypeLine, "invalid layout qualifier", qualifierType.c_str());
    }

    return qualifier;
}

void TParseContext::parseLocalSize(const TString &qualifierType,
                                   const TSourceLoc &qualifierTypeLine,
                                   int intValue,
                                   const TSourceLoc &intValueLine,
                                   const std::string &intValueString,
                                   size_t index,
                                   sh::WorkGroupSize *localSize)
{
    checkLayoutQualifierSupported(qualifierTypeLine, qualifierType, 310);
    if (intValue < 1)
    {
        std::stringstream reasonStream;
        reasonStream << "out of range: " << getWorkGroupSizeString(index) << " must be positive";
        std::string reason = reasonStream.str();
        error(intValueLine, reason.c_str(), intValueString.c_str());
    }
    (*localSize)[index] = intValue;
}

void TParseContext::parseNumViews(int intValue,
                                  const TSourceLoc &intValueLine,
                                  const std::string &intValueString,
                                  int *numViews)
{
    // This error is only specified in WebGL, but tightens unspecified behavior in the native
    // specification.
    if (intValue < 1)
    {
        error(intValueLine, "out of range: num_views must be positive", intValueString.c_str());
    }
    *numViews = intValue;
}

TLayoutQualifier TParseContext::parseLayoutQualifier(const TString &qualifierType,
                                                     const TSourceLoc &qualifierTypeLine,
                                                     int intValue,
                                                     const TSourceLoc &intValueLine)
{
    TLayoutQualifier qualifier = TLayoutQualifier::create();

    std::string intValueString = Str(intValue);

    if (qualifierType == "location")
    {
        // must check that location is non-negative
        if (intValue < 0)
        {
            error(intValueLine, "out of range: location must be non-negative",
                  intValueString.c_str());
        }
        else
        {
            qualifier.location           = intValue;
            qualifier.locationsSpecified = 1;
        }
    }
    else if (qualifierType == "binding")
    {
        checkLayoutQualifierSupported(qualifierTypeLine, qualifierType, 310);
        if (intValue < 0)
        {
            error(intValueLine, "out of range: binding must be non-negative",
                  intValueString.c_str());
        }
        else
        {
            qualifier.binding = intValue;
        }
    }
    else if (qualifierType == "local_size_x")
    {
        parseLocalSize(qualifierType, qualifierTypeLine, intValue, intValueLine, intValueString, 0u,
                       &qualifier.localSize);
    }
    else if (qualifierType == "local_size_y")
    {
        parseLocalSize(qualifierType, qualifierTypeLine, intValue, intValueLine, intValueString, 1u,
                       &qualifier.localSize);
    }
    else if (qualifierType == "local_size_z")
    {
        parseLocalSize(qualifierType, qualifierTypeLine, intValue, intValueLine, intValueString, 2u,
                       &qualifier.localSize);
    }
    else if (qualifierType == "num_views" && mMultiviewAvailable &&
             (isExtensionEnabled("GL_OVR_multiview") || isExtensionEnabled("GL_OVR_multiview2")) &&
             mShaderType == GL_VERTEX_SHADER)
    {
        parseNumViews(intValue, intValueLine, intValueString, &qualifier.numViews);
    }
    else
    {
        error(qualifierTypeLine, "invalid layout qualifier", qualifierType.c_str());
    }

    return qualifier;
}

TTypeQualifierBuilder *TParseContext::createTypeQualifierBuilder(const TSourceLoc &loc)
{
    return new TTypeQualifierBuilder(
        new TStorageQualifierWrapper(symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary, loc),
        mShaderVersion);
}

TLayoutQualifier TParseContext::joinLayoutQualifiers(TLayoutQualifier leftQualifier,
                                                     TLayoutQualifier rightQualifier,
                                                     const TSourceLoc &rightQualifierLocation)
{
    return sh::JoinLayoutQualifiers(leftQualifier, rightQualifier, rightQualifierLocation,
                                    mDiagnostics);
}

TFieldList *TParseContext::combineStructFieldLists(TFieldList *processedFields,
                                                   const TFieldList *newlyAddedFields,
                                                   const TSourceLoc &location)
{
    for (TField *field : *newlyAddedFields)
    {
        for (TField *oldField : *processedFields)
        {
            if (oldField->name() == field->name())
            {
                error(location, "duplicate field name in structure", field->name().c_str());
            }
        }
        processedFields->push_back(field);
    }
    return processedFields;
}

TFieldList *TParseContext::addStructDeclaratorListWithQualifiers(
    const TTypeQualifierBuilder &typeQualifierBuilder,
    TPublicType *typeSpecifier,
    TFieldList *fieldList)
{
    TTypeQualifier typeQualifier = typeQualifierBuilder.getVariableTypeQualifier(mDiagnostics);

    typeSpecifier->qualifier       = typeQualifier.qualifier;
    typeSpecifier->layoutQualifier = typeQualifier.layoutQualifier;
    typeSpecifier->memoryQualifier = typeQualifier.memoryQualifier;
    typeSpecifier->invariant       = typeQualifier.invariant;
    if (typeQualifier.precision != EbpUndefined)
    {
        typeSpecifier->precision = typeQualifier.precision;
    }
    return addStructDeclaratorList(*typeSpecifier, fieldList);
}

TFieldList *TParseContext::addStructDeclaratorList(const TPublicType &typeSpecifier,
                                                   TFieldList *fieldList)
{
    checkPrecisionSpecified(typeSpecifier.getLine(), typeSpecifier.precision,
                            typeSpecifier.getBasicType());

    checkIsNonVoid(typeSpecifier.getLine(), (*fieldList)[0]->name(), typeSpecifier.getBasicType());

    checkWorkGroupSizeIsNotSpecified(typeSpecifier.getLine(), typeSpecifier.layoutQualifier);

    for (unsigned int i = 0; i < fieldList->size(); ++i)
    {
        //
        // Careful not to replace already known aspects of type, like array-ness
        //
        TType *type = (*fieldList)[i]->type();
        type->setBasicType(typeSpecifier.getBasicType());
        type->setPrimarySize(typeSpecifier.getPrimarySize());
        type->setSecondarySize(typeSpecifier.getSecondarySize());
        type->setPrecision(typeSpecifier.precision);
        type->setQualifier(typeSpecifier.qualifier);
        type->setLayoutQualifier(typeSpecifier.layoutQualifier);
        type->setMemoryQualifier(typeSpecifier.memoryQualifier);
        type->setInvariant(typeSpecifier.invariant);

        // don't allow arrays of arrays
        if (type->isArray())
        {
            checkIsValidTypeForArray(typeSpecifier.getLine(), typeSpecifier);
        }
        if (typeSpecifier.array)
            type->setArraySize(static_cast<unsigned int>(typeSpecifier.arraySize));
        if (typeSpecifier.getUserDef())
        {
            type->setStruct(typeSpecifier.getUserDef()->getStruct());
        }

        checkIsBelowStructNestingLimit(typeSpecifier.getLine(), *(*fieldList)[i]);
    }

    return fieldList;
}

TTypeSpecifierNonArray TParseContext::addStructure(const TSourceLoc &structLine,
                                                   const TSourceLoc &nameLine,
                                                   const TString *structName,
                                                   TFieldList *fieldList)
{
    TStructure *structure = new TStructure(structName, fieldList);
    TType *structureType  = new TType(structure);

    // Store a bool in the struct if we're at global scope, to allow us to
    // skip the local struct scoping workaround in HLSL.
    structure->setAtGlobalScope(symbolTable.atGlobalLevel());

    if (!structName->empty())
    {
        checkIsNotReserved(nameLine, *structName);
        TVariable *userTypeDef = new TVariable(structName, *structureType, true);
        if (!symbolTable.declare(userTypeDef))
        {
            error(nameLine, "redefinition of a struct", structName->c_str());
        }
    }

    // ensure we do not specify any storage qualifiers on the struct members
    for (unsigned int typeListIndex = 0; typeListIndex < fieldList->size(); typeListIndex++)
    {
        const TField &field        = *(*fieldList)[typeListIndex];
        const TQualifier qualifier = field.type()->getQualifier();
        switch (qualifier)
        {
            case EvqGlobal:
            case EvqTemporary:
                break;
            default:
                error(field.line(), "invalid qualifier on struct member",
                      getQualifierString(qualifier));
                break;
        }
        if (field.type()->isInvariant())
        {
            error(field.line(), "invalid qualifier on struct member", "invariant");
        }
        if (IsImage(field.type()->getBasicType()))
        {
            error(field.line(), "disallowed type in struct", field.type()->getBasicString());
        }

        checkMemoryQualifierIsNotSpecified(field.type()->getMemoryQualifier(), field.line());

        checkBindingIsNotSpecified(field.line(), field.type()->getLayoutQualifier().binding);

        checkLocationIsNotSpecified(field.line(), field.type()->getLayoutQualifier());
    }

    TTypeSpecifierNonArray typeSpecifierNonArray;
    typeSpecifierNonArray.initialize(EbtStruct, structLine);
    typeSpecifierNonArray.userDef           = structureType;
    typeSpecifierNonArray.isStructSpecifier = true;
    exitStructDeclaration();

    return typeSpecifierNonArray;
}

TIntermSwitch *TParseContext::addSwitch(TIntermTyped *init,
                                        TIntermBlock *statementList,
                                        const TSourceLoc &loc)
{
    TBasicType switchType = init->getBasicType();
    if ((switchType != EbtInt && switchType != EbtUInt) || init->isMatrix() || init->isArray() ||
        init->isVector())
    {
        error(init->getLine(), "init-expression in a switch statement must be a scalar integer",
              "switch");
        return nullptr;
    }

    if (statementList)
    {
        if (!ValidateSwitchStatementList(switchType, mDiagnostics, statementList, loc))
        {
            return nullptr;
        }
    }

    TIntermSwitch *node = intermediate.addSwitch(init, statementList, loc);
    if (node == nullptr)
    {
        error(loc, "erroneous switch statement", "switch");
        return nullptr;
    }
    return node;
}

TIntermCase *TParseContext::addCase(TIntermTyped *condition, const TSourceLoc &loc)
{
    if (mSwitchNestingLevel == 0)
    {
        error(loc, "case labels need to be inside switch statements", "case");
        return nullptr;
    }
    if (condition == nullptr)
    {
        error(loc, "case label must have a condition", "case");
        return nullptr;
    }
    if ((condition->getBasicType() != EbtInt && condition->getBasicType() != EbtUInt) ||
        condition->isMatrix() || condition->isArray() || condition->isVector())
    {
        error(condition->getLine(), "case label must be a scalar integer", "case");
    }
    TIntermConstantUnion *conditionConst = condition->getAsConstantUnion();
    // TODO(oetuaho@nvidia.com): Get rid of the conditionConst == nullptr check once all constant
    // expressions can be folded. Right now we don't allow constant expressions that ANGLE can't
    // fold in case labels.
    if (condition->getQualifier() != EvqConst || conditionConst == nullptr)
    {
        error(condition->getLine(), "case label must be constant", "case");
    }
    TIntermCase *node = intermediate.addCase(condition, loc);
    if (node == nullptr)
    {
        error(loc, "erroneous case statement", "case");
        return nullptr;
    }
    return node;
}

TIntermCase *TParseContext::addDefault(const TSourceLoc &loc)
{
    if (mSwitchNestingLevel == 0)
    {
        error(loc, "default labels need to be inside switch statements", "default");
        return nullptr;
    }
    TIntermCase *node = intermediate.addCase(nullptr, loc);
    if (node == nullptr)
    {
        error(loc, "erroneous default statement", "default");
        return nullptr;
    }
    return node;
}

TIntermTyped *TParseContext::createUnaryMath(TOperator op,
                                             TIntermTyped *child,
                                             const TSourceLoc &loc)
{
    ASSERT(child != nullptr);

    switch (op)
    {
        case EOpLogicalNot:
            if (child->getBasicType() != EbtBool || child->isMatrix() || child->isArray() ||
                child->isVector())
            {
                unaryOpError(loc, GetOperatorString(op), child->getCompleteString());
                return nullptr;
            }
            break;
        case EOpBitwiseNot:
            if ((child->getBasicType() != EbtInt && child->getBasicType() != EbtUInt) ||
                child->isMatrix() || child->isArray())
            {
                unaryOpError(loc, GetOperatorString(op), child->getCompleteString());
                return nullptr;
            }
            break;
        case EOpPostIncrement:
        case EOpPreIncrement:
        case EOpPostDecrement:
        case EOpPreDecrement:
        case EOpNegative:
        case EOpPositive:
            if (child->getBasicType() == EbtStruct || child->getBasicType() == EbtBool ||
                child->isArray() || IsOpaqueType(child->getBasicType()))
            {
                unaryOpError(loc, GetOperatorString(op), child->getCompleteString());
                return nullptr;
            }
        // Operators for built-ins are already type checked against their prototype.
        default:
            break;
    }

    TIntermUnary *node = new TIntermUnary(op, child);
    node->setLine(loc);

    TIntermTyped *foldedNode = node->fold(mDiagnostics);
    if (foldedNode)
        return foldedNode;

    return node;
}

TIntermTyped *TParseContext::addUnaryMath(TOperator op, TIntermTyped *child, const TSourceLoc &loc)
{
    TIntermTyped *node = createUnaryMath(op, child, loc);
    if (node == nullptr)
    {
        return child;
    }
    return node;
}

TIntermTyped *TParseContext::addUnaryMathLValue(TOperator op,
                                                TIntermTyped *child,
                                                const TSourceLoc &loc)
{
    checkCanBeLValue(loc, GetOperatorString(op), child);
    return addUnaryMath(op, child, loc);
}

bool TParseContext::binaryOpCommonCheck(TOperator op,
                                        TIntermTyped *left,
                                        TIntermTyped *right,
                                        const TSourceLoc &loc)
{
    if (left->getType().getStruct() || right->getType().getStruct())
    {
        switch (op)
        {
            case EOpIndexDirectStruct:
                ASSERT(left->getType().getStruct());
                break;
            case EOpEqual:
            case EOpNotEqual:
            case EOpAssign:
            case EOpInitialize:
                if (left->getType() != right->getType())
                {
                    return false;
                }
                break;
            default:
                error(loc, "Invalid operation for structs", GetOperatorString(op));
                return false;
        }
    }

    if (left->isArray() || right->isArray())
    {
        if (mShaderVersion < 300)
        {
            error(loc, "Invalid operation for arrays", GetOperatorString(op));
            return false;
        }

        if (left->isArray() != right->isArray())
        {
            error(loc, "array / non-array mismatch", GetOperatorString(op));
            return false;
        }

        switch (op)
        {
            case EOpEqual:
            case EOpNotEqual:
            case EOpAssign:
            case EOpInitialize:
                break;
            default:
                error(loc, "Invalid operation for arrays", GetOperatorString(op));
                return false;
        }
        // At this point, size of implicitly sized arrays should be resolved.
        if (left->getArraySize() != right->getArraySize())
        {
            error(loc, "array size mismatch", GetOperatorString(op));
            return false;
        }
    }

    // Check ops which require integer / ivec parameters
    bool isBitShift = false;
    switch (op)
    {
        case EOpBitShiftLeft:
        case EOpBitShiftRight:
        case EOpBitShiftLeftAssign:
        case EOpBitShiftRightAssign:
            // Unsigned can be bit-shifted by signed and vice versa, but we need to
            // check that the basic type is an integer type.
            isBitShift = true;
            if (!IsInteger(left->getBasicType()) || !IsInteger(right->getBasicType()))
            {
                return false;
            }
            break;
        case EOpBitwiseAnd:
        case EOpBitwiseXor:
        case EOpBitwiseOr:
        case EOpBitwiseAndAssign:
        case EOpBitwiseXorAssign:
        case EOpBitwiseOrAssign:
            // It is enough to check the type of only one operand, since later it
            // is checked that the operand types match.
            if (!IsInteger(left->getBasicType()))
            {
                return false;
            }
            break;
        default:
            break;
    }

    // GLSL ES 1.00 and 3.00 do not support implicit type casting.
    // So the basic type should usually match.
    if (!isBitShift && left->getBasicType() != right->getBasicType())
    {
        return false;
    }

    // Check that:
    // 1. Type sizes match exactly on ops that require that.
    // 2. Restrictions for structs that contain arrays or samplers are respected.
    // 3. Arithmetic op type dimensionality restrictions for ops other than multiply are respected.
    switch (op)
    {
        case EOpAssign:
        case EOpInitialize:
        case EOpEqual:
        case EOpNotEqual:
            // ESSL 1.00 sections 5.7, 5.8, 5.9
            if (mShaderVersion < 300 && left->getType().isStructureContainingArrays())
            {
                error(loc, "undefined operation for structs containing arrays",
                      GetOperatorString(op));
                return false;
            }
            // Samplers as l-values are disallowed also in ESSL 3.00, see section 4.1.7,
            // we interpret the spec so that this extends to structs containing samplers,
            // similarly to ESSL 1.00 spec.
            if ((mShaderVersion < 300 || op == EOpAssign || op == EOpInitialize) &&
                left->getType().isStructureContainingSamplers())
            {
                error(loc, "undefined operation for structs containing samplers",
                      GetOperatorString(op));
                return false;
            }

            if ((op == EOpAssign || op == EOpInitialize) &&
                left->getType().isStructureContainingImages())
            {
                error(loc, "undefined operation for structs containing images",
                      GetOperatorString(op));
                return false;
            }
            if ((left->getNominalSize() != right->getNominalSize()) ||
                (left->getSecondarySize() != right->getSecondarySize()))
            {
                error(loc, "dimension mismatch", GetOperatorString(op));
                return false;
            }
            break;
        case EOpLessThan:
        case EOpGreaterThan:
        case EOpLessThanEqual:
        case EOpGreaterThanEqual:
            if (!left->isScalar() || !right->isScalar())
            {
                error(loc, "comparison operator only defined for scalars", GetOperatorString(op));
                return false;
            }
            break;
        case EOpAdd:
        case EOpSub:
        case EOpDiv:
        case EOpIMod:
        case EOpBitShiftLeft:
        case EOpBitShiftRight:
        case EOpBitwiseAnd:
        case EOpBitwiseXor:
        case EOpBitwiseOr:
        case EOpAddAssign:
        case EOpSubAssign:
        case EOpDivAssign:
        case EOpIModAssign:
        case EOpBitShiftLeftAssign:
        case EOpBitShiftRightAssign:
        case EOpBitwiseAndAssign:
        case EOpBitwiseXorAssign:
        case EOpBitwiseOrAssign:
            if ((left->isMatrix() && right->isVector()) || (left->isVector() && right->isMatrix()))
            {
                return false;
            }

            // Are the sizes compatible?
            if (left->getNominalSize() != right->getNominalSize() ||
                left->getSecondarySize() != right->getSecondarySize())
            {
                // If the nominal sizes of operands do not match:
                // One of them must be a scalar.
                if (!left->isScalar() && !right->isScalar())
                    return false;

                // In the case of compound assignment other than multiply-assign,
                // the right side needs to be a scalar. Otherwise a vector/matrix
                // would be assigned to a scalar. A scalar can't be shifted by a
                // vector either.
                if (!right->isScalar() &&
                    (IsAssignment(op) || op == EOpBitShiftLeft || op == EOpBitShiftRight))
                    return false;
            }
            break;
        default:
            break;
    }

    return true;
}

bool TParseContext::isMultiplicationTypeCombinationValid(TOperator op,
                                                         const TType &left,
                                                         const TType &right)
{
    switch (op)
    {
        case EOpMul:
        case EOpMulAssign:
            return left.getNominalSize() == right.getNominalSize() &&
                   left.getSecondarySize() == right.getSecondarySize();
        case EOpVectorTimesScalar:
            return true;
        case EOpVectorTimesScalarAssign:
            ASSERT(!left.isMatrix() && !right.isMatrix());
            return left.isVector() && !right.isVector();
        case EOpVectorTimesMatrix:
            return left.getNominalSize() == right.getRows();
        case EOpVectorTimesMatrixAssign:
            ASSERT(!left.isMatrix() && right.isMatrix());
            return left.isVector() && left.getNominalSize() == right.getRows() &&
                   left.getNominalSize() == right.getCols();
        case EOpMatrixTimesVector:
            return left.getCols() == right.getNominalSize();
        case EOpMatrixTimesScalar:
            return true;
        case EOpMatrixTimesScalarAssign:
            ASSERT(left.isMatrix() && !right.isMatrix());
            return !right.isVector();
        case EOpMatrixTimesMatrix:
            return left.getCols() == right.getRows();
        case EOpMatrixTimesMatrixAssign:
            ASSERT(left.isMatrix() && right.isMatrix());
            // We need to check two things:
            // 1. The matrix multiplication step is valid.
            // 2. The result will have the same number of columns as the lvalue.
            return left.getCols() == right.getRows() && left.getCols() == right.getCols();

        default:
            UNREACHABLE();
            return false;
    }
}

TIntermTyped *TParseContext::addBinaryMathInternal(TOperator op,
                                                   TIntermTyped *left,
                                                   TIntermTyped *right,
                                                   const TSourceLoc &loc)
{
    if (!binaryOpCommonCheck(op, left, right, loc))
        return nullptr;

    switch (op)
    {
        case EOpEqual:
        case EOpNotEqual:
        case EOpLessThan:
        case EOpGreaterThan:
        case EOpLessThanEqual:
        case EOpGreaterThanEqual:
            break;
        case EOpLogicalOr:
        case EOpLogicalXor:
        case EOpLogicalAnd:
            ASSERT(!left->isArray() && !right->isArray() && !left->getType().getStruct() &&
                   !right->getType().getStruct());
            if (left->getBasicType() != EbtBool || !left->isScalar() || !right->isScalar())
            {
                return nullptr;
            }
            // Basic types matching should have been already checked.
            ASSERT(right->getBasicType() == EbtBool);
            break;
        case EOpAdd:
        case EOpSub:
        case EOpDiv:
        case EOpMul:
            ASSERT(!left->isArray() && !right->isArray() && !left->getType().getStruct() &&
                   !right->getType().getStruct());
            if (left->getBasicType() == EbtBool)
            {
                return nullptr;
            }
            break;
        case EOpIMod:
            ASSERT(!left->isArray() && !right->isArray() && !left->getType().getStruct() &&
                   !right->getType().getStruct());
            // Note that this is only for the % operator, not for mod()
            if (left->getBasicType() == EbtBool || left->getBasicType() == EbtFloat)
            {
                return nullptr;
            }
            break;
        default:
            break;
    }

    if (op == EOpMul)
    {
        op = TIntermBinary::GetMulOpBasedOnOperands(left->getType(), right->getType());
        if (!isMultiplicationTypeCombinationValid(op, left->getType(), right->getType()))
        {
            return nullptr;
        }
    }

    TIntermBinary *node = new TIntermBinary(op, left, right);
    node->setLine(loc);

    // See if we can fold constants.
    TIntermTyped *foldedNode = node->fold(mDiagnostics);
    if (foldedNode)
        return foldedNode;

    return node;
}

TIntermTyped *TParseContext::addBinaryMath(TOperator op,
                                           TIntermTyped *left,
                                           TIntermTyped *right,
                                           const TSourceLoc &loc)
{
    TIntermTyped *node = addBinaryMathInternal(op, left, right, loc);
    if (node == 0)
    {
        binaryOpError(loc, GetOperatorString(op), left->getCompleteString(),
                      right->getCompleteString());
        return left;
    }
    return node;
}

TIntermTyped *TParseContext::addBinaryMathBooleanResult(TOperator op,
                                                        TIntermTyped *left,
                                                        TIntermTyped *right,
                                                        const TSourceLoc &loc)
{
    TIntermTyped *node = addBinaryMathInternal(op, left, right, loc);
    if (node == 0)
    {
        binaryOpError(loc, GetOperatorString(op), left->getCompleteString(),
                      right->getCompleteString());
        TConstantUnion *unionArray = new TConstantUnion[1];
        unionArray->setBConst(false);
        return intermediate.addConstantUnion(unionArray, TType(EbtBool, EbpUndefined, EvqConst),
                                             loc);
    }
    return node;
}

TIntermBinary *TParseContext::createAssign(TOperator op,
                                           TIntermTyped *left,
                                           TIntermTyped *right,
                                           const TSourceLoc &loc)
{
    if (binaryOpCommonCheck(op, left, right, loc))
    {
        if (op == EOpMulAssign)
        {
            op = TIntermBinary::GetMulAssignOpBasedOnOperands(left->getType(), right->getType());
            if (!isMultiplicationTypeCombinationValid(op, left->getType(), right->getType()))
            {
                return nullptr;
            }
        }
        TIntermBinary *node = new TIntermBinary(op, left, right);
        node->setLine(loc);

        return node;
    }
    return nullptr;
}

TIntermTyped *TParseContext::addAssign(TOperator op,
                                       TIntermTyped *left,
                                       TIntermTyped *right,
                                       const TSourceLoc &loc)
{
    TIntermTyped *node = createAssign(op, left, right, loc);
    if (node == nullptr)
    {
        assignError(loc, "assign", left->getCompleteString(), right->getCompleteString());
        return left;
    }
    return node;
}

TIntermTyped *TParseContext::addComma(TIntermTyped *left,
                                      TIntermTyped *right,
                                      const TSourceLoc &loc)
{
    // WebGL2 section 5.26, the following results in an error:
    // "Sequence operator applied to void, arrays, or structs containing arrays"
    if (mShaderSpec == SH_WEBGL2_SPEC &&
        (left->isArray() || left->getBasicType() == EbtVoid ||
         left->getType().isStructureContainingArrays() || right->isArray() ||
         right->getBasicType() == EbtVoid || right->getType().isStructureContainingArrays()))
    {
        error(loc,
              "sequence operator is not allowed for void, arrays, or structs containing arrays",
              ",");
    }

    return TIntermediate::AddComma(left, right, loc, mShaderVersion);
}

TIntermBranch *TParseContext::addBranch(TOperator op, const TSourceLoc &loc)
{
    switch (op)
    {
        case EOpContinue:
            if (mLoopNestingLevel <= 0)
            {
                error(loc, "continue statement only allowed in loops", "");
            }
            break;
        case EOpBreak:
            if (mLoopNestingLevel <= 0 && mSwitchNestingLevel <= 0)
            {
                error(loc, "break statement only allowed in loops and switch statements", "");
            }
            break;
        case EOpReturn:
            if (mCurrentFunctionType->getBasicType() != EbtVoid)
            {
                error(loc, "non-void function must return a value", "return");
            }
            break;
        default:
            // No checks for discard
            break;
    }
    return intermediate.addBranch(op, loc);
}

TIntermBranch *TParseContext::addBranch(TOperator op,
                                        TIntermTyped *returnValue,
                                        const TSourceLoc &loc)
{
    ASSERT(op == EOpReturn);
    mFunctionReturnsValue = true;
    if (mCurrentFunctionType->getBasicType() == EbtVoid)
    {
        error(loc, "void function cannot return a value", "return");
    }
    else if (*mCurrentFunctionType != returnValue->getType())
    {
        error(loc, "function return is not matching type:", "return");
    }
    return intermediate.addBranch(op, returnValue, loc);
}

void TParseContext::checkTextureOffsetConst(TIntermAggregate *functionCall)
{
    ASSERT(functionCall->getOp() == EOpCallBuiltInFunction);
    const TString &name        = functionCall->getFunctionSymbolInfo()->getName();
    TIntermNode *offset        = nullptr;
    TIntermSequence *arguments = functionCall->getSequence();
    if (name == "texelFetchOffset" || name == "textureLodOffset" ||
        name == "textureProjLodOffset" || name == "textureGradOffset" ||
        name == "textureProjGradOffset")
    {
        offset = arguments->back();
    }
    else if (name == "textureOffset" || name == "textureProjOffset")
    {
        // A bias parameter might follow the offset parameter.
        ASSERT(arguments->size() >= 3);
        offset = (*arguments)[2];
    }
    if (offset != nullptr)
    {
        TIntermConstantUnion *offsetConstantUnion = offset->getAsConstantUnion();
        if (offset->getAsTyped()->getQualifier() != EvqConst || !offsetConstantUnion)
        {
            error(functionCall->getLine(), "Texture offset must be a constant expression",
                  name.c_str());
        }
        else
        {
            ASSERT(offsetConstantUnion->getBasicType() == EbtInt);
            size_t size                  = offsetConstantUnion->getType().getObjectSize();
            const TConstantUnion *values = offsetConstantUnion->getUnionArrayPointer();
            for (size_t i = 0u; i < size; ++i)
            {
                int offsetValue = values[i].getIConst();
                if (offsetValue > mMaxProgramTexelOffset || offsetValue < mMinProgramTexelOffset)
                {
                    std::stringstream tokenStream;
                    tokenStream << offsetValue;
                    std::string token = tokenStream.str();
                    error(offset->getLine(), "Texture offset value out of valid range",
                          token.c_str());
                }
            }
        }
    }
}

// GLSL ES 3.10 Revision 4, 4.9 Memory Access Qualifiers
void TParseContext::checkImageMemoryAccessForBuiltinFunctions(TIntermAggregate *functionCall)
{
    ASSERT(functionCall->getOp() == EOpCallBuiltInFunction);
    const TString &name = functionCall->getFunctionSymbolInfo()->getName();

    if (name.compare(0, 5, "image") == 0)
    {
        TIntermSequence *arguments = functionCall->getSequence();
        TIntermTyped *imageNode    = (*arguments)[0]->getAsTyped();

        const TMemoryQualifier &memoryQualifier = imageNode->getMemoryQualifier();

        if (name.compare(5, 5, "Store") == 0)
        {
            if (memoryQualifier.readonly)
            {
                error(imageNode->getLine(),
                      "'imageStore' cannot be used with images qualified as 'readonly'",
                      GetImageArgumentToken(imageNode));
            }
        }
        else if (name.compare(5, 4, "Load") == 0)
        {
            if (memoryQualifier.writeonly)
            {
                error(imageNode->getLine(),
                      "'imageLoad' cannot be used with images qualified as 'writeonly'",
                      GetImageArgumentToken(imageNode));
            }
        }
    }
}

// GLSL ES 3.10 Revision 4, 13.51 Matching of Memory Qualifiers in Function Parameters
void TParseContext::checkImageMemoryAccessForUserDefinedFunctions(
    const TFunction *functionDefinition,
    const TIntermAggregate *functionCall)
{
    ASSERT(functionCall->getOp() == EOpCallFunctionInAST);

    const TIntermSequence &arguments = *functionCall->getSequence();

    ASSERT(functionDefinition->getParamCount() == arguments.size());

    for (size_t i = 0; i < arguments.size(); ++i)
    {
        TIntermTyped *typedArgument        = arguments[i]->getAsTyped();
        const TType &functionArgumentType  = typedArgument->getType();
        const TType &functionParameterType = *functionDefinition->getParam(i).type;
        ASSERT(functionArgumentType.getBasicType() == functionParameterType.getBasicType());

        if (IsImage(functionArgumentType.getBasicType()))
        {
            const TMemoryQualifier &functionArgumentMemoryQualifier =
                functionArgumentType.getMemoryQualifier();
            const TMemoryQualifier &functionParameterMemoryQualifier =
                functionParameterType.getMemoryQualifier();
            if (functionArgumentMemoryQualifier.readonly &&
                !functionParameterMemoryQualifier.readonly)
            {
                error(functionCall->getLine(),
                      "Function call discards the 'readonly' qualifier from image",
                      GetImageArgumentToken(typedArgument));
            }

            if (functionArgumentMemoryQualifier.writeonly &&
                !functionParameterMemoryQualifier.writeonly)
            {
                error(functionCall->getLine(),
                      "Function call discards the 'writeonly' qualifier from image",
                      GetImageArgumentToken(typedArgument));
            }

            if (functionArgumentMemoryQualifier.coherent &&
                !functionParameterMemoryQualifier.coherent)
            {
                error(functionCall->getLine(),
                      "Function call discards the 'coherent' qualifier from image",
                      GetImageArgumentToken(typedArgument));
            }

            if (functionArgumentMemoryQualifier.volatileQualifier &&
                !functionParameterMemoryQualifier.volatileQualifier)
            {
                error(functionCall->getLine(),
                      "Function call discards the 'volatile' qualifier from image",
                      GetImageArgumentToken(typedArgument));
            }
        }
    }
}

TIntermSequence *TParseContext::createEmptyArgumentsList()
{
    return new TIntermSequence();
}

TIntermTyped *TParseContext::addFunctionCallOrMethod(TFunction *fnCall,
                                                     TIntermSequence *arguments,
                                                     TIntermNode *thisNode,
                                                     const TSourceLoc &loc)
{
    if (thisNode != nullptr)
    {
        return addMethod(fnCall, arguments, thisNode, loc);
    }

    TOperator op = fnCall->getBuiltInOp();
    if (op != EOpNull)
    {
        return addConstructor(arguments, op, fnCall->getReturnType(), loc);
    }
    else
    {
        return addNonConstructorFunctionCall(fnCall, arguments, loc);
    }
}

TIntermTyped *TParseContext::addMethod(TFunction *fnCall,
                                       TIntermSequence *arguments,
                                       TIntermNode *thisNode,
                                       const TSourceLoc &loc)
{
    TConstantUnion *unionArray = new TConstantUnion[1];
    int arraySize              = 0;
    TIntermTyped *typedThis    = thisNode->getAsTyped();
    // It's possible for the name pointer in the TFunction to be null in case it gets parsed as
    // a constructor. But such a TFunction can't reach here, since the lexer goes into FIELDS
    // mode after a dot, which makes type identifiers to be parsed as FIELD_SELECTION instead.
    // So accessing fnCall->getName() below is safe.
    if (fnCall->getName() != "length")
    {
        error(loc, "invalid method", fnCall->getName().c_str());
    }
    else if (!arguments->empty())
    {
        error(loc, "method takes no parameters", "length");
    }
    else if (typedThis == nullptr || !typedThis->isArray())
    {
        error(loc, "length can only be called on arrays", "length");
    }
    else
    {
        arraySize = typedThis->getArraySize();
        if (typedThis->getAsSymbolNode() == nullptr)
        {
            // This code path can be hit with expressions like these:
            // (a = b).length()
            // (func()).length()
            // (int[3](0, 1, 2)).length()
            // ESSL 3.00 section 5.9 defines expressions so that this is not actually a valid
            // expression.
            // It allows "An array name with the length method applied" in contrast to GLSL 4.4
            // spec section 5.9 which allows "An array, vector or matrix expression with the
            // length method applied".
            error(loc, "length can only be called on array names, not on array expressions",
                  "length");
        }
    }
    unionArray->setIConst(arraySize);
    return intermediate.addConstantUnion(unionArray, TType(EbtInt, EbpUndefined, EvqConst), loc);
}

TIntermTyped *TParseContext::addNonConstructorFunctionCall(TFunction *fnCall,
                                                           TIntermSequence *arguments,
                                                           const TSourceLoc &loc)
{
    // First find by unmangled name to check whether the function name has been
    // hidden by a variable name or struct typename.
    // If a function is found, check for one with a matching argument list.
    bool builtIn;
    const TSymbol *symbol = symbolTable.find(fnCall->getName(), mShaderVersion, &builtIn);
    if (symbol != nullptr && !symbol->isFunction())
    {
        error(loc, "function name expected", fnCall->getName().c_str());
    }
    else
    {
        symbol = symbolTable.find(TFunction::GetMangledNameFromCall(fnCall->getName(), *arguments),
                                  mShaderVersion, &builtIn);
        if (symbol == nullptr)
        {
            error(loc, "no matching overloaded function found", fnCall->getName().c_str());
        }
        else
        {
            const TFunction *fnCandidate = static_cast<const TFunction *>(symbol);
            //
            // A declared function.
            //
            if (builtIn && !fnCandidate->getExtension().empty())
            {
                checkCanUseExtension(loc, fnCandidate->getExtension());
            }
            TOperator op = fnCandidate->getBuiltInOp();
            if (builtIn && op != EOpNull)
            {
                // A function call mapped to a built-in operation.
                if (fnCandidate->getParamCount() == 1)
                {
                    // Treat it like a built-in unary operator.
                    TIntermNode *unaryParamNode = arguments->front();
                    TIntermTyped *callNode = createUnaryMath(op, unaryParamNode->getAsTyped(), loc);
                    ASSERT(callNode != nullptr);
                    return callNode;
                }
                else
                {
                    TIntermAggregate *callNode =
                        TIntermAggregate::Create(fnCandidate->getReturnType(), op, arguments);
                    callNode->setLine(loc);

                    // Some built-in functions have out parameters too.
                    functionCallLValueErrorCheck(fnCandidate, callNode);

                    // See if we can constant fold a built-in. Note that this may be possible even
                    // if it is not const-qualified.
                    TIntermTyped *foldedNode =
                        intermediate.foldAggregateBuiltIn(callNode, mDiagnostics);
                    if (foldedNode)
                    {
                        return foldedNode;
                    }
                    return callNode;
                }
            }
            else
            {
                // This is a real function call
                TIntermAggregate *callNode = nullptr;

                // If builtIn == false, the function is user defined - could be an overloaded
                // built-in as well.
                // if builtIn == true, it's a builtIn function with no op associated with it.
                // This needs to happen after the function info including name is set.
                if (builtIn)
                {
                    callNode = TIntermAggregate::CreateBuiltInFunctionCall(*fnCandidate, arguments);
                    checkTextureOffsetConst(callNode);
                    checkImageMemoryAccessForBuiltinFunctions(callNode);
                }
                else
                {
                    callNode = TIntermAggregate::CreateFunctionCall(*fnCandidate, arguments);
                    checkImageMemoryAccessForUserDefinedFunctions(fnCandidate, callNode);
                }

                functionCallLValueErrorCheck(fnCandidate, callNode);

                callNode->setLine(loc);

                return callNode;
            }
        }
    }

    // Error message was already written. Put on a dummy node for error recovery.
    return TIntermTyped::CreateZero(TType(EbtFloat, EbpMedium, EvqConst));
}

TIntermTyped *TParseContext::addTernarySelection(TIntermTyped *cond,
                                                 TIntermTyped *trueExpression,
                                                 TIntermTyped *falseExpression,
                                                 const TSourceLoc &loc)
{
    checkIsScalarBool(loc, cond);

    if (trueExpression->getType() != falseExpression->getType())
    {
        binaryOpError(loc, ":", trueExpression->getCompleteString(),
                      falseExpression->getCompleteString());
        return falseExpression;
    }
    if (IsOpaqueType(trueExpression->getBasicType()))
    {
        // ESSL 1.00 section 4.1.7
        // ESSL 3.00 section 4.1.7
        // Opaque/sampler types are not allowed in most types of expressions, including ternary.
        // Note that structs containing opaque types don't need to be checked as structs are
        // forbidden below.
        error(loc, "ternary operator is not allowed for opaque types", ":");
        return falseExpression;
    }

    // ESSL1 sections 5.2 and 5.7:
    // ESSL3 section 5.7:
    // Ternary operator is not among the operators allowed for structures/arrays.
    if (trueExpression->isArray() || trueExpression->getBasicType() == EbtStruct)
    {
        error(loc, "ternary operator is not allowed for structures or arrays", ":");
        return falseExpression;
    }
    // WebGL2 section 5.26, the following results in an error:
    // "Ternary operator applied to void, arrays, or structs containing arrays"
    if (mShaderSpec == SH_WEBGL2_SPEC && trueExpression->getBasicType() == EbtVoid)
    {
        error(loc, "ternary operator is not allowed for void", ":");
        return falseExpression;
    }

    return TIntermediate::AddTernarySelection(cond, trueExpression, falseExpression, loc);
}

//
// Parse an array of strings using yyparse.
//
// Returns 0 for success.
//
int PaParseStrings(size_t count,
                   const char *const string[],
                   const int length[],
                   TParseContext *context)
{
    if ((count == 0) || (string == NULL))
        return 1;

    if (glslang_initialize(context))
        return 1;

    int error = glslang_scan(count, string, length, context);
    if (!error)
        error = glslang_parse(context);

    glslang_finalize(context);

    return (error == 0) && (context->numErrors() == 0) ? 0 : 1;
}

}  // namespace sh
