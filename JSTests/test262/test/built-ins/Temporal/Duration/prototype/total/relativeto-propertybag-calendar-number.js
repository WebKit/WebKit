// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: A number as calendar in relativeTo property bag is converted to a string, then to a calendar
features: [Temporal]
---*/

const instance = new Temporal.Duration(1, 0, 0, 0, 24);

const calendar = 19970327;

let relativeTo = { year: 2019, monthCode: "M11", day: 1, calendar };
const result1 = instance.total({ unit: "days", relativeTo });
assert.sameValue(result1, 367, "19970327 is a valid ISO string for relativeTo.calendar");

relativeTo = { year: 2019, monthCode: "M11", day: 1, calendar: { calendar } };
const result2 = instance.total({ unit: "days", relativeTo });
assert.sameValue(result2, 367, "19970327 is a valid ISO string for relativeTo.calendar (nested property)");

const numbers = [
  1,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  let relativeTo = { year: 2019, monthCode: "M11", day: 1, calendar };
  assert.throws(
    RangeError,
    () => instance.total({ unit: "days", relativeTo }),
    `Number ${calendar} does not convert to a valid ISO string for relativeTo.calendar`
  );
  relativeTo = { year: 2019, monthCode: "M11", day: 1, calendar: { calendar } };
  assert.throws(
    RangeError,
    () => instance.total({ unit: "days", relativeTo }),
    `Number ${calendar} does not convert to a valid ISO string for relativeTo.calendar (nested property)`
  );
}
