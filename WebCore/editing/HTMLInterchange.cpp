/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "HTMLInterchange.h"

#include "Document.h"
#include "TextIterator.h"

using WebCore::isCollapsibleWhitespace;

namespace {

DeprecatedString convertedSpaceString() 
{
    static DeprecatedString convertedSpaceString;
    if (convertedSpaceString.length() == 0) {
        convertedSpaceString = "<span class=\"";
        convertedSpaceString += AppleConvertedSpace;
        convertedSpaceString += "\">";
        convertedSpaceString += QChar(0xa0);
        convertedSpaceString += "</span>";
    }
    return convertedSpaceString;
}

} // end anonymous namespace

// FIXME: convertHTMLTextToInterchangeFormat should probably be in the khtml namespace.
// FIXME: Can't really do this work without taking whitespace mode into account.
// This means that eventually this function needs to be eliminated or at least have
// its parameters changed because it can't do its work on the string without knowing
// what parts are in what whitespace mode.
DeprecatedString convertHTMLTextToInterchangeFormat(const DeprecatedString &in)
{
    DeprecatedString s;

    unsigned int i = 0;
    unsigned int consumed = 0;
    while (i < in.length()) {
        consumed = 1;
        if (isCollapsibleWhitespace(in[i])) {
            // count number of adjoining spaces
            unsigned int j = i + 1;
            while (j < in.length() && isCollapsibleWhitespace(in[j]))
                j++;
            unsigned int count = j - i;
            consumed = count;
            while (count) {
                unsigned int add = count % 3;
                switch (add) {
                    case 0:
                        s += convertedSpaceString();
                        s += ' ';
                        s += convertedSpaceString();
                        add = 3;
                        break;
                    case 1:
                        if (i == 0 || i + 1 == in.length()) // at start or end of string
                            s += convertedSpaceString();
                        else
                            s += ' ';
                        break;
                    case 2:
                        if (i == 0) {
                             // at start of string
                            s += convertedSpaceString();
                            s += ' ';
                        }
                        else if (i + 2 == in.length()) {
                             // at end of string
                            s += convertedSpaceString();
                            s += convertedSpaceString();
                        }
                        else {
                            s += convertedSpaceString();
                            s += ' ';
                        }
                        break;
                }
                count -= add;
            }
        }
        else {
            s += in[i];
        }
        i += consumed;
    }

    return s;
}
