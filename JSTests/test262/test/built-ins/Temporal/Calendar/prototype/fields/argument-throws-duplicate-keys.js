// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.fields
description: Duplicate fields are not allowed in the argument to Calendar.prototype.fields
info: |
    sec-temporal.calendar.prototype.fields step 7.b.iii:
      iii. If _fieldNames_ contains _nextValue_, then
        1. Let _completion_ be ThrowCompletion(a newly created *RangeError* object).
        2. Return ? IteratorClose(_iteratorRecord_, _completion_).
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
assert.throws(RangeError, () => calendar.fields(["day", "month", "day"]));
assert.throws(RangeError, () => calendar.fields(["year", "month", "monthCode", "day", "hour", "minute", "second", "millisecond", "microsecond", "nanosecond", "year"]));

const manyFields = {
  count: 0
};

manyFields[Symbol.iterator] = function*() {
  while (this.count++ < 100) yield "year";
};

assert.throws(RangeError, () => calendar.fields(manyFields), "Rejected duplicate values");
assert.sameValue(manyFields.count, 2, "Rejected first duplicate value");
