// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  Eagerly throw TypeError when `mapper` is not callable.
features:
  - iterator-helpers
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
---*/
//

assertThrowsInstanceOf(() => Iterator.prototype.map(undefined), TypeError);
assertThrowsInstanceOf(() => [].values().map(undefined), TypeError);

assertThrowsInstanceOf(() => Iterator.prototype.map(null), TypeError);
assertThrowsInstanceOf(() => [].values().map(null), TypeError);

assertThrowsInstanceOf(() => Iterator.prototype.map(0), TypeError);
assertThrowsInstanceOf(() => [].values().map(0), TypeError);

assertThrowsInstanceOf(() => Iterator.prototype.map(false), TypeError);
assertThrowsInstanceOf(() => [].values().map(false), TypeError);

assertThrowsInstanceOf(() => Iterator.prototype.map({}), TypeError);
assertThrowsInstanceOf(() => [].values().map({}), TypeError);

assertThrowsInstanceOf(() => Iterator.prototype.map(''), TypeError);
assertThrowsInstanceOf(() => [].values().map(''), TypeError);

assertThrowsInstanceOf(() => Iterator.prototype.map(Symbol('')), TypeError);
assertThrowsInstanceOf(() => [].values().map(Symbol('')), TypeError);

