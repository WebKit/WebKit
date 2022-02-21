// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getinstantfor
description: Appropriate error thrown when argument cannot be converted to Temporal.PlainDateTime
features: [Temporal]
---*/

const timeZone = Temporal.TimeZone.from("UTC");
assert.throws(RangeError, () => timeZone.getInstantFor(undefined), "undefined");
assert.throws(RangeError, () => timeZone.getInstantFor(null), "null");
assert.throws(RangeError, () => timeZone.getInstantFor(true), "boolean");
assert.throws(RangeError, () => timeZone.getInstantFor(""), "empty string");
assert.throws(TypeError, () => timeZone.getInstantFor(Symbol()), "Symbol");
assert.throws(RangeError, () => timeZone.getInstantFor(5), "number");
assert.throws(RangeError, () => timeZone.getInstantFor(5n), "bigint");
assert.throws(TypeError, () => timeZone.getInstantFor({ year: 2020 }), "plain object");
