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
// Super property accesses should play nice with the pretty printer.
class testNonExistent {
    constructor() {
        super["prop"]();
    }
}
// Should fold to super.prop
assertThrownErrorContains(() => new testNonExistent(), 'super.prop');

var ol = { testNonExistent() { super.prop(); } };
assertThrownErrorContains(() => ol.testNonExistent(), "super.prop");

var olElem = { testNonExistent() { var prop = "prop"; super[prop](); } };
assertThrownErrorContains(() => olElem.testNonExistent(), "super[prop]");

