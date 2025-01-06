// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-duration-objects
description: Temporal.Duration.prototype.round() works as expected
features: [Temporal]
---*/

// relativeTo affects days if ZonedDateTime, and duration encompasses DST change
var timeZone = "America/Vancouver";
var skippedHourDay = Temporal.PlainDateTime.from("2000-04-02").toZonedDateTime(timeZone);
var repeatedHourDay = Temporal.PlainDateTime.from("2000-10-29").toZonedDateTime(timeZone);
var inRepeatedHour = new Temporal.ZonedDateTime(972806400_000_000_000n, timeZone);
var oneDay = new Temporal.Duration(0, 0, 0, 1);
var hours12 = new Temporal.Duration(0, 0, 0, 0, 12);
var hours25 = new Temporal.Duration(0, 0, 0, 0, 25);

// start inside repeated hour, end after
assert.sameValue(`${ hours25.round({
  largestUnit: "days",
  relativeTo: inRepeatedHour
}) }`, "P1D");
assert.sameValue(`${ oneDay.round({
  largestUnit: "hours",
  relativeTo: inRepeatedHour
}) }`, "PT25H");

// start after repeated hour, end inside (negative)
var relativeTo = Temporal.PlainDateTime.from("2000-10-30T01:00").toZonedDateTime(timeZone);
assert.sameValue(`${ hours25.negated().round({
  largestUnit: "days",
  relativeTo
}) }`, "-P1D");
assert.sameValue(`${ oneDay.negated().round({
  largestUnit: "hours",
  relativeTo
}) }`, "-PT25H");

// start inside repeated hour, end in skipped hour
assert.sameValue(`${ Temporal.Duration.from({
  days: 126,
  hours: 1
}).round({
  largestUnit: "days",
  relativeTo: inRepeatedHour
}) }`, "P126DT1H");
assert.sameValue(`${ Temporal.Duration.from({
  days: 126,
  hours: 1
}).round({
  largestUnit: "hours",
  relativeTo: inRepeatedHour
}) }`, "PT3026H");

// start in normal hour, end in skipped hour
var relativeTo = Temporal.PlainDateTime.from("2000-04-01T02:30").toZonedDateTime(timeZone);
assert.sameValue(`${ hours25.round({
  largestUnit: "days",
  relativeTo
}) }`, "P1DT1H");
assert.sameValue(`${ oneDay.round({
  largestUnit: "hours",
  relativeTo
}) }`, "PT24H");

// start before skipped hour, end >1 day after
assert.sameValue(`${ hours25.round({
  largestUnit: "days",
  relativeTo: skippedHourDay
}) }`, "P1DT2H");
assert.sameValue(`${ oneDay.round({
  largestUnit: "hours",
  relativeTo: skippedHourDay
}) }`, "PT23H");

// start after skipped hour, end >1 day before (negative)
var relativeTo = Temporal.PlainDateTime.from("2000-04-03T00:00").toZonedDateTime(timeZone);
assert.sameValue(`${ hours25.negated().round({
  largestUnit: "days",
  relativeTo
}) }`, "-P1DT2H");
assert.sameValue(`${ oneDay.negated().round({
  largestUnit: "hours",
  relativeTo
}) }`, "-PT23H");

// start before skipped hour, end <1 day after
assert.sameValue(`${ hours12.round({
  largestUnit: "days",
  relativeTo: skippedHourDay
}) }`, "PT12H");

// start after skipped hour, end <1 day before (negative)
var relativeTo = Temporal.PlainDateTime.from("2000-04-02T12:00").toZonedDateTime(timeZone);
assert.sameValue(`${ hours12.negated().round({
  largestUnit: "days",
  relativeTo
}) }`, "-PT12H");

// start before repeated hour, end >1 day after
assert.sameValue(`${ hours25.round({
  largestUnit: "days",
  relativeTo: repeatedHourDay
}) }`, "P1D");
assert.sameValue(`${ oneDay.round({
  largestUnit: "hours",
  relativeTo: repeatedHourDay
}) }`, "PT25H");

// start after repeated hour, end >1 day before (negative)
var relativeTo = Temporal.PlainDateTime.from("2000-10-30T00:00").toZonedDateTime(timeZone);
assert.sameValue(`${ hours25.negated().round({
  largestUnit: "days",
  relativeTo
}) }`, "-P1D");
assert.sameValue(`${ oneDay.negated().round({
  largestUnit: "hours",
  relativeTo
}) }`, "-PT25H");

// start before repeated hour, end <1 day after
assert.sameValue(`${ hours12.round({
  largestUnit: "days",
  relativeTo: repeatedHourDay
}) }`, "PT12H");

// start after repeated hour, end <1 day before (negative)
var relativeTo = Temporal.PlainDateTime.from("2000-10-29T12:00").toZonedDateTime(timeZone);
assert.sameValue(`${ hours12.negated().round({
  largestUnit: "days",
  relativeTo
}) }`, "-PT12H");

// Samoa skipped 24 hours
var relativeTo = Temporal.PlainDateTime.from("2011-12-29T12:00").toZonedDateTime("Pacific/Apia");
assert.sameValue(`${ hours25.round({
  largestUnit: "days",
  relativeTo
}) }`, "P2DT1H");
assert.sameValue(`${ Temporal.Duration.from({ hours: 48 }).round({
  largestUnit: "days",
  relativeTo
}) }`, "P3D");

// casts relativeTo to ZonedDateTime if possible
assert.sameValue(`${ hours25.round({
  largestUnit: "days",
  relativeTo: {
    year: 2000,
    month: 10,
    day: 29,
    timeZone
  }
}) }`, "P1D");
