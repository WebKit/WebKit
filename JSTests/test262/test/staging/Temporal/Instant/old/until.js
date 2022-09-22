// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-instant-objects
description: Temporal.Instant.until() works
features: [Temporal]
---*/

var earlier = Temporal.Instant.from("1969-07-24T16:50:35.123456789Z");
var later = Temporal.Instant.from("2019-10-29T10:46:38.271986102Z");
var diff = earlier.until(later);
assert.sameValue(`${ later.until(earlier) }`, `${ diff.negated() }`)
assert.sameValue(`${ later.since(earlier) }`, `${ diff }`)
assert(earlier.add(diff).equals(later))
assert(later.subtract(diff).equals(earlier))
var feb20 = Temporal.Instant.from("2020-02-01T00:00Z");
var feb21 = Temporal.Instant.from("2021-02-01T00:00Z");

// can return minutes and hours
assert.sameValue(`${ feb20.until(feb21, { largestUnit: "hours" }) }`, "PT8784H");
assert.sameValue(`${ feb20.until(feb21, { largestUnit: "minutes" }) }`, "PT527040M");

// can return subseconds
var latersub = feb20.add({
  hours: 24,
  milliseconds: 250,
  microseconds: 250,
  nanoseconds: 250
});
var msDiff = feb20.until(latersub, { largestUnit: "milliseconds" });
assert.sameValue(msDiff.seconds, 0);
assert.sameValue(msDiff.milliseconds, 86400250);
assert.sameValue(msDiff.microseconds, 250);
assert.sameValue(msDiff.nanoseconds, 250);
var µsDiff = feb20.until(latersub, { largestUnit: "microseconds" });
assert.sameValue(µsDiff.milliseconds, 0);
assert.sameValue(µsDiff.microseconds, 86400250250);
assert.sameValue(µsDiff.nanoseconds, 250);
var nsDiff = feb20.until(latersub, { largestUnit: "nanoseconds" });
assert.sameValue(nsDiff.microseconds, 0);
assert.sameValue(nsDiff.nanoseconds, 86400250250250);

// options may be a function object
assert.sameValue(`${ feb20.until(feb21, () => {
}) }`, "PT31622400S");

// assumes a different default for largestUnit if smallestUnit is larger than seconds
assert.sameValue(`${ earlier.until(later, {
  smallestUnit: "hours",
  roundingMode: "halfExpand"
}) }`, "PT440610H");
assert.sameValue(`${ earlier.until(later, {
  smallestUnit: "minutes",
  roundingMode: "halfExpand"
}) }`, "PT26436596M");
var largestUnit = "hours";

// rounds to an increment of hours
assert.sameValue(`${ earlier.until(later, {
  largestUnit,
  smallestUnit: "hours",
  roundingIncrement: 4,
  roundingMode: "halfExpand"
}) }`, "PT440608H");

// rounds to an increment of minutes
assert.sameValue(`${ earlier.until(later, {
  largestUnit,
  smallestUnit: "minutes",
  roundingIncrement: 30,
  roundingMode: "halfExpand"
}) }`, "PT440610H");

// rounds to an increment of seconds
assert.sameValue(`${ earlier.until(later, {
  largestUnit,
  smallestUnit: "seconds",
  roundingIncrement: 15,
  roundingMode: "halfExpand"
}) }`, "PT440609H56M");

// rounds to an increment of milliseconds
assert.sameValue(`${ earlier.until(later, {
  largestUnit,
  smallestUnit: "milliseconds",
  roundingIncrement: 10,
  roundingMode: "halfExpand"
}) }`, "PT440609H56M3.15S");

// rounds to an increment of microseconds
assert.sameValue(`${ earlier.until(later, {
  largestUnit,
  smallestUnit: "microseconds",
  roundingIncrement: 10,
  roundingMode: "halfExpand"
}) }`, "PT440609H56M3.14853S");

// rounds to an increment of nanoseconds
assert.sameValue(`${ earlier.until(later, {
  largestUnit,
  smallestUnit: "nanoseconds",
  roundingIncrement: 10,
  roundingMode: "halfExpand"
}) }`, "PT440609H56M3.14852931S");

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
    largestUnit,
    smallestUnit: "hours",
    roundingIncrement
  };
  assert(earlier.until(later, options) instanceof Temporal.Duration);
});

// valid increments divide into 60
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
      largestUnit,
      smallestUnit,
      roundingIncrement
    };
    assert(earlier.until(later, options) instanceof Temporal.Duration);
  });
});

// valid increments divide into 1000
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
      largestUnit,
      smallestUnit,
      roundingIncrement
    };
    assert(earlier.until(later, options) instanceof Temporal.Duration);
  });
});

// throws on increments that do not divide evenly into the next highest
assert.throws(RangeError, () => earlier.until(later, {
  largestUnit,
  smallestUnit: "hours",
  roundingIncrement: 11
}));
assert.throws(RangeError, () => earlier.until(later, {
  largestUnit,
  smallestUnit: "minutes",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => earlier.until(later, {
  largestUnit,
  smallestUnit: "seconds",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => earlier.until(later, {
  largestUnit,
  smallestUnit: "milliseconds",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => earlier.until(later, {
  largestUnit,
  smallestUnit: "microseconds",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => earlier.until(later, {
  largestUnit,
  smallestUnit: "nanoseconds",
  roundingIncrement: 29
}));

// throws on increments that are equal to the next highest
assert.throws(RangeError, () => earlier.until(later, {
  largestUnit,
  smallestUnit: "hours",
  roundingIncrement: 24
}));
assert.throws(RangeError, () => earlier.until(later, {
  largestUnit,
  smallestUnit: "minutes",
  roundingIncrement: 60
}));
assert.throws(RangeError, () => earlier.until(later, {
  largestUnit,
  smallestUnit: "seconds",
  roundingIncrement: 60
}));
assert.throws(RangeError, () => earlier.until(later, {
  largestUnit,
  smallestUnit: "milliseconds",
  roundingIncrement: 1000
}));
assert.throws(RangeError, () => earlier.until(later, {
  largestUnit,
  smallestUnit: "microseconds",
  roundingIncrement: 1000
}));
assert.throws(RangeError, () => earlier.until(later, {
  largestUnit,
  smallestUnit: "nanoseconds",
  roundingIncrement: 1000
}));
