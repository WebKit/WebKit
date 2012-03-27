//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "Macro.h"

#include <algorithm>

#include "stl_utils.h"

namespace pp
{

Macro::Macro(Type type,
             std::string* name,
             TokenVector* parameters,
             TokenVector* replacements)
    : mType(type),
      mName(name),
      mParameters(parameters),
      mReplacements(replacements)
{
}

Macro::~Macro()
{
    delete mName;

    if (mParameters)
    {
        std::for_each(mParameters->begin(), mParameters->end(), Delete());
        delete mParameters;
    }

    if (mReplacements)
    {
        std::for_each(mReplacements->begin(), mReplacements->end(), Delete());
        delete mReplacements;
    }
}

}  // namespace pp

