// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaindatetimeiso
description: Conversion of ISO date-time strings to Temporal.TimeZone instances
features: [Temporal]
---*/

let timeZone = "2021-08-19T17:30";
assert.throws(RangeError, () => Temporal.Now.plainDateTimeISO(timeZone), "bare date-time string is not a time zone");
assert.throws(RangeError, () => Temporal.Now.plainDateTimeISO({ timeZone }), "bare date-time string is not a time zone");

// The following are all valid strings so should not throw:

[
  "2021-08-19T17:30Z",
  "2021-08-19T1730Z",
  "2021-08-19T17:30-07:00",
  "2021-08-19T1730-07:00",
  "2021-08-19T17:30-0700",
  "2021-08-19T1730-0700",
  "2021-08-19T17:30[UTC]",
  "2021-08-19T1730[UTC]",
  "2021-08-19T17:30Z[UTC]",
  "2021-08-19T1730Z[UTC]",
  "2021-08-19T17:30-07:00[UTC]",
  "2021-08-19T1730-07:00[UTC]",
  "2021-08-19T17:30-0700[UTC]",
  "2021-08-19T1730-0700[UTC]",
].forEach((timeZone) => {
  Temporal.Now.plainDateTimeISO(timeZone);
  Temporal.Now.plainDateTimeISO({ timeZone });
});
