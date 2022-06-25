// Copyright (C) 2021 the V8 project authors. All rights reserved.
// Copyright (C) 2022 Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.mergefields
description: >
  Temporal.Calendar.prototype.mergeFields will merge own data properties on its
  arguments
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
  cal.mergeFields({ a: 1, b: 2 }, { c: 3, d: 4 }),
  { a: 1, b: 2, c: 3, d: 4 },
  "properties are merged"
);

assert.deepEqual(
  cal.mergeFields({ a: 1, b: 2 }, { b: 3, c: 4 }),
  { a: 1, b: 3, c: 4 },
  "property in additionalFields should overwrite one in fields"
);
