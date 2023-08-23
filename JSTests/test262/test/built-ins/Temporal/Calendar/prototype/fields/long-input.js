// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.fields
description: >
  Temporal.Calendar.prototype.fields will throw when its input iterable yields an
  invalid field.
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
  iv. If nextValue is not one of "year", "month", "monthCode", or "day", then
  1. Let completion be ThrowCompletion(a newly created RangeError object).
  2. Return ? IteratorClose(iteratorRecord, completion).
features: [Symbol, Symbol.iterator, Temporal, computed-property-names, generators]
---*/
let cal = new Temporal.Calendar("iso8601")
let i = 0;
const fields = {
  *[Symbol.iterator]() {
      // The first three are valid values
      yield "year";
      i++;
      yield "month";
      i++;
      yield "monthCode";
      i++;
      // The fourth one is wrong and should throw after the next line.
      yield "garbage";
      // The following three lines should not be reached if the implemention
      // correctly check the previous line.
      i++;
      yield "day";
      i++;
  }
}
assert.throws(RangeError, () => cal.fields(fields), "Garbage content");
// stop after the third one.
assert.sameValue(i, 3);
