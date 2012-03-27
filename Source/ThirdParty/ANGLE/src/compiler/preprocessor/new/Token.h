//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_PREPROCESSOR_TOKEN_H_
#define COMPILER_PREPROCESSOR_TOKEN_H_

#include <string>
#include <vector>

#include "common/angleutils.h"

namespace pp
{

class Token
{
  public:
    typedef int Location;
    static Location encodeLocation(int line, int file);
    static void decodeLocation(Location loc, int* line, int* file);

    // Takes ownership of string.
    Token(Location location, int type, std::string* value);
    ~Token();

    Location location() const { return mLocation; }
    int type() const { return mType; }
    const std::string* value() const { return mValue; }

  private:
    DISALLOW_COPY_AND_ASSIGN(Token);

    Location mLocation;
    int mType;
    std::string* mValue;
};

typedef std::vector<Token*> TokenVector;
typedef TokenVector::const_iterator TokenIterator;

extern std::ostream& operator<<(std::ostream& out, const Token& token);

}  // namepsace pp
#endif  // COMPILER_PREPROCESSOR_TOKEN_H_

