// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, deepEqual.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
function isNeg(x) {
  if (Object.is(x, -0) || x < 0) {
    return true;
  }
  return false;
}

{
  const a1 = [-Infinity, -2, -1, -0, 0, 1, 2, Infinity];
  const expectedObj = { neg: [-Infinity, -2, -1, -0], pos: [0, 1, 2, Infinity] };
  Object.setPrototypeOf(expectedObj, null);

  const groupedArray = Object.groupBy(a1, x => isNeg(x) ? 'neg' : 'pos');
  const mappedArray = Map.groupBy(a1, x => isNeg(x) ? 'neg' : 'pos');

  assert.sameValue(Object.getPrototypeOf(groupedArray), null)
  assert.deepEqual(groupedArray, expectedObj);
  assert.deepEqual(mappedArray.get("neg"), expectedObj["neg"]);
  assert.deepEqual(mappedArray.get("pos"), expectedObj["pos"]);


  const expectedObj2 = {"undefined": [1,2,3]}
  Object.setPrototypeOf(expectedObj2, null);
  assert.deepEqual(Object.groupBy([1,2,3], () => {}), expectedObj2);
  assert.deepEqual(Object.groupBy([], () => {}), Object.create(null));
  assert.deepEqual((Map.groupBy([1,2,3], () => {})).get(undefined), [1,2,3]);
  assert.sameValue((Map.groupBy([1,2,3], () => {})).size, 1);

  const negMappedArray = Map.groupBy(a1, x => isNeg(x) ? -0 : 0);
  assert.deepEqual(negMappedArray.get(0), a1);
  assert.deepEqual(negMappedArray.size, 1);

  assertThrowsInstanceOf(() => Object.groupBy([], undefined), TypeError);
  assertThrowsInstanceOf(() => Object.groupBy([], null), TypeError);
  assertThrowsInstanceOf(() => Object.groupBy([], 0), TypeError);
  assertThrowsInstanceOf(() => Object.groupBy([], ""), TypeError);
  assertThrowsInstanceOf(() => Map.groupBy([], undefined), TypeError);
  assertThrowsInstanceOf(() => Map.groupBy([], null), TypeError);
  assertThrowsInstanceOf(() => Map.groupBy([], 0), TypeError);
  assertThrowsInstanceOf(() => Map.groupBy([], ""), TypeError);
}

const array = [ 'test' ];
Object.defineProperty(Map.prototype, 4, {
  get() {
    throw new Error('monkey-patched Map get call');
  },
  set(v) {
    throw new Error('monkey-patched Map set call');
  },
  has(v) {
    throw new Error('monkey-patched Map has call');
  }
});

const map1 = Map.groupBy(array, key => key.length);

assert.sameValue('test', map1.get(4)[0])

Object.defineProperty(Array.prototype, '4', {
  set(v) {
    throw new Error('user observable array set');
  },
  get() {
    throw new Error('user observable array get');
  }
});

const map2 = Map.groupBy(array, key => key.length);
const arr = Object.groupBy(array, key => key.length);

assert.sameValue('test', map2.get(4)[0])
assert.sameValue('test', arr[4][0])

Object.defineProperty(Object.prototype, "foo", {
  get() { throw new Error("user observable object get"); },
  set(v) { throw new Error("user observable object set"); }
});
Object.groupBy([1, 2, 3], () => 'foo');

// Ensure property key is correctly accessed
count = 0;
p = Object.groupBy([1], () => ({ toString() { count++; return 10 } }));
assert.sameValue(count, 1);
Map.groupBy([1], () => ({ toString() { count++; return 10 } }));
assert.sameValue(count, 1);

