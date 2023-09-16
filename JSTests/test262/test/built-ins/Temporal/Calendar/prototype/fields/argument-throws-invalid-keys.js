// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.fields
description: >
  Calendar.prototype.fields rejects input field names that are not singular
  names of Temporal date units
info: |
    sec-temporal.calendar.prototype.fields step 7.b.ii:
      7. Repeat, while next is not false,
        a. Set next to ? IteratorStep(iteratorRecord).
        b. If next is not false, then
          i. Let nextValue be ? IteratorValue(next).
          ii. If Type(nextValue) is not String, then
            1. Let completion be ThrowCompletion(a newly created TypeError object).
            2. Return ? IteratorClose(iteratorRecord, completion).
    sec-temporal.calendar.prototype.fields step 7.b.iv:
          iv. If _nextValue_ is not one of *"year"*, *"month"*, *"monthCode"*, or *"day"*, then
            1. Let _completion_ be ThrowCompletion(a newly created *RangeError* object).
            2. Return ? IteratorClose(_iteratorRecord_, _completion_).

features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
assert.throws(TypeError, () => calendar.fields([1]));
assert.throws(TypeError, () => calendar.fields([1n]));
assert.throws(TypeError, () => calendar.fields([Symbol('foo')]));
assert.throws(TypeError, () => calendar.fields([true]));
assert.throws(TypeError, () => calendar.fields([null]));
assert.throws(TypeError, () => calendar.fields([{}]));
assert.throws(TypeError, () => calendar.fields([undefined]));
assert.throws(TypeError, () => calendar.fields(["day", 1]));
assert.throws(RangeError, () => calendar.fields(["hour"]));
assert.throws(RangeError, () => calendar.fields(["minute"]));
assert.throws(RangeError, () => calendar.fields(["second"]));
assert.throws(RangeError, () => calendar.fields(["millisecond"]));
assert.throws(RangeError, () => calendar.fields(["microsecond"]));
assert.throws(RangeError, () => calendar.fields(["nanosecond"]));
assert.throws(RangeError, () => calendar.fields(["month", "days"]));
assert.throws(RangeError, () => calendar.fields(["days"]));
assert.throws(RangeError, () => calendar.fields(["people"]));
