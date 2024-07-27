// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-duration-objects
description: Temporal.Duration.prototype.round() works as expected
features: [Temporal]
---*/

var d = new Temporal.Duration(5, 5, 5, 5, 5, 5, 5, 5, 5, 5);
var d2 = new Temporal.Duration(0, 0, 0, 5, 5, 5, 5, 5, 5, 5);
var relativeTo = Temporal.PlainDate.from("2020-01-01");

// succeeds with largestUnit: 'auto'
assert.sameValue(`${ Temporal.Duration.from({ hours: 25 }).round({ largestUnit: "auto" }) }`, "PT25H");
var hours25 = new Temporal.Duration(0, 0, 0, 0, 25);

// days are 24 hours if relativeTo not given
assert.sameValue(`${ hours25.round({ largestUnit: "days" }) }`, "P1DT1H");

// days are 24 hours if relativeTo is PlainDate
var relativeTo = Temporal.PlainDate.from("2017-01-01");
assert.sameValue(`${ hours25.round({
  largestUnit: "days",
  relativeTo
}) }`, "P1DT1H");

// days are 24 hours if relativeTo is ZonedDateTime, and duration encompasses no DST change
var relativeTo = Temporal.ZonedDateTime.from("2017-01-01T00:00[+04:30]");
assert.sameValue(`${ hours25.round({
  largestUnit: "days",
  relativeTo
}) }`, "P1DT1H");

// casts relativeTo to PlainDate if possible
assert.sameValue(`${ hours25.round({
  largestUnit: "days",
  relativeTo: "2019-11-02"
}) }`, "P1DT1H");
assert.sameValue(`${ hours25.round({
  largestUnit: "days",
  relativeTo: {
    year: 2019,
    month: 11,
    day: 2
  }
}) }`, "P1DT1H");

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
  assert.sameValue(`${ d.round({
    smallestUnit: "seconds",
    relativeTo
  }) }`, "P5Y6M10DT5H5M5S");
});

// does not accept non-string primitives for relativeTo
[
  20200101,
  20200101n,
  null,
  true,
].forEach(relativeTo => {
  assert.throws(
    TypeError, () => d.round({ smallestUnit: "seconds", relativeTo})
  );
});

// throws on wrong offset for ZonedDateTime relativeTo string
assert.throws(RangeError, () => d.round({
  smallestUnit: "seconds",
  relativeTo: "1971-01-01T00:00+02:00[-00:44:30]"
}));

// relativeTo object must contain at least the required correctly-spelled properties
assert.throws(TypeError, () => hours25.round({
  largestUnit: "days",
  relativeTo: {
    month: 11,
    day: 3
  }
}));
assert.throws(TypeError, () => hours25.round({
  largestUnit: "days",
  relativeTo: {
    year: 2019,
    month: 11
  }
}));
assert.throws(TypeError, () => hours25.round({
  largestUnit: "days",
  relativeTo: {
    year: 2019,
    day: 3
  }
}));

// incorrectly-spelled properties are ignored in relativeTo
var oneMonth = Temporal.Duration.from({ months: 1 });
assert.sameValue(`${ oneMonth.round({
  largestUnit: "days",
  relativeTo: {
    year: 2020,
    month: 1,
    day: 1,
    months: 2
  }
}) }`, "P31D");

// throws if neither one of largestUnit or smallestUnit is given
var hoursOnly = new Temporal.Duration(0, 0, 0, 0, 1);
[
  {},
  () => {
  },
  { roundingMode: "ceil" }
].forEach(roundTo => {
  assert.throws(RangeError, () => d.round(roundTo));
  assert.throws(RangeError, () => hoursOnly.round(roundTo));
});

// relativeTo not required to round non-calendar units in durations w/o calendar units (string param)
assert.sameValue(`${ d2.round("days") }`, "P5D");
assert.sameValue(`${ d2.round("hours") }`, "P5DT5H");
assert.sameValue(`${ d2.round("minutes") }`, "P5DT5H5M");
assert.sameValue(`${ d2.round("seconds") }`, "P5DT5H5M5S");
assert.sameValue(`${ d2.round("milliseconds") }`, "P5DT5H5M5.005S");
assert.sameValue(`${ d2.round("microseconds") }`, "P5DT5H5M5.005005S");
assert.sameValue(`${ d2.round("nanoseconds") }`, "P5DT5H5M5.005005005S");

// relativeTo is required to round calendar units even in durations w/o calendar units (string param)
assert.throws(RangeError, () => d2.round("years"));
assert.throws(RangeError, () => d2.round("months"));
assert.throws(RangeError, () => d2.round("weeks"));

