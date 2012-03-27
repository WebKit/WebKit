//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "Input.h"

#include <cstdio>

#include "compiler/debug.h"

namespace pp
{

Input::Input(int count, const char* const string[], const int length[])
    : mCount(count),
      mString(string),
      mLength(length),
      mIndex(-1),
      mSize(0),
      mError(kErrorNone),
      mState(kStateInitial)
{
    ASSERT(mCount >= 0);
    switchToNextString();
}

bool Input::eof() const
{
    ASSERT(mIndex <= mCount);
    return mIndex == mCount;
}

int Input::read(char* buf, int bufSize)
{
    int nread = 0;
    int startIndex = mIndex;
    // Keep reading until the buffer is full or the current string is exhausted.
    while ((mIndex == startIndex) && (nread < bufSize))
    {
        int c = getChar();
        if (c == EOF)
        {
            if (mState == kStateBlockComment)
                mError = kErrorUnexpectedEOF;
            break;
        }

        switch (mState)
        {
          case kStateInitial:
            if (c == '/')
            {
                // Potentially a comment.
                switch (peekChar())
                {
                  case '/':
                    getChar();  // Eat '/'.
                    mState = kStateLineComment;
                    break;
                  case '*':
                    getChar();  // Eat '*'.
                    mState = kStateBlockComment;
                    break;
                  default:
                    // Not a comment.
                    buf[nread++] = c;
                    break;
                }
            } else
            {
                buf[nread++] = c;
            }
            break;

          case kStateLineComment:
            if (c == '\n')
            {
                buf[nread++] = c;
                mState = kStateInitial;
            }
            break;

          case kStateBlockComment:
            if (c == '*' && (peekChar() == '/'))
            {
                getChar();   // Eat '/'.
                buf[nread++] = ' ';  // Replace comment with whitespace.
                mState = kStateInitial;
            } else if (c == '\n')
            {
                // Line breaks are never skipped.
                buf[nread++] = c;
            }
            break;

          default:
            ASSERT(false);
            break;
        }
    }

    return nread;
}

int Input::getChar()
{
    if (eof()) return EOF;

    const char* str = mString[mIndex];
    int c = str[mSize++];

    // Switch to next string if the current one is fully read.
    int length = stringLength(mIndex);
    // We never read from empty string.
    ASSERT(length != 0);
    if (((length < 0) && (str[mSize] == '\0')) ||
        ((length > 0) && (mSize == length)))
        switchToNextString();

    return c;
}

int Input::peekChar()
{
    // Save the current read position.
    int index = mIndex;
    int size = mSize;
    int c = getChar();

    // Restore read position.
    mIndex = index;
    mSize = size;
    return c;
}

void Input::switchToNextString()
{
    ASSERT(mIndex < mCount);

    mSize = 0;
    do
    {
        ++mIndex;
    } while (!eof() && isStringEmpty(mIndex));
}

bool Input::isStringEmpty(int index)
{
    ASSERT(index < mCount);

    const char* str = mString[mIndex];
    int length = stringLength(mIndex);
    return (length == 0) || ((length < 0) && (str[0] == '\0'));
}

int Input::stringLength(int index)
{
    ASSERT(index < mCount);
    return mLength ? mLength[index] : -1;
}

}  // namespace pp

