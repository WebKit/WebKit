// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.from
description: overflow property is extracted with ISO-invalid string argument.
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "ownKeys options",
  "getOwnPropertyDescriptor options.overflow",
  "get options.overflow",
];

let actual = [];
const options = TemporalHelpers.propertyBagObserver(actual, { overflow: "constrain" }, "options");

assert.throws(RangeError, () => Temporal.PlainDate.from("2020-13-34", options));
assert.compareArray(actual, expected);
