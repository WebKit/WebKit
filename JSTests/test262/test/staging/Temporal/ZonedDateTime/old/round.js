// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.round()
features: [Temporal]
---*/

var zdt = Temporal.ZonedDateTime.from("1976-11-18T15:23:30.123456789+01:00[+01:00]");

// throws without parameter
assert.throws(TypeError, () => zdt.round());

// throws without required smallestUnit parameter
assert.throws(RangeError, () => zdt.round({}));
assert.throws(RangeError, () => zdt.round({
  roundingIncrement: 1,
  roundingMode: "ceil"
}));

// throws on disallowed or invalid smallestUnit (string param)
[
  "era",
  "year",
  "month",
  "week",
  "years",
  "months",
  "weeks",
  "nonsense"
].forEach(smallestUnit => {
  assert.throws(RangeError, () => zdt.round(smallestUnit));
});

// rounds to an increment of hours
assert.sameValue(`${ zdt.round({
  smallestUnit: "hour",
  roundingIncrement: 4
}) }`, "1976-11-18T16:00:00+01:00[+01:00]");

// rounds to an increment of minutes
assert.sameValue(`${ zdt.round({
  smallestUnit: "minute",
  roundingIncrement: 15
}) }`, "1976-11-18T15:30:00+01:00[+01:00]");

// rounds to an increment of seconds
assert.sameValue(`${ zdt.round({
  smallestUnit: "second",
  roundingIncrement: 30
}) }`, "1976-11-18T15:23:30+01:00[+01:00]");

// rounds to an increment of milliseconds
assert.sameValue(`${ zdt.round({
  smallestUnit: "millisecond",
  roundingIncrement: 10
}) }`, "1976-11-18T15:23:30.12+01:00[+01:00]");

// rounds to an increment of microseconds
assert.sameValue(`${ zdt.round({
  smallestUnit: "microsecond",
  roundingIncrement: 10
}) }`, "1976-11-18T15:23:30.12346+01:00[+01:00]");

// rounds to an increment of nanoseconds
assert.sameValue(`${ zdt.round({
  smallestUnit: "nanosecond",
  roundingIncrement: 10
}) }`, "1976-11-18T15:23:30.12345679+01:00[+01:00]");

// 1 day is a valid increment
assert.sameValue(`${ zdt.round({
  smallestUnit: "day",
  roundingIncrement: 1
}) }`, "1976-11-19T00:00:00+01:00[+01:00]");

// valid hour increments divide into 24
var smallestUnit = "hour";
[
  1,
  2,
  3,
  4,
  6,
  8,
  12
].forEach(roundingIncrement => {
  assert(zdt.round({
    smallestUnit,
    roundingIncrement
  }) instanceof Temporal.ZonedDateTime);
});
[
  "minute",
  "second"
].forEach(smallestUnit => {
  // valid minutes/seconds increments divide into 60`, () => {
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
      assert(zdt.round({
        smallestUnit,
        roundingIncrement
      }) instanceof Temporal.ZonedDateTime);
    });
  });
[
  "millisecond",
  "microsecond",
  "nanosecond"
].forEach(smallestUnit => {
  // valid increments divide into 1000`
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
      assert(zdt.round({
        smallestUnit,
        roundingIncrement
      }) instanceof Temporal.ZonedDateTime);
    });
  });

// throws on increments that do not divide evenly into the next highest
assert.throws(RangeError, () => zdt.round({
  smallestUnit: "day",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => zdt.round({
  smallestUnit: "hour",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => zdt.round({
  smallestUnit: "minute",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => zdt.round({
  smallestUnit: "second",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => zdt.round({
  smallestUnit: "millisecond",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => zdt.round({
  smallestUnit: "microsecond",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => zdt.round({
  smallestUnit: "nanosecond",
  roundingIncrement: 29
}));

// throws on increments that are equal to the next highest
assert.throws(RangeError, () => zdt.round({
  smallestUnit: "hour",
  roundingIncrement: 24
}));
assert.throws(RangeError, () => zdt.round({
  smallestUnit: "minute",
  roundingIncrement: 60
}));
assert.throws(RangeError, () => zdt.round({
  smallestUnit: "second",
  roundingIncrement: 60
}));
assert.throws(RangeError, () => zdt.round({
  smallestUnit: "millisecond",
  roundingIncrement: 1000
}));
assert.throws(RangeError, () => zdt.round({
  smallestUnit: "microsecond",
  roundingIncrement: 1000
}));
assert.throws(RangeError, () => zdt.round({
  smallestUnit: "nanosecond",
  roundingIncrement: 1000
}));
var bal = Temporal.ZonedDateTime.from("1976-11-18T23:59:59.999999999+01:00[+01:00]");
[
  "day",
  "hour",
  "minute",
  "second",
  "millisecond",
  "microsecond"
].forEach(smallestUnit => {
  assert.sameValue(`${ bal.round({ smallestUnit }) }`, "1976-11-19T00:00:00+01:00[+01:00]");
});
