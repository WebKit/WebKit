// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.DateTimeFormat.prototype.formatToParts
description: TypeError thrown if timeZone.getOffsetNanosecondsFor is not callable
features: [Temporal]
---*/

const formatter = new Intl.DateTimeFormat(undefined, { calendar: "iso8601" });
const date = new Temporal.PlainDate(2021, 8, 4);
const datetime = new Temporal.PlainDateTime(2021, 8, 4, 0, 30, 45, 123, 456, 789);
const monthDay = new Temporal.PlainMonthDay(8, 4);
const time = new Temporal.PlainTime(0, 30, 45, 123, 456, 789);
const month = new Temporal.PlainYearMonth(2021, 8);

Temporal.TimeZone.prototype.getPossibleInstantsFor = function () {
  return [];
};

[undefined, null, true, Math.PI, 'string', Symbol('sym'), 42n, {}].forEach(notCallable => {
  Temporal.TimeZone.prototype.getOffsetNanosecondsFor = notCallable;

  assert.throws(
    TypeError,
    () => formatter.formatToParts(date),
    "Uncallable getOffsetNanosecondsFor should throw TypeError (PlainDate case)"
  );
  assert.throws(
    TypeError,
    () => formatter.formatToParts(datetime),
    "Uncallable getOffsetNanosecondsFor should throw TypeError (PlainDateTime case)"
  );
  assert.throws(
    TypeError,
    () => formatter.formatToParts(monthDay),
    "Uncallable getOffsetNanosecondsFor should throw TypeError (PlainMonthDay case)"
  );
  assert.throws(
    TypeError,
    () => formatter.formatToParts(time),
    "Uncallable getOffsetNanosecondsFor should throw TypeError (PlainTime case)"
  );
  assert.throws(
    TypeError,
    () => formatter.formatToParts(month),
    "Uncallable getOffsetNanosecondsFor should throw TypeError (PlainYearMonth case)"
  );
});
