// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-get-aggregate-error.prototype.errors
description: >
  Return a new array from the [[AggregateErrors]] list, after an iterable errors arg.
info: |
  get AggregateError.prototype.errors

  1. Let E be the this value.
  ...
  5. Return ! CreateArrayFromList(E.[[AggregateErrors]]).
includes: [compareArray.js]
features: [AggregateError, Symbol.iterator]
---*/

var count = 0;
var errors = {
  [Symbol.iterator]() {
    return {
      next() {
        count += 1;
        return {
          done: count === 3,
          get value() {
            return count * 3;
          }
        };
      }
    };
  }
};

var case1 = new AggregateError(errors);

assert.sameValue(count, 3);

var get1 = case1.errors;
var get2 = case1.errors;

assert.sameValue(Array.isArray(get1), true);
assert.sameValue(Array.isArray(get2), true);

assert.notSameValue(get1, errors, 'creates a new array #1');
assert.notSameValue(get2, errors, 'creates a new array #2');
assert.notSameValue(get1, get2, 'creates a new array everytime it gets the values');

assert.compareArray(get1, [3, 6], 'get accessor does not trigger a new iteration #1');
assert.compareArray(get2, [3, 6], 'get accessor does not trigger a new iteration #2');

assert.sameValue(count, 3, 'count is preserved');
