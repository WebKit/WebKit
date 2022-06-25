// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.subtract
description: Strings with fractional duration units are treated with the correct sign
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.PlainTime();

const resultHours = instance.subtract("-PT24.567890123H");
TemporalHelpers.assertPlainTime(resultHours, 0, 34, 4, 404, 442, 800, "negative fractional hours");

const resultMinutes = instance.subtract("-PT1440.567890123M");
TemporalHelpers.assertPlainTime(resultMinutes, 0, 0, 34, 73, 407, 380, "negative fractional minutes");
