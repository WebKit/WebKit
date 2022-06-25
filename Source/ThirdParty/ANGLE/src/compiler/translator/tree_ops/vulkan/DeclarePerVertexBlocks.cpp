//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DeclarePerVertexBlocks: Declare gl_PerVertex blocks if not already.
//

#include "compiler/translator/tree_ops/vulkan/DeclarePerVertexBlocks.h"

#include "compiler/translator/Compiler.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"

namespace sh
{
namespace
{
using PerVertexMemberFlags = std::array<bool, 4>;

int GetPerVertexFieldIndex(const TQualifier qualifier, const ImmutableString &name)
{
    switch (qualifier)
    {
        case EvqPosition:
            ASSERT(name == "gl_Position");
            return 0;
        case EvqPointSize:
            ASSERT(name == "gl_PointSize");
            return 1;
        case EvqClipDistance:
            ASSERT(name == "gl_ClipDistance");
            return 2;
        case EvqCullDistance:
            ASSERT(name == "gl_CullDistance");
            return 3;
        default:
            return -1;
    }
}

// Traverser that:
//
// 1. Inspects global qualifier declarations and extracts whether any of the gl_PerVertex built-ins
//    are invariant or precise.  These declarations are then dropped.
// 2. Finds the array size of gl_ClipDistance and gl_CullDistance built-in, if any.
class InspectPerVertexBuiltInsTraverser : public TIntermTraverser
{
  public:
    InspectPerVertexBuiltInsTraverser(TCompiler *compiler,
                                      TSymbolTable *symbolTable,
                                      PerVertexMemberFlags *invariantFlagsOut,
                                      PerVertexMemberFlags *preciseFlagsOut,
                                      uint32_t *clipDistanceArraySizeOut,
                                      uint32_t *cullDistanceArraySizeOut)
        : TIntermTraverser(true, false, false, symbolTable),
          mInvariantFlagsOut(invariantFlagsOut),
          mPreciseFlagsOut(preciseFlagsOut),
          mClipDistanceArraySizeOut(clipDistanceArraySizeOut),
          mCullDistanceArraySizeOut(cullDistanceArraySizeOut)
    {}

    bool visitGlobalQualifierDeclaration(Visit visit,
                                         TIntermGlobalQualifierDeclaration *node) override
    {
        TIntermSymbol *symbol = node->getSymbol();

        const int fieldIndex =
            GetPerVertexFieldIndex(symbol->getType().getQualifier(), symbol->getName());
        if (fieldIndex < 0)
        {
            return false;
        }

        if (node->isInvariant())
        {
            (*mInvariantFlagsOut)[fieldIndex] = true;
        }
        else if (node->isPrecise())
        {
            (*mPreciseFlagsOut)[fieldIndex] = true;
        }

        mMultiReplacements.emplace_back(getParentNode()->getAsBlock(), node, TIntermSequence());

        return false;
    }

    void visitSymbol(TIntermSymbol *symbol) override
    {
        const TType &type = symbol->getType();
        switch (type.getQualifier())
        {
            case EvqClipDistance:
                *mClipDistanceArraySizeOut = type.getOutermostArraySize();
                break;
            case EvqCullDistance:
                *mCullDistanceArraySizeOut = type.getOutermostArraySize();
                break;
            default:
                break;
        }
    }

  private:
    PerVertexMemberFlags *mInvariantFlagsOut;
    PerVertexMemberFlags *mPreciseFlagsOut;
    uint32_t *mClipDistanceArraySizeOut;
    uint32_t *mCullDistanceArraySizeOut;
};

// Traverser that:
//
// 1. Declares the input and output gl_PerVertex types and variables if not already (based on shader
//    type).
// 2. Turns built-in references into indexes into these variables.
class DeclarePerVertexBlocksTraverser : public TIntermTraverser
{
  public:
    DeclarePerVertexBlocksTraverser(TCompiler *compiler,
                                    TSymbolTable *symbolTable,
                                    const PerVertexMemberFlags &invariantFlags,
                                    const PerVertexMemberFlags &preciseFlags,
                                    uint32_t clipDistanceArraySize,
                                    uint32_t cullDistanceArraySize)
        : TIntermTraverser(true, false, false, symbolTable),
          mShaderType(compiler->getShaderType()),
          mShaderVersion(compiler->getShaderVersion()),
          mResources(compiler->getResources()),
          mClipDistanceArraySize(clipDistanceArraySize),
          mCullDistanceArraySize(cullDistanceArraySize),
          mPerVertexInVar(nullptr),
          mPerVertexOutVar(nullptr),
          mPerVertexInVarRedeclared(false),
          mPerVertexOutVarRedeclared(false),
          mPerVertexOutInvariantFlags(invariantFlags),
          mPerVertexOutPreciseFlags(preciseFlags)
    {}

