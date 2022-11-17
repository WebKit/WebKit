// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.toPlainYearMonth()
features: [Temporal]
---*/

var tz = new Temporal.TimeZone("-08:00");

// works
var zdt = Temporal.Instant.from("2019-10-29T09:46:38.271986102Z").toZonedDateTimeISO(tz);
assert.sameValue(`${ zdt.toPlainYearMonth() }`, "2019-10");

// preserves the calendar
var fakeGregorian = {
  yearMonthFromFields(fields) {
    var ym = Temporal.Calendar.from("iso8601").yearMonthFromFields(fields);
    var {isoYear, isoMonth, isoDay} = ym.getISOFields();
    return new Temporal.PlainYearMonth(isoYear, isoMonth, this, isoDay);
  },
  year(date) { return date.withCalendar("iso8601").year; },
  monthCode(date) { return date.withCalendar("iso8601").monthCode; },
  toString() { return "gregory"; },
};
var zdt = Temporal.Instant.from("2019-10-29T09:46:38.271986102Z").toZonedDateTime({
  timeZone: tz,
  calendar: fakeGregorian
});
assert.sameValue(zdt.toPlainYearMonth().calendar, fakeGregorian);
