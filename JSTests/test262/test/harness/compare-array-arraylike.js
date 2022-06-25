// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Accepts spreadable arguments
includes: [compareArray.js]
---*/

function checkFormatOfAssertionMessage(func, errorMessage) {
  var caught = false;
  try {
      func();
  } catch (error) {
      caught = true;
      assert.sameValue(error.constructor, Test262Error);
      assert.sameValue(error.message, errorMessage);
  }

  assert(caught, `Expected ${func} to throw, but it didn't.`);
}

const fixture = { length: 3, 0: 0, 1: 'a', 2: undefined};

assert.compareArray(fixture, [0, 'a', undefined]);
assert.compareArray([0, 'a', undefined], fixture);

checkFormatOfAssertionMessage(() => {
  assert.compareArray(fixture, [], 'fixture and []');
}, 'Expected [0, a, undefined] and [] to have the same contents. fixture and []');

checkFormatOfAssertionMessage(() => {
  assert.compareArray([], fixture, '[] and fixture');
}, 'Expected [] and [0, a, undefined] to have the same contents. [] and fixture');
