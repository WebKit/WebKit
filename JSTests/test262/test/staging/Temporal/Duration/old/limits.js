// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-duration-objects
description: min/max values
features: [Temporal]
---*/

var units = [
  "years",
  "months",
  "weeks",
  "days",
  "hours",
  "minutes",
  "seconds",
  "milliseconds",
  "microseconds",
  "nanoseconds"
];

// minimum is zero
assert.sameValue(`${ new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0) }`, "PT0S");
units.forEach(unit => assert.sameValue(`${ Temporal.Duration.from({ [unit]: 0 }) }`, "PT0S"));
[
  "P0Y",
  "P0M",
  "P0W",
  "P0D",
  "PT0H",
  "PT0M",
  "PT0S"
].forEach(str => assert.sameValue(`${ Temporal.Duration.from(str) }`, "PT0S"));

// unrepresentable number is not allowed
units.forEach((unit, ix) => {
  assert.throws(RangeError, () => new Temporal.Duration(...Array(ix).fill(0), 1e+400));
  assert.throws(RangeError, () => Temporal.Duration.from({ [unit]: 1e+400 }));
});
var manyNines = "9".repeat(309);
[
  `P${ manyNines }Y`,
  `P${ manyNines }M`,
  `P${ manyNines }W`,
  `P${ manyNines }D`,
  `PT${ manyNines }H`,
  `PT${ manyNines }M`,
  `PT${ manyNines }S`
].forEach(str => assert.throws(RangeError, () => Temporal.Duration.from(str)));

// max safe integer is allowed
[
  "P9007199254740991Y",
  "P9007199254740991M",
  "P9007199254740991W",
  "P9007199254740991D",
  "PT9007199254740991H",
  "PT9007199254740991M",
  "PT9007199254740991S",
  "PT9007199254740.991S",
  "PT9007199254.740991S",
  "PT9007199.254740991S"
].forEach((str, ix) => {
  assert.sameValue(`${ new Temporal.Duration(...Array(ix).fill(0), Number.MAX_SAFE_INTEGER) }`, str);
  assert.sameValue(`${ Temporal.Duration.from(str) }`, str);
});

// larger integers are allowed but may lose precision
function test(ix, prefix, suffix, infix = "") {
  function doAsserts(duration) {
    var str = duration.toString();
    assert.sameValue(str.slice(0, prefix.length + 10), `${ prefix }1000000000`);
    assert(str.includes(infix));
    assert.sameValue(str.slice(-1), suffix);
    assert.sameValue(str.length, prefix.length + suffix.length + infix.length + 27);
  }
  doAsserts(new Temporal.Duration(...Array(ix).fill(0), 1e+26, ...Array(9 - ix).fill(0)));
  doAsserts(Temporal.Duration.from({ [units[ix]]: 1e+26 }));
  if (!infix)
    doAsserts(Temporal.Duration.from(`${ prefix }100000000000001000000000000${ suffix }`));
}
test(0, "P", "Y");
test(1, "P", "M");
test(2, "P", "W");
test(3, "P", "D");
test(4, "PT", "H");
test(5, "PT", "M");
test(6, "PT", "S");
test(7, "PT", "S", ".");
test(8, "PT", "S", ".");
test(9, "PT", "S", ".");
