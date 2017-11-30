//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Applies the necessary AST transformations to support multiview rendering through instancing.
// Check the header file For more information.
//

#include "compiler/translator/DeclareAndInitBuiltinsForInstancedMultiview.h"

#include "compiler/translator/FindMain.h"
#include "compiler/translator/InitializeVariables.h"
#include "compiler/translator/IntermNode_util.h"
#include "compiler/translator/IntermTraverse.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{

class ReplaceVariableTraverser : public TIntermTraverser
{
  public:
    ReplaceVariableTraverser(const TString &symbolName, TIntermSymbol *newSymbol)
        : TIntermTraverser(true, false, false), mSymbolName(symbolName), mNewSymbol(newSymbol)
    {
    }

    void visitSymbol(TIntermSymbol *node) override
    {
        TName &name = node->getName();
        if (name.getString() == mSymbolName)
        {
            queueReplacement(mNewSymbol->deepCopy(), OriginalNode::IS_DROPPED);
        }
    }

  private:
    TString mSymbolName;
    TIntermSymbol *mNewSymbol;
};

TIntermSymbol *CreateGLInstanceIDSymbol(const TSymbolTable &symbolTable)
{
    return ReferenceBuiltInVariable("gl_InstanceID", symbolTable, 300);
}

// Adds the InstanceID and ViewID_OVR initializers to the end of the initializers' sequence.
void InitializeViewIDAndInstanceID(TIntermTyped *viewIDSymbol,
                                   TIntermTyped *instanceIDSymbol,
                                   unsigned numberOfViews,
                                   const TSymbolTable &symbolTable,
                                   TIntermSequence *initializers)
{
    // Create an unsigned numberOfViews node.
    TConstantUnion *numberOfViewsUnsignedConstant = new TConstantUnion();
    numberOfViewsUnsignedConstant->setUConst(numberOfViews);
    TIntermConstantUnion *numberOfViewsUint =
        new TIntermConstantUnion(numberOfViewsUnsignedConstant, TType(EbtUInt, EbpHigh, EvqConst));

    // Create a uint(gl_InstanceID) node.
    TIntermSequence *glInstanceIDSymbolCastArguments = new TIntermSequence();
    glInstanceIDSymbolCastArguments->push_back(CreateGLInstanceIDSymbol(symbolTable));
    TIntermAggregate *glInstanceIDAsUint = TIntermAggregate::CreateConstructor(
        TType(EbtUInt, EbpHigh, EvqTemporary), glInstanceIDSymbolCastArguments);

    // Create a uint(gl_InstanceID) / numberOfViews node.
    TIntermBinary *normalizedInstanceID =
        new TIntermBinary(EOpDiv, glInstanceIDAsUint, numberOfViewsUint);

    // Create an int(uint(gl_InstanceID) / numberOfViews) node.
    TIntermSequence *normalizedInstanceIDCastArguments = new TIntermSequence();
    normalizedInstanceIDCastArguments->push_back(normalizedInstanceID);
    TIntermAggregate *normalizedInstanceIDAsInt = TIntermAggregate::CreateConstructor(
        TType(EbtInt, EbpHigh, EvqTemporary), normalizedInstanceIDCastArguments);

    // Create an InstanceID = int(uint(gl_InstanceID) / numberOfViews) node.
    TIntermBinary *instanceIDInitializer =
        new TIntermBinary(EOpAssign, instanceIDSymbol->deepCopy(), normalizedInstanceIDAsInt);
    initializers->push_back(instanceIDInitializer);

    // Create a uint(gl_InstanceID) % numberOfViews node.
    TIntermBinary *normalizedViewID =
        new TIntermBinary(EOpIMod, glInstanceIDAsUint->deepCopy(), numberOfViewsUint->deepCopy());

    // Create a ViewID_OVR = uint(gl_InstanceID) % numberOfViews node.
    TIntermBinary *viewIDInitializer =
        new TIntermBinary(EOpAssign, viewIDSymbol->deepCopy(), normalizedViewID);
    initializers->push_back(viewIDInitializer);
}

// Replaces every occurrence of a symbol with the name specified in symbolName with newSymbolNode.
void ReplaceSymbol(TIntermBlock *root, const TString &symbolName, TIntermSymbol *newSymbolNode)
{
    ReplaceVariableTraverser traverser(symbolName, newSymbolNode);
    root->traverse(&traverser);
    traverser.updateTree();
}

void DeclareGlobalVariable(TIntermBlock *root, TIntermTyped *typedNode)
{
    TIntermSequence *globalSequence = root->getSequence();
    TIntermDeclaration *declaration = new TIntermDeclaration();
    declaration->appendDeclarator(typedNode->deepCopy());
    globalSequence->insert(globalSequence->begin(), declaration);
}

