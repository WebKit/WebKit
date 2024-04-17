// Copyright 2023 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
author: Ron Buckton
description: >
  Adding ignoreCase (`i`) modifier in group affects character classes in group.
info: |
  Runtime Semantics: CompileAtom
  The syntax-directed operation CompileAtom takes arguments direction (forward or backward) and modifiers (a Modifiers Record) and returns a Matcher.

  Atom :: `(` `?` RegularExpressionFlags `:` Disjunction `)`
    1. Let addModifiers be the source text matched by RegularExpressionFlags.
    2. Let removeModifiers be the empty String.
    3. Let newModifiers be UpdateModifiers(modifiers, CodePointsToString(addModifiers), removeModifiers).
    4. Return CompileSubpattern of Disjunction with arguments direction and newModifiers.

  Atom :: `(` `?` RegularExpressionFlags `-` RegularExpressionFlags `:` Disjunction `)`
    1. Let addModifiers be the source text matched by the first RegularExpressionFlags.
    2. Let removeModifiers be the source text matched by the second RegularExpressionFlags.
    3. Let newModifiers be UpdateModifiers(modifiers, CodePointsToString(addModifiers), CodePointsToString(removeModifiers)).
    4. Return CompileSubpattern of Disjunction with arguments direction and newModifiers.

  UpdateModifiers ( modifiers, add, remove )
  The abstract operation UpdateModifiers takes arguments modifiers (a Modifiers Record), add (a String), and remove (a String) and returns a Modifiers. It performs the following steps when called:

  1. Let dotAll be modifiers.[[DotAll]].
  2. Let ignoreCase be modifiers.[[IgnoreCase]].
  3. Let multiline be modifiers.[[Multiline]].
  4. If add contains "s", set dotAll to true.
  5. If add contains "i", set ignoreCase to true.
  6. If add contains "m", set multiline to true.
  7. If remove contains "s", set dotAll to false.
  8. If remove contains "i", set ignoreCase to false.
  9. If remove contains "m", set multiline to false.
  10. Return the Modifiers Record { [[DotAll]]: dotAll, [[IgnoreCase]]: ignoreCase, [[Multiline]]: multiline }.

esid: sec-compileatom
features: [regexp-modifiers]
---*/

var re1 = /(?i:[ab])c/;
assert(re1.test("ac"), "[ab] should match a");
assert(re1.test("Ac"), "[ab] should match A");
assert(re1.test("bc"), "[ab] should match b");
assert(re1.test("Bc"), "[ab] should match B");

var re2 = /(?i-:[ab])c/;
assert(re2.test("ac"), "[ab] should match a");
assert(re2.test("Ac"), "[ab] should match A");
assert(re2.test("bc"), "[ab] should match b");
assert(re2.test("Bc"), "[ab] should match B");

var re3 = /(?i:[^ab])c/;
assert(!re3.test("ac"), "[^ab] should not match a");
assert(!re3.test("Ac"), "[^ab] should not match A");
assert(!re3.test("bc"), "[^ab] should not match b");
assert(!re3.test("Bc"), "[^ab] should not match B");

var re4 = /(?i-:[^ab])c/;
assert(!re4.test("ac"), "[^ab] should not match a");
assert(!re4.test("Ac"), "[^ab] should not match A");
assert(!re4.test("bc"), "[^ab] should not match b");
assert(!re4.test("Bc"), "[^ab] should not match B");
