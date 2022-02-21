// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.since
description: Negative zero, as an extended year, is rejected
features: [Temporal, arrow-function]
---*/

const arg = "-000000-10-31";
const instance = new Temporal.PlainDate(2000, 5, 2);

assert.throws(
    RangeError,
    () => { instance.since(arg); },
    "reject minus zero as extended year"
);
