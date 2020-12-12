//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/TranslatorMetalDirect/Name.h"
#include "common/debug.h"
#include "compiler/translator/TranslatorMetalDirect/Debug.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

using namespace sh;

////////////////////////////////////////////////////////////////////////////////

template <typename T>
static ImmutableString GetName(T const &object)
{
    if (object.symbolType() == SymbolType::Empty)
    {
        return kEmptyImmutableString;
    }
    return object.name();
}

Name::Name(const TField &field) : Name(GetName(field), field.symbolType()) {}

Name::Name(const TSymbol &symbol) : Name(GetName(symbol), symbol.symbolType()) {}

bool Name::operator==(const Name &other) const
{
    return mRawName == other.mRawName && mSymbolType == other.mSymbolType;
}

bool Name::operator!=(const Name &other) const
{
    return !(*this == other);
}

bool Name::operator<(const Name &other) const
{
    if (mRawName < other.mRawName)
    {
        return true;
    }
    if (other.mRawName < mRawName)
    {
        return false;
    }
    return mSymbolType < other.mSymbolType;
}

bool Name::empty() const
{
    return mSymbolType == SymbolType::Empty;
}

bool Name::beginsWith(const Name &prefix) const
{
    if (mSymbolType != prefix.mSymbolType)
    {
        return false;
    }
    return mRawName.beginsWith(prefix.mRawName);
}

void Name::emit(TInfoSinkBase &out) const
{
    switch (mSymbolType)
    {
        case SymbolType::BuiltIn:
        case SymbolType::UserDefined:
            ASSERT(!mRawName.empty());
            out << mRawName;
            break;

        case SymbolType::AngleInternal:
            ASSERT(!mRawName.empty());
            if (mRawName.beginsWith(kAngleInternalPrefix))
            {
                out << mRawName;
            }
            else if (mRawName[0] != '_')
            {
                out << kAngleInternalPrefix << '_' << mRawName;
            }
            else
            {
                out << kAngleInternalPrefix << mRawName;
            }
            break;

        case SymbolType::Empty:
            LOGIC_ERROR();
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////

namespace
{

// NOTE: This matches more things than FindSymbolNode.
class ExpressionContainsNameVisitor : public TIntermTraverser
{
    Name mName;
    bool mFoundName = false;

  public:
    ExpressionContainsNameVisitor(const Name &name)
        : TIntermTraverser(true, false, false), mName(name)
    {}

    bool foundName() const { return mFoundName; }

    void visitSymbol(TIntermSymbol *node) override
    {
        if (Name(node->variable()) == mName)
        {
            mFoundName = true;
        }
    }

    bool visitSwizzle(Visit, TIntermSwizzle *) override { return !mFoundName; }

    bool visitBinary(Visit visit, TIntermBinary *node) override { return !mFoundName; }

    bool visitUnary(Visit visit, TIntermUnary *node) override  { return !mFoundName; }

    bool visitTernary(Visit visit, TIntermTernary *node) override { return !mFoundName; }

    bool visitAggregate(Visit visit, TIntermAggregate *node) override
    {
        if (node->isConstructor())
        {
            const TType &type           = node->getType();
            const TStructure *structure = type.getStruct();
            if (structure && Name(*structure) == mName)
            {
                mFoundName = true;
            }
        }
        else
        {
            const TFunction *func = node->getFunction();
            if (func && Name(*func) == mName)
            {
                mFoundName = true;
            }
        }
        return !mFoundName;
    }

    bool visitIfElse(Visit visit, TIntermIfElse *node) override
    {
        LOGIC_ERROR();
        return false;
    }

    bool visitSwitch(Visit, TIntermSwitch *) override
    {
        LOGIC_ERROR();
        return false;
    }
    bool visitCase(Visit, TIntermCase *) override
    {
        LOGIC_ERROR();
        return false;
    }

    void visitFunctionPrototype(TIntermFunctionPrototype *) override { LOGIC_ERROR(); }

    bool visitFunctionDefinition(Visit, TIntermFunctionDefinition *) override
    {
        LOGIC_ERROR();
        return false;
    }

    bool visitBlock(Visit, TIntermBlock *) override
    {
        LOGIC_ERROR();
        return false;
    }

    bool visitGlobalQualifierDeclaration(Visit, TIntermGlobalQualifierDeclaration *) override
    {
        LOGIC_ERROR();
        return false;
    }

    bool visitDeclaration(Visit, TIntermDeclaration *) override
    {
        LOGIC_ERROR();
        return false;
    }

    bool visitLoop(Visit, TIntermLoop *) override
    {
        LOGIC_ERROR();
        return false;
    }

    bool visitBranch(Visit, TIntermBranch *) override
    {
        LOGIC_ERROR();
        return false;
    }

    void visitPreprocessorDirective(TIntermPreprocessorDirective *) override { LOGIC_ERROR(); }
};

}  // anonymous namespace

bool sh::ExpressionContainsName(const Name &name, TIntermTyped &node)
{
    ExpressionContainsNameVisitor visitor(name);
    node.traverse(&visitor);
    return visitor.foundName();
}
