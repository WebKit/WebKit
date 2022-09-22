// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-instant-objects
description: Temporal.Instant.since() works
features: [Temporal]
---*/

var earlier = Temporal.Instant.from("1976-11-18T15:23:30.123456789Z");
var later = Temporal.Instant.from("2019-10-29T10:46:38.271986102Z");
var diff = later.since(earlier);
assert.sameValue(`${ earlier.since(later) }`, `${ diff.negated() }`)
assert.sameValue(`${ earlier.until(later) }`, `${ diff }`)
assert(earlier.add(diff).equals(later))
assert(later.subtract(diff).equals(earlier))
var feb20 = Temporal.Instant.from("2020-02-01T00:00Z");
var feb21 = Temporal.Instant.from("2021-02-01T00:00Z");

// can return minutes and hours
assert.sameValue(`${ feb21.since(feb20, { largestUnit: "hours" }) }`, "PT8784H");
assert.sameValue(`${ feb21.since(feb20, { largestUnit: "minutes" }) }`, "PT527040M");

// can return subseconds
var latersub = feb20.add({
  hours: 24,
  milliseconds: 250,
  microseconds: 250,
  nanoseconds: 250
});
var msDiff = latersub.since(feb20, { largestUnit: "milliseconds" });
assert.sameValue(msDiff.seconds, 0);
assert.sameValue(msDiff.milliseconds, 86400250);
assert.sameValue(msDiff.microseconds, 250);
assert.sameValue(msDiff.nanoseconds, 250);
var µsDiff = latersub.since(feb20, { largestUnit: "microseconds" });
assert.sameValue(µsDiff.milliseconds, 0);
assert.sameValue(µsDiff.microseconds, 86400250250);
assert.sameValue(µsDiff.nanoseconds, 250);
var nsDiff = latersub.since(feb20, { largestUnit: "nanoseconds" });
assert.sameValue(nsDiff.microseconds, 0);
assert.sameValue(nsDiff.nanoseconds, 86400250250250);

// options may be a function object
assert.sameValue(`${ feb21.since(feb20, () => {
}) }`, "PT31622400S");

// assumes a different default for largestUnit if smallestUnit is larger than seconds
assert.sameValue(`${ later.since(earlier, {
  smallestUnit: "hours",
  roundingMode: "halfExpand"
}) }`, "PT376435H");
assert.sameValue(`${ later.since(earlier, {
  smallestUnit: "minutes",
  roundingMode: "halfExpand"
}) }`, "PT22586123M");
var largestUnit = "hours";

// rounds to an increment of hours
assert.sameValue(`${ later.since(earlier, {
  largestUnit,
  smallestUnit: "hours",
  roundingIncrement: 3,
  roundingMode: "halfExpand"
}) }`, "PT376434H");

// rounds to an increment of minutes
assert.sameValue(`${ later.since(earlier, {
  largestUnit,
  smallestUnit: "minutes",
  roundingIncrement: 30,
  roundingMode: "halfExpand"
}) }`, "PT376435H30M");

// rounds to an increment of seconds
assert.sameValue(`${ later.since(earlier, {
  largestUnit,
  smallestUnit: "seconds",
  roundingIncrement: 15,
  roundingMode: "halfExpand"
}) }`, "PT376435H23M15S");

// rounds to an increment of milliseconds
assert.sameValue(`${ later.since(earlier, {
  largestUnit,
  smallestUnit: "milliseconds",
  roundingIncrement: 10,
  roundingMode: "halfExpand"
}) }`, "PT376435H23M8.15S");

// rounds to an increment of microseconds
assert.sameValue(`${ later.since(earlier, {
  largestUnit,
  smallestUnit: "microseconds",
  roundingIncrement: 10,
  roundingMode: "halfExpand"
}) }`, "PT376435H23M8.14853S");

// rounds to an increment of nanoseconds
assert.sameValue(`${ later.since(earlier, {
  largestUnit,
  smallestUnit: "nanoseconds",
  roundingIncrement: 10,
  roundingMode: "halfExpand"
}) }`, "PT376435H23M8.14852931S");

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
  assert(later.since(earlier, options) instanceof Temporal.Duration);
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
    assert(later.since(earlier, options) instanceof Temporal.Duration);
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
    assert(later.since(earlier, options) instanceof Temporal.Duration);
  });
});

// throws on increments that do not divide evenly into the next highest
assert.throws(RangeError, () => later.since(earlier, {
  largestUnit,
  smallestUnit: "hours",
  roundingIncrement: 11
}));
assert.throws(RangeError, () => later.since(earlier, {
  largestUnit,
  smallestUnit: "minutes",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => later.since(earlier, {
  largestUnit,
  smallestUnit: "seconds",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => later.since(earlier, {
  largestUnit,
  smallestUnit: "milliseconds",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => later.since(earlier, {
  largestUnit,
  smallestUnit: "microseconds",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => later.since(earlier, {
  largestUnit,
  smallestUnit: "nanoseconds",
  roundingIncrement: 29
}));

// throws on increments that are equal to the next highest
assert.throws(RangeError, () => later.since(earlier, {
  largestUnit,
  smallestUnit: "hours",
  roundingIncrement: 24
}));
assert.throws(RangeError, () => later.since(earlier, {
  largestUnit,
  smallestUnit: "minutes",
  roundingIncrement: 60
}));
assert.throws(RangeError, () => later.since(earlier, {
  largestUnit,
  smallestUnit: "seconds",
  roundingIncrement: 60
}));
assert.throws(RangeError, () => later.since(earlier, {
  largestUnit,
  smallestUnit: "milliseconds",
  roundingIncrement: 1000
}));
assert.throws(RangeError, () => later.since(earlier, {
  largestUnit,
  smallestUnit: "microseconds",
  roundingIncrement: 1000
}));
assert.throws(RangeError, () => later.since(earlier, {
  largestUnit,
  smallestUnit: "nanoseconds",
  roundingIncrement: 1000
}));
