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
var re = /(z\1){3}/;
var str = 'zzz';
var actual = re.exec(str);
var expected = makeExpectedMatch(['zzz', 'z'], 0, str);
checkRegExpMatch(actual, expected);

