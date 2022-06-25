// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.tostring
description: Verify that the year is appropriately formatted as 4 or 6 digits
features: [Temporal]
---*/

// For PlainMonthDay, the ISO reference year is only present in the string if
// the calendar is not ISO 8601
class NotISO extends Temporal.Calendar {
  constructor() { super("iso8601"); }
  toString() { return "not-iso"; }
}
const calendar = new NotISO();

let instance = new Temporal.PlainMonthDay(12, 3, calendar, -100000);
assert.sameValue(instance.toString(), "-100000-12-03[u-ca=not-iso]", "large negative year formatted as 6-digit");

instance = new Temporal.PlainMonthDay(4, 5, calendar, -10000);
assert.sameValue(instance.toString(), "-010000-04-05[u-ca=not-iso]", "smallest 5-digit negative year formatted as 6-digit");

instance = new Temporal.PlainMonthDay(6, 7, calendar, -9999);
assert.sameValue(instance.toString(), "-009999-06-07[u-ca=not-iso]", "largest 4-digit negative year formatted as 6-digit");

instance = new Temporal.PlainMonthDay(8, 9, calendar, -1000);
assert.sameValue(instance.toString(), "-001000-08-09[u-ca=not-iso]", "smallest 4-digit negative year formatted as 6-digit");

instance = new Temporal.PlainMonthDay(10, 9, calendar, -999);
assert.sameValue(instance.toString(), "-000999-10-09[u-ca=not-iso]", "largest 3-digit negative year formatted as 6-digit");

instance = new Temporal.PlainMonthDay(8, 7, calendar, -1);
assert.sameValue(instance.toString(), "-000001-08-07[u-ca=not-iso]", "year -1 formatted as 6-digit");

instance = new Temporal.PlainMonthDay(6, 5, calendar, 0);
assert.sameValue(instance.toString(), "0000-06-05[u-ca=not-iso]", "year 0 formatted as 4-digit");

instance = new Temporal.PlainMonthDay(4, 3, calendar, 1);
assert.sameValue(instance.toString(), "0001-04-03[u-ca=not-iso]", "year 1 formatted as 4-digit");

instance = new Temporal.PlainMonthDay(2, 10, calendar, 999);
assert.sameValue(instance.toString(), "0999-02-10[u-ca=not-iso]", "largest 3-digit positive year formatted as 4-digit");

instance = new Temporal.PlainMonthDay(1, 23, calendar, 1000);
assert.sameValue(instance.toString(), "1000-01-23[u-ca=not-iso]", "smallest 4-digit positive year formatted as 4-digit");

instance = new Temporal.PlainMonthDay(4, 5, calendar, 9999);
assert.sameValue(instance.toString(), "9999-04-05[u-ca=not-iso]", "largest 4-digit positive year formatted as 4-digit");

instance = new Temporal.PlainMonthDay(6, 7, calendar, 10000);
assert.sameValue(instance.toString(), "+010000-06-07[u-ca=not-iso]", "smallest 5-digit positive year formatted as 6-digit");

instance = new Temporal.PlainMonthDay(8, 9, calendar, 100000);
assert.sameValue(instance.toString(), "+100000-08-09[u-ca=not-iso]", "large positive year formatted as 6-digit");
