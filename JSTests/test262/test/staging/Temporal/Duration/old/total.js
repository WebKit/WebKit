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

// accepts datetime strings or fields for relativeTo
[
  "2020-01-01",
  "20200101",
  "2020-01-01T00:00:00.000000000",
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

// does not accept non-string primitives for relativeTo
[
  20200101,
  20200101n,
  null,
  true,
].forEach(relativeTo => {
  assert.throws(
    TypeError, () => d.total({ unit: "months", relativeTo})
  );
});

// throws on wrong offset for ZonedDateTime relativeTo string
assert.throws(RangeError, () => d.total({
  unit: "months",
  relativeTo: "1971-01-01T00:00+02:00[-00:44:30]"
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
