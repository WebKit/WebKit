// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-RegExp-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 *
 * Author: Geoffrey Sneddon <geoffers+mozilla@gmail.com>
 */

var BUGNUMBER = 646490;
var summary =
  "RegExp.prototype.exec doesn't get the lastIndex and ToInteger() it for " +
  "non-global regular expressions when it should";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var re = /./, called = 0;
re.lastIndex = {valueOf: function() { called++; return 0; }};
re.exec(".");
re.lastIndex = {toString: function() { called++; return "0"; }};
re.exec(".");
re.lastIndex = {
  valueOf: function() { called++; return 0; },
  toString: function() { called--; }
};
re.exec(".");
assert.sameValue(called, 3, "FAIL, got " + called);

/******************************************************************************/

print("All tests passed!");
