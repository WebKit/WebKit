// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.equals
description: Fuzzy matching behaviour for UTC offset in ISO 8601 string
features: [Temporal]
---*/

const timeZone = new Temporal.TimeZone("+00:44:30.123456789");
const instance = new Temporal.ZonedDateTime(0n, timeZone);

const result = instance.equals("1970-01-01T00:44:30.123456789+00:45[+00:44:30.123456789]");
assert.sameValue(result, true, "UTC offset rounded to minutes is accepted");

assert.throws(RangeError, () => instance.equals("1970-01-01T00:44:30.123456789+00:44:30[+00:44:30.123456789]"), "no other rounding than minutes is accepted");

const properties = { offset: "+00:45", year: 1970, month: 1, day: 1, minute: 44, second: 30, millisecond: 123, microsecond: 456, nanosecond: 123, timeZone };
assert.throws(RangeError, () => instance.equals(properties), "no fuzzy matching is done on offset in property bag");
