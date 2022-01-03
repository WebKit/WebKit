// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.DateTimeFormat.prototype.formatRangeToParts
description: TypeError thrown if timeZone.getOffsetNanosecondsFor is not callable
features: [Temporal]
---*/

const formatter = new Intl.DateTimeFormat(undefined, { calendar: "iso8601" });
const date1 = new Temporal.PlainDate(2021, 8, 4);
const date2 = new Temporal.PlainDate(2021, 9, 4);
const datetime1 = new Temporal.PlainDateTime(2021, 8, 4, 0, 30, 45, 123, 456, 789);
const datetime2 = new Temporal.PlainDateTime(2021, 9, 4, 0, 30, 45, 123, 456, 789);
const monthDay1 = new Temporal.PlainMonthDay(8, 4);
const monthDay2 = new Temporal.PlainMonthDay(9, 4);
const time1 = new Temporal.PlainTime(0, 30, 45, 123, 456, 789);
const time2 = new Temporal.PlainTime(1, 30, 45, 123, 456, 789);
const month1 = new Temporal.PlainYearMonth(2021, 8);
const month2 = new Temporal.PlainYearMonth(2022, 8);

Temporal.TimeZone.prototype.getPossibleInstantsFor = function () {
  return [];
};

[undefined, null, true, Math.PI, 'string', Symbol('sym'), 42n, {}].forEach(notCallable => {
  Temporal.TimeZone.prototype.getOffsetNanosecondsFor = notCallable;

  assert.throws(
    TypeError,
    () => formatter.formatRangeToParts(date1, date2),
    "Uncallable getOffsetNanosecondsFor should throw TypeError (PlainDate case)"
  );
  assert.throws(
    TypeError,
    () => formatter.formatRangeToParts(datetime1, datetime2),
    "Uncallable getOffsetNanosecondsFor should throw TypeError (PlainDateTime case)"
  );
  assert.throws(
    TypeError,
    () => formatter.formatRangeToParts(monthDay1, monthDay2),
    "Uncallable getOffsetNanosecondsFor should throw TypeError (PlainMonthDay case)"
  );
  assert.throws(
    TypeError,
    () => formatter.formatRangeToParts(time1, time2),
    "Uncallable getOffsetNanosecondsFor should throw TypeError (PlainTime case)"
  );
  assert.throws(
    TypeError,
    () => formatter.formatRangeToParts(month1, month2),
    "Uncallable getOffsetNanosecondsFor should throw TypeError (PlainYearMonth case)"
  );
});
