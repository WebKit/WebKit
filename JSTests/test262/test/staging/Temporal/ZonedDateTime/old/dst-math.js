// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: math around DST
features: [Temporal]
---*/

var tz = new Temporal.TimeZone("America/Los_Angeles");
var hourBeforeDstStart = new Temporal.PlainDateTime(2020, 3, 8, 1).toZonedDateTime(tz);
var dayBeforeDstStart = new Temporal.PlainDateTime(2020, 3, 7, 2, 30).toZonedDateTime(tz);

// add 1 hour to get to DST start
var added = hourBeforeDstStart.add({ hours: 1 });
assert.sameValue(added.hour, 3);
var diff = hourBeforeDstStart.until(added, { largestUnit: "hours" });
assert.sameValue(`${ diff }`, "PT1H");
assert.sameValue(`${ diff }`, `${ added.since(hourBeforeDstStart, { largestUnit: "hours" }) }`);
var undo = added.subtract(diff);
assert.sameValue(`${ undo }`, `${ hourBeforeDstStart }`);

// add 2 hours to get to DST start +1
var added = hourBeforeDstStart.add({ hours: 2 });
assert.sameValue(added.hour, 4);
var diff = hourBeforeDstStart.until(added, { largestUnit: "hours" });
assert.sameValue(`${ diff }`, "PT2H");
assert.sameValue(`${ diff }`, `${ added.since(hourBeforeDstStart, { largestUnit: "hours" }) }`);
var undo = added.subtract(diff);
assert.sameValue(`${ undo }`, `${ hourBeforeDstStart }`);

// add 1.5 hours to get to 0.5 hours after DST start
var added = hourBeforeDstStart.add({
  hours: 1,
  minutes: 30
});
assert.sameValue(added.hour, 3);
assert.sameValue(added.minute, 30);
var diff = hourBeforeDstStart.until(added, { largestUnit: "hours" });
assert.sameValue(`${ diff }`, "PT1H30M");
assert.sameValue(`${ diff }`, `${ added.since(hourBeforeDstStart, { largestUnit: "hours" }) }`);
var undo = added.subtract(diff);
assert.sameValue(`${ undo }`, `${ hourBeforeDstStart }`);

// Samoa date line change (add): 10:00PM 29 Dec 2011 -> 11:00PM 31 Dec 2011
var timeZone = Temporal.TimeZone.from("Pacific/Apia");
var dayBeforeSamoaDateLineChangeAbs = timeZone.getInstantFor(new Temporal.PlainDateTime(2011, 12, 29, 22));
var start = dayBeforeSamoaDateLineChangeAbs.toZonedDateTimeISO(timeZone);
var added = start.add({
  days: 1,
  hours: 1
});
assert.sameValue(added.day, 31);
assert.sameValue(added.hour, 23);
assert.sameValue(added.minute, 0);
var diff = start.until(added, { largestUnit: "days" });
assert.sameValue(`${ diff }`, "P2DT1H");
var undo = added.subtract(diff);
assert.sameValue(`${ undo }`, `${ start }`);

// Samoa date line change (subtract): 11:00PM 31 Dec 2011 -> 10:00PM 29 Dec 2011
var timeZone = Temporal.TimeZone.from("Pacific/Apia");
var dayAfterSamoaDateLineChangeAbs = timeZone.getInstantFor(new Temporal.PlainDateTime(2011, 12, 31, 23));
var start = dayAfterSamoaDateLineChangeAbs.toZonedDateTimeISO(timeZone);
var skipped = start.subtract({
  days: 1,
  hours: 1
});
assert.sameValue(skipped.day, 31);
assert.sameValue(skipped.hour, 22);
assert.sameValue(skipped.minute, 0);
var end = start.subtract({
  days: 2,
  hours: 1
});
assert.sameValue(end.day, 29);
assert.sameValue(end.hour, 22);
assert.sameValue(end.minute, 0);
var diff = end.since(start, { largestUnit: "days" });
assert.sameValue(`${ diff }`, "-P2DT1H");
var undo = start.add(diff);
assert.sameValue(`${ undo }`, `${ end }`);

// 3:30 day before DST start -> 3:30 day of DST start
var start = dayBeforeDstStart.add({ hours: 1 });
var added = start.add({ days: 1 });
assert.sameValue(added.day, 8);
assert.sameValue(added.hour, 3);
assert.sameValue(added.minute, 30);
var diff = start.until(added, { largestUnit: "days" });
assert.sameValue(`${ diff }`, "P1D");
var undo = added.subtract(diff);
assert.sameValue(`${ undo }`, `${ start }`);

// 2:30 day before DST start -> 3:30 day of DST start
var added = dayBeforeDstStart.add({ days: 1 });
assert.sameValue(added.day, 8);
assert.sameValue(added.hour, 3);
assert.sameValue(added.minute, 30);
var diff = dayBeforeDstStart.until(added, { largestUnit: "days" });
assert.sameValue(`${ diff }`, "P1D");
var undo = dayBeforeDstStart.add(diff);
assert.sameValue(`${ undo }`, `${ added }`);

