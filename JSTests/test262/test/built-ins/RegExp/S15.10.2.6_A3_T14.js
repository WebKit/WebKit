// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The production Assertion :: \b evaluates by returning an internal
    AssertionTester closure that takes a State argument x and performs the ...
es5id: 15.10.2.6_A3_T14
description: >
    Execute /e\b/.exec("pilot\nsoviet robot\topenoffic\u0065") and
    check results
---*/

var __executed = /e\b/.exec("pilot\nsoviet robot\topenoffic\u0065");

var __expected = ["e"];
__expected.index = 28;
__expected.input = "pilot\nsoviet robot\topenoffice";

assert.sameValue(
  __executed.length,
  __expected.length,
  'The value of __executed.length is expected to equal the value of __expected.length'
);

assert.sameValue(
  __executed.index,
  __expected.index,
  'The value of __executed.index is expected to equal the value of __expected.index'
);

assert.sameValue(
  __executed.input,
  __expected.input,
  'The value of __executed.input is expected to equal the value of __expected.input'
);

for(var index=0; index<__expected.length; index++) {
  assert.sameValue(
    __executed[index],
    __expected[index],
    'The value of __executed[index] is expected to equal the value of __expected[index]'
  );
}
