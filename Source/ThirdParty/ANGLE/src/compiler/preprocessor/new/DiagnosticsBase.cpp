//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "Diagnostics.h"

#include <cassert>

namespace pp
{

Diagnostics::~Diagnostics()
{
}

void Diagnostics::report(ID id,
                         const SourceLocation& loc,
                         const std::string& text)
{
    // TODO(alokp): Keep a count of errors and warnings.
    print(id, loc, text);
}

Diagnostics::Severity Diagnostics::severity(ID id)
{
    if ((id > ERROR_BEGIN) && (id < ERROR_END))
        return ERROR;

    if ((id > WARNING_BEGIN) && (id < WARNING_END))
        return WARNING;

    assert(false);
    return ERROR;
}

}  // namespace pp
