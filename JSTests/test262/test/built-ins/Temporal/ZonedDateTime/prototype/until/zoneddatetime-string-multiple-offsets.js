// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.until
description: ZonedDateTime strings with UTC offset fractional part are not confused with time fractional part
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const timeZone = new Temporal.TimeZone("+00:01:30.987654321");
const instance = new Temporal.ZonedDateTime(0n, timeZone);
const str = "1970-01-01T00:02:00.000000000+00:02[+00:01:30.987654321]";

const result = instance.until(str);
TemporalHelpers.assertDuration(result, 0, 0, 0, 0, 0, 0, 29, 12, 345, 679, "Time zone determined from bracket name");
