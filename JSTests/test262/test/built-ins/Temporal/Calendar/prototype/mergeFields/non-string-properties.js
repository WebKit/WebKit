// Copyright (C) 2021 the V8 project authors. All rights reserved.
// Copyright (C) 2022 Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.mergefields
description: Only string keys from the arguments are merged
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
  cal.mergeFields({ 1: 2 }, { 3: 4 }),
  { "1": 2, "3": 4 },
  "number keys are actually string keys and are merged as such"
);
assert.deepEqual(
  cal.mergeFields({ 1n: 2 }, { 2n: 4 }),
  { "1": 2, "2": 4 },
  "bigint keys are actually string keys and are merged as such"
);

const foo = Symbol("foo");
const bar = Symbol("bar");
assert.deepEqual(cal.mergeFields({ [foo]: 1 }, { [bar]: 2 }), {}, "symbol keys are not merged");
