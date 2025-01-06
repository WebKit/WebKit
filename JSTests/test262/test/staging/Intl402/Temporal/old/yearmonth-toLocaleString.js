// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: yearmonth.toLocaleString()
features: [Temporal]
---*/

// Tolerate implementation variance by expecting consistency without being prescriptive.
// TODO: can we change tests to be less reliant on CLDR formats while still testing that
// Temporal and Intl are behaving as expected?
// Workarounds for https://unicode-org.atlassian.net/browse/CLDR-16243
const deMonthDayRangeSeparator = new Intl.DateTimeFormat("de-AT", { month: "numeric", day: "numeric" })
  .formatRangeToParts(1 * 86400 * 1000, 90 * 86400 * 1000)
  .find((part) => part.type === "literal" && part.source === "shared").value;
const deMonthYearSeparator = new Intl.DateTimeFormat("de-AT", { year: "numeric", month: "numeric" })
  .formatToParts(0)
  .find((part) => part.type === "literal").value;

var calendar = new Intl.DateTimeFormat("en-US").resolvedOptions().calendar;
var yearmonth = Temporal.PlainYearMonth.from({
  year: 1976,
  month: 11,
  calendar
});
assert.sameValue(`${yearmonth.toLocaleString("en-US", { timeZone: "America/New_York" })}`, "11/1976");
assert.sameValue(
  `${yearmonth.toLocaleString("de-AT", { timeZone: "Europe/Vienna", calendar })}`,
  `11${deMonthYearSeparator}1976`
);
