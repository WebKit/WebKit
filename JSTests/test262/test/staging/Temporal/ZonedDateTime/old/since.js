// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.since()
features: [Temporal]
---*/

var zdt = Temporal.ZonedDateTime.from("1976-11-18T15:23:30.123456789+01:00[Europe/Vienna]");

// zdt.since(earlier) === earlier.until(zdt) with default options
var earlier = Temporal.ZonedDateTime.from({
  year: 1966,
  month: 3,
  day: 3,
  hour: 18,
  timeZone: "Europe/Vienna"
});
assert.sameValue(`${ zdt.since(earlier) }`, `${ earlier.until(zdt) }`);

// casts argument
assert.sameValue(`${ zdt.since({
  year: 2019,
  month: 10,
  day: 29,
  hour: 10,
  timeZone: "Europe/Vienna"
}) }`, "-PT376434H36M29.876543211S");
assert.sameValue(`${ zdt.since("2019-10-29T10:46:38.271986102+01:00[Europe/Vienna]") }`, "-PT376435H23M8.148529313S");
var feb20 = Temporal.ZonedDateTime.from("2020-02-01T00:00+01:00[Europe/Vienna]");
var feb21 = Temporal.ZonedDateTime.from("2021-02-01T00:00+01:00[Europe/Vienna]");

// defaults to returning hours
assert.sameValue(`${ feb21.since(feb20) }`, "PT8784H");
assert.sameValue(`${ feb21.since(feb20, { largestUnit: "auto" }) }`, "PT8784H");
assert.sameValue(`${ feb21.since(feb20, { largestUnit: "hours" }) }`, "PT8784H");
assert.sameValue(`${ Temporal.ZonedDateTime.from("2021-02-01T00:00:00.000000001+01:00[Europe/Vienna]").since(feb20) }`, "PT8784H0.000000001S");
assert.sameValue(`${ feb21.since(Temporal.ZonedDateTime.from("2020-02-01T00:00:00.000000001+01:00[Europe/Vienna]")) }`, "PT8783H59M59.999999999S");

// can return lower or higher units
assert.sameValue(`${ feb21.since(feb20, { largestUnit: "years" }) }`, "P1Y");
assert.sameValue(`${ feb21.since(feb20, { largestUnit: "months" }) }`, "P12M");
assert.sameValue(`${ feb21.since(feb20, { largestUnit: "weeks" }) }`, "P52W2D");
assert.sameValue(`${ feb21.since(feb20, { largestUnit: "days" }) }`, "P366D");
assert.sameValue(`${ feb21.since(feb20, { largestUnit: "minutes" }) }`, "PT527040M");
assert.sameValue(`${ feb21.since(feb20, { largestUnit: "seconds" }) }`, "PT31622400S");

// can return subseconds
var later = feb20.add({
  days: 1,
  milliseconds: 250,
  microseconds: 250,
  nanoseconds: 250
});
var msDiff = later.since(feb20, { largestUnit: "milliseconds" });
assert.sameValue(msDiff.seconds, 0);
assert.sameValue(msDiff.milliseconds, 86400250);
assert.sameValue(msDiff.microseconds, 250);
assert.sameValue(msDiff.nanoseconds, 250);
var µsDiff = later.since(feb20, { largestUnit: "microseconds" });
assert.sameValue(µsDiff.milliseconds, 0);
assert.sameValue(µsDiff.microseconds, 86400250250);
assert.sameValue(µsDiff.nanoseconds, 250);
var nsDiff = later.since(feb20, { largestUnit: "nanoseconds" });
assert.sameValue(nsDiff.microseconds, 0);
assert.sameValue(nsDiff.nanoseconds, 86400250250250);

// does not include higher units than necessary
var lastFeb20 = Temporal.ZonedDateTime.from("2020-02-29T00:00+01:00[Europe/Vienna]");
var lastFeb21 = Temporal.ZonedDateTime.from("2021-02-28T00:00+01:00[Europe/Vienna]");
assert.sameValue(`${ lastFeb21.since(lastFeb20) }`, "PT8760H");
assert.sameValue(`${ lastFeb21.since(lastFeb20, { largestUnit: "months" }) }`, "P11M28D");
assert.sameValue(`${ lastFeb21.since(lastFeb20, { largestUnit: "years" }) }`, "P11M28D");

// weeks and months are mutually exclusive
var laterDateTime = zdt.add({
  days: 42,
  hours: 3
});
var weeksDifference = laterDateTime.since(zdt, { largestUnit: "weeks" });
assert.notSameValue(weeksDifference.weeks, 0);
assert.sameValue(weeksDifference.months, 0);
var monthsDifference = laterDateTime.since(zdt, { largestUnit: "months" });
assert.sameValue(monthsDifference.weeks, 0);
assert.notSameValue(monthsDifference.months, 0);

// no two different calendars
var zdt1 = new Temporal.ZonedDateTime(0n, "UTC");
var zdt2 = new Temporal.ZonedDateTime(0n, "UTC", Temporal.Calendar.from("japanese"));
assert.throws(RangeError, () => zdt1.since(zdt2));

