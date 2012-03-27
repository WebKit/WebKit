//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "Preprocessor.h"

#include <algorithm>

#include "compiler/debug.h"
#include "Context.h"
#include "stl_utils.h"

namespace pp
{

Preprocessor::Preprocessor() : mContext(NULL)
{
}

Preprocessor::~Preprocessor()
{
    delete mContext;
}

bool Preprocessor::init()
{
    mContext = new Context;
    return mContext->init();
}

bool Preprocessor::process(int count,
                           const char* const string[],
                           const int length[])
{
    ASSERT((count >=0) && (string != NULL));
    if ((count < 0) || (string == NULL))
        return false;

    reset();

    return mContext->process(count, string, length, &mTokens);
}

void Preprocessor::reset()
{
    std::for_each(mTokens.begin(), mTokens.end(), Delete());
    mTokens.clear();
}

}  // namespace pp

