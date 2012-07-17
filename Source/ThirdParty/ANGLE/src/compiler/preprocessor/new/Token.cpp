//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "Token.h"

namespace pp
{

void Token::reset()
{
    type = 0;
    flags = 0;
    location = SourceLocation();
    value.clear();
}

bool Token::equals(const Token& other) const
{
    return (type == other.type) &&
           (flags == other.flags) &&
           (location == other.location) &&
           (value == other.value);
}

void Token::setAtStartOfLine(bool start)
{
    if (start)
        flags |= AT_START_OF_LINE;
    else
        flags &= ~AT_START_OF_LINE;
}

void Token::setHasLeadingSpace(bool space)
{
    if (space)
        flags |= HAS_LEADING_SPACE;
    else
        flags &= ~HAS_LEADING_SPACE;
}

void Token::setExpansionDisabled(bool disable)
{
    if (disable)
        flags |= EXPANSION_DISABLED;
    else
        flags &= ~EXPANSION_DISABLED;
}

std::ostream& operator<<(std::ostream& out, const Token& token)
{
    if (token.hasLeadingSpace())
        out << " ";

    out << token.value;
    return out;
}

}  // namespace pp
