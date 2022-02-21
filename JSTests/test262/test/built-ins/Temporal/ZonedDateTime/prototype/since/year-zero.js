// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.since
description: Negative zero, as an extended year, is rejected
features: [Temporal]
---*/

const timeZone = new Temporal.TimeZone("UTC");
const instance = new Temporal.ZonedDateTime(0n, timeZone);
const str = "-0000000-01-01T00:02:00.000000000+00:00[UTC]";

assert.throws(RangeError, () => { instance.since(str); }, "reject minus zero as extended year");
