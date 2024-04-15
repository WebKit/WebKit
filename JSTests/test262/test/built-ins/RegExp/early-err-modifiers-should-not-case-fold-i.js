// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Code points other than "i", "m", "s" should not be case folded to "i", "m", or "s" (regular expression flags)
esid: sec-patterns-static-semantics-early-errors
features: [regexp-modifiers]
info: |
    Atom :: ( ? RegularExpresisonFlags : Disjunction )
    It is a Syntax Error if the source text matched by RegularExpressionFlags contains any code points other than "i", "m", "s", or if it contains the same code point more than once.

---*/

assert.throws(SyntaxError, function () {
  RegExp("(?I:a)", "i");
}, 'RegExp("(?I:a)", "i"): ');
