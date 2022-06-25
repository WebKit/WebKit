// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-pattern-semantics
es6id: 21.2.2
description: >
    NonEmptyClassRangesNoDash production cannot specify a multi-character set
    ("A" parameter)
info: |
    The production
    NonemptyClassRangesNoDash::ClassAtomNoDash-ClassAtomClassRanges evaluates
    as follows:

    1. Evaluate ClassAtomNoDash to obtain a CharSet A.
    2. Evaluate ClassAtom to obtain a CharSet B.
    3. Evaluate ClassRanges to obtain a CharSet C.
    4. Call CharacterRange(A, B) and let D be the resulting CharSet.

    21.2.2.15.1 Runtime Semantics: CharacterRange Abstract Operation

    1. If A does not contain exactly one character or B does not contain
       exactly one character, throw a SyntaxError exception.

    The `u` flag precludes the Annex B extension that enables this pattern.
negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

/[\d-a]/u;
