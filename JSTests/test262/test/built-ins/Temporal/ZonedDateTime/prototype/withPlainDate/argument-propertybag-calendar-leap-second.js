// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.withplaindate
description: Leap second is a valid ISO string for a calendar in a property bag
features: [Temporal]
---*/

const timeZone = new Temporal.TimeZone("UTC");
const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, timeZone);

const calendar = "2016-12-31T23:59:60+00:00[UTC]";

let arg = { year: 1976, monthCode: "M11", day: 18, calendar };
const result1 = instance.withPlainDate(arg);
assert.sameValue(
  result1.epochNanoseconds,
  217_129_600_000_000_000n,
  "leap second is a valid ISO string for calendar"
);

arg = { year: 1976, monthCode: "M11", day: 18, calendar: { calendar } };
const result2 = instance.withPlainDate(arg);
assert.sameValue(
  result2.epochNanoseconds,
  217_129_600_000_000_000n,
  "leap second is a valid ISO string for calendar (nested property)"
);
