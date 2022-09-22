// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-duration-objects
description: Temporal.Duration.prototype.total()
features: [Temporal]
---*/

var d = new Temporal.Duration(5, 5, 5, 5, 5, 5, 5, 5, 5, 5);
var d2 = new Temporal.Duration(0, 0, 0, 5, 5, 5, 5, 5, 5, 5);
var relativeTo = Temporal.PlainDate.from("2020-01-01");

// throws on disallowed or invalid unit (object param)
[
  "era",
  "nonsense"
].forEach(unit => {
  assert.throws(RangeError, () => d.total({ unit }));
});

// throws on disallowed or invalid unit (string param)
[
  "era",
  "nonsense"
].forEach(unit => {
  assert.throws(RangeError, () => d.total(unit));
});

// does not lose precision for seconds and smaller units
var s = Temporal.Duration.from({
  milliseconds: 2,
  microseconds: 31
}).total({ unit: "seconds" });
assert.sameValue(s, 0.002031);

// accepts datetime string equivalents or fields for relativeTo
[
  "2020-01-01",
  "2020-01-01T00:00:00.000000000",
  20200101n,
  {
    year: 2020,
    month: 1,
    day: 1
  }
].forEach(relativeTo => {
  var daysPastJuly1 = 5 * 7 + 5 - 30;
  var partialDayNanos = d.hours * 3600000000000 + d.minutes * 60000000000 + d.seconds * 1000000000 + d.milliseconds * 1000000 + d.microseconds * 1000 + d.nanoseconds;
  var partialDay = partialDayNanos / (3600000000000 * 24);
  var partialMonth = (daysPastJuly1 + partialDay) / 31;
  var totalMonths = 5 * 12 + 5 + 1 + partialMonth;
  var total = d.total({
    unit: "months",
    relativeTo
  });
  assert.sameValue(total.toPrecision(15), totalMonths.toPrecision(15));
});

// throws on wrong offset for ZonedDateTime relativeTo string
assert.throws(RangeError, () => d.total({
  unit: "months",
  relativeTo: "1971-01-01T00:00+02:00[Africa/Monrovia]"
}));

// does not throw on HH:MM rounded offset for ZonedDateTime relativeTo string
var oneMonth = Temporal.Duration.from({ months: 1 });
assert.sameValue(oneMonth.total({
  unit: "months",
  relativeTo: "1971-01-01T00:00-00:45[Africa/Monrovia]"
}), 1);

// throws on HH:MM rounded offset for ZonedDateTime relativeTo property bag
assert.throws(RangeError, () => d.total({
  unit: "months",
  relativeTo: {
    year: 1971,
    month: 1,
    day: 1,
    offset: "-00:45",
    timeZone: "Africa/Monrovia"
  }
}));

// relativeTo object must contain at least the required correctly-spelled properties
assert.throws(TypeError, () => d.total({
  unit: "months",
  relativeTo: {}
}));
assert.throws(TypeError, () => d.total({
  unit: "months",
  relativeTo: {
    years: 2020,
    month: 1,
    day: 1
  }
}));

// incorrectly-spelled properties are ignored in relativeTo
var oneMonth = Temporal.Duration.from({ months: 1 });
assert.sameValue(oneMonth.total({
  unit: "months",
  relativeTo: {
    year: 2020,
    month: 1,
    day: 1,
    months: 2
  }
}), 1);

// throws RangeError if unit property is missing
[
  {},
  () => {
  },
  { roundingMode: "ceil" }
].forEach(roundTo => assert.throws(RangeError, () => d.total(roundTo)));

// relativeTo required to round calendar units even in durations w/o calendar units (object param)
assert.throws(RangeError, () => d2.total({ unit: "years" }));
assert.throws(RangeError, () => d2.total({ unit: "months" }));
assert.throws(RangeError, () => d2.total({ unit: "weeks" }));

// relativeTo required to round calendar units even in durations w/o calendar units (string param)
assert.throws(RangeError, () => d2.total("years"));
assert.throws(RangeError, () => d2.total("months"));
assert.throws(RangeError, () => d2.total("weeks"));

// relativeTo is required to round durations with calendar units (object param)
assert.throws(RangeError, () => d.total({ unit: "years" }));
assert.throws(RangeError, () => d.total({ unit: "months" }));
assert.throws(RangeError, () => d.total({ unit: "weeks" }));
assert.throws(RangeError, () => d.total({ unit: "days" }));
assert.throws(RangeError, () => d.total({ unit: "hours" }));
assert.throws(RangeError, () => d.total({ unit: "minutes" }));
assert.throws(RangeError, () => d.total({ unit: "seconds" }));
assert.throws(RangeError, () => d.total({ unit: "milliseconds" }));
assert.throws(RangeError, () => d.total({ unit: "microseconds" }));
assert.throws(RangeError, () => d.total({ unit: "nanoseconds" }));

