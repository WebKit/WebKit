// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-duration-objects
description: Temporal.Duration.prototype.add() works as expected
features: [Temporal]
---*/

var oneDay = new Temporal.Duration(0, 0, 0, 1);
var hours24 = new Temporal.Duration(0, 0, 0, 0, 24);

// relativeTo does not affect days if PlainDate
var relativeTo = Temporal.PlainDate.from("2017-01-01");
assert.sameValue(`${ oneDay.add(hours24, { relativeTo }) }`, "P2D");

// relativeTo does not affect days if ZonedDateTime, and duration encompasses no DST change
var relativeTo = Temporal.ZonedDateTime.from("2017-01-01T00:00[America/Montevideo]");
assert.sameValue(`${ oneDay.add(hours24, { relativeTo }) }`, "P2D");
var skippedHourDay = Temporal.ZonedDateTime.from("2019-03-10T00:00[America/Vancouver]");
var repeatedHourDay = Temporal.ZonedDateTime.from("2019-11-03T00:00[America/Vancouver]");
var inRepeatedHour = Temporal.ZonedDateTime.from("2019-11-03T01:00-07:00[America/Vancouver]");
var hours12 = new Temporal.Duration(0, 0, 0, 0, 12);
var hours25 = new Temporal.Duration(0, 0, 0, 0, 25);

// relativeTo affects days if ZonedDateTime, and duration encompasses DST change", 

// start inside repeated hour, end after", 
assert.sameValue(`${ hours25.add(oneDay, { relativeTo: inRepeatedHour }) }`, "P2D");
assert.sameValue(`${ oneDay.add(hours25, { relativeTo: inRepeatedHour }) }`, "P2DT1H");

// start after repeated hour, end inside (negative)"
var relativeTo = Temporal.ZonedDateTime.from("2019-11-05T01:00[America/Vancouver]");
assert.sameValue(`${ hours25.negated().add(oneDay.negated(), { relativeTo }) }`, "-P2DT1H");
assert.sameValue(`${ oneDay.negated().add(hours25.negated(), { relativeTo }) }`, "-P2D");

// start inside repeated hour, end in skipped hour", 
assert.sameValue(`${ hours25.add(Temporal.Duration.from({
  days: 125,
  hours: 1
}), { relativeTo: inRepeatedHour }) }`, "P126DT1H");
assert.sameValue(`${ oneDay.add(Temporal.Duration.from({
  days: 125,
  hours: 1
}), { relativeTo: inRepeatedHour }) }`, "P126DT1H");

// start in normal hour, end in skipped hour",
var relativeTo = Temporal.ZonedDateTime.from("2019-03-08T02:30[America/Vancouver]");
assert.sameValue(`${ oneDay.add(hours25, { relativeTo }) }`, "P2DT1H");
assert.sameValue(`${ hours25.add(oneDay, { relativeTo }) }`, "P2D");

// start before skipped hour, end >1 day after", 
assert.sameValue(`${ hours25.add(oneDay, { relativeTo: skippedHourDay }) }`, "P2DT2H");
assert.sameValue(`${ oneDay.add(hours25, { relativeTo: skippedHourDay }) }`, "P2DT1H");

// start after skipped hour, end >1 day before (negative)", 
var relativeTo = Temporal.ZonedDateTime.from("2019-03-11T00:00[America/Vancouver]");
assert.sameValue(`${ hours25.negated().add(oneDay.negated(), { relativeTo }) }`, "-P2DT2H");
assert.sameValue(`${ oneDay.negated().add(hours25.negated(), { relativeTo }) }`, "-P2DT1H");

// start before skipped hour, end <1 day after", 
assert.sameValue(`${ hours12.add(oneDay, { relativeTo: skippedHourDay }) }`, "P1DT13H");
assert.sameValue(`${ oneDay.add(hours12, { relativeTo: skippedHourDay }) }`, "P1DT12H");

