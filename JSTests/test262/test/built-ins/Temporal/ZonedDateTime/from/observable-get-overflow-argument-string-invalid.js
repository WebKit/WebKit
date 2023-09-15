// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: options properties are extracted with ISO-invalid string argument.
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "ownKeys options",
  "getOwnPropertyDescriptor options.disambiguation",
  "get options.disambiguation",
  "getOwnPropertyDescriptor options.offset",
  "get options.offset",
  "getOwnPropertyDescriptor options.overflow",
  "get options.overflow",
];

let actual = [];
const options = TemporalHelpers.propertyBagObserver(actual, {
  disambiguation: "compatible",
  offset: "ignore",
  overflow: "reject",
}, "options");

assert.throws(RangeError, () => Temporal.ZonedDateTime.from("2020-13-34T25:60:60+99:99[UTC]", options));
assert.compareArray(actual, expected);