// relativeTo is required to round durations with calendar units (string param)
assert.throws(RangeError, () => d.total("years"));
assert.throws(RangeError, () => d.total("months"));
assert.throws(RangeError, () => d.total("weeks"));
assert.throws(RangeError, () => d.total("days"));
assert.throws(RangeError, () => d.total("hours"));
assert.throws(RangeError, () => d.total("minutes"));
assert.throws(RangeError, () => d.total("seconds"));
assert.throws(RangeError, () => d.total("milliseconds"));
assert.throws(RangeError, () => d.total("microseconds"));
assert.throws(RangeError, () => d.total("nanoseconds"));
var d2Nanoseconds = d2.days * 24 * 3600000000000 + d2.hours * 3600000000000 + d2.minutes * 60000000000 + d2.seconds * 1000000000 + d2.milliseconds * 1000000 + d2.microseconds * 1000 + d2.nanoseconds;
var totalD2 = {
  days: d2Nanoseconds / (24 * 3600000000000),
  hours: d2Nanoseconds / 3600000000000,
  minutes: d2Nanoseconds / 60000000000,
  seconds: d2Nanoseconds / 1000000000,
  milliseconds: d2Nanoseconds / 1000000,
  microseconds: d2Nanoseconds / 1000,
  nanoseconds: d2Nanoseconds
};

// relativeTo not required to round fixed-length units in durations without variable units
assert(Math.abs(d2.total({ unit: "days" }) - totalD2.days) < Number.EPSILON);
assert(Math.abs(d2.total({ unit: "hours" }) - totalD2.hours) < Number.EPSILON);
assert(Math.abs(d2.total({ unit: "minutes" }) - totalD2.minutes) < Number.EPSILON);
assert(Math.abs(d2.total({ unit: "seconds" }) - totalD2.seconds) < Number.EPSILON);
assert(Math.abs(d2.total({ unit: "milliseconds" }) - totalD2.milliseconds) < Number.EPSILON);
assert(Math.abs(d2.total({ unit: "microseconds" }) - totalD2.microseconds) < Number.EPSILON);
assert.sameValue(d2.total({ unit: "nanoseconds" }), totalD2.nanoseconds);

// relativeTo not required to round fixed-length units in durations without variable units (negative)
var negativeD2 = d2.negated();
assert(Math.abs(negativeD2.total({ unit: "days" }) - -totalD2.days) < Number.EPSILON);
assert(Math.abs(negativeD2.total({ unit: "hours" }) - -totalD2.hours) < Number.EPSILON);
assert(Math.abs(negativeD2.total({ unit: "minutes" }) - -totalD2.minutes) < Number.EPSILON);
assert(Math.abs(negativeD2.total({ unit: "seconds" }) - -totalD2.seconds) < Number.EPSILON);
assert(Math.abs(negativeD2.total({ unit: "milliseconds" }) - -totalD2.milliseconds) < Number.EPSILON);
assert(Math.abs(negativeD2.total({ unit: "microseconds" }) - -totalD2.microseconds) < Number.EPSILON);
assert.sameValue(negativeD2.total({ unit: "nanoseconds" }), -totalD2.nanoseconds);
var endpoint = relativeTo.toPlainDateTime().add(d);
var options = unit => ({
  largestUnit: unit,
  smallestUnit: unit,
  roundingMode: "trunc"
});
var fullYears = 5;
var fullDays = endpoint.since(relativeTo, options("days")).days;
var fullMilliseconds = endpoint.since(relativeTo, options("milliseconds")).milliseconds;
var partialDayMilliseconds = fullMilliseconds - fullDays * 24 * 3600000 + 0.005005;
var fractionalDay = partialDayMilliseconds / (24 * 3600000);
var partialYearDays = fullDays - (fullYears * 365 + 2);
var fractionalYear = partialYearDays / 365 + fractionalDay / 365;
var fractionalMonths = ((endpoint.day - 1) * (24 * 3600000) + partialDayMilliseconds) / (31 * 24 * 3600000);
var totalResults = {
  years: fullYears + fractionalYear,
  months: 66 + fractionalMonths,
  weeks: (fullDays + fractionalDay) / 7,
  days: fullDays + fractionalDay,
  hours: fullDays * 24 + partialDayMilliseconds / 3600000,
  minutes: fullDays * 24 * 60 + partialDayMilliseconds / 60000,
  seconds: fullDays * 24 * 60 * 60 + partialDayMilliseconds / 1000,
  milliseconds: fullMilliseconds + 0.005005,
  microseconds: fullMilliseconds * 1000 + 5.005,
  nanoseconds: fullMilliseconds * 1000000 + 5005
};
for (var [unit, expected] of Object.entries(totalResults)) {
  assert.sameValue(d.total({
    unit,
    relativeTo
  }).toPrecision(15), expected.toPrecision(15));
}
for (var unit of [
    "microseconds",
    "nanoseconds"
  ]) {
  assert(d.total({
    unit,
    relativeTo
  }).toString().startsWith("174373505005"));
}

