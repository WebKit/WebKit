// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.withplaindate
description: Negative zero, as an extended year, is rejected
features: [Temporal, arrow-function]
---*/

const arg = "-000000-10-31";
const instance = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC");

assert.throws(
    RangeError,
    () => { instance.withPlainDate(arg); },
    "reject minus zero as extended year"
);
