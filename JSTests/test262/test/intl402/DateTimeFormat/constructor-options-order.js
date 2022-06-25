// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-initializedatetimeformat
description: Checks the order of getting options for the DateTimeFormat constructor.
includes: [compareArray.js]
---*/

const expected = [
  // ToDateTimeOptions step 4.
  "weekday", "year", "month", "day",
  // ToDateTimeOptions step 5.
  "hour", "minute", "second",

  // InitializeDateTimeFormat step 4.
  "localeMatcher",
  // InitializeDateTimeFormat step 6.
  "hour12",
  // InitializeDateTimeFormat step 7.
  "hourCycle",
  // InitializeDateTimeFormat step 17.
  "timeZone",
  // InitializeDateTimeFormat step 22.
  "weekday",
  "era",
  "year",
  "month",
  "day",
  "hour",
  "minute",
  "second",
  "timeZoneName",
  // InitializeDateTimeFormat step 25.
  "formatMatcher",
];

const actual = [];

const options = {
  get day() {
    actual.push("day");
    return "numeric";
  },

  get era() {
    actual.push("era");
    return "long";
  },

  get formatMatcher() {
    actual.push("formatMatcher");
    return "best fit";
  },

  get hour() {
    actual.push("hour");
    return "numeric";
  },

  get hour12() {
    actual.push("hour12");
    return true;
  },

  get hourCycle() {
    actual.push("hourCycle");
    return "h24";
  },

  get localeMatcher() {
    actual.push("localeMatcher");
    return "best fit";
  },

  get minute() {
    actual.push("minute");
    return "numeric";
  },

  get month() {
    actual.push("month");
    return "numeric";
  },

  get second() {
    actual.push("second");
    return "numeric";
  },

  get timeZone() {
    actual.push("timeZone");
    return "UTC";
  },

  get timeZoneName() {
    actual.push("timeZoneName");
    return "long";
  },

  get weekday() {
    actual.push("weekday");
    return "long";
  },

  get year() {
    actual.push("year");
    return "numeric";
  },
};

new Intl.DateTimeFormat("en", options);

assert.compareArray(actual, expected);
