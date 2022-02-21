// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getoffsetnanosecondsfor
description: Appropriate error thrown when argument cannot be converted to Temporal.Instant
features: [Temporal]
---*/

const timeZone = Temporal.TimeZone.from("UTC");
assert.throws(RangeError, () => timeZone.getOffsetNanosecondsFor(undefined), "undefined");
assert.throws(RangeError, () => timeZone.getOffsetNanosecondsFor(null), "null");
assert.throws(RangeError, () => timeZone.getOffsetNanosecondsFor(true), "boolean");
assert.throws(RangeError, () => timeZone.getOffsetNanosecondsFor(""), "empty string");
assert.throws(TypeError, () => timeZone.getOffsetNanosecondsFor(Symbol()), "Symbol");
assert.throws(RangeError, () => timeZone.getOffsetNanosecondsFor(5), "number");
assert.throws(RangeError, () => timeZone.getOffsetNanosecondsFor(5n), "bigint");
assert.throws(RangeError, () => timeZone.getOffsetNanosecondsFor({}), "plain object");
