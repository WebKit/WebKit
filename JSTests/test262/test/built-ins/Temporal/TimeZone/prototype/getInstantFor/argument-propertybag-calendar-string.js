// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getinstantfor
description: A calendar ID is valid input for Calendar
features: [Temporal]
---*/

const instance = new Temporal.TimeZone("UTC");

const calendar = "iso8601";

const arg = { year: 1976, monthCode: "M11", day: 18, calendar };
const result = instance.getInstantFor(arg);
assert.sameValue(result.epochNanoseconds, 217_123_200_000_000_000n, `Calendar created from string "${calendar}"`);