var earlier = Temporal.ZonedDateTime.from('2019-01-08T09:22:36.123456789+01:00[Europe/Vienna]');
var later = Temporal.ZonedDateTime.from('2021-09-07T14:39:40.987654321+02:00[Europe/Vienna]');
// assumes a different default for largestUnit if smallestUnit is larger than days
assert.sameValue(`${ later.since(earlier, {
  smallestUnit: "years",
  roundingMode: "halfExpand"
}) }`, "P3Y");
assert.sameValue(`${ later.since(earlier, {
  smallestUnit: "months",
  roundingMode: "halfExpand"
}) }`, "P32M");
assert.sameValue(`${ later.since(earlier, {
  smallestUnit: "weeks",
  roundingMode: "halfExpand"
}) }`, "P139W");

// rounds to an increment of hours
assert.sameValue(`${ later.since(earlier, {
  smallestUnit: "hours",
  roundingIncrement: 3,
  roundingMode: "halfExpand"
}) }`, "PT23355H");

// rounds to an increment of minutes
assert.sameValue(`${ later.since(earlier, {
  smallestUnit: "minutes",
  roundingIncrement: 30,
  roundingMode: "halfExpand"
}) }`, "PT23356H30M");

// rounds to an increment of seconds
assert.sameValue(`${ later.since(earlier, {
  smallestUnit: "seconds",
  roundingIncrement: 15,
  roundingMode: "halfExpand"
}) }`, "PT23356H17M");

// rounds to an increment of milliseconds
assert.sameValue(`${ later.since(earlier, {
  smallestUnit: "milliseconds",
  roundingIncrement: 10,
  roundingMode: "halfExpand"
}) }`, "PT23356H17M4.86S");

// rounds to an increment of microseconds
assert.sameValue(`${ later.since(earlier, {
  smallestUnit: "microseconds",
  roundingIncrement: 10,
  roundingMode: "halfExpand"
}) }`, "PT23356H17M4.8642S");

// rounds to an increment of nanoseconds
assert.sameValue(`${ later.since(earlier, {
  smallestUnit: "nanoseconds",
  roundingIncrement: 10,
  roundingMode: "halfExpand"
}) }`, "PT23356H17M4.86419753S");

// valid hour increments divide into 24
[
  1,
  2,
  3,
  4,
  6,
  8,
  12
].forEach(roundingIncrement => {
  var options = {
    smallestUnit: "hours",
    roundingIncrement
  };
  assert(later.since(earlier, options) instanceof Temporal.Duration);
});
[
  "minutes",
  "seconds"
].forEach(smallestUnit => {
  [
    1,
    2,
    3,
    4,
    5,
    6,
    10,
    12,
    15,
    20,
    30
  ].forEach(roundingIncrement => {
    var options = {
      smallestUnit,
      roundingIncrement
    };
    assert(later.since(earlier, options) instanceof Temporal.Duration);
  });
});
[
  "milliseconds",
  "microseconds",
  "nanoseconds"
].forEach(smallestUnit => {
  [
    1,
    2,
    4,
    5,
    8,
    10,
    20,
    25,
    40,
    50,
    100,
    125,
    200,
    250,
    500
  ].forEach(roundingIncrement => {
    var options = {
      smallestUnit,
      roundingIncrement
    };
    assert(later.since(earlier, options) instanceof Temporal.Duration);
  });
});

// throws on increments that do not divide evenly into the next highest
assert.throws(RangeError, () => later.since(earlier, {
  smallestUnit: "hours",
  roundingIncrement: 11
}));
assert.throws(RangeError, () => later.since(earlier, {
  smallestUnit: "minutes",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => later.since(earlier, {
  smallestUnit: "seconds",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => later.since(earlier, {
  smallestUnit: "milliseconds",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => later.since(earlier, {
  smallestUnit: "microseconds",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => later.since(earlier, {
  smallestUnit: "nanoseconds",
  roundingIncrement: 29
}));

// throws on increments that are equal to the next highest
assert.throws(RangeError, () => later.since(earlier, {
  smallestUnit: "hours",
  roundingIncrement: 24
}));
assert.throws(RangeError, () => later.since(earlier, {
  smallestUnit: "minutes",
  roundingIncrement: 60
}));
assert.throws(RangeError, () => later.since(earlier, {
  smallestUnit: "seconds",
  roundingIncrement: 60
}));
assert.throws(RangeError, () => later.since(earlier, {
  smallestUnit: "milliseconds",
  roundingIncrement: 1000
}));
assert.throws(RangeError, () => later.since(earlier, {
  smallestUnit: "microseconds",
  roundingIncrement: 1000
}));
assert.throws(RangeError, () => later.since(earlier, {
  smallestUnit: "nanoseconds",
  roundingIncrement: 1000
}));

// rounds relative to the receiver
var dt1 = Temporal.ZonedDateTime.from("2019-01-01T00:00+00:00[UTC]");
var dt2 = Temporal.ZonedDateTime.from("2020-07-02T00:00+00:00[UTC]");
assert.sameValue(`${ dt2.since(dt1, {
  smallestUnit: "years",
  roundingMode: "halfExpand"
}) }`, "P1Y");
assert.sameValue(`${ dt1.since(dt2, {
  smallestUnit: "years",
  roundingMode: "halfExpand"
}) }`, "-P2Y");

