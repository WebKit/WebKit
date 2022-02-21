// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getplaindatetimefor
description: timeZone.getOffsetNanosecondsFor not called when argument cannot be converted to Temporal.Instant
features: [Temporal]
---*/

const timeZone = Temporal.TimeZone.from("UTC");
let called = false;
timeZone.getOffsetNanosecondsFor = () => called = true;
assert.throws(RangeError, () => timeZone.getPlainDateTimeFor(undefined), "undefined");
assert.throws(RangeError, () => timeZone.getPlainDateTimeFor(null), "null");
assert.throws(RangeError, () => timeZone.getPlainDateTimeFor(true), "boolean");
assert.throws(RangeError, () => timeZone.getPlainDateTimeFor(""), "empty string");
assert.throws(TypeError, () => timeZone.getPlainDateTimeFor(Symbol()), "Symbol");
assert.throws(RangeError, () => timeZone.getPlainDateTimeFor(5), "number");
assert.throws(RangeError, () => timeZone.getPlainDateTimeFor(5n), "bigint");
assert.throws(RangeError, () => timeZone.getPlainDateTimeFor({}), "plain object");
assert.sameValue(called, false);