// relativeTo not required to round non-calendar units in durations w/o calendar units (object param)
assert.sameValue(`${ d2.round({ smallestUnit: "days" }) }`, "P5D");
assert.sameValue(`${ d2.round({ smallestUnit: "hours" }) }`, "P5DT5H");
assert.sameValue(`${ d2.round({ smallestUnit: "minutes" }) }`, "P5DT5H5M");
assert.sameValue(`${ d2.round({ smallestUnit: "seconds" }) }`, "P5DT5H5M5S");
assert.sameValue(`${ d2.round({ smallestUnit: "milliseconds" }) }`, "P5DT5H5M5.005S");
assert.sameValue(`${ d2.round({ smallestUnit: "microseconds" }) }`, "P5DT5H5M5.005005S");
assert.sameValue(`${ d2.round({ smallestUnit: "nanoseconds" }) }`, "P5DT5H5M5.005005005S");

// relativeTo is required to round calendar units even in durations w/o calendar units (object param)
assert.throws(RangeError, () => d2.round({ smallestUnit: "years" }));
assert.throws(RangeError, () => d2.round({ smallestUnit: "months" }));
assert.throws(RangeError, () => d2.round({ smallestUnit: "weeks" }));

// relativeTo is required for rounding durations with calendar units
assert.throws(RangeError, () => d.round({ largestUnit: "years" }));
assert.throws(RangeError, () => d.round({ largestUnit: "months" }));
assert.throws(RangeError, () => d.round({ largestUnit: "weeks" }));
assert.throws(RangeError, () => d.round({ largestUnit: "days" }));
assert.throws(RangeError, () => d.round({ largestUnit: "hours" }));
assert.throws(RangeError, () => d.round({ largestUnit: "minutes" }));
assert.throws(RangeError, () => d.round({ largestUnit: "seconds" }));
assert.throws(RangeError, () => d.round({ largestUnit: "milliseconds" }));
assert.throws(RangeError, () => d.round({ largestUnit: "microseconds" }));
assert.throws(RangeError, () => d.round({ largestUnit: "nanoseconds" }));

// durations do not balance beyond their current largest unit by default
var relativeTo = Temporal.PlainDate.from("2020-01-01");
var fortyDays = Temporal.Duration.from({ days: 40 });
assert.sameValue(`${ fortyDays.round({ smallestUnit: "seconds" }) }`, "P40D");

// halfExpand is the default
assert.sameValue(`${ d.round({
  smallestUnit: "years",
  relativeTo
}) }`, "P6Y");
assert.sameValue(`${ d.negated().round({
  smallestUnit: "years",
  relativeTo
}) }`, "-P6Y");

// balances up differently depending on relativeTo
var fortyDays = Temporal.Duration.from({ days: 40 });
assert.sameValue(`${ fortyDays.round({
  largestUnit: "years",
  relativeTo: "2020-01-01"
}) }`, "P1M9D");
assert.sameValue(`${ fortyDays.round({
  largestUnit: "years",
  relativeTo: "2020-02-01"
}) }`, "P1M11D");
assert.sameValue(`${ fortyDays.round({
  largestUnit: "years",
  relativeTo: "2020-03-01"
}) }`, "P1M9D");
assert.sameValue(`${ fortyDays.round({
  largestUnit: "years",
  relativeTo: "2020-04-01"
}) }`, "P1M10D");
var minusForty = Temporal.Duration.from({ days: -40 });
assert.sameValue(`${ minusForty.round({
  largestUnit: "years",
  relativeTo: "2020-02-01"
}) }`, "-P1M9D");
assert.sameValue(`${ minusForty.round({
  largestUnit: "years",
  relativeTo: "2020-01-01"
}) }`, "-P1M9D");
assert.sameValue(`${ minusForty.round({
  largestUnit: "years",
  relativeTo: "2020-03-01"
}) }`, "-P1M11D");
assert.sameValue(`${ minusForty.round({
  largestUnit: "years",
  relativeTo: "2020-04-01"
}) }`, "-P1M9D");

// balances up to the next unit after rounding
var almostWeek = Temporal.Duration.from({
  days: 6,
  hours: 20
});
assert.sameValue(`${ almostWeek.round({
  largestUnit: "weeks",
  smallestUnit: "days",
  relativeTo: "2020-01-01"
}) }`, "P1W");

// balances days up to both years and months
var twoYears = Temporal.Duration.from({
  months: 11,
  days: 396
});
assert.sameValue(`${ twoYears.round({
  largestUnit: "years",
  relativeTo: "2017-01-01"
}) }`, "P2Y");

// does not balance up to weeks if largestUnit is larger than weeks
var monthAlmostWeek = Temporal.Duration.from({
  months: 1,
  days: 6,
  hours: 20
});
assert.sameValue(`${ monthAlmostWeek.round({
  smallestUnit: "days",
  relativeTo: "2020-01-01"
}) }`, "P1M7D");

