// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-instant-objects
description: Temporal.Instant.round works
features: [Temporal]
---*/

var inst = Temporal.Instant.from("1976-11-18T14:23:30.123456789Z");

// throws without required smallestUnit parameter
assert.throws(RangeError, () => inst.round({}));
assert.throws(RangeError, () => inst.round({
  roundingIncrement: 1,
  roundingMode: "ceil"
}));

// rounds to an increment of hours
assert.sameValue(`${ inst.round({
  smallestUnit: "hour",
  roundingIncrement: 4
}) }`, "1976-11-18T16:00:00Z");

// rounds to an increment of minutes
assert.sameValue(`${ inst.round({
  smallestUnit: "minute",
  roundingIncrement: 15
}) }`, "1976-11-18T14:30:00Z");

// rounds to an increment of seconds
assert.sameValue(`${ inst.round({
  smallestUnit: "second",
  roundingIncrement: 30
}) }`, "1976-11-18T14:23:30Z");

// rounds to an increment of milliseconds
assert.sameValue(`${ inst.round({
  smallestUnit: "millisecond",
  roundingIncrement: 10
}) }`, "1976-11-18T14:23:30.12Z");

// rounds to an increment of microseconds
assert.sameValue(`${ inst.round({
  smallestUnit: "microsecond",
  roundingIncrement: 10
}) }`, "1976-11-18T14:23:30.12346Z");

// rounds to an increment of nanoseconds
assert.sameValue(`${ inst.round({
  smallestUnit: "nanosecond",
  roundingIncrement: 10
}) }`, "1976-11-18T14:23:30.12345679Z");

// rounds to days by specifying increment of 86400 seconds in various units
var expected = "1976-11-19T00:00:00Z";
assert.sameValue(`${ inst.round({
  smallestUnit: "hour",
  roundingIncrement: 24
}) }`, expected);
assert.sameValue(`${ inst.round({
  smallestUnit: "minute",
  roundingIncrement: 1440
}) }`, expected);
assert.sameValue(`${ inst.round({
  smallestUnit: "second",
  roundingIncrement: 86400
}) }`, expected);
assert.sameValue(`${ inst.round({
  smallestUnit: "millisecond",
  roundingIncrement: 86400000
}) }`, expected);
assert.sameValue(`${ inst.round({
  smallestUnit: "microsecond",
  roundingIncrement: 86400000000
}) }`, expected);
assert.sameValue(`${ inst.round({
  smallestUnit: "nanosecond",
  roundingIncrement: 86400000000000
}) }`, expected);

// allows increments that divide evenly into solar days
assert(inst.round({
  smallestUnit: "second",
  roundingIncrement: 864
}) instanceof Temporal.Instant);

// throws on increments that do not divide evenly into solar days
assert.throws(RangeError, () => inst.round({
  smallestUnit: "hour",
  roundingIncrement: 7
}));
assert.throws(RangeError, () => inst.round({
  smallestUnit: "minute",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => inst.round({
  smallestUnit: "second",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => inst.round({
  smallestUnit: "millisecond",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => inst.round({
  smallestUnit: "microsecond",
  roundingIncrement: 29
}));
assert.throws(RangeError, () => inst.round({
  smallestUnit: "nanosecond",
  roundingIncrement: 29
}));

// accepts plural units
[
  "hour",
  "minute",
  "second",
  "millisecond",
  "microsecond",
  "nanosecond"
].forEach(smallestUnit => {
  assert(inst.round({ smallestUnit }).equals(inst.round({ smallestUnit: `${ smallestUnit }s` })));
});

// accepts string parameter as shortcut for {smallestUnit}
[
  "hour",
  "minute",
  "second",
  "millisecond",
  "microsecond",
  "nanosecond"
].forEach(smallestUnit => {
  assert(inst.round(smallestUnit).equals(inst.round({ smallestUnit })));
});
