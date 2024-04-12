//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EmulateFramebufferFetch.h: Replace input, gl_LastFragData and gl_LastFragColorARM with usages of
// input attachments.
//

#include "compiler/translator/tree_ops/spirv/EmulateFramebufferFetch.h"

#include "common/bitset_utils.h"
#include "compiler/translator/Compiler.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/BuiltIn.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"
#include "compiler/translator/tree_util/RunAtTheBeginningOfShader.h"
#include "compiler/translator/util.h"

namespace sh
{
namespace
{
using InputAttachmentIndexUsage = angle::BitSet<32>;

// A traverser that looks at which inout variables exist, which gl_LastFragData indices have been
// used and whether gl_LastFragColorARM is referenced.  It builds a set of indices correspondingly;
// these are input attachment indices the shader may read from.
class InputAttachmentUsageTraverser : public TIntermTraverser
{
  public:
    InputAttachmentUsageTraverser(uint32_t maxDrawBuffers, TVector<const TType *> *attachmentTypes)
        : TIntermTraverser(true, false, false),
          mMaxDrawBuffers(maxDrawBuffers),
          mAttachmentTypes(attachmentTypes),
          mUsesLastFragColorARM(false)
    {
        mAttachmentTypes->resize(maxDrawBuffers, nullptr);
    }

    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override;
    bool visitBinary(Visit visit, TIntermBinary *node) override;
    void visitSymbol(TIntermSymbol *node) override;

    InputAttachmentIndexUsage getIndexUsage() const { return mIndexUsage; }
    bool usesLastFragColorARM() const { return mUsesLastFragColorARM; }

  private:
    void setInputAttachmentIndex(uint32_t index, const TType *type);

