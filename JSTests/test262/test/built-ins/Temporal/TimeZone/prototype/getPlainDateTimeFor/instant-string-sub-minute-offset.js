// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getplaindatetimefor
description: Temporal.Instant string with sub-minute offset
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.TimeZone("UTC");

const str = "1970-01-01T00:19:32.37+00:19:32.37";
const result = instance.getPlainDateTimeFor(str);
TemporalHelpers.assertPlainDateTime(result, 1970, 1, "M01", 1, 0, 0, 0, 0, 0, 0, "if present, sub-minute offset is accepted exactly");
