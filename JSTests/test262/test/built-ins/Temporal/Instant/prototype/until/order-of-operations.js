// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.until
description: Properties on objects passed to until() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get other.toString",
  "call other.toString",
  "get options.largestUnit",
  "get options.largestUnit.toString",
  "call options.largestUnit.toString",
  "get options.roundingIncrement",
  "get options.roundingIncrement.valueOf",
  "call options.roundingIncrement.valueOf",
  "get options.roundingMode",
  "get options.roundingMode.toString",
  "call options.roundingMode.toString",
  "get options.smallestUnit",
  "get options.smallestUnit.toString",
  "call options.smallestUnit.toString",
];
const actual = [];

const instance = new Temporal.Instant(0n);
instance.until(
  TemporalHelpers.toPrimitiveObserver(actual, "2001-09-09T01:46:40Z", "other"),
  TemporalHelpers.propertyBagObserver(actual, {
    largestUnit: "hours",
    roundingIncrement: 1,
    roundingMode: "halfExpand",
    smallestUnit: "minutes",
  }, "options"),
);
assert.compareArray(actual, expected, "order of operations");
