// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.from
description: A PlainTime object is copied, not returned directly
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const plainTime = Temporal.PlainTime.from("11:42:00");
const result = Temporal.PlainTime.from(plainTime);
assert.notSameValue(result, plainTime);
TemporalHelpers.assertPlainTime(result, 11, 42, 0, 0, 0, 0);
