// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.withcalendar
description: Objects of a subclass are never created as return values.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const customCalendar = {
  era() { return undefined; },
  eraYear() { return undefined; },
  year() { return 1900; },
  month() { return 2; },
  monthCode() { return "M02"; },
  day() { return 5; },
  toString() { return "custom-calendar"; },
};

TemporalHelpers.checkSubclassingIgnored(
  Temporal.PlainDate,
  [2000, 5, 2],
  "withCalendar",
  [customCalendar],
  (result) => {
    TemporalHelpers.assertPlainDate(result, 1900, 2, "M02", 5);
    assert.sameValue(result.calendar, customCalendar, "calendar result");
  },
);
