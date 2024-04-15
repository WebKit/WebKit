// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-%asyncfromsynciteratorprototype%.throw
description: >
 If syncIterator's "throw" method is `undefined`,
 and its "return" method returns `undefined`,
 the iterator will close returning the `undefined` value,
 which will be ignored and instead a rejected Promise with a new TypeError is returned.
info: |
  %AsyncFromSyncIteratorPrototype%.throw ( value )
  ...
  2. Let promiseCapability be ! NewPromiseCapability(%Promise%).
  ...
  5. Let return be GetMethod(syncIterator, "throw").
  ...
  7. If throw is undefined, then
    a. NOTE: If syncIterator does not have a throw method, close it to give it a chance to clean up before we reject the capability.
    b. Let closeCompletion be Completion { [[Type]]: normal, [[Value]]: empty, [[Target]]: empty }.
    c. Set result to IteratorClose(syncIteratorRecord, closeCompletion).
    ...
    g. Perform ! Call(promiseCapability.[[Reject]], undefined, « a newly created TypeError object »).
    h. Return promiseCapability.[[Promise]].
    ...

  IteratorClose ( iterator, completion )
  ...
  2. Let iterator be iteratorRecord.[[Iterator]].
  3. Let innerResult be Completion(GetMethod(iterator, "return")).
  4. If innerResult.[[Type]] is normal, then
    a. Let return be innerResult.[[Value]].
    ...
    c. Set innerResult to Completion(Call(return, iterator)).
  ...
  8. Return ? completion.

flags: [async]
features: [async-iteration]
includes: [asyncHelpers.js]
---*/

var returnCount = 0;

const obj = {
  [Symbol.iterator]() {
    return {
      next() {
        return {value: 1, done: false};
      },
      return() {
        returnCount += 1;
        return {value: 2, done: true};
      }
    };
  }
};

async function* wrapper() {
  yield* obj;
}

var iter = wrapper();

asyncTest(async function () {
  await assert.throwsAsync(TypeError, async () => {
    await iter.next();
    return iter.throw();
  }, "Promise should be rejected");
  assert.sameValue(returnCount, 1, 'iterator closed properly');
  const result = await iter.next();
  assert(result.done, "the iterator is completed");
  assert.sameValue(result.value, undefined, "value is undefined");
})
