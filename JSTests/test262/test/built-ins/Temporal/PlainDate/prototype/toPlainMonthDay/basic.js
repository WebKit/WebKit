// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.toplainmonthday
description: Basic toPlainMonthDay tests.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
const pd = new Temporal.PlainDate(1970, 12, 24, calendar);
const pmd = pd.toPlainMonthDay();
TemporalHelpers.assertPlainMonthDay(pmd, "M12", 24);
assert.sameValue(pmd.calendar, calendar);
