// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    This defines helper objects and functions for testing Temporal.
defines: [TemporalHelpers]
features: [Symbol.species, Symbol.iterator, Temporal]
---*/

var TemporalHelpers = {
  /*
   * assertDuration(duration, years, ...,  nanoseconds[, description]):
   *
   * Shorthand for asserting that each field of a Temporal.Duration is equal to
   * an expected value.
   */
  assertDuration(duration, years, months, weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds, description = "") {
    assert(duration instanceof Temporal.Duration, `${description} instanceof`);
    assert.sameValue(duration.years, years, `${description} years result`);
    assert.sameValue(duration.months, months, `${description} months result`);
    assert.sameValue(duration.weeks, weeks, `${description} weeks result`);
    assert.sameValue(duration.days, days, `${description} days result`);
    assert.sameValue(duration.hours, hours, `${description} hours result`);
    assert.sameValue(duration.minutes, minutes, `${description} minutes result`);
    assert.sameValue(duration.seconds, seconds, `${description} seconds result`);
    assert.sameValue(duration.milliseconds, milliseconds, `${description} milliseconds result`);
    assert.sameValue(duration.microseconds, microseconds, `${description} microseconds result`);
    assert.sameValue(duration.nanoseconds, nanoseconds, `${description} nanoseconds result`);
  },

  /*
   * assertPlainDate(date, year, ..., nanosecond[, description[, era, eraYear]]):
   *
   * Shorthand for asserting that each field of a Temporal.PlainDate is equal to
   * an expected value. (Except the `calendar` property, since callers may want
   * to assert either object equality with an object they put in there, or the
   * result of date.calendar.toString().)
   */
  assertPlainDate(date, year, month, monthCode, day, description = "", era = undefined, eraYear = undefined) {
    assert(date instanceof Temporal.PlainDate, `${description} instanceof`);
    assert.sameValue(date.era, era, `${description} era result`);
    assert.sameValue(date.eraYear, eraYear, `${description} eraYear result`);
    assert.sameValue(date.year, year, `${description} year result`);
    assert.sameValue(date.month, month, `${description} month result`);
    assert.sameValue(date.monthCode, monthCode, `${description} monthCode result`);
    assert.sameValue(date.day, day, `${description} day result`);
  },

  /*
   * assertPlainDateTime(datetime, year, ..., nanosecond[, description[, era, eraYear]]):
   *
   * Shorthand for asserting that each field of a Temporal.PlainDateTime is
   * equal to an expected value. (Except the `calendar` property, since callers
   * may want to assert either object equality with an object they put in there,
   * or the result of datetime.calendar.toString().)
   */
  assertPlainDateTime(datetime, year, month, monthCode, day, hour, minute, second, millisecond, microsecond, nanosecond, description = "", era = undefined, eraYear = undefined) {
    assert(datetime instanceof Temporal.PlainDateTime, `${description} instanceof`);
    assert.sameValue(datetime.era, era, `${description} era result`);
    assert.sameValue(datetime.eraYear, eraYear, `${description} eraYear result`);
    assert.sameValue(datetime.year, year, `${description} year result`);
    assert.sameValue(datetime.month, month, `${description} month result`);
    assert.sameValue(datetime.monthCode, monthCode, `${description} monthCode result`);
    assert.sameValue(datetime.day, day, `${description} day result`);
    assert.sameValue(datetime.hour, hour, `${description} hour result`);
    assert.sameValue(datetime.minute, minute, `${description} minute result`);
    assert.sameValue(datetime.second, second, `${description} second result`);
    assert.sameValue(datetime.millisecond, millisecond, `${description} millisecond result`);
    assert.sameValue(datetime.microsecond, microsecond, `${description} microsecond result`);
    assert.sameValue(datetime.nanosecond, nanosecond, `${description} nanosecond result`);
  },

  /*
   * assertPlainMonthDay(monthDay, monthCode, day[, description]):
   *
   * Shorthand for asserting that each field of a Temporal.PlainMonthDay is
   * equal to an expected value. (Except the `calendar` property, since callers
   * may want to assert either object equality with an object they put in there,
   * or the result of monthDay.calendar.toString().)
   */
  assertPlainMonthDay(monthDay, monthCode, day, description = "") {
    assert(monthDay instanceof Temporal.PlainMonthDay, `${description} instanceof`);
    assert.sameValue(monthDay.monthCode, monthCode, `${description} monthCode result`);
    assert.sameValue(monthDay.day, day, `${description} day result`);
  },

  /*
   * assertPlainTime(time, hour, ..., nanosecond[, description]):
   *
   * Shorthand for asserting that each field of a Temporal.PlainTime is equal to
   * an expected value.
   */
  assertPlainTime(time, hour, minute, second, millisecond, microsecond, nanosecond, description = "") {
    assert(time instanceof Temporal.PlainTime, `${description} instanceof`);
    assert.sameValue(time.hour, hour, `${description} hour result`);
    assert.sameValue(time.minute, minute, `${description} minute result`);
    assert.sameValue(time.second, second, `${description} second result`);
    assert.sameValue(time.millisecond, millisecond, `${description} millisecond result`);
    assert.sameValue(time.microsecond, microsecond, `${description} microsecond result`);
    assert.sameValue(time.nanosecond, nanosecond, `${description} nanosecond result`);
  },

  /*
   * assertPlainYearMonth(yearMonth, year, month, monthCode[, description[, era, eraYear]]):
   *
   * Shorthand for asserting that each field of a Temporal.PlainYearMonth is
   * equal to an expected value. (Except the `calendar` property, since callers
   * may want to assert either object equality with an object they put in there,
   * or the result of yearMonth.calendar.toString().)
   */
  assertPlainYearMonth(yearMonth, year, month, monthCode, description = "", era = undefined, eraYear = undefined) {
    assert(yearMonth instanceof Temporal.PlainYearMonth, `${description} instanceof`);
    assert.sameValue(yearMonth.era, era, `${description} era result`);
    assert.sameValue(yearMonth.eraYear, eraYear, `${description} eraYear result`);
    assert.sameValue(yearMonth.year, year, `${description} year result`);
    assert.sameValue(yearMonth.month, month, `${description} month result`);
    assert.sameValue(yearMonth.monthCode, monthCode, `${description} monthCode result`);
  },
  /*
   * Check that any calendar-carrying Temporal object has its [[Calendar]]
   * internal slot read by ToTemporalCalendar, and does not fetch the calendar
   * by calling getters.
   * The custom calendar object is passed in to func() so that it can do its
   * own additional assertions involving the calendar if necessary. (Sometimes
   * there is nothing to assert as the calendar isn't stored anywhere that can
   * be asserted about.)
   */
  checkToTemporalCalendarFastPath(func) {
    class CalendarFastPathCheck extends Temporal.Calendar {
      constructor() {
        super("iso8601");
      }

      toString() {
        return "fast-path-check";
      }
    }
    const calendar = new CalendarFastPathCheck();

    const plainDate = new Temporal.PlainDate(2000, 5, 2, calendar);
    const plainDateTime = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321, calendar);
    const plainMonthDay = new Temporal.PlainMonthDay(5, 2, calendar);
    const plainYearMonth = new Temporal.PlainYearMonth(2000, 5, calendar);
    const zonedDateTime = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", calendar);

    [plainDate, plainDateTime, plainMonthDay, plainYearMonth, zonedDateTime].forEach((temporalObject) => {
      const actual = [];
      const expected = [];

      Object.defineProperty(temporalObject, "calendar", {
        get() {
          actual.push("get calendar");
          return calendar;
        },
      });

      func(temporalObject, calendar);
      assert.compareArray(actual, expected, "calendar getter not called");
    });
  },

  /*
   * specificOffsetTimeZone():
   *
   * This returns an instance of a custom time zone class, which returns a
   * specific custom value from its getOffsetNanosecondsFrom() method. This is
   * for the purpose of testing the validation of what this method returns.
   *
   * It also returns an empty array from getPossibleInstantsFor(), so as to
   * trigger calls to getOffsetNanosecondsFor() when used from the
   * BuiltinTimeZoneGetInstantFor operation.
   */
  specificOffsetTimeZone(offsetValue) {
    class SpecificOffsetTimeZone extends Temporal.TimeZone {
      constructor(offsetValue) {
        super("UTC");
        this._offsetValue = offsetValue;
      }

      getOffsetNanosecondsFor() {
        return this._offsetValue;
      }

      getPossibleInstantsFor() {
        return [];
      }
    }
    return new SpecificOffsetTimeZone(offsetValue);
  },
};
