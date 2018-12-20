// Copyright (C) 2016 Michael Ficarra.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: Assert _NativeFunction_ Syntax
info: |
    This regex makes a best-effort determination that the tested string matches
    the NativeFunction grammar production without requiring a correct tokeniser.

    NativeFunction :
      function _IdentifierName_ opt ( _FormalParameters_ ) { [ native code ] }

---*/
const NATIVE_FUNCTION_RE = /\bfunction\b[\s\S]*\([\s\S]*\)[\s\S]*\{[\s\S]*\[[\s\S]*\bnative\b[\s\S]+\bcode\b[\s\S]*\][\s\S]*\}/;

const assertToStringOrNativeFunction = function(fn, expected) {
  const actual = "" + fn;
  try {
    assert.sameValue(actual, expected);
  } catch (unused) {
    assertNativeFunction(fn, expected);
  }
};

const assertNativeFunction = function(fn, special) {
  const actual = "" + fn;
  assert(
    NATIVE_FUNCTION_RE.test(actual),
    "Conforms to NativeFunction Syntax: '" + actual + "'." + (special ? "(" + special + ")" : "")
  );
};
