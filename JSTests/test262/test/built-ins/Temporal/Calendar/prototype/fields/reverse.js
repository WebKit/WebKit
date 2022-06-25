// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.fields
description: >
  Temporal.Calendar.prototype.fields will return the iterable in array if all
  input are valid regardless of it's order.
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
  iv. If nextValue is not one of "year", "month", "monthCode", "day", "hour", "minute", "second", "millisecond", "microsecond", "nanosecond", then
  1. Let completion be ThrowCompletion(a newly created RangeError object).
  2. Return ? IteratorClose(iteratorRecord, completion).
features: [Symbol, Symbol.iterator, Temporal, computed-property-names, generators]
includes: [compareArray.js]
---*/
let cal = new Temporal.Calendar("iso8601")
const fields = {
  *[Symbol.iterator]() {
     yield "nanosecond";
     yield "microsecond";
     yield "millisecond";
     yield "second";
     yield "minute";
     yield "hour";
     yield "day";
     yield "monthCode";
     yield "month";
     yield "year";
  }
}
assert.compareArray(cal.fields(fields), Array.from(fields),
    'valid fields should be supported even if they are in reversed order of the spec');
