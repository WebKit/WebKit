//
// Copyright (c) 2002-2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/BuiltInFunctionEmulator.h"
#include "angle_gl.h"
#include "compiler/translator/Cache.h"
#include "compiler/translator/IntermTraverse.h"
#include "compiler/translator/SymbolTable.h"

namespace sh
{

class BuiltInFunctionEmulator::BuiltInFunctionEmulationMarker : public TIntermTraverser
{
  public:
    BuiltInFunctionEmulationMarker(BuiltInFunctionEmulator &emulator)
        : TIntermTraverser(true, false, false), mEmulator(emulator)
    {
    }

    bool visitUnary(Visit visit, TIntermUnary *node) override
    {
        if (visit == PreVisit)
        {
            bool needToEmulate =
                mEmulator.setFunctionCalled(node->getOp(), node->getOperand()->getType());
            if (needToEmulate)
                node->setUseEmulatedFunction();
        }
        return true;
    }

    bool visitAggregate(Visit visit, TIntermAggregate *node) override
    {
        if (visit == PreVisit)
        {
            // Here we handle all the built-in functions mapped to ops, not just the ones that are
            // currently identified as problematic.
            if (node->isConstructor() || node->isFunctionCall())
            {
                return true;
            }
            const TIntermSequence &sequence = *(node->getSequence());
            bool needToEmulate              = false;
            // Right now we only handle built-in functions with two to four parameters.
            if (sequence.size() == 2)
            {
                TIntermTyped *param1 = sequence[0]->getAsTyped();
                TIntermTyped *param2 = sequence[1]->getAsTyped();
                if (!param1 || !param2)
                    return true;
                needToEmulate = mEmulator.setFunctionCalled(node->getOp(), param1->getType(),
                                                            param2->getType());
            }
            else if (sequence.size() == 3)
            {
                TIntermTyped *param1 = sequence[0]->getAsTyped();
                TIntermTyped *param2 = sequence[1]->getAsTyped();
                TIntermTyped *param3 = sequence[2]->getAsTyped();
                if (!param1 || !param2 || !param3)
                    return true;
                needToEmulate = mEmulator.setFunctionCalled(node->getOp(), param1->getType(),
                                                            param2->getType(), param3->getType());
            }
            else if (sequence.size() == 4)
            {
                TIntermTyped *param1 = sequence[0]->getAsTyped();
                TIntermTyped *param2 = sequence[1]->getAsTyped();
                TIntermTyped *param3 = sequence[2]->getAsTyped();
                TIntermTyped *param4 = sequence[3]->getAsTyped();
                if (!param1 || !param2 || !param3 || !param4)
                    return true;
                needToEmulate =
                    mEmulator.setFunctionCalled(node->getOp(), param1->getType(), param2->getType(),
                                                param3->getType(), param4->getType());
            }
            else
            {
                return true;
            }

            if (needToEmulate)
                node->setUseEmulatedFunction();
        }
        return true;
    }

