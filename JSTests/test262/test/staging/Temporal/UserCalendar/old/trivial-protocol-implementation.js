// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Trivial protocol implementation
features: [Temporal]
---*/

function decimalToISO(year, month, day, overflow = "constrain") {
  if (overflow === "constrain") {
    if (month < 1)
      month = 1;
    if (month > 10)
      month = 10;
    if (day < 1)
      day = 1;
    if (day > 10)
      day = 10;
  } else if (overflow === "reject") {
    if (month < 1 || month > 10 || day < 1 || day > 10) {
      throw new RangeError("invalid value");
    }
  }
  var days = year * 100 + (month - 1) * 10 + (day - 1);
  return new Temporal.PlainDate(1970, 1, 1, "iso8601").add({ days });
}
function isoToDecimal(date) {
  var {isoYear, isoMonth, isoDay} = date.getISOFields();
  var isoDate = new Temporal.PlainDate(isoYear, isoMonth, isoDay);
  var {days} = isoDate.since(new Temporal.PlainDate(1970, 1, 1), { largestUnit: "days" });
  var year = Math.floor(days / 100);
  days %= 100;
  return {
    year,
    days
  };
}
var obj = {
  toString() {
    return "decimal";
  },
  dateFromFields(fields, options) {
    var {
      overflow = "constrain"
    } = options ? options : {};
    var {month, monthCode} = fields;
    if (month === undefined)
      month = +monthCode.slice(1);
    var isoDate = decimalToISO(fields.year, month, fields.day, 0, 0, 0, overflow);
    return new Temporal.PlainDate(isoDate.year, isoDate.month, isoDate.day, this);
  },
  yearMonthFromFields(fields, options) {
    var {
      overflow = "constrain"
    } = options ? options : {};
    var {month, monthCode} = fields;
    if (month === undefined)
      month = +monthCode.slice(1);
    var isoDate = decimalToISO(fields.year, month, 1, 0, 0, 0, overflow);
    return new Temporal.PlainYearMonth(isoDate.year, isoDate.month, this, isoDate.day);
  },
  monthDayFromFields(fields, options) {
    var {
      overflow = "constrain"
    } = options ? options : {};
    var {month, monthCode} = fields;
    if (month === undefined)
      month = +monthCode.slice(1);
    var isoDate = decimalToISO(0, month, fields.day, 0, 0, 0, overflow);
    return new Temporal.PlainMonthDay(isoDate.month, isoDate.day, this, isoDate.year);
  },
  year(date) {
    return isoToDecimal(date).year;
  },
  month(date) {
    var {days} = isoToDecimal(date);
    return Math.floor(days / 10) + 1;
  },
  monthCode(date) {
    return `M${ this.month(date).toString().padStart(2, "0") }`;
  },
  day(date) {
    var {days} = isoToDecimal(date);
    return days % 10 + 1;
  }
};
var date = Temporal.PlainDate.from({
  year: 184,
  month: 2,
  day: 9,
  calendar: obj
});
var dt = Temporal.PlainDateTime.from({
  year: 184,
  month: 2,
  day: 9,
  hour: 12,
  calendar: obj
});
var ym = Temporal.PlainYearMonth.from({
  year: 184,
  month: 2,
  calendar: obj
});
var md = Temporal.PlainMonthDay.from({
  monthCode: "M02",
  day: 9,
  calendar: obj
});

// is a calendar
assert.sameValue(typeof obj, "object")

// .id is not available in from()
assert.throws(RangeError, () => Temporal.Calendar.from("decimal"));
assert.throws(RangeError, () => Temporal.Calendar.from("2020-06-05T09:34-00:00[UTC][u-ca=decimal]"));

// Temporal.PlainDate.from()
assert.sameValue(`${ date }`, "2020-06-05[u-ca=decimal]")

// Temporal.PlainDate fields
assert.sameValue(date.year, 184);
assert.sameValue(date.month, 2);
assert.sameValue(date.day, 9);

// date.with()
var date2 = date.with({ year: 0 });
assert.sameValue(date2.year, 0);

// date.withCalendar()
var date2 = Temporal.PlainDate.from("2020-06-05T12:00");
assert(date2.withCalendar(obj).equals(date));

// Temporal.PlainDateTime.from()
assert.sameValue(`${ dt }`, "2020-06-05T12:00:00[u-ca=decimal]")

// Temporal.PlainDateTime fields
assert.sameValue(dt.year, 184);
assert.sameValue(dt.month, 2);
assert.sameValue(dt.day, 9);
assert.sameValue(dt.hour, 12);
assert.sameValue(dt.minute, 0);
assert.sameValue(dt.second, 0);
assert.sameValue(dt.millisecond, 0);
assert.sameValue(dt.microsecond, 0);
assert.sameValue(dt.nanosecond, 0);

// datetime.with()
var dt2 = dt.with({ year: 0 });
assert.sameValue(dt2.year, 0);

// datetime.withCalendar()
var dt2 = Temporal.PlainDateTime.from("2020-06-05T12:00");
assert(dt2.withCalendar(obj).equals(dt));

// Temporal.PlainYearMonth.from()
assert.sameValue(`${ ym }`, "2020-05-28[u-ca=decimal]")

// Temporal.PlainYearMonth fields
assert.sameValue(dt.year, 184);
assert.sameValue(dt.month, 2);

// yearmonth.with()
var ym2 = ym.with({ year: 0 });
assert.sameValue(ym2.year, 0);

// Temporal.PlainMonthDay.from()
assert.sameValue(`${ md }`, "1970-01-19[u-ca=decimal]")

// Temporal.PlainMonthDay fields
assert.sameValue(md.monthCode, "M02");
assert.sameValue(md.day, 9);

// monthday.with()
var md2 = md.with({ monthCode: "M01" });
assert.sameValue(md2.monthCode, "M01");

// timezone.getPlainDateTimeFor()
var tz = Temporal.TimeZone.from("UTC");
var inst = Temporal.Instant.fromEpochSeconds(0);
var dt = tz.getPlainDateTimeFor(inst, obj);
assert.sameValue(dt.calendar.id, obj.id);

// Temporal.Now.plainDateTime()
var nowDateTime = Temporal.Now.plainDateTime(obj, "UTC");
assert.sameValue(nowDateTime.calendar.id, obj.id);

// Temporal.Now.plainDate()
var nowDate = Temporal.Now.plainDate(obj, "UTC");
assert.sameValue(nowDate.calendar.id, obj.id);