// balances differently depending on relativeTo
var fortyDays = Temporal.Duration.from({ days: 40 });
assert.sameValue(fortyDays.total({
  unit: "months",
  relativeTo: "2020-02-01"
}).toPrecision(16), (1 + 11 / 31).toPrecision(16));
assert.sameValue(fortyDays.total({
  unit: "months",
  relativeTo: "2020-01-01"
}).toPrecision(16), (1 + 9 / 29).toPrecision(16));

// balances differently depending on relativeTo (negative)
var negativeFortyDays = Temporal.Duration.from({ days: -40 });
assert.sameValue(negativeFortyDays.total({
  unit: "months",
  relativeTo: "2020-03-01"
}).toPrecision(16), (-(1 + 11 / 31)).toPrecision(16));
assert.sameValue(negativeFortyDays.total({
  unit: "months",
  relativeTo: "2020-04-01"
}).toPrecision(16), (-(1 + 9 / 29)).toPrecision(16));

var oneDay = new Temporal.Duration(0, 0, 0, 1);
// relativeTo does not affect days if PlainDate
var relativeTo = Temporal.PlainDate.from("2017-01-01");
assert.sameValue(oneDay.total({
  unit: "hours",
  relativeTo
}), 24);

// relativeTo does not affect days if ZonedDateTime, and duration encompasses no DST change
var relativeTo = Temporal.ZonedDateTime.from("2017-01-01T00:00[America/Montevideo]");
assert.sameValue(oneDay.total({
  unit: "hours",
  relativeTo
}), 24);
var skippedHourDay = Temporal.ZonedDateTime.from("2019-03-10T00:00[America/Vancouver]");
var repeatedHourDay = Temporal.ZonedDateTime.from("2019-11-03T00:00[America/Vancouver]");
var inRepeatedHour = Temporal.ZonedDateTime.from("2019-11-03T01:00-07:00[America/Vancouver]");
var hours12 = new Temporal.Duration(0, 0, 0, 0, 12);
var hours25 = new Temporal.Duration(0, 0, 0, 0, 25);

//relativeTo affects days if ZonedDateTime, and duration encompasses DST change"

// start inside repeated hour, end after",  
assert.sameValue(hours25.total({
  unit: "days",
  relativeTo: inRepeatedHour
}), 1);
assert.sameValue(oneDay.total({
  unit: "hours",
  relativeTo: inRepeatedHour
}), 25);

// start after repeated hour, end inside (negative)",  
var relativeTo = Temporal.ZonedDateTime.from("2019-11-04T01:00[America/Vancouver]");
assert.sameValue(hours25.negated().total({
  unit: "days",
  relativeTo
}), -1);
assert.sameValue(oneDay.negated().total({
  unit: "hours",
  relativeTo
}), -25);

// start inside repeated hour, end in skipped hour",  
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

// start in normal hour, end in skipped hour",  
var relativeTo = Temporal.ZonedDateTime.from("2019-03-09T02:30[America/Vancouver]");
var totalDays = hours25.total({
  unit: "days",
  relativeTo
});
assert(Math.abs(totalDays - (1 + 1 / 24)) < Number.EPSILON);
assert.sameValue(oneDay.total({
  unit: "hours",
  relativeTo
}), 24);

// start before skipped hour, end >1 day after",  
var totalDays = hours25.total({
  unit: "days",
  relativeTo: skippedHourDay
});
assert(Math.abs(totalDays - (1 + 2 / 24)) < Number.EPSILON);
assert.sameValue(oneDay.total({
  unit: "hours",
  relativeTo: skippedHourDay
}), 23);

// start after skipped hour, end >1 day before (negative)",  
var relativeTo = Temporal.ZonedDateTime.from("2019-03-11T00:00[America/Vancouver]");
var totalDays = hours25.negated().total({
  unit: "days",
  relativeTo
});
assert(Math.abs(totalDays - (-1 - 2 / 24)) < Number.EPSILON);
assert.sameValue(oneDay.negated().total({
  unit: "hours",
  relativeTo
}), -23);