  private:
    BuiltInFunctionEmulator &mEmulator;
};

BuiltInFunctionEmulator::BuiltInFunctionEmulator()
{
}

FunctionId BuiltInFunctionEmulator::addEmulatedFunction(TOperator op,
                                                        const TType *param,
                                                        const char *emulatedFunctionDefinition)
{
    FunctionId id(op, param);
    mEmulatedFunctions[id] = std::string(emulatedFunctionDefinition);
    return id;
}

FunctionId BuiltInFunctionEmulator::addEmulatedFunction(TOperator op,
                                                        const TType *param1,
                                                        const TType *param2,
                                                        const char *emulatedFunctionDefinition)
{
    FunctionId id(op, param1, param2);
    mEmulatedFunctions[id] = std::string(emulatedFunctionDefinition);
    return id;
}

FunctionId BuiltInFunctionEmulator::addEmulatedFunctionWithDependency(
    const FunctionId &dependency,
    TOperator op,
    const TType *param1,
    const TType *param2,
    const char *emulatedFunctionDefinition)
{
    FunctionId id(op, param1, param2);
    mEmulatedFunctions[id]    = std::string(emulatedFunctionDefinition);
    mFunctionDependencies[id] = dependency;
    return id;
}

FunctionId BuiltInFunctionEmulator::addEmulatedFunction(TOperator op,
                                                        const TType *param1,
                                                        const TType *param2,
                                                        const TType *param3,
                                                        const char *emulatedFunctionDefinition)
{
    FunctionId id(op, param1, param2, param3);
    mEmulatedFunctions[id] = std::string(emulatedFunctionDefinition);
    return id;
}

FunctionId BuiltInFunctionEmulator::addEmulatedFunction(TOperator op,
                                                        const TType *param1,
                                                        const TType *param2,
                                                        const TType *param3,
                                                        const TType *param4,
                                                        const char *emulatedFunctionDefinition)
{
    FunctionId id(op, param1, param2, param3, param4);
    mEmulatedFunctions[id] = std::string(emulatedFunctionDefinition);
    return id;
}

FunctionId BuiltInFunctionEmulator::addEmulatedFunctionWithDependency(
    const FunctionId &dependency,
    TOperator op,
    const TType *param1,
    const TType *param2,
    const TType *param3,
    const TType *param4,
    const char *emulatedFunctionDefinition)
{
    FunctionId id(op, param1, param2, param3, param4);
    mEmulatedFunctions[id]    = std::string(emulatedFunctionDefinition);
    mFunctionDependencies[id] = dependency;
    return id;
}

bool BuiltInFunctionEmulator::isOutputEmpty() const
{
    return (mFunctions.size() == 0);
}

void BuiltInFunctionEmulator::outputEmulatedFunctions(TInfoSinkBase &out) const
{
    for (const auto &function : mFunctions)
    {
        const char *body = findEmulatedFunction(function);
        ASSERT(body);
        out << body;
        out << "\n\n";
    }
}

bool BuiltInFunctionEmulator::setFunctionCalled(TOperator op, const TType &param)
{
    return setFunctionCalled(FunctionId(op, &param));
}

bool BuiltInFunctionEmulator::setFunctionCalled(TOperator op,
                                                const TType &param1,
                                                const TType &param2)
{
    return setFunctionCalled(FunctionId(op, &param1, &param2));
}

bool BuiltInFunctionEmulator::setFunctionCalled(TOperator op,
                                                const TType &param1,
                                                const TType &param2,
                                                const TType &param3)
{
    return setFunctionCalled(FunctionId(op, &param1, &param2, &param3));
}

bool BuiltInFunctionEmulator::setFunctionCalled(TOperator op,
                                                const TType &param1,
                                                const TType &param2,
                                                const TType &param3,
                                                const TType &param4)
{
    return setFunctionCalled(FunctionId(op, &param1, &param2, &param3, &param4));
}

const char *BuiltInFunctionEmulator::findEmulatedFunction(const FunctionId &functionId) const
{
    for (const auto &queryFunction : mQueryFunctions)
    {
        const char *result = queryFunction(functionId);
        if (result)
        {
            return result;
        }
    }

    const auto &result = mEmulatedFunctions.find(functionId);
    if (result != mEmulatedFunctions.end())
    {
        return result->second.c_str();
    }

    return nullptr;
}

bool BuiltInFunctionEmulator::setFunctionCalled(const FunctionId &functionId)
{
    if (!findEmulatedFunction(functionId))
    {
        return false;
    }

    for (size_t i = 0; i < mFunctions.size(); ++i)
    {
        if (mFunctions[i] == functionId)
            return true;
    }
    // If the function depends on another, mark the dependency as called.
    auto dependency = mFunctionDependencies.find(functionId);
    if (dependency != mFunctionDependencies.end())
    {
        setFunctionCalled((*dependency).second);
    }
    // Copy the functionId if it needs to be stored, to make sure that the TType pointers inside
    // remain valid and constant.
    mFunctions.push_back(functionId.getCopy());
    return true;
}

void BuiltInFunctionEmulator::markBuiltInFunctionsForEmulation(TIntermNode *root)
{
    ASSERT(root);

    if (mEmulatedFunctions.empty() && mQueryFunctions.empty())
        return;

    BuiltInFunctionEmulationMarker marker(*this);
    root->traverse(&marker);
}

void BuiltInFunctionEmulator::cleanup()
{
    mFunctions.clear();
    mFunctionDependencies.clear();
}

void BuiltInFunctionEmulator::addFunctionMap(BuiltinQueryFunc queryFunc)
{
    mQueryFunctions.push_back(queryFunc);
}

// static
void BuiltInFunctionEmulator::WriteEmulatedFunctionName(TInfoSinkBase &out, const char *name)
{
    ASSERT(name[strlen(name) - 1] != '(');
    out << name << "_emu";
}

FunctionId::FunctionId()
    : mOp(EOpNull),
      mParam1(TCache::getType(EbtVoid)),
      mParam2(TCache::getType(EbtVoid)),
      mParam3(TCache::getType(EbtVoid)),
      mParam4(TCache::getType(EbtVoid))
{
}

FunctionId::FunctionId(TOperator op, const TType *param)
    : mOp(op),
      mParam1(param),
      mParam2(TCache::getType(EbtVoid)),
      mParam3(TCache::getType(EbtVoid)),
      mParam4(TCache::getType(EbtVoid))
{
}

FunctionId::FunctionId(TOperator op, const TType *param1, const TType *param2)
    : mOp(op),
      mParam1(param1),
      mParam2(param2),
      mParam3(TCache::getType(EbtVoid)),
      mParam4(TCache::getType(EbtVoid))
{
}

FunctionId::FunctionId(TOperator op, const TType *param1, const TType *param2, const TType *param3)
    : mOp(op), mParam1(param1), mParam2(param2), mParam3(param3), mParam4(TCache::getType(EbtVoid))
{
}

FunctionId::FunctionId(TOperator op,
                       const TType *param1,
                       const TType *param2,
                       const TType *param3,
                       const TType *param4)
    : mOp(op), mParam1(param1), mParam2(param2), mParam3(param3), mParam4(param4)
{
}

bool FunctionId::operator==(const FunctionId &other) const
{
    return (mOp == other.mOp && *mParam1 == *other.mParam1 && *mParam2 == *other.mParam2 &&
            *mParam3 == *other.mParam3 && *mParam4 == *other.mParam4);
}

bool FunctionId::operator<(const FunctionId &other) const
{
    if (mOp != other.mOp)
        return mOp < other.mOp;
    if (*mParam1 != *other.mParam1)
        return *mParam1 < *other.mParam1;
    if (*mParam2 != *other.mParam2)
        return *mParam2 < *other.mParam2;
    if (*mParam3 != *other.mParam3)
        return *mParam3 < *other.mParam3;
    if (*mParam4 != *other.mParam4)
        return *mParam4 < *other.mParam4;
    return false;  // all fields are equal
}

FunctionId FunctionId::getCopy() const
{
    return FunctionId(mOp, new TType(*mParam1), new TType(*mParam2), new TType(*mParam3),
                      new TType(*mParam4));
}

}  // namespace sh
