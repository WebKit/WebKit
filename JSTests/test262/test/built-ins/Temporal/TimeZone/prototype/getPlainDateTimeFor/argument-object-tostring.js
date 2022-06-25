// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getplaindatetimefor
description: Object is converted to a string, then to Temporal.Instant
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.TimeZone("UTC");

const arg = {};
assert.throws(RangeError, () => instance.getPlainDateTimeFor(arg), "[object Object] is not a valid ISO string");

arg.toString = function() {
  return "1970-01-01T00:00Z";
};
const result = instance.getPlainDateTimeFor(arg);
TemporalHelpers.assertPlainDateTime(result, 1970, 1, "M01", 1, 0, 0, 0, 0, 0, 0, "result of toString is interpreted as ISO string");
