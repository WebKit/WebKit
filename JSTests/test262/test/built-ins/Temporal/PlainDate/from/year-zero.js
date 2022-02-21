// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.from
description: Negative zero, as an extended year, is rejected
features: [Temporal]
---*/

const arg = "-000000-10-31";

assert.throws(
    RangeError,
    () => { Temporal.PlainDate.from(arg); },
    "reject minus zero as extended year"
);

