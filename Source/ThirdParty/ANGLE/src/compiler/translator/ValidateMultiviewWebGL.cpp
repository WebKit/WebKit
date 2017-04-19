//
// Copyright (c) 2002-2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ValidateMultiviewWebGL.cpp:
//   Validate the AST according to rules in the WEBGL_multiview extension spec. Only covers those
//   rules not already covered in ParseContext.
//

#include "compiler/translator/ValidateMultiviewWebGL.h"

#include <array>

#include "angle_gl.h"
#include "compiler/translator/Diagnostics.h"
#include "compiler/translator/FindSymbolNode.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/SymbolTable.h"

namespace sh
{

namespace
{

class ValidateMultiviewTraverser : public TIntermTraverser
{
  public:
    // Check for errors and write error messages to diagnostics. Returns true if there are no
    // errors.
    static bool validate(TIntermBlock *root,
                         GLenum shaderType,
                         const TSymbolTable &symbolTable,
                         int shaderVersion,
                         bool multiview2,
                         TDiagnostics *diagnostics);

    bool isValid() { return mValid; }

  protected:
    void visitSymbol(TIntermSymbol *node) override;
    bool visitBinary(Visit visit, TIntermBinary *node) override;
    bool visitUnary(Visit visit, TIntermUnary *node) override;
    bool visitIfElse(Visit visit, TIntermIfElse *node) override;
    bool visitAggregate(Visit visit, TIntermAggregate *node) override;

  private:
    ValidateMultiviewTraverser(GLenum shaderType,
                               const TSymbolTable &symbolTable,
                               int shaderVersion,
                               bool multiview2,
                               TDiagnostics *diagnostics);

    static bool IsGLPosition(TIntermNode *node);
    static bool IsGLViewIDOVR(TIntermNode *node);
    static bool IsSimpleAssignmentToGLPositionX(TIntermBinary *node);
    static bool IsSimpleAssignmentToGLPosition(TIntermBinary *node);

    void validateAndTraverseViewIDConditionalBlock(TIntermBlock *block, const char *token);

    bool mValid;
    bool mMultiview2;
    GLenum mShaderType;
    const TSymbolTable &mSymbolTable;
    const int mShaderVersion;

    bool mInsideViewIDConditional;  // Only set if mMultiview2 is false.
    bool mInsideRestrictedAssignment;
    bool mGLPositionAllowed;
    bool mViewIDOVRAllowed;

