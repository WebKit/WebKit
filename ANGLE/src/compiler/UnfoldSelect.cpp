//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
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
    mTemporaryIndex++;
    node->traverse(this);
}

bool UnfoldSelect::visitSelection(Visit visit, TIntermSelection *node)
{
    TInfoSinkBase &out = mOutputHLSL->getBodyStream();

    if (node->usesTernaryOperator())
    {
        int i = mTemporaryIndex++;

        out << mOutputHLSL->typeString(node->getType()) << " t" << i << ";\n";

        node->getCondition()->traverse(this);
        out << "if(";
        node->getCondition()->traverse(mOutputHLSL);
        out << ")\n"
               "{\n";
        node->getTrueBlock()->traverse(this);
        out << "    t" << i << " = ";
        node->getTrueBlock()->traverse(mOutputHLSL);
        out << ";\n"
               "}\n"
               "else\n"
               "{\n";
        node->getCondition()->traverse(this);
        out << "    t" << i << " = ";
        node->getFalseBlock()->traverse(mOutputHLSL);
        out << ";\n"
               "}\n";

        mTemporaryIndex--;
    }

    return false;
}

int UnfoldSelect::getTemporaryIndex()
{
    return mTemporaryIndex;
}
}
