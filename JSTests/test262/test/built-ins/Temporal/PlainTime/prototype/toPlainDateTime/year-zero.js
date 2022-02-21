// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.toplaindatetime
description: Negative zero, as an extended year, is rejected
features: [Temporal, arrow-function]
---*/

const arg = "-000000-10-31";
const instance = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);

assert.throws(
    RangeError,
    () => { instance.toPlainDateTime(arg); },
    "reject minus zero as extended year"
);
