/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DictionaryLookup_h
#define DictionaryLookup_h

#if PLATFORM(MAC)

#include <wtf/PassRefPtr.h>

@class NSDictionary;

namespace WebCore {

class HitTestResult;
class Range;
class VisiblePosition;
class VisibleSelection;

// FIXME: Some of these functions should probably be in a more generic class.
// https://bugs.webkit.org/show_bug.cgi?id=138567
bool isPositionInRange(const VisiblePosition&, Range*);
bool shouldUseSelection(const VisiblePosition&, const VisibleSelection&);

PassRefPtr<Range> rangeExpandedAroundPositionByCharacters(const VisiblePosition&, int numberOfCharactersToExpand);
PassRefPtr<Range> rangeForDictionaryLookupForSelection(const VisibleSelection&, NSDictionary **options);
PassRefPtr<Range> rangeForDictionaryLookupAtHitTestResult(const HitTestResult&, NSDictionary **options);

} // namespace WebCore

#endif // PLATFORM(MAC)

#endif // DictionaryLookup_h
