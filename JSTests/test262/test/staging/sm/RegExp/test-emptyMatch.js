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
var BUGNUMBER = 1322035;
var summary = 'RegExp.prototype.test should update lastIndex to correct position even if pattern starts with .*';

print(BUGNUMBER + ": " + summary);

var regExp = /.*x?/g;
regExp.test('12345');
assert.sameValue(regExp.lastIndex, 5);

regExp = /.*x*/g;
regExp.test('12345');
assert.sameValue(regExp.lastIndex, 5);

regExp = /.*()/g;
regExp.test('12345');
assert.sameValue(regExp.lastIndex, 5);

regExp = /.*(x|)/g;
regExp.test('12345');
assert.sameValue(regExp.lastIndex, 5);

