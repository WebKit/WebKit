// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Trivial subclass
features: [Temporal]
---*/

class TwoBasedCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  toString() {
    return "two-based";
  }
  dateFromFields(fields, options) {
    var {year, month, monthCode, day} = fields;
    if (month === undefined)
      month = +monthCode.slice(1);
    return super.dateFromFields({
      year,
      monthCode: `M${ (month - 1).toString().padStart(2, "0") }`,
      day
    }, options);
  }
  yearMonthFromFields(fields, options) {
    var {year, month, monthCode} = fields;
    if (month === undefined)
      month = +monthCode.slice(1);
    return super.yearMonthFromFields({
      year,
      monthCode: `M${ (month - 1).toString().padStart(2, "0") }`
    }, options);
  }
  monthDayFromFields(fields, options) {
    var {month, monthCode, day} = fields;
    if (month === undefined)
      month = +monthCode.slice(1);
    return super.monthDayFromFields({
      monthCode: `M${ (month - 1).toString().padStart(2, "0") }`,
      day
    }, options);
  }
  month(date) {
    return date.getISOFields().isoMonth + 1;
  }
  monthCode(date) {
    return `M${ this.month(date).toString().padStart(2, "0") }`;
  }
}
var obj = new TwoBasedCalendar();
var date = Temporal.PlainDate.from({
  year: 2020,
  month: 5,
  day: 5,
  calendar: obj
});
var dt = Temporal.PlainDateTime.from({
  year: 2020,
  month: 5,
  day: 5,
  hour: 12,
  calendar: obj
});
var ym = Temporal.PlainYearMonth.from({
  year: 2020,
  month: 5,
  calendar: obj
});
var md = Temporal.PlainMonthDay.from({
  monthCode: "M05",
  day: 5,
  calendar: obj
});

// is a calendar
assert.sameValue(typeof obj, "object")

// .id property
assert.sameValue(obj.id, "two-based")

// .id is not available in from()
assert.throws(RangeError, () => Temporal.Calendar.from("two-based"));
assert.throws(RangeError, () => Temporal.Calendar.from("2020-06-05T09:34-07:00[America/Vancouver][u-ca=two-based]"));

// Temporal.PlainDate.from()
assert.sameValue(`${ date }`, "2020-04-05[u-ca=two-based]")

// Temporal.PlainDate fields
assert.sameValue(date.year, 2020);
assert.sameValue(date.month, 5);
assert.sameValue(date.day, 5);

// date.with()
var date2 = date.with({ month: 2 });
assert.sameValue(date2.month, 2);

// date.withCalendar()
var date2 = Temporal.PlainDate.from("2020-04-05");
assert(date2.withCalendar(obj).equals(date));

// Temporal.PlainDateTime.from()
assert.sameValue(`${ dt }`, "2020-04-05T12:00:00[u-ca=two-based]")

// Temporal.PlainDateTime fields
assert.sameValue(dt.year, 2020);
assert.sameValue(dt.month, 5);
assert.sameValue(dt.day, 5);
assert.sameValue(dt.hour, 12);
assert.sameValue(dt.minute, 0);
assert.sameValue(dt.second, 0);
assert.sameValue(dt.millisecond, 0);
assert.sameValue(dt.microsecond, 0);
assert.sameValue(dt.nanosecond, 0);

// datetime.with()
var dt2 = dt.with({ month: 2 });
assert.sameValue(dt2.month, 2);

// datetime.withCalendar()
var dt2 = Temporal.PlainDateTime.from("2020-04-05T12:00");
assert(dt2.withCalendar(obj).equals(dt));

// Temporal.PlainYearMonth.from()
assert.sameValue(`${ ym }`, "2020-04-01[u-ca=two-based]")

// Temporal.PlainYearMonth fields
assert.sameValue(dt.year, 2020);
assert.sameValue(dt.month, 5);

// yearmonth.with()
var ym2 = ym.with({ month: 2 });
assert.sameValue(ym2.month, 2);

// Temporal.PlainMonthDay.from()
assert.sameValue(`${ md }`, "1972-04-05[u-ca=two-based]")

// Temporal.PlainMonthDay fields
assert.sameValue(md.monthCode, "M05");
assert.sameValue(md.day, 5);

// monthday.with()
var md2 = md.with({ monthCode: "M02" });
assert.sameValue(md2.monthCode, "M02");

// timezone.getPlainDateTimeFor()
var tz = Temporal.TimeZone.from("UTC");
var instant = Temporal.Instant.fromEpochSeconds(0);
var dt = tz.getPlainDateTimeFor(instant, obj);
assert.sameValue(dt.calendar.id, obj.id);

// Temporal.Now.plainDateTime()
var nowDateTime = Temporal.Now.plainDateTime(obj, "UTC");
assert.sameValue(nowDateTime.calendar.id, obj.id);

// Temporal.Now.plainDate()
var nowDate = Temporal.Now.plainDate(obj, "UTC");
assert.sameValue(nowDate.calendar.id, obj.id);
