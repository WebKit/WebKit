// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: monthday.toLocaleString()
features: [Temporal]
---*/

var calendar = new Intl.DateTimeFormat("en-US").resolvedOptions().calendar;
var monthday = Temporal.PlainMonthDay.from({
  monthCode: "M11",
  day: 18,
  calendar
});
assert.sameValue(`${ monthday.toLocaleString("en-US", { timeZone: "America/New_York" }) }`, "11/18");
assert.sameValue(`${ monthday.toLocaleString("de-AT", {
  timeZone: "Europe/Vienna",
  calendar
}) }`, "18.11.");

// should ignore units not in the data type
assert.sameValue(monthday.toLocaleString("en-US", { timeZoneName: "long" }), "11/18");
assert.sameValue(monthday.toLocaleString("en-US", { year: "numeric" }), "11/18");
assert.sameValue(monthday.toLocaleString("en-US", { hour: "numeric" }), "11/18");
assert.sameValue(monthday.toLocaleString("en-US", { minute: "numeric" }), "11/18");
assert.sameValue(monthday.toLocaleString("en-US", { second: "numeric" }), "11/18");
assert.sameValue(monthday.toLocaleString("en-US", { weekday: "long" }), "11/18");
