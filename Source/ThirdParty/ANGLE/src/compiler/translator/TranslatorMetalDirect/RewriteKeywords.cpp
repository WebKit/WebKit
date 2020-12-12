//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <cctype>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "compiler/translator/TranslatorMetalDirect/AstHelpers.h"
#include "compiler/translator/TranslatorMetalDirect/Debug.h"
#include "compiler/translator/TranslatorMetalDirect/RewriteKeywords.h"
#include "compiler/translator/tree_util/IntermRebuild.h"

using namespace sh;

////////////////////////////////////////////////////////////////////////////////

namespace
{

template <typename T>
using Remapping = std::unordered_map<const T *, const T *>;

class Rewriter : public TIntermRebuild
{
  private:
    const std::set<ImmutableString> &mKeywords;
    IdGen &mIdGen;
    Remapping<TField> modifiedFields;
    Remapping<TFieldList> mFieldLists;
    Remapping<TFunction> mFunctions;
    Remapping<TInterfaceBlock> mInterfaceBlocks;
    Remapping<TStructure> mStructures;
    Remapping<TVariable> mVariables;
    std::string mNewNameBuffer;

  private:
    template <typename T>
    ImmutableString maybeCreateNewName(T const &object)
    {
        if (needsRenaming(object, false))
        {
            return mIdGen.createNewName(Name(object)).rawName();
        }
        return Name(object).rawName();
    }

    const TField *createRenamed(const TField &field)
    {
        auto *renamed =
            new TField(const_cast<TType *>(&getRenamedOrOriginal(*field.type())),
                       maybeCreateNewName(field), field.line(), SymbolType::AngleInternal);

        return renamed;
    }

    const TFieldList *createRenamed(const TFieldList &fieldList)
    {
        auto *renamed = new TFieldList();
        for (const TField *field : fieldList)
        {
            renamed->push_back(const_cast<TField *>(&getRenamedOrOriginal(*field)));
        }
        return renamed;
    }

    const TFunction *createRenamed(const TFunction &function)
    {
        auto *renamed =
            new TFunction(&mSymbolTable, maybeCreateNewName(function), SymbolType::AngleInternal,
                          &getRenamedOrOriginal(function.getReturnType()),
                          function.isKnownToNotHaveSideEffects());

        const size_t paramCount = function.getParamCount();
        for (size_t i = 0; i < paramCount; ++i)
        {
            const TVariable &param = *function.getParam(i);
            renamed->addParameter(&getRenamedOrOriginal(param));
        }

        if (function.isDefined())
        {
            renamed->setDefined();
        }

        if (function.hasPrototypeDeclaration())
        {
            renamed->setHasPrototypeDeclaration();
        }

        return renamed;
    }

    const TInterfaceBlock *createRenamed(const TInterfaceBlock &interfaceBlock)
    {
        TLayoutQualifier layoutQualifier = TLayoutQualifier::Create();
        layoutQualifier.blockStorage     = interfaceBlock.blockStorage();
        layoutQualifier.binding          = interfaceBlock.blockBinding();

        auto *renamed =
            new TInterfaceBlock(&mSymbolTable, maybeCreateNewName(interfaceBlock),
                                &getRenamedOrOriginal(interfaceBlock.fields()), layoutQualifier,
                                SymbolType::AngleInternal, interfaceBlock.extension());

        return renamed;
    }

    const TStructure *createRenamed(const TStructure &structure)
    {
        auto *renamed =
            new TStructure(&mSymbolTable, maybeCreateNewName(structure),
                           &getRenamedOrOriginal(structure.fields()), SymbolType::AngleInternal);

        renamed->setAtGlobalScope(structure.atGlobalScope());

        return renamed;
    }

