// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-duration-objects
description: Temporal.Duration.prototype.subtract() works as expected
features: [Temporal]
---*/

var oneDay = new Temporal.Duration(0, 0, 0, 1);
var hours24 = new Temporal.Duration(0, 0, 0, 0, 24);

// relativeTo does not affect days if PlainDate
var relativeTo = Temporal.PlainDate.from("2017-01-01");
assert.sameValue(`${ oneDay.subtract(hours24, { relativeTo }) }`, "PT0S");

// relativeTo does not affect days if ZonedDateTime, and duration encompasses no DST change
var relativeTo = Temporal.ZonedDateTime.from("2017-01-01T00:00[America/Montevideo]");
assert.sameValue(`${ oneDay.subtract(hours24, { relativeTo }) }`, "PT0S");
var skippedHourDay = Temporal.ZonedDateTime.from("2019-03-10T00:00[America/Vancouver]");
var repeatedHourDay = Temporal.ZonedDateTime.from("2019-11-03T00:00[America/Vancouver]");
var inRepeatedHour = Temporal.ZonedDateTime.from("2019-11-03T01:00-07:00[America/Vancouver]");
var twoDays = new Temporal.Duration(0, 0, 0, 2);
var threeDays = new Temporal.Duration(0, 0, 0, 3);
// relativeTo affects days if ZonedDateTime, and duration encompasses DST change
   
// start inside repeated hour, end after"
assert.sameValue(`${ hours24.subtract(oneDay, { relativeTo: inRepeatedHour }) }`, "-PT1H");
assert.sameValue(`${ oneDay.subtract(hours24, { relativeTo: inRepeatedHour }) }`, "PT1H");

// start inside repeated hour, end in skipped hour
assert.sameValue(`${ Temporal.Duration.from({
  days: 127,
  hours: 1
}).subtract(oneDay, { relativeTo: inRepeatedHour }) }`, "P126DT1H");
assert.sameValue(`${ Temporal.Duration.from({
  days: 127,
  hours: 1
}).subtract(hours24, { relativeTo: inRepeatedHour }) }`, "P126D");

// start in normal hour, end in skipped hour
var relativeTo = Temporal.ZonedDateTime.from("2019-03-09T02:30[America/Vancouver]");
assert.sameValue(`${ hours24.subtract(oneDay, { relativeTo }) }`, "PT1H");
assert.sameValue(`${ oneDay.subtract(hours24, { relativeTo }) }`, "PT0S");

// start before skipped hour, end >1 day after
assert.sameValue(`${ threeDays.subtract(hours24, { relativeTo: skippedHourDay }) }`, "P2D");
assert.sameValue(`${ hours24.subtract(threeDays, { relativeTo: skippedHourDay }) }`, "-P1DT23H");

// start before skipped hour, end <1 day after
assert.sameValue(`${ twoDays.subtract(hours24, { relativeTo: skippedHourDay }) }`, "P1D");
assert.sameValue(`${ hours24.subtract(twoDays, { relativeTo: skippedHourDay }) }`, "-PT23H");

// start before repeated hour, end >1 day after
assert.sameValue(`${ threeDays.subtract(hours24, { relativeTo: repeatedHourDay }) }`, "P2D");
assert.sameValue(`${ hours24.subtract(threeDays, { relativeTo: repeatedHourDay }) }`, "-P2DT1H");

// start before repeated hour, end <1 day after
assert.sameValue(`${ twoDays.subtract(hours24, { relativeTo: repeatedHourDay }) }`, "P1D");
assert.sameValue(`${ hours24.subtract(twoDays, { relativeTo: repeatedHourDay }) }`, "-P1DT1H");

// Samoa skipped 24 hours
var relativeTo = Temporal.ZonedDateTime.from("2011-12-29T12:00-10:00[Pacific/Apia]");
assert.sameValue(`${ twoDays.subtract(Temporal.Duration.from({ hours: 48 }), { relativeTo }) }`, "-P1D");
assert.sameValue(`${ Temporal.Duration.from({ hours: 48 }).subtract(twoDays, { relativeTo }) }`, "P2D");

// casts relativeTo to ZonedDateTime if possible
assert.sameValue(`${ oneDay.subtract(hours24, { relativeTo: "2019-11-03T00:00[America/Vancouver]" }) }`, "PT1H");
assert.sameValue(`${ oneDay.subtract(hours24, {
  relativeTo: {
    year: 2019,
    month: 11,
    day: 3,
    timeZone: "America/Vancouver"
  }
}) }`, "PT1H");

// casts relativeTo to PlainDate if possible
assert.sameValue(`${ oneDay.subtract(hours24, { relativeTo: "2019-11-02" }) }`, "PT0S");
assert.sameValue(`${ oneDay.subtract(hours24, {
  relativeTo: {
    year: 2019,
    month: 11,
    day: 2
  }
}) }`, "PT0S");

// throws on wrong offset for ZonedDateTime relativeTo string
assert.throws(RangeError, () => oneDay.subtract(hours24, { relativeTo: "1971-01-01T00:00+02:00[Africa/Monrovia]" }));

// does not throw on HH:MM rounded offset for ZonedDateTime relativeTo string
assert.sameValue(`${ oneDay.subtract(hours24, { relativeTo: "1971-01-01T00:00-00:45[Africa/Monrovia]" }) }`, "PT0S");

// throws on HH:MM rounded offset for ZonedDateTime relativeTo property bag
assert.throws(RangeError, () => oneDay.subtract(hours24, {
  relativeTo: {
    year: 1971,
    month: 1,
    day: 1,
    offset: "-00:45",
    timeZone: "Africa/Monrovia"
  }
}));

// at least the required properties must be present in relativeTo
assert.throws(TypeError, () => oneDay.subtract(hours24, {
  relativeTo: {
    month: 11,
    day: 3
  }
}));
assert.throws(TypeError, () => oneDay.subtract(hours24, {
  relativeTo: {
    year: 2019,
    month: 11
  }
}));
assert.throws(TypeError, () => oneDay.subtract(hours24, {
  relativeTo: {
    year: 2019,
    day: 3
  }
}));