// start before skipped hour, end <1 day after",  
var totalDays = hours12.total({
  unit: "days",
  relativeTo: skippedHourDay
});
assert(Math.abs(totalDays - 12 / 23) < Number.EPSILON);

// start after skipped hour, end <1 day before (negative)",  
var relativeTo = Temporal.ZonedDateTime.from("2019-03-10T12:00[America/Vancouver]");
var totalDays = hours12.negated().total({
  unit: "days",
  relativeTo
});
assert(Math.abs(totalDays - -12 / 23) < Number.EPSILON);

// start before repeated hour, end >1 day after",  
assert.sameValue(hours25.total({
  unit: "days",
  relativeTo: repeatedHourDay
}), 1);
assert.sameValue(oneDay.total({
  unit: "hours",
  relativeTo: repeatedHourDay
}), 25);

// start after repeated hour, end >1 day before (negative)",  
var relativeTo = Temporal.ZonedDateTime.from("2019-11-04T00:00[America/Vancouver]");
assert.sameValue(hours25.negated().total({
  unit: "days",
  relativeTo
}), -1);
assert.sameValue(oneDay.negated().total({
  unit: "hours",
  relativeTo
}), -25);

// start before repeated hour, end <1 day after",  
var totalDays = hours12.total({
  unit: "days",
  relativeTo: repeatedHourDay
});
assert(Math.abs(totalDays - 12 / 25) < Number.EPSILON);

// start after repeated hour, end <1 day before (negative)",  
var relativeTo = Temporal.ZonedDateTime.from("2019-11-03T12:00[America/Vancouver]");
var totalDays = hours12.negated().total({
  unit: "days",
  relativeTo
});
assert(Math.abs(totalDays - -12 / 25) < Number.EPSILON);

// Samoa skipped 24 hours",  
var relativeTo = Temporal.ZonedDateTime.from("2011-12-29T12:00-10:00[Pacific/Apia]");
var totalDays = hours25.total({
  unit: "days",
  relativeTo
});
assert(Math.abs(totalDays - (2 + 1 / 24)) < Number.EPSILON);
assert.sameValue(Temporal.Duration.from({ hours: 48 }).total({
  unit: "days",
  relativeTo
}), 3);
assert.sameValue(Temporal.Duration.from({ days: 2 }).total({
  unit: "hours",
  relativeTo
}), 24);
assert.sameValue(Temporal.Duration.from({ days: 3 }).total({
  unit: "hours",
  relativeTo
}), 48);

// totaling back up to days
var relativeTo = Temporal.ZonedDateTime.from("2019-11-02T00:00[America/Vancouver]");
assert.sameValue(Temporal.Duration.from({ hours: 48 }).total({ unit: "days" }), 2);
var totalDays = Temporal.Duration.from({ hours: 48 }).total({
  unit: "days",
  relativeTo
});
assert(Math.abs(totalDays - (1 + 24 / 25)) < Number.EPSILON);

// casts relativeTo to ZonedDateTime if possible
assert.sameValue(oneDay.total({
  unit: "hours",
  relativeTo: "2019-11-03T00:00[America/Vancouver]"
}), 25);
assert.sameValue(oneDay.total({
  unit: "hours",
  relativeTo: {
    year: 2019,
    month: 11,
    day: 3,
    timeZone: "America/Vancouver"
  }
}), 25);

// balances up to the next unit after rounding
var almostWeek = Temporal.Duration.from({
  days: 6,
  hours: 20
});
var totalWeeks = almostWeek.total({
  unit: "weeks",
  relativeTo: "2020-01-01"
});
assert(Math.abs(totalWeeks - (6 + 20 / 24) / 7) < Number.EPSILON);

// balances up to the next unit after rounding (negative)
var almostWeek = Temporal.Duration.from({
  days: -6,
  hours: -20
});
var totalWeeks = almostWeek.total({
  unit: "weeks",
  relativeTo: "2020-01-01"
});
assert(Math.abs(totalWeeks - -((6 + 20 / 24) / 7)) < Number.EPSILON);

// balances days up to both years and months
var twoYears = Temporal.Duration.from({
  months: 11,
  days: 396
});
assert.sameValue(twoYears.total({
  unit: "years",
  relativeTo: "2017-01-01"
}), 2);

// balances days up to both years and months (negative)
var twoYears = Temporal.Duration.from({
  months: -11,
  days: -396
});
assert.sameValue(twoYears.total({
  unit: "years",
  relativeTo: "2017-01-01"
}), -2);
