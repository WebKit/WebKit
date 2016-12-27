//
// Copyright (c) 2002-2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// CallDAG.h: Implements a call graph DAG of functions to be re-used accross
// analyses, allows to efficiently traverse the functions in topological
// order.

#include "compiler/translator/CallDAG.h"
#include "compiler/translator/InfoSink.h"

// The CallDAGCreator does all the processing required to create the CallDAG
// structure so that the latter contains only the necessary variables.
class CallDAG::CallDAGCreator : public TIntermTraverser
{
  public:
    CallDAGCreator(TInfoSinkBase *info)
        : TIntermTraverser(true, false, true),
          mCreationInfo(info),
          mCurrentFunction(nullptr),
          mCurrentIndex(0)
    {
    }

    InitResult assignIndices()
    {
        int skipped = 0;
        for (auto &it : mFunctions)
        {
            // Skip unimplemented functions
            if (it.second.node)
            {
                InitResult result = assignIndicesInternal(&it.second);
                if (result != INITDAG_SUCCESS)
                {
                    *mCreationInfo << "\n";
                    return result;
                }
            }
            else
            {
                skipped++;
            }
        }

        ASSERT(mFunctions.size() == mCurrentIndex + skipped);
        return INITDAG_SUCCESS;
    }

    void fillDataStructures(std::vector<Record> *records, std::map<int, int> *idToIndex)
    {
        ASSERT(records->empty());
        ASSERT(idToIndex->empty());

        records->resize(mCurrentIndex);

        for (auto &it : mFunctions)
        {
            CreatorFunctionData &data = it.second;
            // Skip unimplemented functions
            if (!data.node)
            {
                continue;
            }
            ASSERT(data.index < records->size());
            Record &record = (*records)[data.index];

            record.name = data.name.data();
            record.node = data.node;

            record.callees.reserve(data.callees.size());
            for (auto &callee : data.callees)
            {
                record.callees.push_back(static_cast<int>(callee->index));
            }

            (*idToIndex)[data.node->getFunctionSymbolInfo()->getId()] =
                static_cast<int>(data.index);
        }
    }

  private:

    struct CreatorFunctionData
    {
        CreatorFunctionData()
            : node(nullptr),
              index(0),
              indexAssigned(false),
              visiting(false)
        {
        }

        std::set<CreatorFunctionData*> callees;
        TIntermFunctionDefinition *node;
        TString name;
        size_t index;
        bool indexAssigned;
        bool visiting;
    };

