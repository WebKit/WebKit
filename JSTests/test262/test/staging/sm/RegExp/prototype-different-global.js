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
function test(otherGlobal) {
    var otherRegExp = otherGlobal.RegExp;

    for (let name of ["global", "ignoreCase", "multiline", "sticky", "unicode", "source"]) {
        let getter = Object.getOwnPropertyDescriptor(RegExp.prototype, name).get;
        assert.sameValue(typeof getter, "function");

        // Note: TypeError gets reported from wrong global if cross-compartment,
        // so we test both cases.
        let ex;
        try {
            getter.call(otherRegExp.prototype);
        } catch (e) {
            ex = e;
        }
        assert.sameValue(ex instanceof TypeError || ex instanceof otherGlobal.TypeError, true);
    }

    let flagsGetter = Object.getOwnPropertyDescriptor(RegExp.prototype, "flags").get;
    assert.sameValue(flagsGetter.call(otherRegExp.prototype), "");

    assert.sameValue(RegExp.prototype.toString.call(otherRegExp.prototype), "/(?:)/");
}
test(createNewGlobal());
test(createNewGlobal({newCompartment: true}));

