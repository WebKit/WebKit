//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/TranslatorMetalDirect/RewriteOutArgs.h"
#include "compiler/translator/tree_util/IntermRebuild.h"

using namespace sh;

namespace
{

template <typename T>
class SmallMultiSet
{
  public:
    struct Entry
    {
        T elem;
        size_t count;
    };

    const Entry *find(const T &x) const
    {
        for (auto &entry : mEntries)
        {
            if (x == entry.elem)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    size_t multiplicity(const T &x) const
    {
        const Entry *entry = find(x);
        return entry ? entry->count : 0;
    }

    const Entry &insert(const T &x)
    {
        Entry *entry = findMutable(x);
        if (entry)
        {
            ++entry->count;
            return *entry;
        }
        else
        {
            mEntries.push_back({x, 1});
            return mEntries.back();
        }
    }

    void clear() { mEntries.clear(); }

    bool empty() const { return mEntries.empty(); }

    size_t uniqueSize() const { return mEntries.size(); }

  private:
    ANGLE_INLINE Entry *findMutable(const T &x) { return const_cast<Entry *>(find(x)); }

  private:
    std::vector<Entry> mEntries;
};

const TVariable *GetVariable(TIntermNode &node)
{
    TIntermTyped *tyNode = node.getAsTyped();
    ASSERT(tyNode);
    if (TIntermSymbol *symbol = tyNode->getAsSymbolNode())
    {
        return &symbol->variable();
    }
    return nullptr;
}

class Rewriter : public TIntermRebuild
{
    SmallMultiSet<const TVariable *> mVarBuffer;  // reusable buffer
    SymbolEnv &mSymbolEnv;

  public:
    ~Rewriter() override { ASSERT(mVarBuffer.empty()); }

    Rewriter(TCompiler &compiler, SymbolEnv &symbolEnv)
        : TIntermRebuild(compiler, false, true), mSymbolEnv(symbolEnv)
    {}

    static bool argAlreadyProcessed(TIntermTyped *arg)
    {
        if (arg->getAsAggregate())
        {
            const TFunction *func = arg->getAsAggregate()->getFunction();
            if (func && func->symbolType() == SymbolType::AngleInternal && func->name() == "swizzle_ref")
            {
                return true;
            }
        }
        return false;
    }

    PostResult visitAggregatePost(TIntermAggregate &aggregateNode) override
    {
        ASSERT(mVarBuffer.empty());

        const TFunction *func = aggregateNode.getFunction();
        if (!func)
        {
            return aggregateNode;
        }

        TIntermSequence &args = *aggregateNode.getSequence();
        size_t argCount       = args.size();

        auto getParamQualifier = [&](size_t i) {
            const TVariable &param     = *func->getParam(i);
            const TType &paramType     = param.getType();
            const TQualifier paramQual = paramType.getQualifier();
            switch (paramQual)
            {
                case TQualifier::EvqOut:
                case TQualifier::EvqInOut:
                    if (!mSymbolEnv.isReference(param))
                    {
                        mSymbolEnv.markAsReference(param, AddressSpace::Thread);
                    }
                    break;
                default:
                    break;
            }
            return paramQual;
        };

        bool mightAlias = false;

        for (size_t i = 0; i < argCount; ++i)
        {
            const TQualifier paramQual = getParamQualifier(i);

            switch (paramQual)
            {
                case TQualifier::EvqOut:
                case TQualifier::EvqInOut:
                {
                    const TVariable *var = GetVariable(*args[i]);
                    if (mVarBuffer.insert(var).count > 1)
                    {
                        mightAlias = true;
                        i          = argCount;
                    }
                }
                break;

                default:
                    break;
            }
        }

        const bool hasIndeterminateVar = mVarBuffer.find(nullptr);

        if (!mightAlias)
        {
            mightAlias = hasIndeterminateVar && mVarBuffer.uniqueSize() > 1;
        }

        if (mightAlias)
        {
            for (size_t i = 0; i < argCount; ++i)
            {
                TIntermTyped *arg = args[i]->getAsTyped();
                ASSERT(arg);
                if (!argAlreadyProcessed(arg))
                {
                    const TVariable *var       = GetVariable(*arg);
                    const TQualifier paramQual = getParamQualifier(i);

                    if (hasIndeterminateVar || mVarBuffer.multiplicity(var) > 1)
                    {
                        switch (paramQual)
                        {
                            case TQualifier::EvqOut:
                                args[i] = &mSymbolEnv.callFunctionOverload(Name("out"), arg->getType(),
                                                                           *new TIntermSequence{arg});
                                break;

                            case TQualifier::EvqInOut:
                                args[i] = &mSymbolEnv.callFunctionOverload(
                                    Name("inout"), arg->getType(), *new TIntermSequence{arg});
                                break;

                            default:
                                break;
                        }
                    }
                }
            }
        }

        mVarBuffer.clear();

        return aggregateNode;
    }
};

}  // anonymous namespace

bool sh::RewriteOutArgs(TCompiler &compiler, TIntermBlock &root, SymbolEnv &symbolEnv)
{
    Rewriter rewriter(compiler, symbolEnv);
    if (!rewriter.rebuildRoot(root))
    {
        return false;
    }
    return true;
}
