// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: options properties are extracted with string argument.
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
  "get options.disambiguation.toString",
  "call options.disambiguation.toString",
  "get options.offset.toString",
  "call options.offset.toString",
  "get options.overflow.toString",
  "call options.overflow.toString",
];

let actual = [];
const options = TemporalHelpers.propertyBagObserver(actual, {
  disambiguation: "compatible",
  offset: "ignore",
  overflow: "reject",
}, "options");

const result = Temporal.ZonedDateTime.from("2001-09-09T01:46:40+00:00[UTC]", options);
assert.compareArray(actual, expected, "Successful call");
assert.sameValue(result.epochNanoseconds, 1_000_000_000_000_000_000n);

actual.splice(0);  // empty it for the next check
const failureExpected = [
  "ownKeys options",
  "getOwnPropertyDescriptor options.disambiguation",
  "get options.disambiguation",
  "getOwnPropertyDescriptor options.offset",
  "get options.offset",
  "getOwnPropertyDescriptor options.overflow",
  "get options.overflow",
];
assert.throws(TypeError, () => Temporal.ZonedDateTime.from(7, options));
assert.compareArray(actual, failureExpected, "Failing call");