// balances down differently depending on relativeTo
var oneYear = Temporal.Duration.from({ years: 1 });
assert.sameValue(`${ oneYear.round({
  largestUnit: "days",
  relativeTo: "2019-01-01"
}) }`, "P365D");
assert.sameValue(`${ oneYear.round({
  largestUnit: "days",
  relativeTo: "2019-07-01"
}) }`, "P366D");
var minusYear = Temporal.Duration.from({ years: -1 });
assert.sameValue(`${ minusYear.round({
  largestUnit: "days",
  relativeTo: "2020-01-01"
}) }`, "-P365D");
assert.sameValue(`${ minusYear.round({
  largestUnit: "days",
  relativeTo: "2020-07-01"
}) }`, "-P366D");

// rounds to an increment of hours
assert.sameValue(`${ d.round({
  smallestUnit: "hours",
  roundingIncrement: 3,
  relativeTo
}) }`, "P5Y6M10DT6H");

// rounds to an increment of minutes
assert.sameValue(`${ d.round({
  smallestUnit: "minutes",
  roundingIncrement: 30,
  relativeTo
}) }`, "P5Y6M10DT5H");

// rounds to an increment of seconds
assert.sameValue(`${ d.round({
  smallestUnit: "seconds",
  roundingIncrement: 15,
  relativeTo
}) }`, "P5Y6M10DT5H5M");

// rounds to an increment of milliseconds
assert.sameValue(`${ d.round({
  smallestUnit: "milliseconds",
  roundingIncrement: 10,
  relativeTo
}) }`, "P5Y6M10DT5H5M5.01S");

// rounds to an increment of microseconds
assert.sameValue(`${ d.round({
  smallestUnit: "microseconds",
  roundingIncrement: 10,
  relativeTo
}) }`, "P5Y6M10DT5H5M5.00501S");

// rounds to an increment of nanoseconds
assert.sameValue(`${ d.round({
  smallestUnit: "nanoseconds",
  roundingIncrement: 10,
  relativeTo
}) }`, "P5Y6M10DT5H5M5.00500501S");

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
    roundingIncrement,
    relativeTo
  };
  assert(d.round(options) instanceof Temporal.Duration);
});
[
  "minutes",
  "seconds"
].forEach(smallestUnit => {
  // valid minutes/seconds increments divide into 60
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
      var roundTo = {
        smallestUnit,
        roundingIncrement,
        relativeTo
      };
      assert(d.round(roundTo) instanceof Temporal.Duration);
    });
  });
[
  "milliseconds",
  "microseconds",
  "nanoseconds"
].forEach(smallestUnit => {
  // valid milliseconds/microseconds/nanoseconds increments divide into 1000
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
      var roundTo = {
        smallestUnit,
        roundingIncrement,
        relativeTo
      };
      assert(d.round(roundTo) instanceof Temporal.Duration);
    });
  });

