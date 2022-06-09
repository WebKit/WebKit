// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.from
description: A PlainDate object is copied, not returned directly
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const plainDate = new Temporal.PlainDate(2000, 5, 2);
const result = Temporal.PlainDate.from(plainDate);
assert.notSameValue(result, plainDate);
TemporalHelpers.assertPlainDate(result, 2000, 5, "M05", 2);
