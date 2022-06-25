// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate
description: Limits for the PlainDate constructor.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

assert.throws(RangeError, () => new Temporal.PlainDate(-271821, 4, 18), "min");
assert.throws(RangeError, () => new Temporal.PlainDate(275760, 9, 14), "max");
TemporalHelpers.assertPlainDate(new Temporal.PlainDate(-271821, 4, 19),
  -271821, 4, "M04", 19, "min");
TemporalHelpers.assertPlainDate(new Temporal.PlainDate(275760, 9, 13),
  275760, 9, "M09", 13, "max");
