// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.mergefields
description: >
  The default mergeFields algorithm from the ISO 8601 calendar should correctly
  merge the month and monthCode properties
info: |
  1. Let calendar be the this value.
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
  3. Assert: calendar.[[Identifier]] is "iso8601".
  4. Set fields to ? ToObject(fields).
  5. Set additionalFields to ? ToObject(additionalFields).
  6. Return ? DefaultMergeFields(fields, additionalFields).
features: [Temporal]
includes: [deepEqual.js]
---*/

const cal = new Temporal.Calendar("iso8601");

assert.deepEqual(
  cal.mergeFields({ a: 1, b: 2, month: 7 }, { b: 3, c: 4 }),
  { a: 1, b: 3, c: 4, month: 7 },
  "month is copied from fields"
);
assert.deepEqual(
  cal.mergeFields({ a: 1, b: 2, monthCode: "M08" }, { b: 3, c: 4 }),
  { a: 1, b: 3, c: 4, monthCode: "M08" },
  "monthCode is copied from fields"
);
assert.deepEqual(
  cal.mergeFields({ a: 1, b: 2, month: 7, monthCode: "M08" }, { b: 3, c: 4 }),
  { a: 1, b: 3, c: 4, month: 7, monthCode: "M08" },
  "both month and monthCode are copied from fields, no validation is performed"
);

assert.deepEqual(
  cal.mergeFields({ a: 1, b: 2 }, { b: 3, c: 4, month: 5 }),
  { a: 1, b: 3, c: 4, month: 5 },
  "month is copied from additionalFields"
);
assert.deepEqual(
  cal.mergeFields({ a: 1, b: 2 }, { b: 3, c: 4, monthCode: "M06" }),
  { a: 1, b: 3, c: 4, monthCode: "M06" },
  "monthCode is copied from additionalFields"
);
assert.deepEqual(
  cal.mergeFields({ a: 1, b: 2 }, { b: 3, c: 4, month: 5, monthCode: "M06" }),
  { a: 1, b: 3, c: 4, month: 5, monthCode: "M06" },
  "both month and monthCode are copied from additionalFields, no validation is performed"
);

assert.deepEqual(
  cal.mergeFields({ a: 1, b: 2, month: 7 }, { b: 3, c: 4, month: 5 }),
  { a: 1, b: 3, c: 4, month: 5 },
  "month from additionalFields overrides month from fields"
);
assert.deepEqual(
  cal.mergeFields({ a: 1, b: 2, monthCode: "M07" }, { b: 3, c: 4, monthCode: "M05" }),
  { a: 1, b: 3, c: 4, monthCode: "M05" },
  "monthCode from additionalFields overrides monthCode from fields"
);
assert.deepEqual(
  cal.mergeFields({ a: 1, b: 2, monthCode: "M07" }, { b: 3, c: 4, month: 6 }),
  { a: 1, b: 3, c: 4, month: 6 },
  "month's presence on additionalFields blocks monthCode from fields"
);
assert.deepEqual(
  cal.mergeFields({ a: 1, b: 2, month: 7 }, { b: 3, c: 4, monthCode: "M06" }),
  { a: 1, b: 3, c: 4, monthCode: "M06"},
  "monthCode's presence on additionalFields blocks month from fields"
);
assert.deepEqual(
  cal.mergeFields({ a: 1, b: 2, month: 7, monthCode: "M08" },{ b: 3, c: 4, month: 5 }),
  { a: 1, b: 3, c: 4, month: 5 },
  "month's presence on additionalFields blocks both month and monthCode from fields"
);
assert.deepEqual(
  cal.mergeFields({ a: 1, b: 2, month: 7, monthCode: "M08" }, { b: 3, c: 4, monthCode: "M06" }),
  { a: 1, b: 3, c: 4, monthCode: "M06" },
  "monthCode's presence on additionalFields blocks both month and monthCode from fields"
);

assert.deepEqual(
  cal.mergeFields({ a: 1, b: 2, month: 7 }, { b: 3, c: 4, month: 5, monthCode: "M06" }),
  { a: 1, b: 3, c: 4, month: 5, monthCode: "M06" },
  "both month and monthCode are copied from additionalFields even when fields has month"
);
assert.deepEqual(
  cal.mergeFields({ a: 1, b: 2, monthCode: "M07" }, { b: 3, c: 4, month: 5, monthCode: "M06" }),
  { a: 1, b: 3, c: 4, month: 5, monthCode: "M06" },
  "both month and monthCode are copied from additionalFields even when fields has monthCode"
);
assert.deepEqual(
  cal.mergeFields({ a: 1, b: 2, month: 7, monthCode: "M08" }, { b: 3, c: 4, month: 5, monthCode: "M06" }),
  { a: 1, b: 3, c: 4, month: 5, monthCode: "M06" },
  "both month and monthCode are copied from additionalFields even when fields has both month and monthCode"
);