// Adds a branch to write int(ViewID_OVR) to either gl_ViewportIndex or gl_Layer. The branch is
// added to the end of the initializers' sequence.
void SelectViewIndexInVertexShader(TIntermTyped *viewIDSymbol,
                                   TIntermTyped *multiviewBaseViewLayerIndexSymbol,
                                   TIntermSequence *initializers,
                                   const TSymbolTable &symbolTable)
{
    // Create an int(ViewID_OVR) node.
    TIntermSequence *viewIDSymbolCastArguments = new TIntermSequence();
    viewIDSymbolCastArguments->push_back(viewIDSymbol);
    TIntermAggregate *viewIDAsInt = TIntermAggregate::CreateConstructor(
        TType(EbtInt, EbpHigh, EvqTemporary), viewIDSymbolCastArguments);

    // Create a gl_ViewportIndex node.
    TIntermSymbol *viewportIndexSymbol =
        ReferenceBuiltInVariable("gl_ViewportIndex", symbolTable, 0);

    // Create a { gl_ViewportIndex = int(ViewID_OVR) } node.
    TIntermBlock *viewportIndexInitializerInBlock = new TIntermBlock();
    viewportIndexInitializerInBlock->appendStatement(
        new TIntermBinary(EOpAssign, viewportIndexSymbol, viewIDAsInt));

    // Create a gl_Layer node.
    TIntermSymbol *layerSymbol = ReferenceBuiltInVariable("gl_Layer", symbolTable, 0);

    // Create an int(ViewID_OVR) + multiviewBaseViewLayerIndex node
    TIntermBinary *sumOfViewIDAndBaseViewIndex =
        new TIntermBinary(EOpAdd, viewIDAsInt->deepCopy(), multiviewBaseViewLayerIndexSymbol);

    // Create a { gl_Layer = int(ViewID_OVR) + multiviewBaseViewLayerIndex } node.
    TIntermBlock *layerInitializerInBlock = new TIntermBlock();
    layerInitializerInBlock->appendStatement(
        new TIntermBinary(EOpAssign, layerSymbol, sumOfViewIDAndBaseViewIndex));

    // Create a node to compare whether the base view index uniform is less than zero.
    TIntermBinary *multiviewBaseViewLayerIndexZeroComparison =
        new TIntermBinary(EOpLessThan, multiviewBaseViewLayerIndexSymbol->deepCopy(),
                          CreateZeroNode(TType(EbtInt, EbpHigh, EvqConst)));

    // Create an if-else statement to select the code path.
    TIntermIfElse *multiviewBranch =
        new TIntermIfElse(multiviewBaseViewLayerIndexZeroComparison,
                          viewportIndexInitializerInBlock, layerInitializerInBlock);

    initializers->push_back(multiviewBranch);
}

}  // namespace

void DeclareAndInitBuiltinsForInstancedMultiview(TIntermBlock *root,
                                                 unsigned numberOfViews,
                                                 GLenum shaderType,
                                                 ShCompileOptions compileOptions,
                                                 ShShaderOutput shaderOutput,
                                                 TSymbolTable *symbolTable)
{
    ASSERT(shaderType == GL_VERTEX_SHADER || shaderType == GL_FRAGMENT_SHADER);

    TQualifier viewIDQualifier  = (shaderType == GL_VERTEX_SHADER) ? EvqFlatOut : EvqFlatIn;
    TIntermSymbol *viewIDSymbol = new TIntermSymbol(symbolTable->nextUniqueId(), "ViewID_OVR",
                                                    TType(EbtUInt, EbpHigh, viewIDQualifier));
    viewIDSymbol->setInternal(true);

    DeclareGlobalVariable(root, viewIDSymbol);
    ReplaceSymbol(root, "gl_ViewID_OVR", viewIDSymbol);
    if (shaderType == GL_VERTEX_SHADER)
    {
        // Replacing gl_InstanceID with InstanceID should happen before adding the initializers of
        // InstanceID and ViewID.
        TIntermSymbol *instanceIDSymbol = new TIntermSymbol(
            symbolTable->nextUniqueId(), "InstanceID", TType(EbtInt, EbpHigh, EvqGlobal));
        instanceIDSymbol->setInternal(true);
        DeclareGlobalVariable(root, instanceIDSymbol);
        ReplaceSymbol(root, "gl_InstanceID", instanceIDSymbol);

        TIntermSequence *initializers = new TIntermSequence();
        InitializeViewIDAndInstanceID(viewIDSymbol, instanceIDSymbol, numberOfViews, *symbolTable,
                                      initializers);

        // The AST transformation which adds the expression to select the viewport index should
        // be done only for the GLSL and ESSL output.
        const bool selectView = (compileOptions & SH_SELECT_VIEW_IN_NV_GLSL_VERTEX_SHADER) != 0u;
        // Assert that if the view is selected in the vertex shader, then the output is
        // either GLSL or ESSL.
        ASSERT(!selectView || IsOutputGLSL(shaderOutput) || IsOutputESSL(shaderOutput));
        if (selectView)
        {
            // Add a uniform to switch between side-by-side and layered rendering.
            TIntermSymbol *multiviewBaseViewLayerIndexSymbol =
                new TIntermSymbol(symbolTable->nextUniqueId(), "multiviewBaseViewLayerIndex",
                                  TType(EbtInt, EbpHigh, EvqUniform));
            multiviewBaseViewLayerIndexSymbol->setInternal(true);
            DeclareGlobalVariable(root, multiviewBaseViewLayerIndexSymbol);

            // Setting a value to gl_ViewportIndex or gl_Layer should happen after ViewID_OVR's
            // initialization.
            SelectViewIndexInVertexShader(viewIDSymbol->deepCopy(),
                                          multiviewBaseViewLayerIndexSymbol->deepCopy(),
                                          initializers, *symbolTable);
        }

        // Insert initializers at the beginning of main().
        TIntermBlock *initializersBlock = new TIntermBlock();
        initializersBlock->getSequence()->swap(*initializers);
        TIntermBlock *mainBody = FindMainBody(root);
        mainBody->getSequence()->insert(mainBody->getSequence()->begin(), initializersBlock);
    }
}

}  // namespace sh