    void visitSymbol(TIntermSymbol *symbol) override
    {
        const TVariable *variable = &symbol->variable();
        const TType *type         = &variable->getType();

        // Replace gl_out if necessary.
        if (mShaderType == GL_TESS_CONTROL_SHADER && type->getQualifier() == EvqPerVertexOut)
        {
            ASSERT(variable->name() == "gl_out");

            // Declare gl_out if not already.
            if (mPerVertexOutVar == nullptr)
            {
                // Record invariant and precise qualifiers used on the fields so they would be
                // applied to the replacement gl_out.
                for (const TField *field : type->getInterfaceBlock()->fields())
                {
                    const TType &fieldType = *field->type();
                    const int fieldIndex =
                        GetPerVertexFieldIndex(fieldType.getQualifier(), field->name());
                    ASSERT(fieldIndex >= 0);

                    if (fieldType.isInvariant())
                    {
                        mPerVertexOutInvariantFlags[fieldIndex] = true;
                    }
                    if (fieldType.isPrecise())
                    {
                        mPerVertexOutPreciseFlags[fieldIndex] = true;
                    }
                }

                declareDefaultGlOut();
            }

            if (mPerVertexOutVarRedeclared)
            {
                // Traverse the parents and promote the new type.  Replace the root of
                // EOpIndex[In]Direct chain.
                queueAccessChainReplacement(new TIntermSymbol(mPerVertexOutVar));
            }

            return;
        }

        // Replace gl_in if necessary.
        if ((mShaderType == GL_TESS_CONTROL_SHADER || mShaderType == GL_TESS_EVALUATION_SHADER ||
             mShaderType == GL_GEOMETRY_SHADER) &&
            type->getQualifier() == EvqPerVertexIn)
        {
            ASSERT(variable->name() == "gl_in");

            // Declare gl_in if not already.
            if (mPerVertexInVar == nullptr)
            {
                declareDefaultGlIn();
            }

            if (mPerVertexInVarRedeclared)
            {
                // Traverse the parents and promote the new type.  Replace the root of
                // EOpIndex[In]Direct chain.
                queueAccessChainReplacement(new TIntermSymbol(mPerVertexInVar));
            }

            return;
        }

        // Turn gl_Position, gl_PointSize, gl_ClipDistance and gl_CullDistance into references to
        // the output gl_PerVertex.  Note that the default gl_PerVertex is declared as follows:
        //
        //     out gl_PerVertex
        //     {
        //         vec4 gl_Position;
        //         float gl_PointSize;
        //         float gl_ClipDistance[];
        //         float gl_CullDistance[];
        //     };
        //

        if (variable->symbolType() != SymbolType::BuiltIn)
        {
            ASSERT(variable->name() != "gl_Position" && variable->name() != "gl_PointSize" &&
                   variable->name() != "gl_ClipDistance" && variable->name() != "gl_CullDistance" &&
                   variable->name() != "gl_in" && variable->name() != "gl_out");

            return;
        }

        // If this built-in was already visited, reuse the variable defined for it.
        auto replacement = mVariableMap.find(variable);
        if (replacement != mVariableMap.end())
        {
            queueReplacement(replacement->second->deepCopy(), OriginalNode::IS_DROPPED);
            return;
        }

        const int fieldIndex = GetPerVertexFieldIndex(type->getQualifier(), variable->name());

        // Not the built-in we are looking for.
        if (fieldIndex < 0)
        {
            return;
        }

        // Declare the output gl_PerVertex if not already.
        if (mPerVertexOutVar == nullptr)
        {
            declareDefaultGlOut();
        }

        TType *newType = new TType(*type);
        newType->setInterfaceBlockField(mPerVertexOutVar->getType().getInterfaceBlock(),
                                        fieldIndex);

        TVariable *newVariable = new TVariable(mSymbolTable, variable->name(), newType,
                                               variable->symbolType(), variable->extensions());

        TIntermSymbol *newSymbol = new TIntermSymbol(newVariable);
        mVariableMap[variable]   = newSymbol;

        queueReplacement(newSymbol, OriginalNode::IS_DROPPED);
    }

