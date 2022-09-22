// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: calendar with nontrivial mergeFields implementation
features: [Temporal]
---*/

class CenturyCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  toString() {
    return "century";
  }
  century(date) {
    var {isoYear} = date.getISOFields();
    return Math.ceil(isoYear / 100);
  }
  centuryYear(date) {
    var {isoYear} = date.getISOFields();
    return isoYear % 100;
  }
  _validateFields(fields) {
    var {year, century, centuryYear} = fields;
    if (century === undefined !== (centuryYear === undefined)) {
      throw new TypeError("pass either both or neither of century and centuryYear");
    }
    if (year === undefined)
      return (century - 1) * 100 + centuryYear;
    if (century !== undefined) {
      var centuryCalculatedYear = (century - 1) * 100 + centuryYear;
      if (year !== centuryCalculatedYear) {
        throw new RangeError("year must agree with century/centuryYear if both given");
      }
    }
    return year;
  }
  dateFromFields(fields, options) {
    var isoYear = this._validateFields(fields);
    return super.dateFromFields({
      ...fields,
      year: isoYear
    }, options);
  }
  yearMonthFromFields(fields, options) {
    var isoYear = this._validateFields(fields);
    return super.yearMonthFromFields({
      ...fields,
      year: isoYear
    }, options);
  }
  monthDayFromFields(fields, options) {
    var isoYear = this._validateFields(fields);
    return super.monthDayFromFields({
      ...fields,
      year: isoYear
    }, options);
  }
  fields(fields) {
    fields = fields.slice();
    if (fields.includes("year"))
      fields.push("century", "centuryYear");
    return fields;
  }
  mergeFields(fields, additionalFields) {
    var {year, century, centuryYear, ...original} = fields;
    var {
      year: newYear,
      century: newCentury,
      centuryYear: newCenturyYear
    } = additionalFields;
    if (newYear === undefined) {
      original.century = century;
      original.centuryYear = centuryYear;
    }
    if (newCentury === undefined && newCenturyYear === undefined) {
      original.year === year;
    }
    return {
      ...original,
      ...additionalFields
    };
  }
}
var calendar = new CenturyCalendar();
var datetime = new Temporal.PlainDateTime(2019, 9, 15, 0, 0, 0, 0, 0, 0, calendar);
var date = new Temporal.PlainDate(2019, 9, 15, calendar);
var yearmonth = new Temporal.PlainYearMonth(2019, 9, calendar);
var zoned = new Temporal.ZonedDateTime(1568505600000000000n, "UTC", calendar);
var propDesc = {
  century: {
    get() {
      return this.calendar.century(this);
    },
    configurable: true
  },
  centuryYear: {
    get() {
      return this.calendar.centuryYear(this);
    },
    configurable: true
  }
};
Object.defineProperties(Temporal.PlainDateTime.prototype, propDesc);
Object.defineProperties(Temporal.PlainDate.prototype, propDesc);
Object.defineProperties(Temporal.PlainYearMonth.prototype, propDesc);
Object.defineProperties(Temporal.ZonedDateTime.prototype, propDesc);

// property getters work
assert.sameValue(datetime.century, 21);
assert.sameValue(datetime.centuryYear, 19);
assert.sameValue(date.century, 21);
assert.sameValue(date.centuryYear, 19);
assert.sameValue(yearmonth.century, 21);
assert.sameValue(yearmonth.centuryYear, 19);
assert.sameValue(zoned.century, 21);
assert.sameValue(zoned.centuryYear, 19);

// correctly resolves century in with()
assert.sameValue(`${ datetime.with({ century: 20 }) }`, "1919-09-15T00:00:00[u-ca=century]");
assert.sameValue(`${ date.with({ century: 20 }) }`, "1919-09-15[u-ca=century]");
assert.sameValue(`${ yearmonth.with({ century: 20 }) }`, "1919-09-01[u-ca=century]");
assert.sameValue(`${ zoned.with({ century: 20 }) }`, "1919-09-15T00:00:00+00:00[UTC][u-ca=century]");

// correctly resolves centuryYear in with()
assert.sameValue(`${ datetime.with({ centuryYear: 5 }) }`, "2005-09-15T00:00:00[u-ca=century]");
assert.sameValue(`${ date.with({ centuryYear: 5 }) }`, "2005-09-15[u-ca=century]");
assert.sameValue(`${ yearmonth.with({ centuryYear: 5 }) }`, "2005-09-01[u-ca=century]");
assert.sameValue(`${ zoned.with({ centuryYear: 5 }) }`, "2005-09-15T00:00:00+00:00[UTC][u-ca=century]");

// correctly resolves year in with()
assert.sameValue(`${ datetime.with({ year: 1974 }) }`, "1974-09-15T00:00:00[u-ca=century]");
assert.sameValue(`${ date.with({ year: 1974 }) }`, "1974-09-15[u-ca=century]");
assert.sameValue(`${ yearmonth.with({ year: 1974 }) }`, "1974-09-01[u-ca=century]");
assert.sameValue(`${ zoned.with({ year: 1974 }) }`, "1974-09-15T00:00:00+00:00[UTC][u-ca=century]");

delete Temporal.PlainDateTime.prototype.century;
delete Temporal.PlainDateTime.prototype.centuryYear;
delete Temporal.PlainDate.prototype.century;
delete Temporal.PlainDate.prototype.centuryYear;
delete Temporal.PlainYearMonth.prototype.century;
delete Temporal.PlainYearMonth.prototype.centuryYear;
delete Temporal.ZonedDateTime.prototype.century;
delete Temporal.ZonedDateTime.prototype.centuryYear;
