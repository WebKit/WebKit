// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.toPlainMonthDay()
features: [Temporal]
---*/

var tz = new Temporal.TimeZone("America/Los_Angeles");

// works
var zdt = Temporal.Instant.from("2019-10-29T09:46:38.271986102Z").toZonedDateTimeISO(tz);
assert.sameValue(`${ zdt.toPlainMonthDay() }`, "10-29");

// preserves the calendar
var zdt = Temporal.Instant.from("2019-10-29T09:46:38.271986102Z").toZonedDateTime({
  timeZone: tz,
  calendar: "gregory"
});
assert.sameValue(zdt.toPlainMonthDay().calendar.id, "gregory");
