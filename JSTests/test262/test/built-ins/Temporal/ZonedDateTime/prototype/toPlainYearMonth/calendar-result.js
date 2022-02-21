// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.toplainyearmonth
description: TypeError thrown when calendar method returns an object with the wrong brand
info: |
    YearMonthFromFields ( calendar, fields [ , options ] )

    4. Perform ? RequireInternalSlot(yearMonth, [[InitializedTemporalYearMonth]]).
features: [Temporal]
---*/

class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  yearMonthFromFields() {
    return {};
  }
}
const zonedDateTime = new Temporal.ZonedDateTime(957270896123456789n, "UTC", new CustomCalendar());
assert.throws(TypeError, () => zonedDateTime.toPlainYearMonth());
