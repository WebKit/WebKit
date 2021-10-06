// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.fields
description: A non-Array iterable passed as the argument is exhausted
info: |
    sec-temporal.calendar.prototype.fields step 4:
      4. Let _fieldNames_ be ? IterableToList(_fields_).
includes: [compareArray.js]
features: [Temporal]
---*/

const fieldNames = ["day", "month", "monthCode", "year"];
const iterable = {
  iteratorExhausted: false,
  *[Symbol.iterator]() {
    yield* fieldNames;
    this.iteratorExhausted = true;
  },
};

const calendar = new Temporal.Calendar("iso8601");
const result = calendar.fields(iterable);

assert.compareArray(result, fieldNames);
assert(iterable.iteratorExhausted);
