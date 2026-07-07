// Copyright (C) 2026 Danial Asaria (Bloomberg LP). All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-createkeyedpromisecombinatorresultobject
description: >
  Promise.allSettledKeyed result and settled entry properties are data properties
info: |
  CreateKeyedPromiseCombinatorResultObject ( entries )

  ...
  1. Let obj be OrdinaryObjectCreate(null).
  2. For each Record { [[Key]], [[Value]] } entry of entries, do
    a. Perform ! CreateDataPropertyOrThrow(obj, entry.[[Key]], entry.[[Value]]).
  3. Return obj.

  The onFulfilled closure for all-settled:
    ...
    Perform ! CreateDataPropertyOrThrow(obj, "status", "fulfilled").
    Perform ! CreateDataPropertyOrThrow(obj, "value", value).

  The onRejected closure:
    ...
    Perform ! CreateDataPropertyOrThrow(obj, "status", "rejected").
    Perform ! CreateDataPropertyOrThrow(obj, "reason", error).
includes: [asyncHelpers.js, propertyHelper.js]
flags: [async]
features: [await-dictionary]
---*/

var error = new Test262Error();

asyncTest(function() {
  return Promise.allSettledKeyed({
    fulfilled: Promise.resolve(1),
    rejected: Promise.reject(error)
  }).then(function(result) {
    assert.sameValue(Object.getPrototypeOf(result), null, "result is null-prototype");
    assert.sameValue(Object.getPrototypeOf(result.fulfilled), Object.prototype, "fulfilled entry prototype");
    assert.sameValue(Object.getPrototypeOf(result.rejected), Object.prototype, "rejected entry prototype");

    verifyProperty(result, "fulfilled", {
      value: result.fulfilled,
      writable: true,
      enumerable: true,
      configurable: true
    });
    verifyProperty(result, "rejected", {
      value: result.rejected,
      writable: true,
      enumerable: true,
      configurable: true
    });

    verifyProperty(result.fulfilled, "status", {
      value: "fulfilled",
      writable: true,
      enumerable: true,
      configurable: true
    });
    verifyProperty(result.fulfilled, "value", {
      value: 1,
      writable: true,
      enumerable: true,
      configurable: true
    });
    verifyProperty(result.rejected, "status", {
      value: "rejected",
      writable: true,
      enumerable: true,
      configurable: true
    });
    verifyProperty(result.rejected, "reason", {
      value: error,
      writable: true,
      enumerable: true,
      configurable: true
    });
  });
});
