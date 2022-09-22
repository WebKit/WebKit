// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthdayfromfields
description: Empty or a function object may be used as options
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

const result1 = instance.monthDayFromFields({ monthCode: "M12", day: 15 }, {});
TemporalHelpers.assertPlainMonthDay(
  result1, "M12", 15,
  "options may be an empty plain object"
);

const result2 = instance.monthDayFromFields({ monthCode: "M12", day: 15 }, () => {});
TemporalHelpers.assertPlainMonthDay(
  result2, "M12", 15,
  "options may be a function object"
);