    TDiagnostics *mDiagnostics;
};

bool ValidateMultiviewTraverser::validate(TIntermBlock *root,
                                          GLenum shaderType,
                                          const TSymbolTable &symbolTable,
                                          int shaderVersion,
                                          bool multiview2,
                                          TDiagnostics *diagnostics)
{
    ValidateMultiviewTraverser validate(shaderType, symbolTable, shaderVersion, multiview2,
                                        diagnostics);
    ASSERT(root);
    root->traverse(&validate);
    return validate.isValid();
}

ValidateMultiviewTraverser::ValidateMultiviewTraverser(GLenum shaderType,
                                                       const TSymbolTable &symbolTable,
                                                       int shaderVersion,
                                                       bool multiview2,
                                                       TDiagnostics *diagnostics)
    : TIntermTraverser(true, true, true),
      mValid(true),
      mMultiview2(multiview2),
      mShaderType(shaderType),
      mSymbolTable(symbolTable),
      mShaderVersion(shaderVersion),
      mInsideViewIDConditional(false),
      mInsideRestrictedAssignment(false),
      mGLPositionAllowed(multiview2),
      mViewIDOVRAllowed(multiview2),
      mDiagnostics(diagnostics)
{
}

bool ValidateMultiviewTraverser::IsGLPosition(TIntermNode *node)
{
    TIntermSymbol *symbolNode = node->getAsSymbolNode();
    return (symbolNode && symbolNode->getName().getString() == "gl_Position");
}

bool ValidateMultiviewTraverser::IsGLViewIDOVR(TIntermNode *node)
{
    TIntermSymbol *symbolNode = node->getAsSymbolNode();
    return (symbolNode && symbolNode->getName().getString() == "gl_ViewID_OVR");
}

bool ValidateMultiviewTraverser::IsSimpleAssignmentToGLPositionX(TIntermBinary *node)
{
    if (node->getOp() == EOpAssign)
    {
        TIntermSwizzle *leftAsSwizzle = node->getLeft()->getAsSwizzleNode();
        if (leftAsSwizzle && IsGLPosition(leftAsSwizzle->getOperand()) &&
            leftAsSwizzle->offsetsMatch(0))
        {
            return true;
        }
        TIntermBinary *leftAsBinary = node->getLeft()->getAsBinaryNode();
        if (leftAsBinary && leftAsBinary->getOp() == EOpIndexDirect &&
            IsGLPosition(leftAsBinary->getLeft()) &&
            leftAsBinary->getRight()->getAsConstantUnion()->getIConst(0) == 0)
        {
            return true;
        }
    }
    return false;
}

bool ValidateMultiviewTraverser::IsSimpleAssignmentToGLPosition(TIntermBinary *node)
{
    if (node->getOp() == EOpAssign)
    {
        if (IsGLPosition(node->getLeft()))
        {
            return true;
        }
        TIntermSwizzle *leftAsSwizzle = node->getLeft()->getAsSwizzleNode();
        if (leftAsSwizzle && IsGLPosition(leftAsSwizzle->getOperand()))
        {
            return true;
        }
        TIntermBinary *leftAsBinary = node->getLeft()->getAsBinaryNode();
        if (leftAsBinary && leftAsBinary->getOp() == EOpIndexDirect &&
            IsGLPosition(leftAsBinary->getLeft()))
        {
            return true;
        }
    }
    return false;
}

void ValidateMultiviewTraverser::visitSymbol(TIntermSymbol *node)
{
    if (IsGLPosition(node) && !mGLPositionAllowed)
    {
        ASSERT(!mMultiview2);
        mDiagnostics->error(node->getLine(),
                            "Disallowed use of gl_Position when using OVR_multiview",
                            "gl_Position");
        mValid = false;
    }
    else if (IsGLViewIDOVR(node) && !mViewIDOVRAllowed)
    {
        ASSERT(!mMultiview2);
        mDiagnostics->error(node->getLine(),
                            "Disallowed use of gl_ViewID_OVR when using OVR_multiview",
                            "gl_ViewID_OVR");
        mValid = false;
    }
    else if (!mMultiview2 && mShaderType == GL_FRAGMENT_SHADER)
    {
        std::array<const char *, 3> disallowedFragmentShaderSymbols{
            {"gl_FragCoord", "gl_PointCoord", "gl_FrontFacing"}};
        for (auto disallowedSymbol : disallowedFragmentShaderSymbols)
        {
            if (node->getSymbol() == disallowedSymbol)
            {
                mDiagnostics->error(
                    node->getLine(),
                    "Disallowed use of a built-in variable when using OVR_multiview",
                    disallowedSymbol);
                mValid = false;
            }
        }
    }
}

bool ValidateMultiviewTraverser::visitBinary(Visit visit, TIntermBinary *node)
{
    if (!mMultiview2 && mShaderType == GL_VERTEX_SHADER)
    {
        if (visit == PreVisit)
        {
            ASSERT(!mInsideViewIDConditional || mInsideRestrictedAssignment);
            if (node->isAssignment())
            {
                if (mInsideRestrictedAssignment)
                {
                    mDiagnostics->error(node->getLine(),
                                        "Disallowed assignment inside assignment to gl_Position.x "
                                        "when using OVR_multiview",
                                        GetOperatorString(node->getOp()));
                    mValid = false;
                }
                else if (IsSimpleAssignmentToGLPositionX(node) &&
                         FindSymbolNode(node->getRight(), "gl_ViewID_OVR", EbtUInt))
                {
                    if (!getParentNode()->getAsBlock())
                    {
                        mDiagnostics->error(node->getLine(),
                                            "Disallowed use of assignment to gl_Position.x "
                                            "when using OVR_multiview",
                                            "=");
                        mValid = false;
                    }
                    mInsideRestrictedAssignment = true;
                    mViewIDOVRAllowed           = true;
                    node->getRight()->traverse(this);
                    mInsideRestrictedAssignment = false;
                    mViewIDOVRAllowed           = false;
                    return false;
                }
                else if (IsSimpleAssignmentToGLPosition(node))
                {
                    node->getRight()->traverse(this);
                    return false;
                }
            }
        }
    }
    return true;
}

bool ValidateMultiviewTraverser::visitUnary(Visit visit, TIntermUnary *node)
{
    if (visit == PreVisit && !mMultiview2 && mInsideRestrictedAssignment)
    {
        if (node->isAssignment())
        {
            mDiagnostics->error(node->getLine(),
                                "Disallowed unary operator inside assignment to gl_Position.x when "
                                "using OVR_multiview",
                                GetOperatorString(node->getOp()));
            mValid = false;
        }
    }
    return true;
}

void ValidateMultiviewTraverser::validateAndTraverseViewIDConditionalBlock(TIntermBlock *block,
                                                                           const char *token)
{
    if (block->getSequence()->size() > 1)
    {
        mDiagnostics->error(block->getLine(),
                            "Only one assignment to gl_Position allowed inside if block dependent "
                            "on gl_ViewID_OVR when using OVR_multiview",
                            token);
        mValid = false;
    }
    else if (block->getSequence()->size() == 1)
    {
        TIntermBinary *assignment = block->getSequence()->at(0)->getAsBinaryNode();
        if (assignment && IsSimpleAssignmentToGLPositionX(assignment))
        {
            mInsideRestrictedAssignment = true;
            assignment->getRight()->traverse(this);
            mInsideRestrictedAssignment = false;
        }
        else
        {
            mDiagnostics->error(block->getLine(),
                                "Only one assignment to gl_Position.x allowed inside if block "
                                "dependent on gl_ViewID_OVR when using OVR_multiview",
                                token);
            mValid = false;
        }
    }
}

bool ValidateMultiviewTraverser::visitIfElse(Visit visit, TIntermIfElse *node)
{
    if (!mMultiview2 && mShaderType == GL_VERTEX_SHADER)
    {
        ASSERT(visit == PreVisit);

        // Check if the if statement refers to gl_ViewID_OVR and if it does so correctly
        TIntermBinary *binaryCondition              = node->getCondition()->getAsBinaryNode();
        bool conditionIsAllowedComparisonWithViewID = false;
        if (binaryCondition && binaryCondition->getOp() == EOpEqual)
        {
            conditionIsAllowedComparisonWithViewID =
                IsGLViewIDOVR(binaryCondition->getLeft()) &&
                binaryCondition->getRight()->getAsConstantUnion() &&
                binaryCondition->getRight()->getQualifier() == EvqConst;
            if (!conditionIsAllowedComparisonWithViewID)
            {
                conditionIsAllowedComparisonWithViewID =
                    IsGLViewIDOVR(binaryCondition->getRight()) &&
                    binaryCondition->getLeft()->getAsConstantUnion() &&
                    binaryCondition->getLeft()->getQualifier() == EvqConst;
            }
        }
        if (conditionIsAllowedComparisonWithViewID)
        {
            mInsideViewIDConditional = true;
            if (node->getTrueBlock())
            {
                validateAndTraverseViewIDConditionalBlock(node->getTrueBlock(), "if");
            }
            else
            {
                mDiagnostics->error(node->getLine(), "Expected assignment to gl_Position.x", "if");
            }
            if (node->getFalseBlock())
            {
                validateAndTraverseViewIDConditionalBlock(node->getFalseBlock(), "else");
            }
            mInsideViewIDConditional = false;
        }
        else
        {
            node->getCondition()->traverse(this);
            if (node->getTrueBlock())
            {
                node->getTrueBlock()->traverse(this);
            }
            if (node->getFalseBlock())
            {
                node->getFalseBlock()->traverse(this);
            }
        }
        return false;
    }
    return true;
}

bool ValidateMultiviewTraverser::visitAggregate(Visit visit, TIntermAggregate *node)
{
    if (visit == PreVisit && !mMultiview2 && mInsideRestrictedAssignment)
    {
        // Check if the node is an user-defined function call or if an l-value is required, or if
        // there are possible visible side effects, such as image writes.
        if (node->getOp() == EOpCallFunctionInAST)
        {
            mDiagnostics->error(node->getLine(),
                                "Disallowed user defined function call inside assignment to "
                                "gl_Position.x when using OVR_multiview",
                                GetOperatorString(node->getOp()));
            mValid = false;
        }
        else if (node->getOp() == EOpCallBuiltInFunction &&
                 node->getFunctionSymbolInfo()->getName() == "imageStore")
        {
            // TODO(oetuaho@nvidia.com): Record which built-in functions have side effects in
            // the symbol info instead.
            mDiagnostics->error(node->getLine(),
                                "Disallowed function call with side effects inside assignment "
                                "to gl_Position.x when using OVR_multiview",
                                GetOperatorString(node->getOp()));
            mValid = false;
        }
        else if (!node->isConstructor())
        {
            TFunction *builtInFunc = static_cast<TFunction *>(
                mSymbolTable.findBuiltIn(node->getSymbolTableMangledName(), mShaderVersion));
            for (size_t paramIndex = 0u; paramIndex < builtInFunc->getParamCount(); ++paramIndex)
            {
                TQualifier qualifier = builtInFunc->getParam(paramIndex).type->getQualifier();
                if (qualifier == EvqOut || qualifier == EvqInOut)
                {
                    mDiagnostics->error(node->getLine(),
                                        "Disallowed use of a function with an out parameter inside "
                                        "assignment to gl_Position.x when using OVR_multiview",
                                        GetOperatorString(node->getOp()));
                    mValid = false;
                }
            }
        }
    }
    return true;
}

}  // anonymous namespace

bool ValidateMultiviewWebGL(TIntermBlock *root,
                            GLenum shaderType,
                            const TSymbolTable &symbolTable,
                            int shaderVersion,
                            bool multiview2,
                            TDiagnostics *diagnostics)
{
    if (shaderType == GL_VERTEX_SHADER && !FindSymbolNode(root, "gl_ViewID_OVR", EbtUInt))
    {
        return true;
    }
    return ValidateMultiviewTraverser::validate(root, shaderType, symbolTable, shaderVersion,
                                                multiview2, diagnostics);
}

}  // namespace sh
