// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.tozoneddatetime
description: Calendar of the receiver is used
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
const timeCalendar = { toString() { return "iso8601"; } };
const plainDate = new Temporal.PlainDate(2000, 5, 2, calendar);
const result = plainDate.toZonedDateTime({
  timeZone: "UTC",
  plainTime: { hour: 12, minute: 30, calendar: timeCalendar },
});
assert.sameValue(result.epochNanoseconds, 957270600_000_000_000n);
assert.sameValue(result.timeZone.toString(), "UTC");
assert.sameValue(result.calendar, calendar);
