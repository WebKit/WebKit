// Copyright (C) 2023 Justin Grant. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.from
description: Time zone strings with UTC offset fractional part are not confused with time fractional part
features: [Temporal]
---*/

const timeZone = "2021-08-19T17:30:45.123456789-12:12[+01:46]";

const result = Temporal.TimeZone.from(timeZone);
assert.sameValue(result.equals("+01:46"), true, "Time zone string determined from bracket name");
