// Copyright 2019 Googe Inc. All rights reserved.
// Copyright 2020 Apple Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-initializedatetimeformat
description: Checks the order of getting options of 'fractionalSecondDigits' for the DateTimeFormat constructor.
info: |
  ToDateTimeOptions ( options, required, defaults )
  5. If required is "time" or "any", then
    a. For each of the property names "hour", "minute", "second", "fractionalSecondDigits", do

  InitializeDateTimeFormat ( dateTimeFormat, locales, options )
  2. Let options be ? ToDateTimeOptions(options, "any", "date").
  4. Let matcher be ? GetOption(options, "localeMatcher", "string", «  "lookup", "best fit" », "best fit").
  22. For each row of Table 5, except the header row, do
    a. Let value be ? GetOption(options, prop, "string", « the strings given in the Values column of the row », undefined).
  23. Let _opt_.[[FractionalSecondDigits]] be ? GetNumberOption(_options_, `"fractionalSecondDigits"`, 0, 3, 0).
  26. Let matcher be ? GetOption(options, "formatMatcher", "string", «  "basic", "best fit" », "best fit").
includes: [compareArray.js]
features: [Intl.DateTimeFormat-fractionalSecondDigits]
---*/

// Just need to ensure fractionalSecondDigits are get
// between second and localeMatcher the first time and
// between timeZoneName and formatMatcher the second time.
const expected = [
  // InitializeDateTimeFormat step 2.
  //  ToDateTimeOptions step 5.
  "second", "fractionalSecondDigits",
  // InitializeDateTimeFormat step 4.
  "localeMatcher",
  // InitializeDateTimeFormat step 22.
  "second",
  "fractionalSecondDigits",
  "timeZoneName",
  // InitializeDateTimeFormat step 26.
  "formatMatcher",
];

const actual = [];

const options = {
  get second() {
    actual.push("second");
    return "numeric";
  },
  get fractionalSecondDigits() {
    actual.push("fractionalSecondDigits");
    return undefined;
  },
  get localeMatcher() {
    actual.push("localeMatcher");
    return undefined;
  },
  get timeZoneName() {
    actual.push("timeZoneName");
    return undefined;
  },
  get formatMatcher() {
    actual.push("formatMatcher");
    return undefined;
  },
};

new Intl.DateTimeFormat("en", options);
assert.compareArray(actual, expected);
