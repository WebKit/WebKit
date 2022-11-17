// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-duration-objects
description: >
  Various DST arithmetic tests that it's impractical to do without a time zone
  database in the implementation
features: [Temporal]
---*/

// Tests for arithmetic that start inside a repeated hour, and end in a skipped
// hour. We have TemporalHelpers.springForwardFallBackTimeZone which is
// sufficient to test this for Temporal.Duration.prototype.add, and
// Temporal.Duration.prototype.round, but it's impractical to replicate all the
// TZDB data for testing it with other methods such as subtract() where we need
// to calculate to the _next_ transition

var skippedHourDay = Temporal.ZonedDateTime.from("2019-03-10T00:00[America/Vancouver]");
var repeatedHourDay = Temporal.ZonedDateTime.from("2019-11-03T00:00[America/Vancouver]");
var inRepeatedHour = Temporal.ZonedDateTime.from("2019-11-03T01:00-07:00[America/Vancouver]");

// subtract()

var oneDay = new Temporal.Duration(0, 0, 0, 1);
assert.sameValue(`${ Temporal.Duration.from({
  days: 127,
  hours: 1
}).subtract(oneDay, { relativeTo: inRepeatedHour }) }`, "P126DT1H");
var hours24 = new Temporal.Duration(0, 0, 0, 0, 24);
assert.sameValue(`${ Temporal.Duration.from({
  days: 127,
  hours: 1
}).subtract(hours24, { relativeTo: inRepeatedHour }) }`, "P126D");

// total()
var totalDays = Temporal.Duration.from({
  days: 126,
  hours: 1
}).total({
  unit: "days",
  relativeTo: inRepeatedHour
});
assert(Math.abs(totalDays - (126 + 1 / 23)) < Number.EPSILON);
assert.sameValue(Temporal.Duration.from({
  days: 126,
  hours: 1
}).total({
  unit: "hours",
  relativeTo: inRepeatedHour
}), 3026);

// Tests for casting relativeTo to ZonedDateTime when possible:
// Without a TZDB, it's not possible to get a ZonedDateTime with DST from a
// string.

assert.sameValue(
  `${ oneDay.add(hours24, { relativeTo: "2019-11-02T00:00[America/Vancouver]" }) }`,
  "P1DT24H"
);
var hours25 = new Temporal.Duration(0, 0, 0, 0, 25);
assert.sameValue(`${ hours25.round({
  largestUnit: "days",
  relativeTo: "2019-11-03T00:00[America/Vancouver]"
}) }`, "P1D");
assert.sameValue(
  `${ oneDay.subtract(hours24, { relativeTo: "2019-11-03T00:00[America/Vancouver]" }) }`,
  "PT1H"
);
assert.sameValue(oneDay.total({
  unit: "hours",
  relativeTo: "2019-11-03T00:00[America/Vancouver]"
}), 25);
