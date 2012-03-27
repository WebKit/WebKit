//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "Token.h"

#include "token_type.h"

static const int kLocationLineSize = 16;  // in bits.
static const int kLocationLineMask = (1 << kLocationLineSize) - 1;

namespace pp
{

Token::Location Token::encodeLocation(int line, int file)
{
    return (file << kLocationLineSize) | (line & kLocationLineMask);
}

void Token::decodeLocation(Location loc, int* line, int* file)
{
    if (file) *file = loc >> kLocationLineSize;
    if (line) *line = loc & kLocationLineMask;
}

Token::Token(Location location, int type, std::string* value)
    : mLocation(location),
      mType(type),
      mValue(value)
{
}

Token::~Token() {
    delete mValue;
}

std::ostream& operator<<(std::ostream& out, const Token& token)
{
    switch (token.type())
    {
      case INT_CONSTANT:
      case FLOAT_CONSTANT:
      case IDENTIFIER:
        out << *(token.value());
        break;
      default:
        out << static_cast<char>(token.type());
        break;
    }
    return out;
}
}  // namespace pp

