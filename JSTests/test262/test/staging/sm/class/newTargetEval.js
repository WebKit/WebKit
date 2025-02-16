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
// Eval of new.target is invalid outside functions.
try {
    eval('new.target');
    assert.sameValue(false, true);
} catch (e) {
    if (!(e instanceof SyntaxError))
        throw e;
}

// new.target is invalid inside eval inside top-level arrow functions
assertThrowsInstanceOf(() => eval('new.target'), SyntaxError);

// new.target is invalid inside indirect eval.
let ieval = eval;
try {
    (function () { return ieval('new.target'); })();
    assert.sameValue(false, true);
} catch (e) {
    if (!(e instanceof SyntaxError))
        throw e;
}

function assertNewTarget(expected) {
    assert.sameValue(eval('new.target'), expected);
    assert.sameValue((()=>eval('new.target'))(), expected);

    // Also test nestings "by induction"
    assert.sameValue(eval('eval("new.target")'), expected);
    assert.sameValue(eval("eval('eval(`new.target`)')"), expected);
}

const ITERATIONS = 550;
for (let i = 0; i < ITERATIONS; i++)
    assertNewTarget(undefined);

for (let i = 0; i < ITERATIONS; i++)
    new assertNewTarget(assertNewTarget);

