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

#ifndef KHTML_EDITING_VISIBLE_UNITS_H
#define KHTML_EDITING_VISIBLE_UNITS_H

#include "text_affinity.h"

namespace khtml {

class VisiblePosition;

enum EWordSide { RightWordIfOnBoundary = false, LeftWordIfOnBoundary = true };
enum EIncludeLineBreak { DoNotIncludeLineBreak = false, IncludeLineBreak = true };

// words
VisiblePosition startOfWord(const VisiblePosition &, EWordSide = RightWordIfOnBoundary);
VisiblePosition endOfWord(const VisiblePosition &, EWordSide = RightWordIfOnBoundary);
VisiblePosition previousWordPosition(const VisiblePosition &);
VisiblePosition nextWordPosition(const VisiblePosition &);

// lines
VisiblePosition startOfLine(const VisiblePosition &, EAffinity);
VisiblePosition endOfLine(const VisiblePosition &, EAffinity, EIncludeLineBreak = DoNotIncludeLineBreak);
VisiblePosition previousLinePosition(const VisiblePosition &, EAffinity, int x);
VisiblePosition nextLinePosition(const VisiblePosition &, EAffinity, int x);

// sentences
VisiblePosition startOfSentence(const VisiblePosition &);
VisiblePosition endOfSentence(const VisiblePosition &, EIncludeLineBreak = DoNotIncludeLineBreak);
VisiblePosition previousSentencePosition(const VisiblePosition &, EAffinity, int x);
VisiblePosition nextSentencePosition(const VisiblePosition &, EAffinity, int x);

// paragraphs
VisiblePosition startOfParagraph(const VisiblePosition &);
VisiblePosition endOfParagraph(const VisiblePosition &, EIncludeLineBreak = DoNotIncludeLineBreak);
VisiblePosition previousParagraphPosition(const VisiblePosition &, EAffinity, int x);
VisiblePosition nextParagraphPosition(const VisiblePosition &, EAffinity, int x);
bool inSameParagraph(const VisiblePosition &, const VisiblePosition &);
bool isStartOfParagraph(const VisiblePosition &);
bool isEndOfParagraph(const VisiblePosition &);

} // namespace DOM

#endif // KHTML_EDITING_VISIBLE_POSITION_H