    const TType *createRenamed(const TType &type)
    {
        TType *renamed;

        if (const TStructure *structure = type.getStruct())
        {
            renamed = new TType(&getRenamedOrOriginal(*structure), type.isStructSpecifier());
        }
        else if (const TInterfaceBlock *interfaceBlock = type.getInterfaceBlock())
        {
            renamed = new TType(&getRenamedOrOriginal(*interfaceBlock), type.getQualifier(),
                                type.getLayoutQualifier());
        }
        else
        {
            LOGIC_ERROR();  // Can't rename built-in types.
            renamed = nullptr;
        }

        if (type.isArray())
        {
            renamed->makeArrays(type.getArraySizes());
        }
        renamed->setPrecise(type.isPrecise());
        renamed->setInvariant(type.isInvariant());
        renamed->setMemoryQualifier(type.getMemoryQualifier());
        renamed->setLayoutQualifier(type.getLayoutQualifier());

        return renamed;
    }

    const TVariable *createRenamed(const TVariable &variable)
    {
        auto *renamed = new TVariable(&mSymbolTable, maybeCreateNewName(variable),
                                      &getRenamedOrOriginal(variable.getType()),
                                      SymbolType::AngleInternal, variable.extension());

        return renamed;
    }

    template <typename T>
    const T *tryGetRenamedImpl(const T &object, Remapping<T> *remapping)
    {
        if (!needsRenaming(object, true))
        {
            return nullptr;
        }

        if (remapping)
        {
            auto it = remapping->find(&object);
            if (it != remapping->end())
            {
                return it->second;
            }
        }

        const T *renamedObject = createRenamed(object);

        if (remapping)
        {
            (*remapping)[&object] = renamedObject;
        }

        return renamedObject;
    }

    const TField *tryGetRenamed(const TField &field)
    {
        return tryGetRenamedImpl(field, &modifiedFields);
    }

    const TFieldList *tryGetRenamed(const TFieldList &fieldList)
    {
        return tryGetRenamedImpl(fieldList, &mFieldLists);
    }

    const TFunction *tryGetRenamed(const TFunction &func)
    {
        return tryGetRenamedImpl(func, &mFunctions);
    }

    const TInterfaceBlock *tryGetRenamed(const TInterfaceBlock &interfaceBlock)
    {
        return tryGetRenamedImpl(interfaceBlock, &mInterfaceBlocks);
    }

    const TStructure *tryGetRenamed(const TStructure &structure)
    {
        return tryGetRenamedImpl(structure, &mStructures);
    }

    const TType *tryGetRenamed(const TType &type)
    {
        return tryGetRenamedImpl(type, static_cast<Remapping<TType> *>(nullptr));
    }

    const TVariable *tryGetRenamed(const TVariable &variable)
    {
        return tryGetRenamedImpl(variable, &mVariables);
    }

    template <typename T>
    const T &getRenamedOrOriginal(const T &object)
    {
        const T *renamed = tryGetRenamed(object);
        if (renamed)
        {
            return *renamed;
        }
        return object;
    }

    template <typename T>
    bool needsRenamingImpl(const T &object) const
    {
        const SymbolType symbolType = object.symbolType();
        switch (symbolType)
        {
            case SymbolType::BuiltIn:
            case SymbolType::AngleInternal:
            case SymbolType::Empty:
                return false;

            case SymbolType::UserDefined:
                break;
        }

        const ImmutableString name = Name(object).rawName();
        if (mKeywords.find(name) != mKeywords.end())
        {
            return true;
        }

        if (name.beginsWith(kAngleInternalPrefix))
        {
            return true;
        }

        return false;
    }

    bool needsRenaming(const TField &field, bool recursive) const
    {
        return needsRenamingImpl(field) || (recursive && needsRenaming(*field.type(), true));
    }

    bool needsRenaming(const TFieldList &fieldList, bool recursive) const
    {
        ASSERT(recursive);
        for (const TField *field : fieldList)
        {
            if (needsRenaming(*field, true))
            {
                return true;
            }
        }
        return false;
    }

    bool needsRenaming(const TFunction &function, bool recursive) const
    {
        if (needsRenamingImpl(function))
        {
            return true;
        }

        if (!recursive)
        {
            return false;
        }

        const size_t paramCount = function.getParamCount();
        for (size_t i = 0; i < paramCount; ++i)
        {
            const TVariable &param = *function.getParam(i);
            if (needsRenaming(param, true))
            {
                return true;
            }
        }

        return false;
    }

