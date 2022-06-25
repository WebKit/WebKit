// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    String.prototype.split (separator, limit) returns an Array object into which substrings of the result of converting this object to a string have
    been stored. If separator is a regular expression then
    inside of SplitMatch helper the [[Match]] method of R is called giving it the arguments corresponding
es5id: 15.5.4.14_A4_T4
description: Arguments are regexp /l/ and 2, and instance is String("hello")
---*/

var __string = new String("hello");

var __re = /l/;

var __split = __string.split(__re, 2);

assert.sameValue(
  __split.constructor,
  Array,
  'The value of __split.constructor is expected to equal the value of Array'
);

assert.sameValue(__split.length, 2, 'The value of __split.length is 2');
assert.sameValue(__split[0], "he", 'The value of __split[0] is "he"');
assert.sameValue(__split[1], "", 'The value of __split[1] is ""');
