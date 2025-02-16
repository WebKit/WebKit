// Copyright (C) 2024 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-iterator.concat
description: >
  Underlying iterator next returns object with throwing value getter
info: |
  Iterator.concat ( ...items )

  ...
  3. Let closure be a new Abstract Closure with no parameters that captures iterables and performs the following steps when called:
    a. For each Record iterable of iterables, do
      ...
      v. Repeat, while innerAlive is true,
        1. Let iteratorResult be ? IteratorStep(iteratorRecord).
        2. If iteratorResult is done, then
          a. Perform ? IteratorValue(iteratorResult).
          b. Set innerAlive to false.
        3. Else,
          a. Let completion be Completion(GeneratorYield(iteratorResult)).
          b. If completion is an abrupt completion, then
            i. Return ? IteratorClose(iteratorRecord, completion).
    ...
features: [iterator-sequencing]
---*/

let throwingIterator = {
  next() {
    return {
      get value() {
        throw new Test262Error();
      },
      done: false,
    };
  },
  return() {
    throw new Error();
  }
};

let iterable = {
  [Symbol.iterator]() {
    return throwingIterator;
  }
};

let iterator = Iterator.concat(iterable);
let iteratorResult = iterator.next();

assert.throws(Test262Error, function() {
  iteratorResult.value;
});