    bool needsRenaming(const TInterfaceBlock &interfaceBlock, bool recursive) const
    {
        return needsRenamingImpl(interfaceBlock) ||
               (recursive && needsRenaming(interfaceBlock.fields(), true));
    }

    bool needsRenaming(const TStructure &structure, bool recursive) const
    {
        return needsRenamingImpl(structure) ||
               (recursive && needsRenaming(structure.fields(), true));
    }

    bool needsRenaming(const TType &type, bool recursive) const
    {
        if (const TStructure *structure = type.getStruct())
        {
            return needsRenaming(*structure, recursive);
        }
        else if (const TInterfaceBlock *interfaceBlock = type.getInterfaceBlock())
        {
            return needsRenaming(*interfaceBlock, recursive);
        }
        else
        {
            return false;
        }
    }

    bool needsRenaming(const TVariable &variable, bool recursive) const
    {
        return needsRenamingImpl(variable) ||
               (recursive && needsRenaming(variable.getType(), true));
    }

  public:
    Rewriter(TCompiler &compiler, IdGen &idGen, const std::set<ImmutableString> &keywords)
        : TIntermRebuild(compiler, false, true), mKeywords(keywords), mIdGen(idGen)
    {}

    PostResult visitSymbol(TIntermSymbol &symbolNode)
    {
        const TVariable &var = symbolNode.variable();
        if (needsRenaming(var, true))
        {
            const TVariable &rVar = getRenamedOrOriginal(var);
            return *new TIntermSymbol(&rVar);
        }
        return symbolNode;
    }

    PostResult visitFunctionPrototype(TIntermFunctionPrototype &funcProtoNode)
    {
        const TFunction &func = *funcProtoNode.getFunction();
        if (needsRenaming(func, true))
        {
            const TFunction &rFunc = getRenamedOrOriginal(func);
            return *new TIntermFunctionPrototype(&rFunc);
        }
        return funcProtoNode;
    }

    PostResult visitFunctionDefinitionPost(TIntermFunctionDefinition &funcDefNode) override
    {
        TIntermFunctionPrototype &funcProtoNode = *funcDefNode.getFunctionPrototype();
        const TFunction &func                   = *funcProtoNode.getFunction();
        if (needsRenaming(func, true))
        {
            const TFunction &rFunc = getRenamedOrOriginal(func);
            auto *rFuncProtoNode   = new TIntermFunctionPrototype(&rFunc);
            return *new TIntermFunctionDefinition(rFuncProtoNode, funcDefNode.getBody());
        }
        return funcDefNode;
    }

    PostResult visitAggregatePost(TIntermAggregate &aggregateNode) override
    {
        if (aggregateNode.isConstructor())
        {
            const TType &type = aggregateNode.getType();
            if (needsRenaming(type, true))
            {
                const TType &rType = getRenamedOrOriginal(type);
                return TIntermAggregate::CreateConstructor(rType, aggregateNode.getSequence());
            }
        }
        else
        {
            const TFunction &func = *aggregateNode.getFunction();
            if (needsRenaming(func, true))
            {
                const TFunction &rFunc = getRenamedOrOriginal(func);
                switch (aggregateNode.getOp())
                {
                    case TOperator::EOpCallFunctionInAST:
                        return TIntermAggregate::CreateFunctionCall(rFunc,
                                                                    aggregateNode.getSequence());

                    case TOperator::EOpCallInternalRawFunction:
                        return TIntermAggregate::CreateRawFunctionCall(rFunc,
                                                                       aggregateNode.getSequence());

                    default:
                        return TIntermAggregate::CreateBuiltInFunctionCall(
                            rFunc, aggregateNode.getSequence());
                }
            }
        }
        return aggregateNode;
    }
};

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////

bool sh::RewriteKeywords(TCompiler &compiler,
                         TIntermBlock &root,
                         IdGen &idGen,
                         const std::set<ImmutableString> &keywords)
{
    Rewriter rewriter(compiler, idGen, keywords);
    if (!rewriter.rebuildRoot(root))
    {
        return false;
    }
    return true;
}
