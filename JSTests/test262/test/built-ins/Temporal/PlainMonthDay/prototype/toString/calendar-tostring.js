// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.protoype.tostring
description: Number of observable 'toString' calls on the calendar for each value of calendarName
includes: [temporalHelpers.js]
features: [Temporal]
---*/

let calls;
const customCalendar = {
  get id() {
    ++calls;
    return "custom";
  },
  toString() {
    TemporalHelpers.assertUnreachable('toString should not be called');
  },
  dateAdd() {},
  dateFromFields() {},
  dateUntil() {},
  day() {},
  dayOfWeek() {},
  dayOfYear() {},
  daysInMonth() {},
  daysInWeek() {},
  daysInYear() {},
  fields() {},
  inLeapYear() {},
  mergeFields() {},
  month() {},
  monthCode() {},
  monthDayFromFields() {},
  monthsInYear() {},
  weekOfYear() {},
  year() {},
  yearMonthFromFields() {},
  yearOfWeek() {},
};
const monthday = new Temporal.PlainMonthDay(5, 2, customCalendar);
[
  ["always", "1972-05-02[u-ca=custom]", 1],
  ["auto", "1972-05-02[u-ca=custom]", 1],
  ["critical", "1972-05-02[!u-ca=custom]", 1],
  ["never", "1972-05-02", 1],
  [undefined, "1972-05-02[u-ca=custom]", 1],
].forEach(([calendarName, expectedResult, expectedCalls]) => {
  calls = 0;
  const result = monthday.toString({ calendarName });
  assert.sameValue(result, expectedResult, `id for calendarName = ${calendarName}`);
  assert.sameValue(calls, expectedCalls, `calls to id getter for calendarName = ${calendarName}`);
});
