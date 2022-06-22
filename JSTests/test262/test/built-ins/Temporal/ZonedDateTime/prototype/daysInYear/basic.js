// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.daysinyear
description: Checking days in year for a "normal" case (non-undefined, non-boundary case, etc.)
features: [Temporal]
---*/

assert.sameValue((new Temporal.ZonedDateTime(217178610123456789n, "UTC")).daysInYear,
  366, "leap year");
assert.sameValue((new Temporal.ZonedDateTime(248714610123456789n, "UTC")).daysInYear,
  365, "non-leap year");
