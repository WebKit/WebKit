// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Accepts spreadable arguments
includes: [compareArray.js]
---*/

const fixture = [0, 'a', undefined];

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

function f() {
  assert.compareArray(arguments, fixture);
  assert.compareArray(fixture, arguments);

  checkFormatOfAssertionMessage(() => {
    assert.compareArray(arguments, [], 'arguments and []');
  }, 'Expected [0, a, undefined] and [] to have the same contents. arguments and []');

  checkFormatOfAssertionMessage(() => {
    assert.compareArray([], arguments, '[] and arguments');
  }, 'Expected [] and [0, a, undefined] to have the same contents. [] and arguments');
}

f(...fixture);
