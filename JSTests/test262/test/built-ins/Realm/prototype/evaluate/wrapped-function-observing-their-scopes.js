// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm.prototype.evaluate
description: >
  Realm.prototype.evaluate wrapped function observing their scopes
features: [callable-boundary-realms]
---*/

assert.sameValue(
  typeof Realm.prototype.evaluate,
  'function',
  'This test must fail if Realm.prototype.evaluate is not a function'
);

const r = new Realm();
let myValue;

function blueFn(x) {
    myValue = x;
    return myValue;
}

// cb is a new function in the red Realm that chains the call to the blueFn
const redFunction = r.evaluate(`
    var myValue = 'red';
    0, function(cb) {
        cb(42);
        return myValue;
    };
`);

assert.sameValue(redFunction(blueFn), 'red');
assert.sameValue(myValue, 42);