    const TVariable *getRedeclaredPerVertexOutVar()
    {
        return mPerVertexOutVarRedeclared ? mPerVertexOutVar : nullptr;
    }

    const TVariable *getRedeclaredPerVertexInVar()
    {
        return mPerVertexInVarRedeclared ? mPerVertexInVar : nullptr;
    }

  private:
    const TVariable *declarePerVertex(TQualifier qualifier,
                                      uint32_t arraySize,
                                      ImmutableString &variableName)
    {
        TFieldList *fields = new TFieldList;

        const TType *vec4Type  = StaticType::GetBasic<EbtFloat, EbpHigh, 4>();
        const TType *floatType = StaticType::GetBasic<EbtFloat, EbpHigh, 1>();

        TType *positionType     = new TType(*vec4Type);
        TType *pointSizeType    = new TType(*floatType);
        TType *clipDistanceType = new TType(*floatType);
        TType *cullDistanceType = new TType(*floatType);

        positionType->setQualifier(EvqPosition);
        pointSizeType->setQualifier(EvqPointSize);
        clipDistanceType->setQualifier(EvqClipDistance);
        cullDistanceType->setQualifier(EvqCullDistance);

        TPrecision pointSizePrecision = EbpHigh;
        if (mShaderType == GL_VERTEX_SHADER)
        {
            // gl_PointSize is mediump in ES100 and highp in ES300+.
            const TVariable *glPointSize = static_cast<const TVariable *>(
                mSymbolTable->findBuiltIn(ImmutableString("gl_PointSize"), mShaderVersion));
            ASSERT(glPointSize);

            pointSizePrecision = glPointSize->getType().getPrecision();
        }
        pointSizeType->setPrecision(pointSizePrecision);

        // TODO: handle interaction with GS and T*S where the two can have different sizes.  These
        // values are valid for EvqPerVertexOut only.  For EvqPerVertexIn, the size should come from
        // the declaration of gl_in.  http://anglebug.com/5466.
        clipDistanceType->makeArray(std::max(mClipDistanceArraySize, 1u));
        cullDistanceType->makeArray(std::max(mCullDistanceArraySize, 1u));

        if (qualifier == EvqPerVertexOut)
        {
            positionType->setInvariant(mPerVertexOutInvariantFlags[0]);
            pointSizeType->setInvariant(mPerVertexOutInvariantFlags[1]);
            clipDistanceType->setInvariant(mPerVertexOutInvariantFlags[2]);
            cullDistanceType->setInvariant(mPerVertexOutInvariantFlags[3]);

            positionType->setPrecise(mPerVertexOutPreciseFlags[0]);
            pointSizeType->setPrecise(mPerVertexOutPreciseFlags[1]);
            clipDistanceType->setPrecise(mPerVertexOutPreciseFlags[2]);
            cullDistanceType->setPrecise(mPerVertexOutPreciseFlags[3]);
        }

        fields->push_back(new TField(positionType, ImmutableString("gl_Position"), TSourceLoc(),
                                     SymbolType::AngleInternal));
        fields->push_back(new TField(pointSizeType, ImmutableString("gl_PointSize"), TSourceLoc(),
                                     SymbolType::AngleInternal));
        fields->push_back(new TField(clipDistanceType, ImmutableString("gl_ClipDistance"),
                                     TSourceLoc(), SymbolType::AngleInternal));
        fields->push_back(new TField(cullDistanceType, ImmutableString("gl_CullDistance"),
                                     TSourceLoc(), SymbolType::AngleInternal));

        TInterfaceBlock *interfaceBlock =
            new TInterfaceBlock(mSymbolTable, ImmutableString("gl_PerVertex"), fields,
                                TLayoutQualifier::Create(), SymbolType::AngleInternal);

        TType *interfaceBlockType =
            new TType(interfaceBlock, qualifier, TLayoutQualifier::Create());
        if (arraySize > 0)
        {
            interfaceBlockType->makeArray(arraySize);
        }

        TVariable *interfaceBlockVar =
            new TVariable(mSymbolTable, variableName, interfaceBlockType,
                          variableName.empty() ? SymbolType::Empty : SymbolType::AngleInternal);

        return interfaceBlockVar;
    }

