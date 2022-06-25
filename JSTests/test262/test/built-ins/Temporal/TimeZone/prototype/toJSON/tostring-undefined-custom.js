// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.tojson
description: TypeError thrown when toString property not present
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  'get [Symbol.toPrimitive]',
  'get toString',
  'get valueOf',
];

const timeZone = new Temporal.TimeZone("UTC");
TemporalHelpers.observeProperty(actual, timeZone, Symbol.toPrimitive, undefined);
TemporalHelpers.observeProperty(actual, timeZone, "toString", undefined);
TemporalHelpers.observeProperty(actual, timeZone, "valueOf", Object.prototype.valueOf);

assert.throws(TypeError, () => timeZone.toJSON());
assert.compareArray(actual, expected);
