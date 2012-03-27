//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_PREPROCESSOR_INPUT_H_
#define COMPILER_PREPROCESSOR_INPUT_H_

namespace pp
{

// Reads the given set of strings into input buffer.
// Strips comments.
class Input
{
  public:
    Input(int count, const char* const string[], const int length[]);

    enum Error
    {
        kErrorNone,
        kErrorUnexpectedEOF
    };
    Error error() const { return mError; }

    // Returns the index of string currently being scanned.
    int stringIndex() const { return mIndex; }
    // Returns true if EOF has reached.
    bool eof() const;

    // Reads up to bufSize characters into buf.
    // Returns the number of characters read.
    // It replaces each comment by a whitespace. It reads only one string
    // at a time so that the lexer has opportunity to update the string number
    // for meaningful diagnostic messages.
    int read(char* buf, int bufSize);

private:
    enum State
    {
        kStateInitial,
        kStateLineComment,
        kStateBlockComment
    };

    int getChar();
    int peekChar();
    // Switches input buffer to the next non-empty string.
    // This is called when the current string is fully read.
    void switchToNextString();
    // Returns true if the given string is empty.
    bool isStringEmpty(int index);
    // Return the length of the given string.
    // Returns a negative value for null-terminated strings.
    int stringLength(int index);

    // Input.
    int mCount;
    const char* const* mString;
    const int* mLength;

    // Current read position.
    int mIndex;   // Index of string currently being scanned.
    int mSize;    // Size of string already scanned.

    // Current error and state.
    Error mError;
    State mState;
};

}  // namespace pp
#endif  // COMPILER_PREPROCESSOR_INPUT_H_

