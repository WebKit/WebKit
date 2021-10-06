// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getoffsetstringfor
description: Appropriate error thrown when argument cannot be converted to Temporal.Instant
features: [Temporal]
---*/

const timeZone = Temporal.TimeZone.from("UTC");
assert.throws(RangeError, () => timeZone.getOffsetStringFor(undefined), "undefined");
assert.throws(RangeError, () => timeZone.getOffsetStringFor(null), "null");
assert.throws(RangeError, () => timeZone.getOffsetStringFor(true), "boolean");
assert.throws(RangeError, () => timeZone.getOffsetStringFor(""), "string");
assert.throws(TypeError, () => timeZone.getOffsetStringFor(Symbol()), "Symbol");
assert.throws(RangeError, () => timeZone.getOffsetStringFor(5), "number");
assert.throws(RangeError, () => timeZone.getOffsetStringFor(5n), "bigint");
assert.throws(RangeError, () => timeZone.getOffsetStringFor({}), "plain object");
