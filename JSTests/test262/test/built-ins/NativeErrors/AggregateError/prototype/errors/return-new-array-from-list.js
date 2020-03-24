// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-get-aggregate-error.prototype.errors
description: >
  Return a new array from the [[AggregateErrors]] list.
info: |
  get AggregateError.prototype.errors

  1. Let E be the this value.
  ...
  5. Return ! CreateArrayFromList(E.[[AggregateErrors]]).
includes: [compareArray.js]
features: [AggregateError, Symbol]
---*/

var errors = [];
var case1 = new AggregateError(errors);
var case1get1 = case1.errors;
var case1get2 = case1.errors;

assert.sameValue(Array.isArray(case1get1), true);
assert.sameValue(Array.isArray(case1get2), true);

assert.notSameValue(case1get1, errors, 'case1 - creates a new array #1');
assert.notSameValue(case1get2, errors, 'case1 - creates a new array #2');
assert.notSameValue(case1get1, case1get2, 'creates a new array everytime it gets the values');

assert.compareArray(case1get1, errors);
assert.compareArray(case1get2, errors);

/////

errors = [undefined, null, 1, 0, '', {}, Symbol()];
var case2 = new AggregateError(errors);
var case2get1 = case2.errors;
var case2get2 = case2.errors;

assert.sameValue(Array.isArray(case2get1), true);
assert.sameValue(Array.isArray(case2get2), true);

assert.notSameValue(case2get1, errors, 'case2 - creates a new array #1');
assert.notSameValue(case2get2, errors, 'case2 - creates a new array #2');
assert.notSameValue(case2get1, case2get2, 'creates a new array everytime it gets the values');

assert.compareArray(case2get1, errors);
assert.compareArray(case2get2, errors);

/////

errors = [undefined,,,,undefined];
var case3 = new AggregateError(errors);
var case3get1 = case3.errors;
var case3get2 = case3.errors;

assert.sameValue(Array.isArray(case3get1), true);
assert.sameValue(Array.isArray(case3get2), true);

assert.notSameValue(case3get1, errors, 'case3 - creates a new array #1');
assert.notSameValue(case3get2, errors, 'case3 - creates a new array #2');
assert.notSameValue(case3get1, case3get2, 'creates a new array everytime it gets the values');

assert.compareArray(case3get1, errors);
assert.compareArray(case3get2, errors);

assert(Object.prototype.hasOwnProperty.call(case3get1, 0), 'filled array from a sparse origin - case3get1, 0');
assert(Object.prototype.hasOwnProperty.call(case3get1, 1), 'filled array from a sparse origin - case3get1, 1');
assert(Object.prototype.hasOwnProperty.call(case3get1, 2), 'filled array from a sparse origin - case3get1, 2');
assert(Object.prototype.hasOwnProperty.call(case3get1, 3), 'filled array from a sparse origin - case3get1, 3');
assert(Object.prototype.hasOwnProperty.call(case3get1, 4), 'filled array from a sparse origin - case3get1, 4');

assert(Object.prototype.hasOwnProperty.call(case3get2, 0), 'filled array from a sparse origin - case3get2, 0');
assert(Object.prototype.hasOwnProperty.call(case3get2, 1), 'filled array from a sparse origin - case3get2, 1');
assert(Object.prototype.hasOwnProperty.call(case3get2, 2), 'filled array from a sparse origin - case3get2, 2');
assert(Object.prototype.hasOwnProperty.call(case3get2, 3), 'filled array from a sparse origin - case3get2, 3');
assert(Object.prototype.hasOwnProperty.call(case3get2, 4), 'filled array from a sparse origin - case3get2, 4');
