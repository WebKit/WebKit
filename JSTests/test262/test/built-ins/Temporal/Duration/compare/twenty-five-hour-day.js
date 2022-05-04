// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.compare
description: Unbalancing handles DST days with more than 24 hours
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const tz = TemporalHelpers.springForwardFallBackTimeZone();

// 2000-10-29 is a 25-hour day according to this time zone...

const relativeTo = new Temporal.ZonedDateTime(941187600_000_000_000n, tz);

// confirm that we have rewound one year and one day:
assert.sameValue('1999-10-29T01:00:00-08:00[Custom/Spring_Fall]', relativeTo.toString());

const d1 = new Temporal.Duration(1, 0, 0, 1);
const d2 = new Temporal.Duration(1, 0, 0, 0, 25);

// ...so the durations should be equal relative to relativeTo:

assert.sameValue(0,
  Temporal.Duration.compare(d1, d2, { relativeTo }),
  "2000-10-29 is a 25-hour day"
);

assert.sameValue(1,
  Temporal.Duration.compare(d1, { years: 1, hours: 24 }, { relativeTo }),
  "2020-10-29 has more than 24 hours"
);
