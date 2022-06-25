// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.add
description: >
  Calls calendar's dateFromFields method to obtain a start date for the
  operation, based on the sign of the duration
info: |
  8. Let _fields_ be ? PrepareTemporalFields(_yearMonth_, _fieldNames_, «»).
  9. Let _sign_ be ! DurationSign(_duration_.[[Years]], _duration_.[[Months]], _duration_.[[Weeks]], _balanceResult_.[[Days]], 0, 0, 0, 0, 0, 0).
  10. If _sign_ < 0, then
    a. Let _dayFromCalendar_ be ? CalendarDaysInMonth(_calendar_, _yearMonth_).
    b. Let _day_ be ? ToPositiveInteger(_dayFromCalendar_).
  11. Else,
    a. Let _day_ be 1.
  12. Perform ! CreateDataPropertyOrThrow(_fields_, *"day"*, _day_).
  13. Let _date_ be ? DateFromFields(_calendar_, _fields_, *undefined*).
includes: [deepEqual.js, temporalHelpers.js]
features: [Temporal]
---*/

class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
    this.dateFromFieldsCalls = [];
  }
  year(date) {
    // years in this calendar start and end on the same day as ISO 8601 years
    return date.getISOFields().isoYear;
  }
  month(date) {
    // this calendar has 10 months of 36 days each, plus an 11th month of 5 or 6
    const { isoYear, isoMonth, isoDay } = date.getISOFields();
    const isoDate = new Temporal.PlainDate(isoYear, isoMonth, isoDay);
    return Math.floor((isoDate.dayOfYear - 1) / 36) + 1;
  }
  monthCode(date) {
    return "M" + this.month(date).toString().padStart(2, "0");
  }
  day(date) {
    return (date.dayOfYear - 1) % 36 + 1;
  }
  daysInMonth(date) {
    if (this.month(date) < 11) return 36;
    return this.daysInYear(date) - 360;
  }
  _dateFromFieldsImpl({ year, month, monthCode, day }) {
    if (year === undefined) throw new TypeError("year required");
    if (month === undefined && monthCode === undefined) throw new TypeError("one of month or monthCode required");
    if (month !== undefined && month < 1) throw new RangeError("month < 1");
    if (day === undefined) throw new TypeError("day required");

    if (monthCode !== undefined) {
      const numberPart = +(monthCode.slice(1));
      if ("M" + `${numberPart}`.padStart(2, "0") !== monthCode) throw new RangeError("invalid monthCode");
      if (month === undefined) {
        month = numberPart;
      } else if (month !== numberPart) {
        throw new RangeError("month and monthCode must match");
      }
    }

    const isoDayOfYear = (month - 1) * 36 + day;
    return new Temporal.PlainDate(year, 1, 1).add({ days: isoDayOfYear - 1 }).withCalendar(this);
  }
  dateFromFields(...args) {
    this.dateFromFieldsCalls.push(args);
    return this._dateFromFieldsImpl(...args);
  }
  yearMonthFromFields(fields, options) {
    const { isoYear, isoMonth, isoDay } = this._dateFromFieldsImpl({ ...fields, day: 1 }, options).getISOFields();
    return new Temporal.PlainYearMonth(isoYear, isoMonth, this, isoDay);
  }
  monthDayFromFields(fields, options) {
    const { isoYear, isoMonth, isoDay } = this._dateFromFieldsImpl({ ...fields, year: 2000 }, options).getISOFields();
    return new Temporal.PlainMonthDay(isoMonth, isoDay, this, isoYear);
  }
  dateAdd(date, duration, options) {
    if (duration.months) throw new Error("adding months not implemented in this test");
    return super.dateAdd(date, duration, options);
  }
  toString() {
    return "thirty-six";
  }
}

const calendar = new CustomCalendar();
const month2 = Temporal.PlainYearMonth.from({ year: 2022, month: 2, calendar });
const lessThanOneMonth = new Temporal.Duration(0, 0, 0, 35);
const oneMonth = new Temporal.Duration(0, 0, 0, 36);

// Reference ISO dates in the custom calendar:
// M01 = 01-01
// M02 = 02-06
// M03 = 03-14

calendar.dateFromFieldsCalls = [];
TemporalHelpers.assertPlainYearMonth(
  month2.add(lessThanOneMonth),
  2022, 2, "M02",
  "adding positive less than one month's worth of days yields the same month",
  /* era = */ undefined, /* eraYear = */ undefined, /* referenceISODay = */ 6
);
assert.sameValue(calendar.dateFromFieldsCalls.length, 1, "dateFromFields was called");
assert.deepEqual(
  calendar.dateFromFieldsCalls[0][0],
  { year: 2022, monthCode: "M02", day: 1 },
  "first day of month 2 passed to dateFromFields when adding positive duration"
);
assert.sameValue(calendar.dateFromFieldsCalls[0][1], undefined, "undefined options passed");

calendar.dateFromFieldsCalls = [];
TemporalHelpers.assertPlainYearMonth(
  month2.add(oneMonth),
  2022, 3, "M03",
  "adding positive one month's worth of days yields the following month",
  /* era = */ undefined, /* eraYear = */ undefined, /* referenceISODay = */ 14
);
assert.sameValue(calendar.dateFromFieldsCalls.length, 1, "dateFromFields was called");
assert.deepEqual(
  calendar.dateFromFieldsCalls[0][0],
  { year: 2022, monthCode: "M02", day: 1 },
  "first day of month 2 passed to dateFromFields when adding positive duration"
);
assert.sameValue(calendar.dateFromFieldsCalls[0][1], undefined, "undefined options passed");

calendar.dateFromFieldsCalls = [];
TemporalHelpers.assertPlainYearMonth(
  month2.add(lessThanOneMonth.negated()),
  2022, 2, "M02",
  "adding negative less than one month's worth of days yields the same month",
  /* era = */ undefined, /* eraYear = */ undefined, /* referenceISODay = */ 6
);
assert.sameValue(calendar.dateFromFieldsCalls.length, 1, "dateFromFields was called");
assert.deepEqual(
  calendar.dateFromFieldsCalls[0][0],
  { year: 2022, monthCode: "M02", day: 36 },
  "last day of month 2 passed to dateFromFields when adding negative duration"
);
assert.sameValue(calendar.dateFromFieldsCalls[0][1], undefined, "undefined options passed");

calendar.dateFromFieldsCalls = [];
TemporalHelpers.assertPlainYearMonth(
  month2.add(oneMonth.negated()),
  2022, 1, "M01",
  "adding negative one month's worth of days yields the previous month",
  /* era = */ undefined, /* eraYear = */ undefined, /* referenceISODay = */ 1
);
assert.sameValue(calendar.dateFromFieldsCalls.length, 1, "dateFromFields was called");
assert.deepEqual(
  calendar.dateFromFieldsCalls[0][0],
  { year: 2022, monthCode: "M02", day: 36 },
  "last day of month 2 passed to dateFromFields when adding negative duration"
);
assert.sameValue(calendar.dateFromFieldsCalls[0][1], undefined, "undefined options passed");
