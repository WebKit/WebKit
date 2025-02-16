// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-TypedArray-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
const EMPTY = 0;
const INLINE_STORAGE = 10;
const NON_INLINE_STORAGE = 1024;

// Empty typed arrays can be sealed.
{
    let ta = new Int32Array(EMPTY);
    Object.seal(ta);

    assert.sameValue(Object.isExtensible(ta), false);
    assert.sameValue(Object.isSealed(ta), true);
    assert.sameValue(Object.isFrozen(ta), true);
}

// Non-empty typed arrays cannot be sealed.
for (let length of [INLINE_STORAGE, NON_INLINE_STORAGE]) {
    let ta = new Int32Array(length);
    assertThrowsInstanceOf(() => Object.seal(ta), TypeError);

    assert.sameValue(Object.isExtensible(ta), false);
    assert.sameValue(Object.isSealed(ta), false);
    assert.sameValue(Object.isFrozen(ta), false);
}

// Empty typed arrays can be frozen.
{
    let ta = new Int32Array(EMPTY);
    Object.freeze(ta);

    assert.sameValue(Object.isExtensible(ta), false);
    assert.sameValue(Object.isSealed(ta), true);
    assert.sameValue(Object.isFrozen(ta), true);
}

// Non-empty typed arrays cannot be frozen.
for (let length of [INLINE_STORAGE, NON_INLINE_STORAGE]) {
    let ta = new Int32Array(length);
    assertThrowsInstanceOf(() => Object.freeze(ta), TypeError);

    assert.sameValue(Object.isExtensible(ta), false);
    assert.sameValue(Object.isSealed(ta), false);
    assert.sameValue(Object.isFrozen(ta), false);
}

// Non-extensible empty typed arrays are sealed and frozen.
{
    let ta = new Int32Array(EMPTY);
    Object.preventExtensions(ta);

    assert.sameValue(Object.isExtensible(ta), false);
    assert.sameValue(Object.isSealed(ta), true);
    assert.sameValue(Object.isFrozen(ta), true);
}

// Non-extensible non-empty typed arrays are neither sealed nor frozen.
for (let length of [INLINE_STORAGE, NON_INLINE_STORAGE]) {
    let ta = new Int32Array(length);
    Object.preventExtensions(ta);

    assert.sameValue(Object.isExtensible(ta), false);
    assert.sameValue(Object.isSealed(ta), false);
    assert.sameValue(Object.isFrozen(ta), false);
}