    bool visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *node) override
    {
        // Create the record if need be and remember the node.
        if (visit == PreVisit)
        {
            auto it = mFunctions.find(node->getFunctionSymbolInfo()->getName());

            if (it == mFunctions.end())
            {
                mCurrentFunction = &mFunctions[node->getFunctionSymbolInfo()->getName()];
            }
            else
            {
                mCurrentFunction = &it->second;
            }

            mCurrentFunction->node = node;
            mCurrentFunction->name = node->getFunctionSymbolInfo()->getName();
        }
        else if (visit == PostVisit)
        {
            mCurrentFunction = nullptr;
        }
        return true;
    }

    // Aggregates the AST node for each function as well as the name of the functions called by it
    bool visitAggregate(Visit visit, TIntermAggregate *node) override
    {
        switch (node->getOp())
        {
          case EOpPrototype:
            if (visit == PreVisit)
            {
                // Function declaration, create an empty record.
                auto &record = mFunctions[node->getFunctionSymbolInfo()->getName()];
                record.name  = node->getFunctionSymbolInfo()->getName();
            }
            break;
          case EOpFunctionCall:
            {
                // Function call, add the callees
                if (visit == PreVisit)
                {
                    // Do not handle calls to builtin functions
                    if (node->isUserDefined())
                    {
                        auto it = mFunctions.find(node->getFunctionSymbolInfo()->getName());
                        ASSERT(it != mFunctions.end());

                        // We might be in a top-level function call to set a global variable
                        if (mCurrentFunction)
                        {
                            mCurrentFunction->callees.insert(&it->second);
                        }
                    }
                }
                break;
            }
          default:
            break;
        }
        return true;
    }

    // Recursively assigns indices to a sub DAG
    InitResult assignIndicesInternal(CreatorFunctionData *root)
    {
        // Iterative implementation of the index assignment algorithm. A recursive version
        // would be prettier but since the CallDAG creation runs before the limiting of the
        // call depth, we might get stack overflows (computation of the call depth uses the
        // CallDAG).

        ASSERT(root);

        if (root->indexAssigned)
        {
            return INITDAG_SUCCESS;
        }

        // If we didn't have to detect recursion, functionsToProcess could be a simple queue
        // in which we add the function being processed's callees. However in order to detect
        // recursion we need to know which functions we are currently visiting. For that reason
        // functionsToProcess will look like a concatenation of segments of the form
        // [F visiting = true, subset of F callees with visiting = false] and the following
        // segment (if any) will be start with a callee of F.
        // This way we can remember when we started visiting a function, to put visiting back
        // to false.
        TVector<CreatorFunctionData *> functionsToProcess;
        functionsToProcess.push_back(root);

        InitResult result = INITDAG_SUCCESS;

        while (!functionsToProcess.empty())
        {
            CreatorFunctionData *function = functionsToProcess.back();

            if (function->visiting)
            {
                function->visiting      = false;
                function->index         = mCurrentIndex++;
                function->indexAssigned = true;

                functionsToProcess.pop_back();
                continue;
            }

            if (!function->node)
            {
                *mCreationInfo << "Undefined function '" << function->name
                               << ")' used in the following call chain:";
                result = INITDAG_UNDEFINED;
                break;
            }

            if (function->indexAssigned)
            {
                functionsToProcess.pop_back();
                continue;
            }

            function->visiting = true;

            for (auto callee : function->callees)
            {
                functionsToProcess.push_back(callee);

                // Check if the callee is already being visited after pushing it so that it appears
                // in the chain printed in the info log.
                if (callee->visiting)
                {
                    *mCreationInfo << "Recursive function call in the following call chain:";
                    result = INITDAG_RECURSION;
                    break;
                }
            }

            if (result != INITDAG_SUCCESS)
            {
                break;
            }
        }

        // The call chain is made of the function we were visiting when the error was detected.
        if (result != INITDAG_SUCCESS)
        {
            bool first = true;
            for (auto function : functionsToProcess)
            {
                if (function->visiting)
                {
                    if (!first)
                    {
                        *mCreationInfo << " -> ";
                    }
                    *mCreationInfo << function->name << ")";
                    first = false;
                }
            }
        }

        return result;
    }

    TInfoSinkBase *mCreationInfo;

    std::map<TString, CreatorFunctionData> mFunctions;
    CreatorFunctionData *mCurrentFunction;
    size_t mCurrentIndex;
};

// CallDAG

CallDAG::CallDAG()
{
}

CallDAG::~CallDAG()
{
}

const size_t CallDAG::InvalidIndex = std::numeric_limits<size_t>::max();

size_t CallDAG::findIndex(const TFunctionSymbolInfo *functionInfo) const
{
    auto it = mFunctionIdToIndex.find(functionInfo->getId());

    if (it == mFunctionIdToIndex.end())
    {
        return InvalidIndex;
    }
    else
    {
        return it->second;
    }
}

const CallDAG::Record &CallDAG::getRecordFromIndex(size_t index) const
{
    ASSERT(index != InvalidIndex && index < mRecords.size());
    return mRecords[index];
}

const CallDAG::Record &CallDAG::getRecord(const TIntermAggregate *function) const
{
    size_t index = findIndex(function->getFunctionSymbolInfo());
    ASSERT(index != InvalidIndex && index < mRecords.size());
    return mRecords[index];
}

size_t CallDAG::size() const
{
    return mRecords.size();
}

void CallDAG::clear()
{
    mRecords.clear();
    mFunctionIdToIndex.clear();
}

CallDAG::InitResult CallDAG::init(TIntermNode *root, TInfoSinkBase *info)
{
    ASSERT(info);

    CallDAGCreator creator(info);

    // Creates the mapping of functions to callees
    root->traverse(&creator);

    // Does the topological sort and detects recursions
    InitResult result = creator.assignIndices();
    if (result != INITDAG_SUCCESS)
    {
        return result;
    }

    creator.fillDataStructures(&mRecords, &mFunctionIdToIndex);
    return INITDAG_SUCCESS;
}