// throws on increments that do not divide evenly into the next highest
assert.throws(RangeError, () => d.round({
  relativeTo,
  smallestUnit: "hours",
  roundingIncrement: 11
}));
assert.throws(RangeError, () => d.round({
  relativeTo,
  smallestUnit: "minutes",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => d.round({
  relativeTo,
  smallestUnit: "seconds",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => d.round({
  relativeTo,
  smallestUnit: "milliseconds",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => d.round({
  relativeTo,
  smallestUnit: "microseconds",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => d.round({
  relativeTo,
  smallestUnit: "nanoseconds",
  roundingIncrement: 29
}));

// throws on increments that are equal to the next highest
assert.throws(RangeError, () => d.round({
  relativeTo,
  smallestUnit: "hours",
  roundingIncrement: 24
}));
assert.throws(RangeError, () => d.round({
  relativeTo,
  smallestUnit: "minutes",
  roundingIncrement: 60
}));
assert.throws(RangeError, () => d.round({
  relativeTo,
  smallestUnit: "seconds",
  roundingIncrement: 60
}));
assert.throws(RangeError, () => d.round({
  relativeTo,
  smallestUnit: "milliseconds",
  roundingIncrement: 1000
}));
assert.throws(RangeError, () => d.round({
  relativeTo,
  smallestUnit: "microseconds",
  roundingIncrement: 1000
}));
assert.throws(RangeError, () => d.round({
  relativeTo,
  smallestUnit: "nanoseconds",
  roundingIncrement: 1000
}));

// accepts singular units
assert.sameValue(`${ d.round({
  largestUnit: "year",
  relativeTo
}) }`, `${ d.round({
  largestUnit: "years",
  relativeTo
}) }`);
assert.sameValue(`${ d.round({
  smallestUnit: "year",
  relativeTo
}) }`, `${ d.round({
  smallestUnit: "years",
  relativeTo
}) }`);
assert.sameValue(`${ d.round({
  largestUnit: "month",
  relativeTo
}) }`, `${ d.round({
  largestUnit: "months",
  relativeTo
}) }`);
assert.sameValue(`${ d.round({
  smallestUnit: "month",
  relativeTo
}) }`, `${ d.round({
  smallestUnit: "months",
  relativeTo
}) }`);
assert.sameValue(`${ d.round({
  largestUnit: "day",
  relativeTo
}) }`, `${ d.round({
  largestUnit: "days",
  relativeTo
}) }`);
assert.sameValue(`${ d.round({
  smallestUnit: "day",
  relativeTo
}) }`, `${ d.round({
  smallestUnit: "days",
  relativeTo
}) }`);
assert.sameValue(`${ d.round({
  largestUnit: "hour",
  relativeTo
}) }`, `${ d.round({
  largestUnit: "hours",
  relativeTo
}) }`);
assert.sameValue(`${ d.round({
  smallestUnit: "hour",
  relativeTo
}) }`, `${ d.round({
  smallestUnit: "hours",
  relativeTo
}) }`);
assert.sameValue(`${ d.round({
  largestUnit: "minute",
  relativeTo
}) }`, `${ d.round({
  largestUnit: "minutes",
  relativeTo
}) }`);
assert.sameValue(`${ d.round({
  smallestUnit: "minute",
  relativeTo
}) }`, `${ d.round({
  smallestUnit: "minutes",
  relativeTo
}) }`);
assert.sameValue(`${ d.round({
  largestUnit: "second",
  relativeTo
}) }`, `${ d.round({
  largestUnit: "seconds",
  relativeTo
}) }`);
assert.sameValue(`${ d.round({
  smallestUnit: "second",
  relativeTo
}) }`, `${ d.round({
  smallestUnit: "seconds",
  relativeTo
}) }`);
assert.sameValue(`${ d.round({
  largestUnit: "millisecond",
  relativeTo
}) }`, `${ d.round({
  largestUnit: "milliseconds",
  relativeTo
}) }`);
assert.sameValue(`${ d.round({
  smallestUnit: "millisecond",
  relativeTo
}) }`, `${ d.round({
  smallestUnit: "milliseconds",
  relativeTo
}) }`);
assert.sameValue(`${ d.round({
  largestUnit: "microsecond",
  relativeTo
}) }`, `${ d.round({
  largestUnit: "microseconds",
  relativeTo
}) }`);
assert.sameValue(`${ d.round({
  smallestUnit: "microsecond",
  relativeTo
}) }`, `${ d.round({
  smallestUnit: "microseconds",
  relativeTo
}) }`);
assert.sameValue(`${ d.round({
  largestUnit: "nanosecond",
  relativeTo
}) }`, `${ d.round({
  largestUnit: "nanoseconds",
  relativeTo
}) }`);
assert.sameValue(`${ d.round({
  smallestUnit: "nanosecond",
  relativeTo
}) }`, `${ d.round({
  smallestUnit: "nanoseconds",
  relativeTo
}) }`);

// counts the correct number of days when rounding relative to a date
var days = Temporal.Duration.from({ days: 45 });
assert.sameValue(`${ days.round({
  relativeTo: "2019-01-01",
  smallestUnit: "months"
}) }`, "P2M");
assert.sameValue(`${ days.negated().round({
  relativeTo: "2019-02-15",
  smallestUnit: "months"
}) }`, "-P1M");
var yearAndHalf = Temporal.Duration.from({
  days: 547,
  hours: 12
});
assert.sameValue(`${ yearAndHalf.round({
  relativeTo: "2018-01-01",
  smallestUnit: "years"
}) }`, "P2Y");
assert.sameValue(`${ yearAndHalf.round({
  relativeTo: "2018-07-01",
  smallestUnit: "years"
}) }`, "P1Y");
assert.sameValue(`${ yearAndHalf.round({
  relativeTo: "2019-01-01",
  smallestUnit: "years"
}) }`, "P1Y");
assert.sameValue(`${ yearAndHalf.round({
  relativeTo: "2019-07-01",
  smallestUnit: "years"
}) }`, "P1Y");
assert.sameValue(`${ yearAndHalf.round({
  relativeTo: "2020-01-01",
  smallestUnit: "years"
}) }`, "P1Y");
assert.sameValue(`${ yearAndHalf.round({
  relativeTo: "2020-07-01",
  smallestUnit: "years"
}) }`, "P2Y");
