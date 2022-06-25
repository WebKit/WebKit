// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    String.prototype.split (separator, limit) returns an Array object into which substrings of the result of converting this object to a string have
    been stored. If separator is a regular expression then
    inside of SplitMatch helper the [[Match]] method of R is called giving it the arguments corresponding
es5id: 15.5.4.14_A4_T21
description: Argument is regexp /\X/, and instance is String("a b c de f")
---*/

var __string = new String("a b c de f");

var __re = /X/;

var __split = __string.split(__re);

var __expected = ["a b c de f"];

assert.sameValue(
  __split.constructor,
  Array,
  'The value of __split.constructor is expected to equal the value of Array'
);

assert.sameValue(
  __split.length,
  __expected.length,
  'The value of __split.length is expected to equal the value of __expected.length'
);

assert.sameValue(__split[0], __expected[0], 'The value of __split[0] is expected to equal the value of __expected[0]');