    void declareDefaultGlOut()
    {
        ASSERT(!mPerVertexOutVarRedeclared);

        // For tessellation control shaders, gl_out is an array of MaxPatchVertices
        // For other shaders, there's no explicit name or array size

        ImmutableString varName("");
        uint32_t arraySize = 0;
        if (mShaderType == GL_TESS_CONTROL_SHADER)
        {
            varName   = ImmutableString("gl_out");
            arraySize = mResources.MaxPatchVertices;
        }

        mPerVertexOutVar           = declarePerVertex(EvqPerVertexOut, arraySize, varName);
        mPerVertexOutVarRedeclared = true;
    }

    void declareDefaultGlIn()
    {
        ASSERT(!mPerVertexInVarRedeclared);

        // For tessellation shaders, gl_in is an array of MaxPatchVertices.
        // For geometry shaders, gl_in is sized based on the primitive type.

        ImmutableString varName("gl_in");
        uint32_t arraySize = mResources.MaxPatchVertices;
        if (mShaderType == GL_GEOMETRY_SHADER)
        {
            arraySize =
                mSymbolTable->getGlInVariableWithArraySize()->getType().getOutermostArraySize();
        }

        mPerVertexInVar           = declarePerVertex(EvqPerVertexIn, arraySize, varName);
        mPerVertexInVarRedeclared = true;
    }

    GLenum mShaderType;
    int mShaderVersion;
    const ShBuiltInResources &mResources;
    uint32_t mClipDistanceArraySize;
    uint32_t mCullDistanceArraySize;

    const TVariable *mPerVertexInVar;
    const TVariable *mPerVertexOutVar;

    bool mPerVertexInVarRedeclared;
    bool mPerVertexOutVarRedeclared;

    // A map of already replaced built-in variables.
    VariableReplacementMap mVariableMap;

    // Whether each field is invariant or precise.
    PerVertexMemberFlags mPerVertexOutInvariantFlags;
    PerVertexMemberFlags mPerVertexOutPreciseFlags;
};

void AddPerVertexDecl(TIntermBlock *root, const TVariable *variable)
{
    if (variable == nullptr)
    {
        return;
    }

    TIntermDeclaration *decl = new TIntermDeclaration;
    TIntermSymbol *symbol    = new TIntermSymbol(variable);
    decl->appendDeclarator(symbol);

    // Insert the declaration before the first function.
    size_t firstFunctionIndex = FindFirstFunctionDefinitionIndex(root);
    root->insertChildNodes(firstFunctionIndex, {decl});
}
}  // anonymous namespace

bool DeclarePerVertexBlocks(TCompiler *compiler, TIntermBlock *root, TSymbolTable *symbolTable)
{
    if (compiler->getShaderType() == GL_COMPUTE_SHADER ||
        compiler->getShaderType() == GL_FRAGMENT_SHADER)
    {
        return true;
    }

    // First, visit all global qualifier declarations and find which built-ins are invariant or
    // precise.  At the same time, find out the size of gl_ClipDistance and gl_CullDistance arrays.
    PerVertexMemberFlags invariantFlags = {};
    PerVertexMemberFlags preciseFlags   = {};
    uint32_t clipDistanceArraySize = 0, cullDistanceArraySize = 0;

    InspectPerVertexBuiltInsTraverser infoTraverser(compiler, symbolTable, &invariantFlags,
                                                    &preciseFlags, &clipDistanceArraySize,
                                                    &cullDistanceArraySize);
    root->traverse(&infoTraverser);
    if (!infoTraverser.updateTree(compiler, root))
    {
        return false;
    }

    // If not specified, take the clip/cull distance size from the resources.
    if (clipDistanceArraySize == 0)
    {
        clipDistanceArraySize = compiler->getResources().MaxClipDistances;
    }
    if (cullDistanceArraySize == 0)
    {
        cullDistanceArraySize = compiler->getResources().MaxCullDistances;
    }

    // If #pragma STDGL invariant(all) is specified, make all outputs invariant.
    if (compiler->getPragma().stdgl.invariantAll)
    {
        std::fill(invariantFlags.begin(), invariantFlags.end(), true);
    }

    // Then declare the in and out gl_PerVertex I/O blocks.
    DeclarePerVertexBlocksTraverser traverser(compiler, symbolTable, invariantFlags, preciseFlags,
                                              clipDistanceArraySize, cullDistanceArraySize);
    root->traverse(&traverser);
    if (!traverser.updateTree(compiler, root))
    {
        return false;
    }

    AddPerVertexDecl(root, traverser.getRedeclaredPerVertexOutVar());
    AddPerVertexDecl(root, traverser.getRedeclaredPerVertexInVar());

    return compiler->validateAST(root);
}
}  // namespace sh
