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
class testForIn {
    constructor() {
        let hits = 0;
        for (super.prop in { prop1: 1, prop2: 2 })
            hits++;
        assert.sameValue(this.prop, "prop2");
        assert.sameValue(hits, 2);
    }
}

new testForIn();


({
    testForOf() {
        let hits = 0;
        for (super["prop"] of [1, 2])
            hits++;
        assert.sameValue(this.prop, 2);
        assert.sameValue(hits, 2);
    }
}).testForOf();