    uint32_t mMaxDrawBuffers;
    InputAttachmentIndexUsage mIndexUsage;
    TVector<const TType *> *mAttachmentTypes;
    bool mUsesLastFragColorARM;
};

void InputAttachmentUsageTraverser::setInputAttachmentIndex(uint32_t index, const TType *type)
{
    ASSERT(index < mMaxDrawBuffers);
    mIndexUsage.set(index);
    (*mAttachmentTypes)[index] = type;
}

bool InputAttachmentUsageTraverser::visitDeclaration(Visit visit, TIntermDeclaration *node)
{
    const TIntermSequence &sequence = *node->getSequence();
    ASSERT(sequence.size() == 1);

    TIntermSymbol *symbol = sequence.front()->getAsSymbolNode();
    if (symbol == nullptr)
    {
        return true;
    }

    if (symbol->getQualifier() == EvqFragmentInOut)
    {
        ASSERT(symbol->getType().getLayoutQualifier().index <= 0);

        // The input attachment index is identical to the location qualifier.  If there's only one
        // output, GLSL is allowed to not specify the location qualifier, in which case it would
        // implicitly be at location 0.
        const TType &type = symbol->getType();
        const unsigned int baseInputAttachmentIndex =
            std::max(0, type.getLayoutQualifier().location);

        uint32_t arraySize = type.isArray() ? type.getOutermostArraySize() : 1;
        for (unsigned int index = 0; index < arraySize; index++)
        {
            setInputAttachmentIndex(baseInputAttachmentIndex + index, &type);
        }
    }

    return false;
}

bool InputAttachmentUsageTraverser::visitBinary(Visit visit, TIntermBinary *node)
{
    TOperator op = node->getOp();
    if (op != EOpIndexDirect && op != EOpIndexIndirect)
    {
        return true;
    }

    TIntermSymbol *left = node->getLeft()->getAsSymbolNode();
    if (left == nullptr || left->getQualifier() != EvqLastFragData)
    {
        return true;
    }

    ASSERT(left->getName() == "gl_LastFragData");
    const TType &type = left->getType();

    const TConstantUnion *constIndex = node->getRight()->getConstantValue();
    // Non-const indices on gl_LastFragData are not allowed.
    ASSERT(constIndex != nullptr);

    uint32_t index = 0;
    switch (constIndex->getType())
    {
        case EbtInt:
            index = constIndex->getIConst();
            break;
        case EbtUInt:
            index = constIndex->getUConst();
            break;
        case EbtFloat:
            index = static_cast<uint32_t>(constIndex->getFConst());
            break;
        case EbtBool:
            index = constIndex->getBConst() ? 1 : 0;
            break;
        default:
            UNREACHABLE();
            break;
    }
    setInputAttachmentIndex(index, &type);

    return true;
}

void InputAttachmentUsageTraverser::visitSymbol(TIntermSymbol *symbol)
{
    if (symbol->getQualifier() != EvqLastFragColor)
    {
        return;
    }
    ASSERT(symbol->getName() == "gl_LastFragColorARM");

    // gl_LastFragColorARM always reads back from location 0.
    setInputAttachmentIndex(0, &symbol->getType());
    mUsesLastFragColorARM = true;
}

ImmutableString GetInputAttachmentName(size_t index)
{
    std::stringstream nameStream = sh::InitializeStream<std::stringstream>();
    nameStream << "ANGLEInputAttachment" << index;
    return ImmutableString(nameStream.str());
}

TBasicType GetBasicTypeForSubpassInput(TBasicType inputType)
{
    switch (inputType)
    {
        case EbtFloat:
            return EbtSubpassInput;
        case EbtInt:
            return EbtISubpassInput;
        case EbtUInt:
            return EbtUSubpassInput;
        default:
            UNREACHABLE();
            return EbtVoid;
    }
}

// Declare an input attachment variable at a given index.
void DeclareInputAttachmentVariable(TSymbolTable *symbolTable,
                                    const TType &outputType,
                                    size_t index,
                                    InputAttachmentMap *inputAttachmentMapOut,
                                    TIntermSequence *declarationsOut)
{
    const TBasicType subpassInputType = GetBasicTypeForSubpassInput(outputType.getBasicType());

    TType *inputAttachmentType =
        new TType(subpassInputType, outputType.getPrecision(), EvqUniform, 1);
    TLayoutQualifier inputAttachmentQualifier     = inputAttachmentType->getLayoutQualifier();
    inputAttachmentQualifier.inputAttachmentIndex = static_cast<int>(index);
    inputAttachmentType->setLayoutQualifier(inputAttachmentQualifier);

    const TVariable *inputAttachmentVar = new TVariable(
        symbolTable, GetInputAttachmentName(index), inputAttachmentType, SymbolType::AngleInternal);
    (*inputAttachmentMapOut)[static_cast<uint32_t>(index)] = inputAttachmentVar;

    TIntermDeclaration *decl = new TIntermDeclaration;
    decl->appendDeclarator(new TIntermSymbol(inputAttachmentVar));
    declarationsOut->push_back(decl);
}

// Declare a global variable to hold gl_LastFragData/gl_LastFragColorARM
const TVariable *DeclareLastFragDataGlobalVariable(TCompiler *compiler,
                                                   TIntermBlock *root,
                                                   const TVector<const TType *> &attachmentTypes,
                                                   TIntermSequence *declarationsOut)
{
    // Find the first input attachment that is used.  If gl_LastFragColorARM was used, this will be
    // index 0.  Otherwise if this is ES100, any index of gl_LastFragData may be used.  Either way,
    // the global variable is declared the same as gl_LastFragData would have been if used.
    const TType *attachmentType = nullptr;
    for (const TType *type : attachmentTypes)
    {
        if (type != nullptr)
        {
            attachmentType = type;
            break;
        }
    }
    ASSERT(attachmentType != nullptr);
    TType *globalType = new TType(*attachmentType);
    globalType->setQualifier(EvqGlobal);

    // If the type of gl_LastFragColorARM is found, convert it to an array to match gl_LastFragData.
    // This is necessary if the shader uses both gl_LastFragData and gl_LastFragColorARM
    // simultaneously.
    if (!globalType->isArray())
    {
        globalType->makeArray(compiler->getBuiltInResources().MaxDrawBuffers);
    }

    // Declare the global
    const TVariable *global =
        new TVariable(&compiler->getSymbolTable(), ImmutableString("ANGLELastFragData"), globalType,
                      SymbolType::AngleInternal);

    TIntermDeclaration *decl = new TIntermDeclaration;
    decl->appendDeclarator(new TIntermSymbol(global));
    declarationsOut->push_back(decl);

    return global;
}

// Declare an input attachment for each used index.  Additionally, create a global variable for
// gl_LastFragData and gl_LastFragColorARM if needed.
[[nodiscard]] bool DeclareVariables(TCompiler *compiler,
                                    TIntermBlock *root,
                                    InputAttachmentIndexUsage indexUsage,
                                    bool usesLastFragData,
                                    const TVector<const TType *> &attachmentTypes,
                                    InputAttachmentMap *inputAttachmentMapOut,
                                    const TVariable **lastFragDataOut)
{
    TSymbolTable *symbolTable = &compiler->getSymbolTable();

    TIntermSequence declarations;

    // For every detected index, declare an input attachment variable.
    for (size_t index : indexUsage)
    {
        ASSERT(attachmentTypes[index] != nullptr);
        DeclareInputAttachmentVariable(symbolTable, *attachmentTypes[index], index,
                                       inputAttachmentMapOut, &declarations);
    }

    // If gl_LastFragData or gl_LastFragColorARM is used, declare a global variable to retain that.
    // The difference between ES300+ inout variables and gl_LastFrag* is that if the inout variable
    // is read back after being written to, it should contain the latest value written to it, while
    // gl_LastFrag* should contain the value before the fragment shader's invocation.
    //
    // As such, it is enough to initialize inout variables with the values from input attachments,
    // but gl_LastFrag* needs to be stored in a global variable to retain its value even after
    // gl_Frag* has been overwritten.
    *lastFragDataOut = nullptr;
    if (usesLastFragData)
    {
        *lastFragDataOut =
            DeclareLastFragDataGlobalVariable(compiler, root, attachmentTypes, &declarations);
    }

    // Add the declarations to the beginning of the shader.
    TIntermSequence &topLevel = *root->getSequence();
    declarations.insert(declarations.end(), topLevel.begin(), topLevel.end());
    topLevel = std::move(declarations);

    return compiler->validateAST(root);
}

TIntermTyped *CreateSubpassLoadFuncCall(TSymbolTable *symbolTable, const TVariable *inputVariable)
{
    TIntermSequence args = {new TIntermSymbol(inputVariable)};
    return CreateBuiltInFunctionCallNode("subpassLoad", &args, *symbolTable,
                                         kESSLInternalBackendBuiltIns);
}

void GatherInoutVariables(TIntermBlock *root, TVector<const TVariable *> *inoutVariablesOut)
{
    TIntermSequence &topLevel = *root->getSequence();

    for (TIntermNode *node : topLevel)
    {
        TIntermDeclaration *decl = node->getAsDeclarationNode();
        if (decl != nullptr)
        {
            ASSERT(decl->getSequence()->size() == 1);

            TIntermSymbol *symbol = decl->getSequence()->front()->getAsSymbolNode();
            if (symbol != nullptr && symbol->getQualifier() == EvqFragmentInOut)
            {
                ASSERT(symbol->getType().getLayoutQualifier().index <= 0);
                inoutVariablesOut->push_back(&symbol->variable());
            }
        }
    }
}

void InitializeFromInputAttachment(TSymbolTable *symbolTable,
                                   TIntermBlock *block,
                                   const TVariable *inputVariable,
                                   const TVariable *assignVariable,
                                   uint32_t assignVariableArrayIndex)
{
    ASSERT(inputVariable != nullptr);

    TIntermTyped *var = new TIntermSymbol(assignVariable);
    if (var->getType().isArray())
    {
        var = new TIntermBinary(EOpIndexDirect, var, CreateIndexNode(assignVariableArrayIndex));
    }

    TIntermTyped *input = CreateSubpassLoadFuncCall(symbolTable, inputVariable);

    const int vecSize = assignVariable->getType().getNominalSize();
    if (vecSize < 4)
    {
        TVector<int> swizzle = {0, 1, 2, 3};
        swizzle.resize(vecSize);
        input = new TIntermSwizzle(input, swizzle);
    }

    TIntermTyped *assignment = new TIntermBinary(EOpAssign, var, input);

    block->appendStatement(assignment);
}

[[nodiscard]] bool InitializeFromInputAttachments(TCompiler *compiler,
                                                  TIntermBlock *root,
                                                  const InputAttachmentMap &inputAttachmentMap,
                                                  const TVector<const TVariable *> &inoutVariables,
                                                  const TVariable *lastFragData)
{
    TSymbolTable *symbolTable = &compiler->getSymbolTable();
    TIntermBlock *init        = new TIntermBlock;

    // Initialize inout variables
    for (const TVariable *inoutVar : inoutVariables)
    {
        const TType &type = inoutVar->getType();
        const unsigned int baseInputAttachmentIndex =
            std::max(0, type.getLayoutQualifier().location);

        uint32_t arraySize = type.isArray() ? type.getOutermostArraySize() : 1;
        for (unsigned int index = 0; index < arraySize; index++)
        {
            ASSERT(inputAttachmentMap.find(baseInputAttachmentIndex + index) !=
                   inputAttachmentMap.end());

            InitializeFromInputAttachment(symbolTable, init,
                                          inputAttachmentMap.at(baseInputAttachmentIndex + index),
                                          inoutVar, index);
        }
    }

    // Initialize lastFragData, if present
    if (lastFragData != nullptr)
    {
        for (auto &iter : inputAttachmentMap)
        {
            const uint32_t index                = iter.first;
            const TVariable *inputAttachmentVar = iter.second;

            InitializeFromInputAttachment(symbolTable, init, inputAttachmentVar, lastFragData,
                                          index);
        }
    }

    return RunAtTheBeginningOfShader(compiler, root, init);
}

[[nodiscard]] bool ReplaceVariables(TCompiler *compiler,
                                    TIntermBlock *root,
                                    const InputAttachmentMap &inputAttachmentMap,
                                    const TVariable *lastFragData)
{
    TSymbolTable *symbolTable = &compiler->getSymbolTable();

    TVector<const TVariable *> inoutVariables;
    GatherInoutVariables(root, &inoutVariables);

    // Generate code that initializes the global variable and the inout variables with corresponding
    // input attachments.
    if (!InitializeFromInputAttachments(compiler, root, inputAttachmentMap, inoutVariables,
                                        lastFragData))
    {
        return false;
    }

    // Build a map from:
    //
    // - inout to out variables
    // - gl_LastFragData to lastFragData
    // - gl_LastFragColorARM to lastFragData[0]

    VariableReplacementMap replacementMap;
    for (const TVariable *var : inoutVariables)
    {
        TType *outType = new TType(var->getType());
        outType->setQualifier(EvqFragmentOut);
        const TVariable *replacement =
            new TVariable(symbolTable, var->name(), outType, var->symbolType());
        replacementMap[var] = new TIntermSymbol(replacement);
    }

    if (lastFragData != nullptr)
    {
        // Use the user-defined variables if found (and remove their declaration), or the built-in
        // otherwise.
        TIntermSequence &topLevel = *root->getSequence();
        TIntermSequence newTopLevel;

        const TVariable *glLastFragData  = nullptr;
        const TVariable *glLastFragColor = nullptr;

        for (TIntermNode *node : topLevel)
        {
            TIntermDeclaration *decl = node->getAsDeclarationNode();
            if (decl != nullptr)
            {
                ASSERT(decl->getSequence()->size() == 1);

                TIntermSymbol *symbol = decl->getSequence()->front()->getAsSymbolNode();
                if (symbol != nullptr)
                {
                    if (symbol->getQualifier() == EvqLastFragData)
                    {
                        glLastFragData = &symbol->variable();
                        continue;
                    }
                    if (symbol->getQualifier() == EvqLastFragColor)
                    {
                        glLastFragColor = &symbol->variable();
                        continue;
                    }
                }
            }
            newTopLevel.push_back(node);
        }

        topLevel = std::move(newTopLevel);

        if (glLastFragData == nullptr)
        {
            glLastFragData = static_cast<const TVariable *>(
                symbolTable->findBuiltIn(ImmutableString("gl_LastFragData"), 100));
        }
        if (glLastFragColor == nullptr)
        {
            glLastFragColor = static_cast<const TVariable *>(symbolTable->findBuiltIn(
                ImmutableString("gl_LastFragColorARM"), compiler->getShaderVersion()));
        }

        replacementMap[glLastFragData] = new TIntermSymbol(lastFragData);
        replacementMap[glLastFragColor] =
            new TIntermBinary(EOpIndexDirect, new TIntermSymbol(lastFragData), CreateIndexNode(0));
    }

    // Replace the variables accordingly.
    return ReplaceVariables(compiler, root, replacementMap);
}
}  // anonymous namespace

[[nodiscard]] bool EmulateFramebufferFetch(TCompiler *compiler,
                                           TIntermBlock *root,
                                           InputAttachmentMap *inputAttachmentMapOut)
{
    // First, check if input attachments are necessary at all.
    TVector<const TType *> attachmentTypes;
    InputAttachmentUsageTraverser usageTraverser(compiler->getBuiltInResources().MaxDrawBuffers,
                                                 &attachmentTypes);
    root->traverse(&usageTraverser);

    InputAttachmentIndexUsage indexUsage = usageTraverser.getIndexUsage();
    if (!indexUsage.any())
    {
        return true;
    }

    const bool usesLastFragData =
        compiler->getShaderVersion() == 100 || usageTraverser.usesLastFragColorARM();

    // Declare the necessary variables for emulation; input attachments to read from and global
    // variables to hold last frag data.
    const TVariable *lastFragData = nullptr;
    if (!DeclareVariables(compiler, root, indexUsage, usesLastFragData, attachmentTypes,
                          inputAttachmentMapOut, &lastFragData))
    {
        return false;
    }

    // Then replace references to gl_LastFragData with the global, gl_LastFragColorARM with
    // global[0], replace inout variables with out equivalents and make sure input attachments
    // initialize the appropriate variables at the beginning of the shader.
    if (!ReplaceVariables(compiler, root, *inputAttachmentMapOut, lastFragData))
    {
        return false;
    }

    return true;
}

}  // namespace sh
