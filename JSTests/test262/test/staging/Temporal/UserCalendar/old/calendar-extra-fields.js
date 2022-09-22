// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: calendar with extra fields
features: [Temporal]
---*/

class SeasonCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  toString() {
    return "season";
  }
  month(date) {
    var {isoMonth} = date.getISOFields();
    return (isoMonth - 1) % 3 + 1;
  }
  monthCode(date) {
    return `M${ this.month(date).toString().padStart(2, "0") }`;
  }
  season(date) {
    var {isoMonth} = date.getISOFields();
    return Math.floor((isoMonth - 1) / 3) + 1;
  }
  _isoMonthCode(fields) {
    var month = fields.month || +fields.monthCode.slice(1);
    return `M${ ((fields.season - 1) * 3 + month).toString().padStart(2, "0") }`;
  }
  dateFromFields(fields, options) {
    var monthCode = this._isoMonthCode(fields);
    delete fields.month;
    return super.dateFromFields({
      ...fields,
      monthCode
    }, options);
  }
  yearMonthFromFields(fields, options) {
    var monthCode = this._isoMonthCode(fields);
    delete fields.month;
    return super.yearMonthFromFields({
      ...fields,
      monthCode
    }, options);
  }
  monthDayFromFields(fields, options) {
    var monthCode = this._isoMonthCode(fields);
    delete fields.month;
    return super.monthDayFromFields({
      ...fields,
      monthCode
    }, options);
  }
  fields(fields) {
    fields = fields.slice();
    if (fields.includes("month") || fields.includes("monthCode"))
      fields.push("season");
    return fields;
  }
}
var calendar = new SeasonCalendar();
var datetime = new Temporal.PlainDateTime(2019, 9, 15, 0, 0, 0, 0, 0, 0, calendar);
var date = new Temporal.PlainDate(2019, 9, 15, calendar);
var yearmonth = new Temporal.PlainYearMonth(2019, 9, calendar);
var monthday = new Temporal.PlainMonthDay(9, 15, calendar);
var zoned = new Temporal.ZonedDateTime(1568505600000000000n, "UTC", calendar);
var propDesc = {
  get() {
    return this.calendar.season(this);
  },
  configurable: true
};
Object.defineProperty(Temporal.PlainDateTime.prototype, "season", propDesc);
Object.defineProperty(Temporal.PlainDate.prototype, "season", propDesc);
Object.defineProperty(Temporal.PlainYearMonth.prototype, "season", propDesc);
Object.defineProperty(Temporal.PlainMonthDay.prototype, "season", propDesc);
Object.defineProperty(Temporal.ZonedDateTime.prototype, "season", propDesc);

// property getter works
assert.sameValue(datetime.season, 3);
assert.sameValue(datetime.month, 3);
assert.sameValue(datetime.monthCode, "M03");
assert.sameValue(date.season, 3);
assert.sameValue(date.month, 3);
assert.sameValue(date.monthCode, "M03");
assert.sameValue(yearmonth.season, 3);
assert.sameValue(yearmonth.month, 3);
assert.sameValue(yearmonth.monthCode, "M03");
assert.sameValue(monthday.season, 3);
assert.sameValue(monthday.monthCode, "M03");
assert.sameValue(zoned.season, 3);
assert.sameValue(zoned.month, 3);
assert.sameValue(zoned.monthCode, "M03");

// accepts season in from()
assert.sameValue(`${ Temporal.PlainDateTime.from({
  year: 2019,
  season: 3,
  month: 3,
  day: 15,
  calendar
}) }`, "2019-09-15T00:00:00[u-ca=season]");
assert.sameValue(`${ Temporal.PlainDate.from({
  year: 2019,
  season: 3,
  month: 3,
  day: 15,
  calendar
}) }`, "2019-09-15[u-ca=season]");
assert.sameValue(`${ Temporal.PlainYearMonth.from({
  year: 2019,
  season: 3,
  month: 3,
  calendar
}) }`, "2019-09-01[u-ca=season]");
assert.sameValue(`${ Temporal.PlainMonthDay.from({
  season: 3,
  monthCode: "M03",
  day: 15,
  calendar
}) }`, "1972-09-15[u-ca=season]");
assert.sameValue(`${ Temporal.ZonedDateTime.from({
  year: 2019,
  season: 3,
  month: 3,
  day: 15,
  timeZone: "UTC",
  calendar
}) }`, "2019-09-15T00:00:00+00:00[UTC][u-ca=season]");

// accepts season in with()
assert.sameValue(`${ datetime.with({ season: 2 }) }`, "2019-06-15T00:00:00[u-ca=season]");
assert.sameValue(`${ date.with({ season: 2 }) }`, "2019-06-15[u-ca=season]");
assert.sameValue(`${ yearmonth.with({ season: 2 }) }`, "2019-06-01[u-ca=season]");
assert.sameValue(`${ monthday.with({ season: 2 }) }`, "1972-06-15[u-ca=season]");
assert.sameValue(`${ zoned.with({ season: 2 }) }`, "2019-06-15T00:00:00+00:00[UTC][u-ca=season]");

// translates month correctly in with()
assert.sameValue(`${ datetime.with({ month: 2 }) }`, "2019-08-15T00:00:00[u-ca=season]");
assert.sameValue(`${ date.with({ month: 2 }) }`, "2019-08-15[u-ca=season]");
assert.sameValue(`${ yearmonth.with({ month: 2 }) }`, "2019-08-01[u-ca=season]");
assert.sameValue(`${ monthday.with({ monthCode: "M02" }) }`, "1972-08-15[u-ca=season]");
assert.sameValue(`${ zoned.with({ month: 2 }) }`, "2019-08-15T00:00:00+00:00[UTC][u-ca=season]");

delete Temporal.PlainDateTime.prototype.season;
delete Temporal.PlainDate.prototype.season;
delete Temporal.PlainYearMonth.prototype.season;
delete Temporal.PlainMonthDay.prototype.season;
delete Temporal.ZonedDateTime.prototype.season;

