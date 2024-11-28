// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: time.toLocaleString()
features: [Temporal]
---*/

// Tolerate implementation variance by expecting consistency without being prescriptive.
// TODO: can we change tests to be less reliant on CLDR formats while still testing that
// Temporal and Intl are behaving as expected?
const usDayPeriodSpace =
  new Intl.DateTimeFormat("en-US", { timeStyle: "short" })
    .formatToParts(0)
    .find((part, i, parts) => part.type === "literal" && parts[i + 1].type === "dayPeriod")?.value || "";

var time = Temporal.PlainTime.from("1976-11-18T15:23:30");
assert.sameValue(`${time.toLocaleString("en-US", { timeZone: "America/New_York" })}`, `3:23:30${usDayPeriodSpace}PM`);
assert.sameValue(`${time.toLocaleString("de-AT", { timeZone: "Europe/Vienna" })}`, "15:23:30");