// start after skipped hour, end <1 day before (negative)", 
var relativeTo = Temporal.ZonedDateTime.from("2019-03-10T12:00[America/Vancouver]");
assert.sameValue(`${ hours12.negated().add(oneDay.negated(), { relativeTo }) }`, "-P1DT13H");
assert.sameValue(`${ oneDay.negated().add(hours12.negated(), { relativeTo }) }`, "-P1DT12H");

// start before repeated hour, end >1 day after", 
assert.sameValue(`${ hours25.add(oneDay, { relativeTo: repeatedHourDay }) }`, "P2D");
assert.sameValue(`${ oneDay.add(hours25, { relativeTo: repeatedHourDay }) }`, "P2DT1H");

// start after repeated hour, end >1 day before (negative)", 
var relativeTo = Temporal.ZonedDateTime.from("2019-11-04T00:00[America/Vancouver]");
assert.sameValue(`${ hours25.negated().add(oneDay.negated(), { relativeTo }) }`, "-P2D");
assert.sameValue(`${ oneDay.negated().add(hours25.negated(), { relativeTo }) }`, "-P2DT1H");

// start before repeated hour, end <1 day after", 
assert.sameValue(`${ hours12.add(oneDay, { relativeTo: repeatedHourDay }) }`, "P1DT11H");
assert.sameValue(`${ oneDay.add(hours12, { relativeTo: repeatedHourDay }) }`, "P1DT12H");

// start after repeated hour, end <1 day before (negative)", 
var relativeTo = Temporal.ZonedDateTime.from("2019-11-03T12:00[America/Vancouver]");
assert.sameValue(`${ hours12.negated().add(oneDay.negated(), { relativeTo }) }`, "-P1DT11H");
assert.sameValue(`${ oneDay.negated().add(hours12.negated(), { relativeTo }) }`, "-P1DT12H");

// Samoa skipped 24 hours", 
var relativeTo = Temporal.ZonedDateTime.from("2011-12-29T12:00-10:00[Pacific/Apia]");
assert.sameValue(`${ hours25.add(oneDay, { relativeTo }) }`, "P3DT1H");
assert.sameValue(`${ oneDay.add(hours25, { relativeTo }) }`, "P3DT1H");

// casts relativeTo to ZonedDateTime if possible
assert.sameValue(`${ oneDay.add(hours24, { relativeTo: "2019-11-02T00:00[America/Vancouver]" }) }`, "P1DT24H");
assert.sameValue(`${ oneDay.add(hours24, {
  relativeTo: {
    year: 2019,
    month: 11,
    day: 2,
    timeZone: "America/Vancouver"
  }
}) }`, "P1DT24H");

// casts relativeTo to PlainDate if possible
assert.sameValue(`${ oneDay.add(hours24, { relativeTo: "2019-11-02" }) }`, "P2D");
assert.sameValue(`${ oneDay.add(hours24, {
  relativeTo: {
    year: 2019,
    month: 11,
    day: 2
  }
}) }`, "P2D");

// throws on wrong offset for ZonedDateTime relativeTo string
assert.throws(RangeError, () => oneDay.add(hours24, { relativeTo: "1971-01-01T00:00+02:00[Africa/Monrovia]" }));

// does not throw on HH:MM rounded offset for ZonedDateTime relativeTo string
assert.sameValue(`${ oneDay.add(hours24, { relativeTo: "1971-01-01T00:00-00:45[Africa/Monrovia]" }) }`, "P2D");

// throws on HH:MM rounded offset for ZonedDateTime relativeTo property bag
assert.throws(RangeError, () => oneDay.add(hours24, {
  relativeTo: {
    year: 1971,
    month: 1,
    day: 1,
    offset: "-00:45",
    timeZone: "Africa/Monrovia"
  }
}));

// at least the required properties must be present in relativeTo
assert.throws(TypeError, () => oneDay.add(hours24, {
  relativeTo: {
    month: 11,
    day: 3
  }
}));
assert.throws(TypeError, () => oneDay.add(hours24, {
  relativeTo: {
    year: 2019,
    month: 11
  }
}));
assert.throws(TypeError, () => oneDay.add(hours24, {
  relativeTo: {
    year: 2019,
    day: 3
  }
}));
