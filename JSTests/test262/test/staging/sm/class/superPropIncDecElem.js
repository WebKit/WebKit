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
// #1
function base() { }

base.prototype = {
    test() {
        --super[1];
    }
}

var d = new base();
d.test();

// #2
class test2 {
    test() {
        super[1]++;
    }
}

var d = new test2();
d.test()