// 1:30 day DST starts -> 4:30 day DST starts
var start = dayBeforeDstStart.add({ hours: 23 });
var added = start.add({ hours: 2 });
assert.sameValue(added.day, 8);
assert.sameValue(added.hour, 4);
assert.sameValue(added.minute, 30);
var diff = start.until(added, { largestUnit: "days" });
assert.sameValue(`${ diff }`, "PT2H");
var undo = added.subtract(diff);
assert.sameValue(`${ undo }`, `${ start }`);

// 2:00 day before DST starts -> 3:00 day DST starts
var start = hourBeforeDstStart.subtract({ days: 1 }).add({ hours: 1 });
var added = start.add({ days: 1 });
assert.sameValue(added.day, 8);
assert.sameValue(added.hour, 3);
assert.sameValue(added.minute, 0);
var diff = start.until(added, { largestUnit: "days" });
assert.sameValue(`${ diff }`, "P1D");
var undo = start.add(diff);
assert.sameValue(`${ undo }`, `${ added }`);

// 1:00AM day DST starts -> (add 24 hours) -> 2:00AM day after DST starts
var start = hourBeforeDstStart;
var added = start.add({ hours: 24 });
assert.sameValue(added.day, 9);
assert.sameValue(added.hour, 2);
assert.sameValue(added.minute, 0);
var diff = start.until(added, { largestUnit: "days" });
assert.sameValue(`${ diff }`, "P1DT1H");
var undo = added.subtract(diff);
assert.sameValue(`${ undo }`, `${ start }`);

// 12:00AM day DST starts -> (add 24 hours) -> 1:00AM day after DST starts
var start = hourBeforeDstStart.subtract({ hours: 1 });
var added = start.add({ hours: 24 });
assert.sameValue(added.day, 9);
assert.sameValue(added.hour, 1);
assert.sameValue(added.minute, 0);
var diff = start.until(added, { largestUnit: "days" });
assert.sameValue(`${ diff }`, "P1DT1H");
var undo = added.subtract(diff);
assert.sameValue(`${ undo }`, `${ start }`);

// Difference can return day length > 24 hours
var start = Temporal.ZonedDateTime.from("2020-10-30T01:45-07:00[America/Los_Angeles]");
var end = Temporal.ZonedDateTime.from("2020-11-02T01:15-08:00[America/Los_Angeles]");
var diff = start.until(end, { largestUnit: "days" });
assert.sameValue(`${ diff }`, "P2DT24H30M");
var undo = start.add(diff);
assert.sameValue(`${ undo }`, `${ end }`);

// Difference rounding (nearest day) is DST-aware
var start = Temporal.ZonedDateTime.from("2020-03-10T02:30-07:00[America/Los_Angeles]");
var end = Temporal.ZonedDateTime.from("2020-03-07T14:15-08:00[America/Los_Angeles]");
var diff = start.until(end, {
  smallestUnit: "days",
  roundingMode: "halfExpand"
});
assert.sameValue(`${ diff }`, "-P3D");

// Difference rounding (ceil day) is DST-aware
var start = Temporal.ZonedDateTime.from("2020-03-10T02:30-07:00[America/Los_Angeles]");
var end = Temporal.ZonedDateTime.from("2020-03-07T14:15-08:00[America/Los_Angeles]");
var diff = start.until(end, {
  smallestUnit: "days",
  roundingMode: "ceil"
});
assert.sameValue(`${ diff }`, "-P2D");

// Difference rounding (trunc day) is DST-aware
var start = Temporal.ZonedDateTime.from("2020-03-10T02:30-07:00[America/Los_Angeles]");
var end = Temporal.ZonedDateTime.from("2020-03-07T14:15-08:00[America/Los_Angeles]");
var diff = start.until(end, {
  smallestUnit: "days",
  roundingMode: "trunc"
});
assert.sameValue(`${ diff }`, "-P2D");

// Difference rounding (floor day) is DST-aware
var start = Temporal.ZonedDateTime.from("2020-03-10T02:30-07:00[America/Los_Angeles]");
var end = Temporal.ZonedDateTime.from("2020-03-07T14:15-08:00[America/Los_Angeles]");
var diff = start.until(end, {
  smallestUnit: "days",
  roundingMode: "floor"
});
assert.sameValue(`${ diff }`, "-P3D");

// Difference rounding (nearest hour) is DST-aware
var start = Temporal.ZonedDateTime.from("2020-03-10T02:30-07:00[America/Los_Angeles]");
var end = Temporal.ZonedDateTime.from("2020-03-07T14:15-08:00[America/Los_Angeles]");
var diff = start.until(end, {
  largestUnit: "days",
  smallestUnit: "hours",
  roundingMode: "halfExpand"
});
assert.sameValue(`${ diff }`, "-P2DT12H");

