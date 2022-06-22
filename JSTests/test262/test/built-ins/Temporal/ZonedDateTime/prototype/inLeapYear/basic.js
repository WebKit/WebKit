// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.inleapyear
description: Basic test for inLeapYear
features: [Temporal]
---*/

assert.sameValue((new Temporal.ZonedDateTime(217178610123456789n, "UTC")).inLeapYear,
  true, "leap year");
assert.sameValue((new Temporal.ZonedDateTime(248714610123456789n, "UTC")).inLeapYear,
  false, "non-leap year");
