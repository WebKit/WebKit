//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// UnfoldSelect is an AST traverser to output the select operator ?: as if-else statements
//

#include "compiler/UnfoldSelect.h"

#include "compiler/InfoSink.h"
#include "compiler/OutputHLSL.h"

namespace sh
{
UnfoldSelect::UnfoldSelect(TParseContext &context, OutputHLSL *outputHLSL) : mContext(context), mOutputHLSL(outputHLSL)
{
    mTemporaryIndex = 0;
}

void UnfoldSelect::traverse(TIntermNode *node)
{
    int rewindIndex = mTemporaryIndex;
    node->traverse(this);
    mTemporaryIndex = rewindIndex;
}

bool UnfoldSelect::visitSelection(Visit visit, TIntermSelection *node)
{
    TInfoSinkBase &out = mOutputHLSL->getBodyStream();

    if (node->usesTernaryOperator())
    {
        int i = mTemporaryIndex;

        out << mOutputHLSL->typeString(node->getType()) << " s" << i << ";\n";

        mTemporaryIndex = i + 1;
        node->getCondition()->traverse(this);
        out << "if(";
        mTemporaryIndex = i + 1;
        node->getCondition()->traverse(mOutputHLSL);
        out << ")\n"
               "{\n";
        mTemporaryIndex = i + 1;
        node->getTrueBlock()->traverse(this);
        out << "    s" << i << " = ";
        mTemporaryIndex = i + 1;
        node->getTrueBlock()->traverse(mOutputHLSL);
        out << ";\n"
               "}\n"
               "else\n"
               "{\n";
        mTemporaryIndex = i + 1;
        node->getFalseBlock()->traverse(this);
        out << "    s" << i << " = ";
        mTemporaryIndex = i + 1;
        node->getFalseBlock()->traverse(mOutputHLSL);
        out << ";\n"
               "}\n";

        mTemporaryIndex = i + 1;
    }

    return false;
}

bool UnfoldSelect::visitLoop(Visit visit, TIntermLoop *node)
{
    int rewindIndex = mTemporaryIndex;

    if (node->getInit())
    {
        node->getInit()->traverse(this);
    }
    
    if (node->getCondition())
    {
        node->getCondition()->traverse(this);
    }

    if (node->getExpression())
    {
        node->getExpression()->traverse(this);
    }

    mTemporaryIndex = rewindIndex;

    return false;
}

int UnfoldSelect::getNextTemporaryIndex()
{
    return mTemporaryIndex++;
}
}
