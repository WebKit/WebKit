// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.toplainmonthday
description: TypeError thrown when calendar method returns an object with the wrong brand
info: |
    MonthDayFromFields ( calendar, fields, options )

    4. Perform ? RequireInternalSlot(monthDay, [[InitializedTemporalMonthDay]]).
features: [Temporal]
---*/

class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  monthDayFromFields() {
    return {};
  }
}
const zonedDateTime = new Temporal.ZonedDateTime(957270896123456789n, "UTC", new CustomCalendar());
assert.throws(TypeError, () => zonedDateTime.toPlainMonthDay());
