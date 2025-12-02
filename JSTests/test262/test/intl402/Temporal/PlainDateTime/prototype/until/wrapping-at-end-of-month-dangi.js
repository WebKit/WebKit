// Copyright (C) 2025 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.until
description: Tests balancing of days to months at end of month (dangi calendar)
includes: [temporalHelpers.js]
features: [Temporal, Intl.Era-monthcode]
---*/

const calendar = "dangi";

// Difference between end of 30-day month to end of following 29-day month
{
  const end = Temporal.PlainDateTime.from({ year: 2023, monthCode: "M06", day: 29, hour: 12, minute: 34, calendar });
  for (const largestUnit of ["years", "months"]) {
    TemporalHelpers.assertDuration(
      Temporal.PlainDateTime.from({ year: 2023, monthCode: "M05", day: 29, hour: 12, minute: 34, calendar }).until(end, { largestUnit }),
      0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
      `M05-29 (30d) to M06-29 (29d) is one month (${largestUnit})`
    );
    TemporalHelpers.assertDuration(
      Temporal.PlainDateTime.from({ year: 2023, monthCode: "M05", day: 30, hour: 12, minute: 34, calendar }).until(end, { largestUnit }),
      0, 0, 0, 29, 0, 0, 0, 0, 0, 0,
      `M05-30 to M06-29 is 29 days, not one month (${largestUnit})`
    );
  }
}

// Difference between end of 30-day M04 to end of 29-day M04L
{
  const end = Temporal.PlainDateTime.from({ year: 2020, monthCode: "M04L", day: 29, hour: 12, minute: 34, calendar });
  for (const largestUnit of ["years", "months"]) {
    TemporalHelpers.assertDuration(
      Temporal.PlainDateTime.from({ year: 2020, monthCode: "M04", day: 29, hour: 12, minute: 34, calendar }).until(end, { largestUnit }),
      0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
      `M04-29 (30d) to M04L-29 (29d) is one month (${largestUnit})`
    );
    TemporalHelpers.assertDuration(
      Temporal.PlainDateTime.from({ year: 2020, monthCode: "M04", day: 30, hour: 12, minute: 34, calendar }).until(end, { largestUnit }),
      0, 0, 0, 29, 0, 0, 0, 0, 0, 0,
      `M04-30 to M04L-29 (29d) is 29 days, not one month (${largestUnit})`
    );
  }
}

// Difference between end of 30-day month to end of not-immediately-following
// 29-day month
{
  const end = Temporal.PlainDateTime.from({ year: 2023, monthCode: "M09", day: 29, hour: 12, minute: 34, calendar });
  for (const largestUnit of ["years", "months"]) {
    TemporalHelpers.assertDuration(
      Temporal.PlainDateTime.from({ year: 2023, monthCode: "M05", day: 29, hour: 12, minute: 34, calendar }).until(end, { largestUnit }),
      0, 4, 0, 0, 0, 0, 0, 0, 0, 0,
      `M05-29 (30d) to M09-29 (29d) is 4 months (${largestUnit})`
    );
    TemporalHelpers.assertDuration(
      Temporal.PlainDateTime.from({ year: 2023, monthCode: "M05", day: 30, hour: 12, minute: 34, calendar }).until(end, { largestUnit }),
      0, 3, 0, 29, 0, 0, 0, 0, 0, 0,
      `M05-30 to M09-29 (29d) is 3 months 29 days, not 4 months (${largestUnit})`
    );
  }
}

// Difference between end of 30-day month in one year to 29-day month in later
// year
{
  const end = Temporal.PlainDateTime.from({ year: 2023, monthCode: "M09", day: 29, hour: 12, minute: 34, calendar });
  TemporalHelpers.assertDuration(
    Temporal.PlainDateTime.from({ year: 2021, monthCode: "M05", day: 29, hour: 12, minute: 34, calendar }).until(end, { largestUnit: "months" }),
    0, 29, 0, 0, 0, 0, 0, 0, 0, 0,
    "2021-M05-29 (30d) to 2023-M09-29 (29d) is 29 days"
  );
  TemporalHelpers.assertDuration(
    Temporal.PlainDateTime.from({ year: 2021, monthCode: "M05", day: 29, hour: 12, minute: 34, calendar }).until(end, { largestUnit: "years" }),
    2, 4, 0, 0, 0, 0, 0, 0, 0, 0,
    "2021-M05-29 (30d) to 2023-M09-29 (29d) is 2 years, 4 months"
  );
  TemporalHelpers.assertDuration(
    Temporal.PlainDateTime.from({ year: 2021, monthCode: "M05", day: 30, hour: 12, minute: 34, calendar }).until(end, { largestUnit: "months" }),
    0, 28, 0, 29, 0, 0, 0, 0, 0, 0,
    "2021-M05-30 to 2023-M09-29 (29d) is 28 months, 29 days, not 29 months"
  );
  TemporalHelpers.assertDuration(
    Temporal.PlainDateTime.from({ year: 2021, monthCode: "M05", day: 30, hour: 12, minute: 34, calendar }).until(end, { largestUnit: "years" }),
    2, 3, 0, 29, 0, 0, 0, 0, 0, 0,
    "2021-M05-30 to 2023-M09-29 (29d) is 2 years, 3 months, 29 days, not 2 years 4 months"
  );
}
