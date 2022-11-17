// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.toPlainMonthDay()
features: [Temporal]
---*/

var tz = new Temporal.TimeZone("-08:00");

// works
var zdt = Temporal.Instant.from("2019-10-29T09:46:38.271986102Z").toZonedDateTimeISO(tz);
assert.sameValue(`${ zdt.toPlainMonthDay() }`, "10-29");

// preserves the calendar
var fakeGregorian = {
  monthDayFromFields(fields) {
    var md = Temporal.Calendar.from("iso8601").monthDayFromFields(fields);
    var {isoYear, isoMonth, isoDay} = md.getISOFields();
    return new Temporal.PlainMonthDay(isoMonth, isoDay, this, isoYear);
  },
  monthCode(date) { return date.withCalendar("iso8601").monthCode; },
  day(date) { return date.withCalendar("iso8601").day; },
  toString() { return "gregory"; },
};
var zdt = Temporal.Instant.from("2019-10-29T09:46:38.271986102Z").toZonedDateTime({
  timeZone: tz,
  calendar: fakeGregorian
});
assert.sameValue(zdt.toPlainMonthDay().calendar, fakeGregorian);
