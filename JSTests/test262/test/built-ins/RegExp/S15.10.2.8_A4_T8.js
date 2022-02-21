// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The production Atom :: . evaluates as follows:
    i) Let A be the set of all characters except the four line terminator characters <LF>, <CR>, <LS>, or <PS>
    ii) Call CharacterSetMatcher(A, false) and return its Matcher result
es5id: 15.10.2.8_A4_T8
description: Execute /.+/.exec("`1234567890-=~!@#$%^&*()_+") and check results
---*/

var __string = "`1234567890-=~!@#$%^&*()_+";
var __executed = /.+/.exec(__string);

var __expected = ["`1234567890-=~!@#$%^&*()_+"];
__expected.index = 0;
__expected.input = __string;

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