// Difference rounding (ceil hour) is DST-aware
var start = Temporal.ZonedDateTime.from("2020-03-10T02:30-07:00[America/Los_Angeles]");
var end = Temporal.ZonedDateTime.from("2020-03-07T14:15-08:00[America/Los_Angeles]");
var diff = start.until(end, {
  largestUnit: "days",
  smallestUnit: "hours",
  roundingMode: "ceil"
});
assert.sameValue(`${ diff }`, "-P2DT12H");

// Difference rounding (trunc hour) is DST-aware
var start = Temporal.ZonedDateTime.from("2020-03-10T02:30-07:00[America/Los_Angeles]");
var end = Temporal.ZonedDateTime.from("2020-03-07T14:15-08:00[America/Los_Angeles]");
var diff = start.until(end, {
  largestUnit: "days",
  smallestUnit: "hours",
  roundingMode: "trunc"
});
assert.sameValue(`${ diff }`, "-P2DT12H");

// Difference rounding (floor hour) is DST-aware
var start = Temporal.ZonedDateTime.from("2020-03-10T02:30-07:00[America/Los_Angeles]");
var end = Temporal.ZonedDateTime.from("2020-03-07T14:15-08:00[America/Los_Angeles]");
var diff = start.until(end, {
  largestUnit: "days",
  smallestUnit: "hours",
  roundingMode: "floor"
});
assert.sameValue(`${ diff }`, "-P2DT13H");

// Difference when date portion ends inside a DST-skipped period
var start = Temporal.ZonedDateTime.from("2020-03-07T02:30-08:00[America/Los_Angeles]");
var end = Temporal.ZonedDateTime.from("2020-03-08T03:15-07:00[America/Los_Angeles]");
var diff = start.until(end, { largestUnit: "days" });
assert.sameValue(`${ diff }`, "PT23H45M");

// Difference when date portion ends inside day skipped by Samoa's 24hr 2011 transition
var end = Temporal.ZonedDateTime.from("2011-12-31T05:00+14:00[Pacific/Apia]");
var start = Temporal.ZonedDateTime.from("2011-12-28T10:00-10:00[Pacific/Apia]");
var diff = start.until(end, { largestUnit: "days" });
assert.sameValue(`${ diff }`, "P1DT19H");

// Rounding up to hours causes one more day of overflow (positive)
var start = Temporal.ZonedDateTime.from("2020-01-01T00:00-08:00[America/Los_Angeles]");
var end = Temporal.ZonedDateTime.from("2020-01-03T23:59-08:00[America/Los_Angeles]");
var diff = start.until(end, {
  largestUnit: "days",
  smallestUnit: "hours",
  roundingMode: "halfExpand"
});
assert.sameValue(`${ diff }`, "P3D");

// Rounding up to hours causes one more day of overflow (negative)
var start = Temporal.ZonedDateTime.from("2020-01-01T00:00-08:00[America/Los_Angeles]");
var end = Temporal.ZonedDateTime.from("2020-01-03T23:59-08:00[America/Los_Angeles]");
var diff = end.until(start, {
  largestUnit: "days",
  smallestUnit: "hours",
  roundingMode: "halfExpand"
});
assert.sameValue(`${ diff }`, "-P3D");

// addition and difference work near DST start
var stepsPerHour = 2;
var minutesPerStep = 60 / stepsPerHour;
var hoursUntilEnd = 26;
var startHourRange = 3;
for (var i = 0; i < startHourRange * stepsPerHour; i++) {
  var start = hourBeforeDstStart.add({ minutes: minutesPerStep * i });
  for (var j = 0; j < hoursUntilEnd * stepsPerHour; j++) {
    var end = start.add({ minutes: j * minutesPerStep });
    var diff = start.until(end, { largestUnit: "days" });
    var expectedMinutes = minutesPerStep * (j % stepsPerHour);
    assert.sameValue(diff.minutes, expectedMinutes);
    var diff60 = Math.floor(j / stepsPerHour);
    if (i >= stepsPerHour) {
      var expectedDays = diff60 < 24 ? 0 : diff60 < 48 ? 1 : 2;
      var expectedHours = diff60 < 24 ? diff60 : diff60 < 48 ? diff60 - 24 : diff60 - 48;
      assert.sameValue(diff.hours, expectedHours);
      assert.sameValue(diff.days, expectedDays);
    } else {
      var expectedDays = diff60 < 23 ? 0 : diff60 < 47 ? 1 : 2;
      var expectedHours = diff60 < 23 ? diff60 : diff60 < 47 ? diff60 - 23 : diff60 - 47;
      assert.sameValue(diff.hours, expectedHours);
      assert.sameValue(diff.days, expectedDays);
    }
  }
}
