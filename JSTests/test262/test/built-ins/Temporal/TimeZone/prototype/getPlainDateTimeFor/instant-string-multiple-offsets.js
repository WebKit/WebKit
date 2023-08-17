// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getplaindatetimefor
description: Instant strings with UTC offset fractional part are not confused with time fractional part
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.TimeZone("UTC");
const str = "1970-01-01T00:02:00.000000000+00:02[+01:30]";

const result = instance.getPlainDateTimeFor(str);
TemporalHelpers.assertPlainDateTime(result, 1970, 1, "M01", 1, 0, 0, 0, 0, 0, 0, "UTC offset determined from offset part of string");
