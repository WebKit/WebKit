//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_PREPROCESSOR_INPUT_H_
#define COMPILER_PREPROCESSOR_INPUT_H_

#include <vector>

namespace pp
{

// Holds and reads input for Lexer.
class Input
{
  public:
    Input();
    Input(int count, const char* const string[], const int length[]);

    int count() const { return mCount; }
    const char* string(int index) const { return mString[index]; }
    int length(int index) const { return mLength[index]; }

    int read(char* buf, int maxSize);

    struct Location
    {
        int sIndex;  // String index;
        int cIndex;  // Char index.

        Location() : sIndex(0), cIndex(0) { }
    };
    const Location& readLoc() const { return mReadLoc; }

  private:
    // Input.
    int mCount;
    const char* const* mString;
    std::vector<int> mLength;

    Location mReadLoc;
};

}  // namespace pp
#endif  // COMPILER_PREPROCESSOR_INPUT_H_

