// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/

class A {
  #x = 10;
  g() {
    return this.#x;
  }
};

var p = new Proxy(new A, {});
var completed = false;
try {
  p.g();
  completed = true;
} catch (e) {
  assert.sameValue(e instanceof TypeError, true);
}
assert.sameValue(completed, false);


