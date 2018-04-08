// Copyright (C) 2017 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    This regex makes a best-effort determination that the tested string matches
    the NativeFunction grammar production without requiring a correct tokeniser.
---*/
const NATIVE_FUNCTION_RE = /\bfunction\b[\s\S]*\([\s\S]*\)[\s\S]*\{[\s\S]*\[[\s\S]*\bnative\b[\s\S]+\bcode\b[\s\S]*\][\s\S]*\}/;

const assertToStringOrNativeFunction = function(fn, expected) {
  const actual = "" + fn;
  try {
    assert.sameValue(actual, expected);
  } catch (unused) {
    assertNativeFunction(fn);
  }
};

const assertNativeFunction = function(fn) {
  const actual = "" + fn;
  assert(NATIVE_FUNCTION_RE.test(actual), "looks pretty much like a NativeFunction");
};
