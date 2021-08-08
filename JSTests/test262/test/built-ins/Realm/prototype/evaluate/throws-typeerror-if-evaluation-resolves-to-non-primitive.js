// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm.prototype.evaluate
description: >
  Realm.prototype.evaluate throws a TypeError if evaluate resolves to non-primitive values
features: [callable-boundary-realms]
---*/

assert.sameValue(
  typeof Realm.prototype.evaluate,
  'function',
  'This test must fail if Realm.prototype.evaluate is not a function'
);

const r = new Realm();

assert.throws(TypeError, () => r.evaluate('globalThis'), 'globalThis');
assert.throws(TypeError, () => r.evaluate('[]'), 'array literal');
assert.throws(TypeError, () => r.evaluate(`
    ({
        [Symbol.toPrimitive]() { return 'string'; },
        toString() { return 'str'; },
        valueOf() { return 1; }
    });
`), 'object literal with immediate primitive coercion methods');
assert.throws(TypeError, () => r.evaluate('Object.create(null)'), 'ordinary object with null __proto__');
