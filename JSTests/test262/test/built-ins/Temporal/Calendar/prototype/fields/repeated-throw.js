// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.fields
description: >
  Temporal.Calendar.prototype.fields will throw if its input iterable yields 
  the same value twice.
info: |
  ## 12.4.21 Temporal.Calendar.prototype.fields ( fields )
  1. Let calendar be the this value.
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
  4. Let iteratorRecord be ? Getiterator(fields, sync).
  5. Let fieldNames be a new empty List.
  6. Let next be true.
  7. Repeat, while next is not false,
  a. Set next to ? IteratorStep(iteratorRecord).
  b. If next is not false, then
  i. Let nextValue be ? IteratorValue(next).
  iii. If fieldNames contains nextValue, then
   1. Let completion be ThrowCompletion(a newly created RangeError object).
   2. Return ? IteratorClose(iteratorRecord, completion).
features: [Symbol, Symbol.iterator, Temporal, computed-property-names, generators]
---*/
let cal = new Temporal.Calendar("iso8601")
let i = 0;
const fields = {
  *[Symbol.iterator]() {
      yield "month";
      i++;
      yield "year";
      i++;
      yield "year";
      i++;
  }
}
assert.throws(
    RangeError, () => cal.fields(fields), "repeated valid value should throw");
assert.sameValue(i, 2, "Should stop at 2");

// Test all valid value will throw while repeate
[ "nanosecond", "microsecond", "millisecond", "second",
  "minute", "hour", "day", "monthCode", "month", "year" ].forEach((f) => {
  i = 0;
  const fields2 = {
    *[Symbol.iterator]() {
      yield f;
      i++;
      yield f;
      i++;
    }
  }
  assert.throws(
    RangeError, () => cal.fields(fields2), "repeated valid value should throw");
  assert.sameValue(i, 1, "Should stop at 1");
});
