// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.toPlainDate()
features: [Temporal]
---*/

var tz = new Temporal.TimeZone("-07:00");

// works
var zdt = Temporal.Instant.from("2019-10-29T09:46:38.271986102Z").toZonedDateTimeISO(tz);
assert.sameValue(`${ zdt.toPlainDate() }`, "2019-10-29");

// preserves the calendar
const fakeGregorian = { toString() { return "gregory" }};
var zdt = Temporal.Instant.from("2019-10-29T09:46:38.271986102Z").toZonedDateTime({
  timeZone: tz,
  calendar: fakeGregorian
});
assert.sameValue(zdt.toPlainDate().calendar, fakeGregorian);
