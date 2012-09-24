/*
 * Copyright 2007 Google Inc. All rights reserved.
 * Copyright 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Canonicalization functions for the paths of URLs.

#include "config.h"
#include "URLCanon.h"

#include "URLCanonInternal.h"
#include "URLParseInternal.h"

#if USE(WTFURL)

namespace WTF {

namespace URLCanonicalizer {

namespace {

enum CharacterFlags {
    // Pass through unchanged, whether escaped or unescaped. This doesn't
    // actually set anything so you can't OR it to check, it's just to make the
    // table below more clear when neither ESCAPE or UNESCAPE is set.
    PASS = 0,

    // This character requires special handling in doPartialPath. Doing this test
    // first allows us to filter out the common cases of regular characters that
    // can be directly copied.
    SPECIAL = 1,

    // This character must be escaped in the canonical output. Note that all
    // escaped chars also have the "special" bit set so that the code that looks
    // for this is triggered. Not valid with PASS or ESCAPE
    ESCAPE_BIT = 2,
    ESCAPE = ESCAPE_BIT | SPECIAL,

    // This character must be unescaped in canonical output. Not valid with
    // ESCAPE or PASS. We DON'T set the SPECIAL flag since if we encounter these
    // characters unescaped, they should just be copied.
    UNESCAPE = 4,

    // This character is disallowed in URLs. Note that the "special" bit is also
    // set to trigger handling.
    INVALID_BIT = 8,
    INVALID = INVALID_BIT | SPECIAL,
};

// This table contains one of the above flag values. Note some flags are more
// than one bits because they also turn on the "special" flag. Special is the
// only flag that may be combined with others.
//
// This table is designed to match exactly what IE does with the characters.
//
// Dot is even more special, and the escaped version is handled specially by
// isDot. Therefore, we don't need the "escape" flag, and even the "unescape"
// bit is never handled (we just need the "special") bit.
const unsigned char kPathCharLookup[0x100] = {
//   null     control chars...
     INVALID,  ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,
//   control chars...
     ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,
//   ' '       !         "         #         $         %         &         '         (         )         *         +         ,         -         .         /
     ESCAPE,   PASS,     ESCAPE,   ESCAPE,   PASS,     ESCAPE,   PASS,     PASS,     PASS,     PASS,     PASS,     PASS,     PASS,     UNESCAPE, SPECIAL,  PASS,
//   0         1         2         3         4         5         6         7         8         9         :         ;         <         =          >        ?
     UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, PASS,     PASS,     ESCAPE,   PASS,     ESCAPE,   ESCAPE,
//   @         A         B         C         D         E         F         G         H         I         J         K         L         M         N         O
     PASS,     UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE,
//   P         Q         R         S         T         U         V         W         X         Y         Z         [         \         ]         ^         _
     UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, PASS,     ESCAPE,   PASS,     ESCAPE,   UNESCAPE,
//   `         a         b         c         d         e         f         g         h         i         j         k         l         m         n         o
     ESCAPE,   UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE,
//   p         q         r         s         t         u         v         w         x         y         z         {         |         }         ~         <NBSP>
     UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, UNESCAPE, ESCAPE,   ESCAPE,   ESCAPE,   UNESCAPE, ESCAPE,
//   ...all the high-bit characters are escaped
     ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,
     ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,
     ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,
     ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,
     ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,
     ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,
     ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,
     ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE,   ESCAPE};

enum DotDisposition {
    // The given dot is just part of a filename and is not special.
    NOT_A_DIRECTORY,

    // The given dot is the current directory.
    DIRECTORY_CUR,

    // The given dot is the first of a double dot that should take us up one.
    DIRECTORY_UP
};

// When the path resolver finds a dot, this function is called with the
// character following that dot to see what it is. The return value
// indicates what type this dot is (see above). This code handles the case
// where the dot is at the end of the input.
//
// |*consumedLength| will contain the number of characters in the input that
// express what we found.
//
// If the input is "../foo", |afterDot| = 1, |end| = 6, and
// at the end, |*consumedLength| = 2 for the "./" this function consumed. The
// original dot length should be handled by the caller.
template<typename CHAR>
DotDisposition ClassifyAfterDot(const CHAR* spec, int afterDot, int end, int* consumedLength)
{
    if (afterDot == end) {
        // Single dot at the end.
        *consumedLength = 0;
        return DIRECTORY_CUR;
    }
    if (URLParser::isURLSlash(spec[afterDot])) {
        // Single dot followed by a slash.
        *consumedLength = 1; // Consume the slash
        return DIRECTORY_CUR;
    }

    int secondDotLength = isDot(spec, afterDot, end);
    if (secondDotLength) {
        int afterSecondDot = afterDot + secondDotLength;
        if (afterSecondDot == end) {
            // Double dot at the end.
            *consumedLength = secondDotLength;
            return DIRECTORY_UP;
        }
        if (URLParser::isURLSlash(spec[afterSecondDot])) {
            // Double dot followed by a slash.
            *consumedLength = secondDotLength + 1;
            return DIRECTORY_UP;
        }
    }

    // The dots are followed by something else, not a directory.
    *consumedLength = 0;
    return NOT_A_DIRECTORY;
}

// Rewinds the output to the previous slash. It is assumed that the output
// ends with a slash and this doesn't count (we call this when we are
// appending directory paths, so the previous path component has and ending
// slash).
//
// This will stop at the first slash (assumed to be at position
// |pathBeginInOutput| and not go any higher than that. Some web pages
// do ".." too many times, so we need to handle that brokenness.
//
// It searches for a literal slash rather than including a backslash as well
// because it is run only on the canonical output.
//
// The output is guaranteed to end in a slash when this function completes.
void BackUpToPreviousSlash(int pathBeginInOutput, URLBuffer<char>& output)
{
    ASSERT(output.length() > 0);

    int i = output.length() - 1;
    ASSERT(output.at(i) == '/');
    if (i == pathBeginInOutput)
        return; // We're at the first slash, nothing to do.

    // Now back up (skipping the trailing slash) until we find another slash.
    i--;
    while (output.at(i) != '/' && i > pathBeginInOutput)
        i--;

    // Now shrink the output to just include that last slash we found.
    output.setLength(i + 1);
}

// Appends the given path to the output. It assumes that if the input path
// starts with a slash, it should be copied to the output. If no path has
// already been appended to the output (the case when not resolving
// relative URLs), the path should begin with a slash.
//
// If there are already path components (this mode is used when appending
// relative paths for resolving), it assumes that the output already has
// a trailing slash and that if the input begins with a slash, it should be
// copied to the output.
//
// We do not collapse multiple slashes in a row to a single slash. It seems
// no web browsers do this, and we don't want incompababilities, even though
// it would be correct for most systems.
template<typename CharacterType, typename UCHAR>
bool doPartialPath(const CharacterType* spec, const URLComponent& path, int pathBeginInOutput, URLBuffer<char>& output)
{
    int end = path.end();

    bool success = true;
    for (int i = path.begin(); i < end; ++i) {
        UCHAR uch = static_cast<UCHAR>(spec[i]);
        if (sizeof(CharacterType) > sizeof(char) && uch >= 0x80) {
            // We only need to test wide input for having non-ASCII characters. For
            // narrow input, we'll always just use the lookup table. We don't try to
            // do anything tricky with decoding/validating UTF-8. This function will
            // read one or two UTF-16 characters and append the output as UTF-8. This
            // call will be removed in 8-bit mode.
            success &= AppendUTF8EscapedChar(spec, &i, end, output);
        } else {
            // Normal ASCII character or 8-bit input, use the lookup table.
            unsigned char outCh = static_cast<unsigned char>(uch);
            unsigned char flags = kPathCharLookup[outCh];
            if (flags & SPECIAL) {
                // Needs special handling of some sort.
                int dotlen;
                if ((dotlen = isDot(spec, i, end)) > 0) {
                    // See if this dot was preceeded by a slash in the output. We
                    // assume that when canonicalizing paths, they will always
                    // start with a slash and not a dot, so we don't have to
                    // bounds check the output.
                    //
                    // Note that we check this in the case of dots so we don't have to
                    // special case slashes. Since slashes are much more common than
                    // dots, this actually increases performance measurably (though
                    // slightly).
                    ASSERT(output.length() > pathBeginInOutput);
                    if (output.length() > pathBeginInOutput && output.at(output.length() - 1) == '/') {
                        // Slash followed by a dot, check to see if this is means relative
                        int consumedLength;
                        switch (ClassifyAfterDot<CharacterType>(spec, i + dotlen, end, &consumedLength)) {
                        case NOT_A_DIRECTORY:
                            // Copy the dot to the output, it means nothing special.
                            output.append('.');
                            i += dotlen - 1;
                            break;
                        case DIRECTORY_CUR: // Current directory, just skip the input.
                            i += dotlen + consumedLength - 1;
                            break;
                        case DIRECTORY_UP:
                            BackUpToPreviousSlash(pathBeginInOutput, output);
                            i += dotlen + consumedLength - 1;
                            break;
                        }
                    } else {
                        // This dot is not preceeded by a slash, it is just part of some
                        // file name.
                        output.append('.');
                        i += dotlen - 1;
                    }
                } else if (outCh == '\\') {
                    // Convert backslashes to forward slashes
                    output.append('/');
                } else if (outCh == '%') {
                    // Handle escape sequences.
                    unsigned char unescapedValue;
                    if (DecodeEscaped(spec, &i, end, &unescapedValue)) {
                        // Valid escape sequence, see if we keep, reject, or unescape it.
                        char unescapedFlags = kPathCharLookup[unescapedValue];

                        if (unescapedFlags & UNESCAPE) {
                            // This escaped value shouldn't be escaped, copy it.
                            output.append(unescapedValue);
                        } else if (unescapedFlags & INVALID_BIT) {
                            // Invalid escaped character, copy it and remember the error.
                            output.append('%');
                            output.append(static_cast<char>(spec[i - 1]));
                            output.append(static_cast<char>(spec[i]));
                            success = false;
                        } else {
                            // Valid escaped character but we should keep it escaped. We
                            // don't want to change the case of any hex letters in case
                            // the server is sensitive to that, so we just copy the two
                            // characters without checking (DecodeEscape will have advanced
                            // to the last character of the pair).
                            output.append('%');
                            output.append(static_cast<char>(spec[i - 1]));
                            output.append(static_cast<char>(spec[i]));
                        }
                    } else {
                        // Invalid escape sequence. IE7 rejects any URLs with such
                        // sequences, while Firefox, IE6, and Safari all pass it through
                        // unchanged. We are more permissive unlike IE7. I don't think this
                        // can cause significant problems, if it does, we should change
                        // to be more like IE7.
                        output.append('%');
                    }

                } else if (flags & INVALID_BIT) {
                    // For NULLs, etc. fail.
                    appendURLEscapedCharacter(outCh, output);
                    success = false;

                } else if (flags & ESCAPE_BIT) {
                    // This character should be escaped.
                    appendURLEscapedCharacter(outCh, output);
                }
            } else {
                // Nothing special about this character, just append it.
                output.append(outCh);
            }
        }
    }
    return success;
}

template<typename CharacterType, typename UCHAR>
bool doPath(const CharacterType* spec, const URLComponent& path, URLBuffer<char>& output, URLComponent& outputPath)
{
    bool success = true;
    outputPath.setBegin(output.length());
    if (path.length() > 0) {
        // Write out an initial slash if the input has none. If we just parse a URL
        // and then canonicalize it, it will of course have a slash already. This
        // check is for the replacement and relative URL resolving cases of file
        // URLs.
        if (!URLParser::isURLSlash(spec[path.begin()]))
            output.append('/');

        success = doPartialPath<CharacterType, UCHAR>(spec, path, outputPath.begin(), output);
    } else {
        // No input, canonical path is a slash.
        output.append('/');
    }
    outputPath.setLength(output.length() - outputPath.begin());
    return success;
}

} // namespace

bool CanonicalizePath(const char* spec, const URLComponent& path, URLBuffer<char>& output, URLComponent* outputPath)
{
    return doPath<char, unsigned char>(spec, path, output, *outputPath);
}

bool CanonicalizePath(const UChar* spec, const URLComponent& path, URLBuffer<char>& output, URLComponent* outputPath)
{
    return doPath<UChar, UChar>(spec, path, output, *outputPath);
}

bool CanonicalizePartialPath(const char* spec, const URLComponent& path, int pathBeginInOutput, URLBuffer<char>& output)
{
    return doPartialPath<char, unsigned char>(spec, path, pathBeginInOutput, output);
}

bool CanonicalizePartialPath(const UChar* spec, const URLComponent& path, int pathBeginInOutput, URLBuffer<char>& output)
{
    return doPartialPath<UChar, UChar>(spec, path, pathBeginInOutput, output);
}

} // namespace URLCanonicalizer

} // namespace WTF

#endif // USE(WTFURL)
